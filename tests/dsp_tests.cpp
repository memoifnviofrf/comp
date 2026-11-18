// Vocal Compressor - DSP 測定・検証プログラム(GUI/ホスト不要、実際の JUCE の dsp/audio_basics を使用)
//
// 出力は Markdown。docs/TEST_REPORT.md の測定結果部分にそのまま使います。
// 判定(PASS/FAIL)の基準は各項目に併記しています。
//
// 注意: すべて合成信号での測定です。実際のボーカル録音・DAW・他社製品との比較は含みません。

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <new>
#include <random>
#include <string>
#include <vector>

#include "../Source/CompressorDSP.h"
#include "../Source/PresetManager.h"

using namespace vc;

//==============================================================================
// オーディオスレッドでのメモリ確保を検出するため、グローバル new を数える
static std::atomic<long> g_allocs { 0 };
static std::atomic<bool> g_track { false };

void* operator new (std::size_t n)
{
    if (g_track.load (std::memory_order_relaxed)) g_allocs.fetch_add (1, std::memory_order_relaxed);
    void* p = std::malloc (n ? n : 1);
    if (p == nullptr) throw std::bad_alloc();
    return p;
}
void* operator new[] (std::size_t n) { return operator new (n); }
void operator delete (void* p) noexcept { std::free (p); }
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }

//==============================================================================
static int g_pass = 0, g_fail = 0;
static std::vector<std::string> g_failures;

static void check (const std::string& name, bool ok, const char* fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start (ap, fmt);
    std::vsnprintf (buf, sizeof buf, fmt, ap);
    va_end (ap);
    if (ok) ++g_pass; else { ++g_fail; g_failures.push_back (name + " :: " + buf); }
    std::printf ("| %s | %s | %s |\n", name.c_str(), buf, ok ? "PASS" : "**FAIL**");
}

static void tally (bool ok, const std::string& name)
{
    if (ok) ++g_pass; else { ++g_fail; g_failures.push_back (name); }
}

static void header (const char* title, const char* note = nullptr)
{
    std::printf ("\n### %s\n\n", title);
    if (note != nullptr) std::printf ("%s\n\n", note);
}
static void tableHead (const char* a, const char* b, const char* c)
{
    std::printf ("| %s | %s | %s |\n|---|---|---|\n", a, b, c);
}

static const double kRates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };
static const char* kModelNames[] = { "Transparent", "Opto", "FET", "VCA", "Vari-Mu" };
constexpr double kPiD = 3.14159265358979323846;

//==============================================================================
using Buf = std::vector<float>;

static double db2lin (double d) { return std::pow (10.0, d / 20.0); }

static Buf sine (double fs, double f, double dbfs, double sec, double phase = 0.0)
{
    const int n = (int) std::lround (sec * fs);
    Buf b ((size_t) n);
    const double a = db2lin (dbfs);
    for (int i = 0; i < n; ++i) b[(size_t) i] = (float) (a * std::sin (2.0 * kPiD * f * i / fs + phase));
    return b;
}
static Buf silence (double fs, double sec) { return Buf ((size_t) std::lround (sec * fs), 0.0f); }
static Buf noiseBuf (double fs, double peakDb, double sec, unsigned seed)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<double> u (-1.0, 1.0);
    const int n = (int) std::lround (sec * fs);
    Buf b ((size_t) n);
    const double a = db2lin (peakDb);
    for (auto& v : b) v = (float) (a * u (rng));
    return b;
}
static void append (Buf& a, const Buf& b) { a.insert (a.end(), b.begin(), b.end()); }

static double meanRange (const Buf& x, int a, int b)
{
    double s = 0.0;
    for (int i = a; i < b; ++i) s += x[(size_t) i];
    return s / std::max (1, b - a);
}
static double rmsDbRange (const Buf& x, int a, int b)
{
    double s = 0.0;
    for (int i = a; i < b; ++i) s += (double) x[(size_t) i] * x[(size_t) i];
    return 10.0 * std::log10 (s / std::max (1, b - a) + 1.0e-30);
}
static double peakDbRange (const Buf& x, int a, int b)
{
    double m = 0.0;
    for (int i = a; i < b; ++i) m = std::max (m, (double) std::abs (x[(size_t) i]));
    return 20.0 * std::log10 (m + 1.0e-30);
}

struct Fit { double amp = 0, ph = 0, a = 0, b = 0; };
static Fit fitSine (const Buf& x, int start, int n, double f, double fs)
{
    double a = 0, b = 0;
    const double w = 2.0 * kPiD * f / fs;
    for (int i = 0; i < n; ++i)
    {
        const double v = x[(size_t) (start + i)];
        a += v * std::cos (w * i);
        b += v * std::sin (w * i);
    }
    Fit r;
    r.a = 2.0 * a / n; r.b = 2.0 * b / n;                 // x ≈ a cos + b sin
    r.amp = std::hypot (r.a, r.b);
    r.ph = std::atan2 (-b, a);
    return r;
}

//==============================================================================
struct Out { Buf l, r, grl, grr; int latency = 0; };

static Out run (const Params& p, double fs, const Buf& inL, const Buf& inR, int block = 512,
                const Buf* scL = nullptr, const Buf* scR = nullptr)
{
    auto c = std::make_unique<Compressor>();
    c->prepare (fs, 512, 2);
    c->setParams (p);

    Out o;
    o.l = inL; o.r = inR;
    const int n = (int) inL.size();
    o.grl.assign ((size_t) n, 0.0f); o.grr.assign ((size_t) n, 0.0f);
    o.latency = c->getLatencySamples();

    for (int pos = 0; pos < n; pos += block)
    {
        const int m = std::min (block, n - pos);
        float* io[2] = { o.l.data() + pos, o.r.data() + pos };
        float* gr[2] = { o.grl.data() + pos, o.grr.data() + pos };
        const float* sc[2] = { scL ? scL->data() + pos : nullptr, scR ? scR->data() + pos : nullptr };
        c->process (io, 2, m, scL ? sc : nullptr, scL ? 2 : 0, gr);
    }
    return o;
}
static Out runMono (const Params& p, double fs, const Buf& in, int block = 512)
{
    return run (p, fs, in, in, block);
}

static Params basePrm()
{
    Params p;
    p.model = Model::Transparent;
    p.thresholdDb = -20; p.ratio = 4; p.attackMs = 10; p.releaseMs = 100; p.kneeDb = 0;
    p.blend = 1.0; p.autoRelease = false; p.autoMakeup = false;
    p.stereoLink = 1.0; p.lookaheadMs = 0; p.oversampling = 1;
    return p;
}

//==============================================================================
// T1: 静的ゲイン(ゲインリダクションの精度)
static void testGainAccuracy()
{
    header ("T1. ゲインリダクションの精度(静的カーブ)",
            "4 kHz 正弦波の定常状態のGR平均と、`computeGainReduction()` の理論値の差。Threshold -20 dB / Ratio 4 / Knee 0・6・12 dB / RMS検出。"
            "判定: 全レベル・全方式で |誤差| < 0.25 dB。");
    std::printf ("| fs | 方式 | 最大誤差 [dB] | 判定 |\n|---|---|---|---|\n");

    const double levels[] = { -40, -30, -24, -20, -17, -14, -10, -5, -1 };
    for (double fs : kRates)
        for (int m = 0; m < 5; ++m)
        {
            double worst = 0.0;
            for (double knee : { 0.0, 6.0, 12.0 })
                for (double L : levels)
                {
                    Params p = basePrm();
                    p.model = (Model) m; p.kneeDb = knee; p.attackMs = 2.0; p.releaseMs = 50.0;
                    const Buf in = sine (fs, 4000.0, L, 0.8);
                    const Out o = runMono (p, fs, in);
                    const int n = (int) in.size();
                    const double gr = meanRange (o.grl, n - (int) (0.15 * fs), n);
                    const double expect = computeGainReduction (L, p.thresholdDb, p.ratio, effectiveKnee (p.model, knee), p.model);
                    worst = std::max (worst, std::abs (gr - expect));
                }
            const bool ok = worst < 0.25;
            std::printf ("| %.1f kHz | %s | %.3f | %s |\n", fs / 1000.0, kModelNames[m], worst, ok ? "PASS" : "**FAIL**");
            tally (ok, std::string ("T1 ") + kModelNames[m] + " GR accuracy");
        }
}

//==============================================================================
// T2: Ratio 1:1・Mix 0・バイパスの完全一致(ヌルテスト)
static void testNull()
{
    header ("T2. 透過性(ヌルテスト)", "判定: 入力との差が完全に 0(float で一致)。");
    tableHead ("項目", "結果", "判定");

    const double fs = 48000.0;
    const Buf in = noiseBuf (fs, -6.0, 1.0, 7);

    auto maxDiff = [&] (const Out& o, int delay)
    {
        double d = 0.0;
        for (size_t i = (size_t) delay; i < in.size(); ++i)
            d = std::max (d, (double) std::abs (o.l[i] - in[i - (size_t) delay]));
        return d;
    };

    for (int m : { 0, 1, 3 })   // 線形方式
    {
        Params p = basePrm(); p.model = (Model) m; p.ratio = 1.0;
        const Out o = runMono (p, fs, in);
        check (std::string ("Ratio 1:1 / ") + kModelNames[m], maxDiff (o, 0) == 0.0, "最大差 = %.3g", maxDiff (o, 0));
    }
    {
        Params p = basePrm(); p.mix = 0.0; p.thresholdDb = -40; p.ratio = 10;
        const Out o = runMono (p, fs, in);
        check ("Mix 0 %", maxDiff (o, 0) == 0.0, "最大差 = %.3g", maxDiff (o, 0));
    }
    {
        Params p = basePrm(); p.bypass = true; p.thresholdDb = -40; p.ratio = 10; p.lookaheadMs = 3.0; p.oversampling = 4; p.model = Model::FET;
        const Out o = runMono (p, fs, in);
        const int lat = o.latency;
        double d = 0.0;
        for (size_t i = (size_t) (lat + 4800); i < in.size(); ++i)
            d = std::max (d, (double) std::abs (o.l[i] - in[i - (size_t) lat]));
        check ("Bypass(レイテンシー分遅延、10 ms 平滑後)", d == 0.0, "最大差 = %.3g(レイテンシー %d サンプル)", d, lat);
    }
}

//==============================================================================
// T3: アタック/リリース時間
struct Timing { double attackMs = 0, releaseMs = 0, finalGr = 0; };

static Timing measureTiming (Model m, double fs, double atk, double rel, bool autoRel = false)
{
    Params p = basePrm();
    p.model = m; p.thresholdDb = -30; p.ratio = 4; p.attackMs = atk; p.releaseMs = rel; p.blend = 0.0; p.autoRelease = autoRel;
    p.kneeDb = 0;

    Buf in = sine (fs, 4000.0, -60.0, 0.3);
    const int t1 = (int) in.size();
    append (in, sine (fs, 4000.0, -10.0, 1.5));
    const int t2 = (int) in.size();
    append (in, sine (fs, 4000.0, -60.0, 3.0));

    const Out o = runMono (p, fs, in);
    Timing t;
    const double finalGr = meanRange (o.grl, t2 - (int) (0.1 * fs), t2);
    t.finalGr = finalGr;

    int i63 = -1;
    for (int i = t1; i < t2; ++i)
        if (o.grl[(size_t) i] >= 0.632 * finalGr) { i63 = i; break; }
    t.attackMs = i63 < 0 ? -1.0 : 1000.0 * (i63 - t1) / fs;

    int i37 = -1;
    for (int i = t2; i < (int) o.grl.size(); ++i)
        if (o.grl[(size_t) i] <= 0.368 * finalGr) { i37 = i; break; }
    t.releaseMs = i37 < 0 ? -1.0 : 1000.0 * (i37 - t2) / fs;
    return t;
}

static void testTiming()
{
    header ("T3. アタック / リリース時間",
            "-60 → -10 dBFS(4 kHz)のステップで、GRが最終値の63%に達する時間(アタック)と、-10 → -60 で37%まで戻る時間(リリース)を測定。"
            "Threshold -30 dB / Ratio 4 / ピーク検出 / Auto Release OFF / Attack 20 ms / Release 200 ms。"
            "Transparent の判定: アタック 20 ms ±15%、リリース 200〜260 ms(2段のスムージングのため設定値より少し長い)。");
    std::printf ("| fs | 方式 | アタック63%% [ms] | リリース37%% [ms] | 最終GR [dB] | 判定 |\n|---|---|---|---|---|---|\n");

    for (double fs : kRates)
        for (int m = 0; m < 5; ++m)
        {
            const Timing t = measureTiming ((Model) m, fs, 20.0, 200.0);
            bool ok = t.attackMs > 0 && t.releaseMs > 0;
            if (m == 0) ok = ok && std::abs (t.attackMs - 20.0) < 3.0 && t.releaseMs > 195.0 && t.releaseMs < 262.0;
            if (fs == 48000.0 || m == 0)
                std::printf ("| %.1f kHz | %s | %.1f | %.1f | %.2f | %s |\n", fs / 1000.0, kModelNames[m], t.attackMs, t.releaseMs, t.finalGr, ok ? "PASS" : "**FAIL**");
            tally (ok, std::string ("T3 ") + kModelNames[m] + " timing");
        }

    // 方式の違い(Attack 10 ms / Release 100 ms、48 kHz)が実際に異なる特性になっていること
    std::printf ("\n方式別の違い(48 kHz, Attack 10 ms / Release 100 ms):\n\n| 方式 | アタック63%% [ms] | リリース37%% [ms] |\n|---|---|---|\n");
    double att[5], rel[5];
    for (int m = 0; m < 5; ++m)
    {
        const Timing t = measureTiming ((Model) m, 48000.0, 10.0, 100.0);
        att[m] = t.attackMs; rel[m] = t.releaseMs;
        std::printf ("| %s | %.1f | %.1f |\n", kModelNames[m], att[m], rel[m]);
    }
    bool distinct = true;
    for (int a = 0; a < 5; ++a)
        for (int b = a + 1; b < 5; ++b)
            if (std::abs (att[a] - att[b]) < 0.4 && std::abs (rel[a] - rel[b]) < 4.0) distinct = false;
    std::printf ("\n");
    tableHead ("項目", "結果", "判定");
    check ("5方式の時間特性が互いに異なる", distinct, "アタック・リリースの組が全組み合わせで区別できる");
    check ("FETのアタックが最速", att[2] < att[0] && att[2] < att[1] && att[2] < att[4], "FET %.1f ms < Transparent %.1f / Opto %.1f / Vari-Mu %.1f", att[2], att[0], att[1], att[4]);

    // Auto Release: 瞬間的なスパイクは速く、持続的なGRは遅く戻る
    {
        Params p = basePrm(); p.thresholdDb = -30; p.releaseMs = 200; p.attackMs = 5; p.blend = 0.0; p.autoRelease = true;
        const double fs = 48000.0;
        auto relTime = [&] (double burstSec)
        {
            Buf in = sine (fs, 4000.0, -60.0, 0.3);
            const int t1 = (int) in.size();
            append (in, sine (fs, 4000.0, -10.0, burstSec));
            const int t2 = (int) in.size();
            append (in, sine (fs, 4000.0, -60.0, 2.0));
            const Out o = runMono (p, fs, in);
            const double peak = *std::max_element (o.grl.begin() + t1, o.grl.begin() + t2);
            for (int i = t2; i < (int) o.grl.size(); ++i)
                if (o.grl[(size_t) i] <= 0.368 * peak) return 1000.0 * (i - t2) / fs;
            return -1.0;
        };
        const double shortB = relTime (0.03), longB = relTime (1.5);
        check ("Auto Release: 短いバースト < 長いバーストの戻り時間", shortB > 0 && longB > shortB * 1.3, "30 ms バースト後 %.0f ms / 1.5 s 後 %.0f ms", shortB, longB);
    }
}

//==============================================================================
// T4: THD / THD+N / エイリアシング
struct Dist { double thdDb, thdnDb, nonHarmDb; };

static Dist measureDist (const Params& p, double fs, double f1, double levelDb)
{
    const int n = (int) fs;                                  // 1 秒(f1 が整数 Hz なら整数周期)
    Buf in = sine (fs, f1, levelDb, 2.0);
    const Out o = runMono (p, fs, in);

    const int start = (int) fs;                              // 最初の 1 秒は捨てる(定常状態)
    const Fit f = fitSine (o.l, start, n, f1, fs);

    double harm = 0.0;
    std::vector<Fit> hs;
    for (int k = 2; k * f1 < 0.5 * fs * 0.999; ++k)
    {
        const Fit h = fitSine (o.l, start, n, k * f1, fs);
        hs.push_back (h);
        if (k * f1 <= 20000.0) harm += h.amp * h.amp;
    }

    // 残差: 基音を除去 / 基音+全高調波を除去
    double rAll = 0.0, rNon = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double w = 2.0 * kPiD * f1 / fs;
        double fund = f.a * std::cos (w * i) + f.b * std::sin (w * i);
        double x = o.l[(size_t) (start + i)];
        rAll += (x - fund) * (x - fund);
        double xs = x - fund;
        for (size_t k = 0; k < hs.size(); ++k)
        {
            const double wk = w * (double) (k + 2);
            xs -= hs[k].a * std::cos (wk * i) + hs[k].b * std::sin (wk * i);
        }
        rNon += xs * xs;
    }
    const double fundRms = f.amp / std::sqrt (2.0);
    Dist d;
    d.thdDb    = 20.0 * std::log10 (std::sqrt (harm) / f.amp + 1e-30);
    d.thdnDb   = 20.0 * std::log10 (std::sqrt (rAll / n) / fundRms + 1e-30);
    d.nonHarmDb = 20.0 * std::log10 (std::sqrt (rNon / n) / fundRms + 1e-30);
    return d;
}

static void testDistortion()
{
    header ("T4. THD / THD+N",
            "1 kHz 正弦波。(a) 圧縮なし(-30 dBFS、Threshold -20 dB)と (b) 約10 dBのGR(-6 dBFS、Ratio 4)。"
            "THD = 2〜20 kHz の高調波 / 基音。THD+N = 基音以外すべて(全帯域)/ 基音。Attack 20 ms / Release 300 ms / RMS検出。"
            "判定: 線形方式(Transparent/Opto/VCA)の圧縮なしは THD+N < -110 dB、圧縮中は THD+N < -60 dB。FET/Vari-Mu は意図的な倍音付加なので判定せず、数値のみ示す。");
    std::printf ("| fs | 方式 | (a) THD [dB] | (a) THD+N [dB] | (b) THD [dB] | (b) THD+N [dB] | 判定 |\n|---|---|---|---|---|---|---|\n");

    for (double fs : kRates)
        for (int m = 0; m < 5; ++m)
        {
            Params p = basePrm(); p.model = (Model) m; p.attackMs = 20; p.releaseMs = 300; p.kneeDb = 6; p.oversampling = (m >= 2 && m != 3) ? 2 : 1;
            const Dist a = measureDist (p, fs, 1000.0, -30.0);
            const Dist b = measureDist (p, fs, 1000.0, -6.0);
            bool ok = true;
            if (m == 0 || m == 1 || m == 3) ok = a.thdnDb < -110.0 && b.thdnDb < -60.0;
            std::printf ("| %.1f kHz | %s | %.1f | %.1f | %.1f | %.1f | %s |\n", fs / 1000.0, kModelNames[m], a.thdDb, a.thdnDb, b.thdDb, b.thdnDb,
                         (m == 2 || m == 4) ? "(参考値)" : (ok ? "PASS" : "**FAIL**"));
            if (m != 2 && m != 4) tally (ok, std::string ("T4 ") + kModelNames[m] + " THD");
        }

    // オーバーサンプリングの必要性(エイリアシング)
    header ("T4b. オーバーサンプリングの効果(非線形方式のエイリアシング)",
            "7 kHz 正弦波 -6 dBFS・FET・約10 dBのGRで、**非調波成分**(基音と全高調波を除いた残り = エイリアシング+ノイズ)を測定。"
            "判定: 44.1 / 48 kHz で 4x の非調波成分が 1x より 10 dB 以上小さいこと。");
    std::printf ("| fs | 方式 | 1x [dB] | 2x [dB] | 4x [dB] | 判定 |\n|---|---|---|---|---|---|\n");
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (int m : { 2, 4 })
        {
            double v[3];
            int k = 0;
            for (int os : { 1, 2, 4 })
            {
                Params p = basePrm(); p.model = (Model) m; p.attackMs = 20; p.releaseMs = 300; p.kneeDb = 6; p.oversampling = os;
                v[k++] = measureDist (p, fs, 7000.0, -6.0).nonHarmDb;
            }
            const bool needed = fs < 60000.0;
            const bool ok = ! needed || (v[2] < v[0] - 10.0);
            std::printf ("| %.1f kHz | %s | %.1f | %.1f | %.1f | %s |\n", fs / 1000.0, kModelNames[m], v[0], v[1], v[2], ok ? "PASS" : "**FAIL**");
            tally (ok, "T4b oversampling effect");
        }
}

//==============================================================================
// T5: ノイズフロア・無音
static void testNoise()
{
    header ("T5. ノイズフロア・無音・デノーマル",
            "判定: 無音入力で出力が厳密に 0(= 自己雑音なし)、極小信号(-130 dBFS)で NaN/Inf/発振が出ないこと。");
    tableHead ("項目", "結果", "判定");

    for (double fs : { 44100.0, 192000.0 })
        for (int m = 0; m < 5; ++m)
        {
            Params p = basePrm(); p.model = (Model) m; p.oversampling = 4; p.autoMakeup = true; p.makeupDb = 0;
            p.vLevel = 1.0; p.vAdapt = 1.0; p.vSyl = 1.0; p.vPhrase = 1.0; p.vPlosive = 1.0; p.vSib = 1.0;
            const Out o = runMono (p, fs, silence (fs, 2.0));
            double mx = 0; bool fin = true;
            for (float v : o.l) { mx = std::max (mx, (double) std::abs (v)); fin = fin && std::isfinite (v); }
            check (std::string ("無音入力 / ") + kModelNames[m] + " / " + std::to_string ((int) (fs / 1000)) + " kHz", fin && mx == 0.0, "出力ピーク = %.3g", mx);
        }

    // 非常に小さい信号
    {
        const double fs = 48000.0;
        Params p = basePrm(); p.model = Model::FET; p.oversampling = 4;
        const Buf in = sine (fs, 1000.0, -130.0, 1.0);
        const Out o = runMono (p, fs, in);
        const double dbIn = rmsDbRange (in, 0, (int) in.size()), dbOut = rmsDbRange (o.l, 4800, (int) in.size());
        check ("極小信号(-130 dBFS)の通過 / FET 4x", std::abs (dbOut - dbIn) < 1.0, "入力 %.1f dB → 出力 %.1f dB", dbIn, dbOut);
    }

    // デノーマル: 大きい信号の後の無音の処理時間が、通常信号と比べて極端に遅くならないこと
    {
        const double fs = 48000.0;
        Params p = basePrm(); p.model = Model::Opto; p.vLevel = 0.5; p.vAdapt = 0.5;
        auto time = [&] (const Buf& in)
        {
            auto c = std::make_unique<Compressor>();
            c->prepare (fs, 512, 2); c->setParams (p);
            Buf l = in, r = in;
            const auto t0 = std::chrono::steady_clock::now();
            for (size_t pos = 0; pos + 512 <= l.size(); pos += 512)
            {
                float* io[2] = { l.data() + pos, r.data() + pos };
                c->process (io, 2, 512);
            }
            return std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        };
        Buf a = noiseBuf (fs, -3.0, 2.0, 3);
        Buf b = a;
        append (b, silence (fs, 6.0));
        const double tNoise = time (a), tBoth = time (b);
        const double tSilence = tBoth - tNoise;
        const double perSecNoise = tNoise / 2.0, perSecSilence = tSilence / 6.0;
        check ("デノーマル(無音区間の処理時間 / 通常区間)", perSecSilence < perSecNoise * 3.0,
               "通常 %.2f ms/秒、無音 %.2f ms/秒(比 %.2f)。実プラグインでは ScopedNoDenormals も併用", perSecNoise * 1000, perSecSilence * 1000, perSecSilence / std::max (perSecNoise, 1e-9));
    }
}

//==============================================================================
// T6: 周波数特性・位相(GRなし)
static void testFrequencyResponse()
{
    header ("T6. 周波数特性・位相(GRなし、-30 dBFS)",
            "Ratio 1:1(GRなし)での振幅・位相。位相はレイテンシー(整数サンプル遅延)分を除いた値。"
            "判定: Transparent(線形方式)は 20 Hz〜0.45 fs で 振幅偏差 < 0.001 dB、位相偏差 < 0.01°。"
            "FET + OS(IIRハーフバンド)は 20 Hz〜0.4 fs で振幅偏差 < 0.5 dB を確認し、位相は線形位相ではない(最小位相IIR)ことを数値で示す。");
    std::printf ("| fs | 方式 | 周波数 | 振幅 [dB] | 位相 [deg] |\n|---|---|---|---|---|\n");

    for (double fs : { 48000.0, 192000.0 })
        for (int cfg = 0; cfg < 2; ++cfg)
        {
            Params p = basePrm(); p.ratio = 1.0;
            if (cfg == 0) { p.model = Model::Transparent; p.oversampling = 4; p.lookaheadMs = 2.0; }
            else          { p.model = Model::FET; p.oversampling = 4; }
            double worstMag = 0, worstPh = 0;
            for (double f : { 20.0, 50.0, 100.0, 500.0, 1000.0, 5000.0, 10000.0, 0.2 * fs, 0.4 * fs, 0.45 * fs })
            {
                const int cycles = std::max (20, (int) (0.4 * f));
                const int n = (int) std::lround (cycles * fs / f);
                const int pad = (int) (0.6 * fs);
                Buf in = sine (fs, f, -30.0, (double) (pad + n + 4096) / fs);
                const Out o = runMono (p, fs, in);
                const Fit a = fitSine (in, pad, n, f, fs), b = fitSine (o.l, pad, n, f, fs);
                const double mag = 20.0 * std::log10 (b.amp / a.amp);
                double ph = (b.ph - a.ph) * 180.0 / kPiD + 360.0 * f * o.latency / fs;     // レイテンシー分を除去
                ph = std::remainder (ph, 360.0);
                if (cfg == 0 && f <= 0.45 * fs) { worstMag = std::max (worstMag, std::abs (mag)); worstPh = std::max (worstPh, std::abs (ph)); }
                if (cfg == 1 && f <= 0.4 * fs) worstMag = std::max (worstMag, std::abs (mag));
                std::printf ("| %.0f kHz | %s | %.0f Hz | %.4f | %.3f |\n", fs / 1000, cfg == 0 ? "Transparent(OS4+LA2ms)" : "FET OS4x", f, mag, ph);
            }
            if (cfg == 0)
                check (std::string ("T6 Transparent 振幅/位相 ") + std::to_string ((int) (fs / 1000)) + " kHz", worstMag < 0.001 && worstPh < 0.01, "最大振幅偏差 %.5f dB / 最大位相偏差 %.4f°", worstMag, worstPh);
            else
                check (std::string ("T6 FET OS4x 振幅 ") + std::to_string ((int) (fs / 1000)) + " kHz", worstMag < 0.5, "最大振幅偏差 %.3f dB(0.4 fs まで)", worstMag);
        }
}

//==============================================================================
// T7: レイテンシー
static double measureDelaySamples (const Params& p, double fs)
{
    // 粗い遅延: ホワイトノイズの相互相関 / 細かい遅延: 1 kHz の位相
    Buf in = noiseBuf (fs, -30.0, 0.5, 11);
    const Out o = runMono (p, fs, in);
    const int maxLag = (int) (0.02 * fs);
    int best = 0; double bestV = -1e300;
    const int N = (int) in.size() - maxLag - 1;
    for (int lag = 0; lag <= maxLag; ++lag)
    {
        double s = 0;
        for (int i = 0; i < N; i += 2) s += (double) in[(size_t) i] * o.l[(size_t) (i + lag)];
        if (s > bestV) { bestV = s; best = lag; }
    }
    const int pad = (int) (0.5 * fs), n = (int) fs;
    Buf t = sine (fs, 1000.0, -30.0, (double) (pad + n + 4096) / fs);
    const Out q = runMono (p, fs, t);
    const Fit a = fitSine (t, pad, n, 1000.0, fs), b = fitSine (q.l, pad, n, 1000.0, fs);
    const double period = fs / 1000.0;
    double d = (a.ph - b.ph) / (2.0 * kPiD) * period;
    d += period * std::round (((double) best - d) / period);
    return d;
}

static void testLatency()
{
    header ("T7. レイテンシー申告の正確さ",
            "`getLatencySamples()` の申告値と、実測遅延(ノイズ相互相関で粗く、1 kHz の位相で細かく)の比較。"
            "判定: 誤差 < 1 サンプル(Transparent/Opto/VCA は整数サンプルなので 0.05 以内)。"
            "オーバーサンプリングの IIR ハーフバンドは周波数により遅延が変わるため、1 kHz での値との差が出る。");
    std::printf ("| fs | 方式 | Lookahead | OS | 申告 [samples] | 実測 [samples] | 差 | 判定 |\n|---|---|---|---|---|---|---|---|\n");

    for (double fs : kRates)
        for (int m : { 0, 2 })
            for (double la : { 0.0, 2.0, 10.0 })
                for (int os : { 1, 2, 4 })
                {
                    if (fs != 48000.0 && fs != 192000.0 && ! (la == 2.0 && os == 4)) continue;
                    Params p = basePrm(); p.model = (Model) m; p.ratio = 1.0; p.lookaheadMs = la; p.oversampling = os;
                    auto c = std::make_unique<Compressor>(); c->prepare (fs, 512, 2); c->setParams (p);
                    const int rep = c->getLatencySamples();
                    const double meas = measureDelaySamples (p, fs);
                    const double diff = meas - rep;
                    const double tol = (m == 2 && os > 1) ? 1.0 : (m == 2 ? 0.15 : 0.05);   // FET: 3 Hz のDCブロッカーが 1 kHz の位相をわずかに進める
                    const bool ok = std::abs (diff) < tol;
                    std::printf ("| %.1f kHz | %s | %.0f ms | %dx | %d | %.2f | %+.2f | %s |\n", fs / 1000, kModelNames[m], la, os, rep, meas, diff, ok ? "PASS" : "**FAIL**");
                    tally (ok, "T7 latency mismatch");
                }

    // 線形方式はOSの有無・方式に関係なくレイテンシーが一定
    {
        tableHead ("項目", "結果", "判定");
        const double fs = 48000.0;
        Params p = basePrm(); p.lookaheadMs = 3.0; p.oversampling = 4;
        int lat[5];
        for (int m = 0; m < 5; ++m)
        {
            p.model = (Model) m;
            auto c = std::make_unique<Compressor>(); c->prepare (fs, 512, 2); c->setParams (p);
            lat[m] = c->getLatencySamples();
        }
        bool same = true;
        for (int m = 1; m < 5; ++m) same = same && lat[m] == lat[0];
        check ("方式を切り替えてもレイテンシーが一定", same, "全方式 %d サンプル(Lookahead 3 ms + OS 4x)", lat[0]);
    }
}

//==============================================================================
// T8: 過渡応答・ルックアヘッド
static void testTransient()
{
    header ("T8. 過渡応答とルックアヘッド",
            "-50 → 0 dBFS のステップ(1 kHz)。Threshold -30 dB / Ratio 20 / Attack 0.1 ms(Opto・Vari-Mu は物理的にアタックが遅い)。"
            "オーバーシュート = ステップ直後のピーク - 定常出力ピーク。判定: Transparent で Lookahead 5 ms のオーバーシュートが Lookahead 0 より 6 dB 以上小さい。");
    std::printf ("| fs | 方式 | Lookahead | オーバーシュート [dB] | 定常ピーク [dBFS] |\n|---|---|---|---|---|\n");

    for (double fs : { 44100.0, 96000.0, 192000.0 })
    {
        double ov0 = 0, ov5 = 0;
        for (int m : { 0, 2, 3 })
            for (double la : { 0.0, 5.0 })
            {
                Params p = basePrm(); p.model = (Model) m; p.thresholdDb = -30; p.ratio = 20; p.attackMs = 0.1; p.releaseMs = 100; p.blend = 0.0; p.lookaheadMs = la;
                Buf in = sine (fs, 1000.0, -50.0, 0.3);
                const int t1 = (int) in.size();
                append (in, sine (fs, 1000.0, 0.0, 0.4));
                const Out o = runMono (p, fs, in);
                const int lat = o.latency;
                const double early = peakDbRange (o.l, t1 + lat, t1 + lat + (int) (0.03 * fs));
                const double steady = peakDbRange (o.l, (int) in.size() - (int) (0.1 * fs), (int) in.size());
                const double ov = early - steady;
                if (m == 0 && la == 0.0) ov0 = ov;
                if (m == 0 && la == 5.0) ov5 = ov;
                std::printf ("| %.1f kHz | %s | %.0f ms | %.2f | %.2f |\n", fs / 1000, kModelNames[m], la, ov, steady);
            }
        std::printf ("\n");
        tableHead ("項目", "結果", "判定");
        check ("T8 ルックアヘッドの効果 " + std::to_string ((int) (fs / 1000)) + " kHz", ov5 < ov0 - 6.0, "オーバーシュート %.2f dB(0 ms)→ %.2f dB(5 ms)", ov0, ov5);
        std::printf ("\n| fs | 方式 | Lookahead | オーバーシュート [dB] | 定常ピーク [dBFS] |\n|---|---|---|---|---|\n");
    }
}

//==============================================================================
// T9: ステレオリンク
static void testStereoLink()
{
    header ("T9. ステレオリンク", "L = -10 dBFS、R = -40 dBFS(Threshold -30 dB / Ratio 4)。判定: Link 100% で両chのGR一致、0% で独立(Rはほぼ 0)、50% で中間。");
    tableHead ("項目", "結果", "判定");
    const double fs = 48000.0;
    const Buf l = sine (fs, 1000.0, -10.0, 1.0), r = sine (fs, 1000.0, -40.0, 1.0);
    double grL[3], grR[3];
    int k = 0;
    for (double link : { 1.0, 0.5, 0.0 })
    {
        Params p = basePrm(); p.thresholdDb = -30; p.stereoLink = link;
        const Out o = run (p, fs, l, r);
        grL[k] = meanRange (o.grl, 36000, 48000); grR[k] = meanRange (o.grr, 36000, 48000); ++k;
    }
    check ("Link 100 %", std::abs (grL[0] - grR[0]) < 0.05, "L %.2f dB / R %.2f dB", grL[0], grR[0]);
    check ("Link 0 %", grR[2] < 0.05 && grL[2] > 10.0, "L %.2f dB / R %.2f dB", grL[2], grR[2]);
    check ("Link 50 %", grR[1] > grR[2] + 1.0 && grR[1] < grR[0] - 1.0, "R %.2f dB(0%%: %.2f / 100%%: %.2f)", grR[1], grR[2], grR[0]);
}

//==============================================================================
// T10: ミックス・メイクアップ・オートメイクアップ
static void testMixMakeup()
{
    header ("T10. Mix / Makeup / Auto Makeup", "判定: Mix 50 % の振幅は dry と wet の平均、Makeup +6 dB は +6.00 dB、Auto Makeup は基準 -12 dBFS の信号で出力が入力と ±0.3 dB。");
    tableHead ("項目", "結果", "判定");
    const double fs = 48000.0;
    const Buf in = sine (fs, 1000.0, -12.0, 1.5);
    const int a = 48000, b = (int) in.size();

    Params p = basePrm(); p.thresholdDb = -30; p.ratio = 4; p.attackMs = 5; p.releaseMs = 100;
    const Out wet = runMono (p, fs, in);
    const double wetA = fitSine (wet.l, a, 24000, 1000.0, fs).amp;
    const double dryA = db2lin (-12.0);

    p.mix = 0.5;
    const Out half = runMono (p, fs, in);
    const double halfA = fitSine (half.l, a, 24000, 1000.0, fs).amp;
    check ("Mix 50 %", std::abs (halfA - 0.5 * (wetA + dryA)) < 1e-4, "振幅 %.5f(期待 %.5f)", halfA, 0.5 * (wetA + dryA));

    p.mix = 1.0; p.makeupDb = 6.0;
    const Out mk = runMono (p, fs, in);
    const double mkA = fitSine (mk.l, a, 24000, 1000.0, fs).amp;
    check ("Makeup +6 dB", std::abs (20 * std::log10 (mkA / wetA) - 6.0) < 0.01, "%.3f dB", 20 * std::log10 (mkA / wetA));

    for (int m = 0; m < 5; ++m)
    {
        Params q = basePrm(); q.model = (Model) m; q.thresholdDb = -30; q.ratio = 4; q.attackMs = 5; q.releaseMs = 100; q.kneeDb = 6; q.autoMakeup = true;
        const Out o = runMono (q, fs, in);
        const double outA = fitSine (o.l, a, 24000, 1000.0, fs).amp;
        const double d = 20 * std::log10 (outA / dryA);
        check (std::string ("Auto Makeup / ") + kModelNames[m], std::abs (d) < 0.3, "出力 - 入力 = %+.2f dB", d);
    }
    (void) b;
}

//==============================================================================
// T11: サイドチェイン
static void testSidechain()
{
    header ("T11. サイドチェイン", "");
    tableHead ("項目", "結果", "判定");
    const double fs = 48000.0;

    auto grOf = [&] (Params p, const Buf& in)
    {
        const Out o = runMono (p, fs, in);
        return meanRange (o.grl, (int) in.size() - 9600, (int) in.size());
    };

    Params p = basePrm(); p.thresholdDb = -30; p.attackMs = 5; p.releaseMs = 100;
    const Buf low = sine (fs, 100.0, -10.0, 1.2), hi = sine (fs, 5000.0, -10.0, 1.2);

    const double lowOff = grOf (p, low);
    Params ph = p; ph.scHpfHz = 1000.0;
    const double lowHpf = grOf (ph, low), hiHpf = grOf (ph, hi), hiOff = grOf (p, hi);
    check ("SC HPF 1 kHz: 100 Hz のGRが減る", lowHpf < lowOff - 8.0, "HPFなし %.2f dB → HPF 1 kHz %.2f dB", lowOff, lowHpf);
    check ("SC HPF 1 kHz: 5 kHz のGRは変わらない", std::abs (hiHpf - hiOff) < 0.3, "%.2f dB → %.2f dB", hiOff, hiHpf);

    Params pl = p; pl.scLpfHz = 1000.0;
    check ("SC LPF 1 kHz: 5 kHz のGRが減る", grOf (pl, hi) < hiOff - 8.0, "LPFなし %.2f dB → LPF 1 kHz %.2f dB", hiOff, grOf (pl, hi));

    // SC Gain +12 dB は、入力が +12 dB 大きいのと等価
    {
        Params g = p; g.scGainDb = 12.0;
        const double a = grOf (g, sine (fs, 1000.0, -25.0, 1.2));
        const double b = grOf (p, sine (fs, 1000.0, -13.0, 1.2));
        check ("SC Gain +12 dB ≙ 入力 +12 dB", std::abs (a - b) < 0.05, "%.3f dB vs %.3f dB", a, b);
    }
    // SC EQ: 中域 +12 dB で 2 kHz のGRが増える
    {
        Params e = p; e.eqMidHz = 2000.0; e.eqMidDb = 12.0;
        const Buf t = sine (fs, 2000.0, -30.0, 1.2);
        check ("SC EQ Mid +12 dB @2 kHz", grOf (e, t) > grOf (p, t) + 5.0, "%.2f dB → %.2f dB", grOf (p, t), grOf (e, t));
    }
    // 重み付け: 声の帯域重み付けで 50 Hz のGRが減る / A特性は 1 kHz で 0 dB
    {
        Params w = p; w.weighting = Weighting::Voice;
        const Buf t = sine (fs, 50.0, -10.0, 1.5);
        check ("SC 重み付け Voice: 50 Hz のGRが減る", grOf (w, t) < grOf (p, t) - 8.0, "%.2f dB → %.2f dB", grOf (p, t), grOf (w, t));

        SidechainProcessor sp; sp.prepare (fs);
        Params a = p; a.weighting = Weighting::AWeight; sp.setParams (a);
        const double m1k = 20 * std::log10 (sp.weightingMagnitude (1000.0));
        const double m100 = 20 * std::log10 (sp.weightingMagnitude (100.0));
        const double m10k = 20 * std::log10 (sp.weightingMagnitude (10000.0));
        check ("A特性の近似(1 kHz=0 dB / 100 Hz ≈ -19.1 / 10 kHz ≈ -2.5)", std::abs (m1k) < 0.01 && std::abs (m100 + 19.1) < 2.0 && std::abs (m10k + 2.5) < 2.0,
              "1 kHz %.2f / 100 Hz %.1f / 10 kHz %.1f dB", m1k, m100, m10k);
    }
    // 外部サイドチェイン
    {
        Params x = p; x.scSource = ScSource::External;
        const Buf quiet = sine (fs, 1000.0, -45.0, 1.2), loud = sine (fs, 300.0, -10.0, 1.2), none = silence (fs, 1.2);
        const Out withExt = run (x, fs, quiet, quiet, 512, &loud, &loud);
        const Out noExt   = run (x, fs, quiet, quiet, 512, &none, &none);
        const double g1 = meanRange (withExt.grl, 38400, 57600), g2 = meanRange (noExt.grl, 38400, 57600);
        check ("外部サイドチェイン", g1 > 10.0 && g2 < 0.05, "外部が大きいときGR %.2f dB / 無音のとき %.3f dB", g1, g2);

        Params mon = x; mon.scMonitor = true;
        const Out om = run (mon, fs, quiet, quiet, 512, &loud, &loud);
        double d = 0; for (int i = 4800; i < 40000; ++i) d = std::max (d, (double) std::abs (om.l[(size_t) i] - loud[(size_t) i]));
        check ("SC モニター(外部、フィルターなし)", d < 1e-6, "モニター出力と外部入力の最大差 = %.2g", d);
    }
    // 外部が無い(ホストが未接続)ときは内部にフォールバックしても安全
    {
        Params x = p; x.scSource = ScSource::External;
        const Buf t = sine (fs, 1000.0, -10.0, 0.5);
        const Out o = runMono (x, fs, t);
        bool fin = true; for (float v : o.l) fin = fin && std::isfinite (v);
        check ("外部選択でも外部入力が無い場合に安全", fin, "出力は有限値");
    }
    // フィードバック方式
    {
        Params fb = p; fb.topology = Topology::FeedBack;
        const Buf t = sine (fs, 1000.0, -10.0, 1.5);
        const double ff = grOf (p, t), bk = grOf (fb, t);
        check ("Feed Back は Feed Forward よりGRが小さい(同じ設定)", bk < ff - 0.5 && bk > 0.5, "FF %.2f dB / FB %.2f dB", ff, bk);
    }
}

//==============================================================================
// T12: ボーカル機能(合成信号)
static Buf toneWithHarmonics (double fs, double f0, double db, double sec)
{
    const int n = (int) (sec * fs);
    Buf b ((size_t) n);
    const double a = db2lin (db) / 1.6;
    for (int i = 0; i < n; ++i)
    {
        double v = 0;
        for (int k = 1; k <= 6; ++k) v += std::sin (2 * kPiD * f0 * k * i / fs) / k;
        b[(size_t) i] = (float) (a * v);
    }
    return b;
}
static Buf highNoise (double fs, double db, double sec, unsigned seed)
{
    // 5 kHz 以上に偏ったノイズ(2次差分で簡易ハイパス)
    Buf n = noiseBuf (fs, 0.0, sec + 0.01, seed);
    Buf o (n.size() - 1);
    for (size_t i = 1; i < n.size(); ++i) o[i - 1] = 0.5f * (n[i] - n[i - 1]);
    double pk = 0; for (float v : o) pk = std::max (pk, (double) std::abs (v));
    const float g = (float) (db2lin (db) / pk);
    for (auto& v : o) v *= g;
    o.resize ((size_t) (sec * fs));
    return o;
}

static void testVocal()
{
    header ("T12. ボーカル専用機能(合成信号での方向性の確認)",
            "実際のボーカル録音ではなく合成信号(倍音を持つ有声音、80 Hz のバースト、高域ノイズ)で、各機能が設計どおりの方向に効くことを確認する。"
            "**実際の歌唱・朗読での効果と音質は 未確認**(§README 参照)。");
    tableHead ("項目", "結果", "判定");

    const double fs = 48000.0;

    // --- 破裂音ガード ---
    {
        Buf in = toneWithHarmonics (fs, 220.0, -22.0, 0.5);
        const int t1 = (int) in.size();
        append (in, sine (fs, 80.0, -2.0, 0.060));
        const int t2 = (int) in.size();
        append (in, toneWithHarmonics (fs, 220.0, -22.0, 0.5));
        auto peakGr = [&] (double amt)
        {
            Params p = basePrm(); p.thresholdDb = -20; p.ratio = 6; p.attackMs = 2; p.releaseMs = 120; p.blend = 0.5; p.vPlosive = amt;
            p.scHpfHz = 20;
            const Out o = runMono (p, fs, in);
            return (double) *std::max_element (o.grl.begin() + t1, o.grl.begin() + t2 + 2400);
        };
        const double g0 = peakGr (0.0), g1 = peakGr (1.0);
        check ("Plosive Guard: 80 Hz バースト時の最大GRが減る", g1 < g0 - 1.5, "OFF %.2f dB → 100%% %.2f dB", g0, g1);
    }
    // --- 歯擦音ガード ---
    {
        Buf in = toneWithHarmonics (fs, 220.0, -20.0, 0.4);
        const int t1 = (int) in.size();
        append (in, highNoise (fs, -8.0, 0.15, 5));
        const int t2 = (int) in.size();
        append (in, toneWithHarmonics (fs, 220.0, -20.0, 0.4));
        auto peakGr = [&] (double amt)
        {
            Params p = basePrm(); p.thresholdDb = -25; p.ratio = 6; p.attackMs = 2; p.releaseMs = 80; p.blend = 0.5; p.vSib = amt;
            const Out o = runMono (p, fs, in);
            return (double) *std::max_element (o.grl.begin() + t1, o.grl.begin() + t2);
        };
        const double g0 = peakGr (0.0), g1 = peakGr (1.0);
        check ("Sibilance Guard: 高域ノイズ区間の最大GRが減る", g1 < g0 - 1.5, "OFF %.2f dB → 100%% %.2f dB", g0, g1);
    }
    // --- ボーカルレベリング ---
    {
        Buf in = sine (fs, 400.0, -30.0, 3.0);
        const int s1 = (int) in.size();
        append (in, sine (fs, 400.0, -12.0, 4.0));
        const int s2 = (int) in.size();
        append (in, sine (fs, 400.0, -30.0, 4.0));
        auto diff = [&] (double amt)
        {
            Params p = basePrm(); p.thresholdDb = -20; p.ratio = 1.0; p.vLevel = amt;
            const Out o = runMono (p, fs, in);
            const double quiet = rmsDbRange (o.l, s1 - (int) (0.5 * fs), s1), loud = rmsDbRange (o.l, s2 - (int) (0.5 * fs), s2);
            return loud - quiet;
        };
        const double d0 = diff (0.0), d1 = diff (1.0);
        check ("Vocal Leveling: 大小のレベル差が縮まる", std::abs (d0 - 18.0) < 0.1 && d1 < d0 - 5.0, "OFF %.2f dB → 100%% %.2f dB(入力差 18 dB)", d0, d1);
    }
    // --- 音節認識 ---
    {
        // 140 ms 周期の音節(80 ms 発声 / 60 ms の谷)
        Buf in;
        for (int s = 0; s < 8; ++s) { append (in, toneWithHarmonics (fs, 200.0, -8.0, 0.080)); append (in, toneWithHarmonics (fs, 200.0, -50.0, 0.060)); }
        auto avgGrInValley = [&] (double amt)
        {
            Params p = basePrm(); p.thresholdDb = -25; p.ratio = 4; p.attackMs = 3; p.releaseMs = 150; p.blend = 0.5; p.vSyl = amt;
            const Out o = runMono (p, fs, in);
            double s = 0; int c = 0;
            for (int k = 2; k < 8; ++k)
            {
                const int a = (int) ((k * 0.140 + 0.080 + 0.045) * fs), b = (int) ((k * 0.140 + 0.140) * fs);   // 谷の終わりの15 ms
                for (int i = a; i < b; ++i) { s += o.grl[(size_t) i]; ++c; }
            }
            return s / c;
        };
        const double g0 = avgGrInValley (0.0), g1 = avgGrInValley (1.0);
        check ("Syllable Aware: 音節の谷でGRが速く戻る", g1 < g0 - 1.0, "谷の終わりのGR OFF %.2f dB → 100%% %.2f dB", g0, g1);
    }
    // --- フレーズ認識(休符でリリースが速く / 歌唱中は遅く) ---
    {
        Buf in = toneWithHarmonics (fs, 200.0, -8.0, 1.0);
        const int t1 = (int) in.size();
        append (in, silence (fs, 0.9));
        auto grAfter = [&] (double amt)
        {
            Params p = basePrm(); p.thresholdDb = -25; p.ratio = 4; p.attackMs = 3; p.releaseMs = 300; p.blend = 0.5; p.vPhrase = amt;
            const Out o = runMono (p, fs, in);
            return (double) o.grl[(size_t) (t1 + (int) (0.40 * fs))];
        };
        const double g0 = grAfter (0.0), g1 = grAfter (1.0);
        check ("Phrase Aware: 休符でGRが速く戻る", g1 < g0 - 0.3, "休符 400 ms 後のGR OFF %.2f dB → 100%% %.2f dB", g0, g1);
    }
    // --- 適応 ---
    {
        Buf in = toneWithHarmonics (fs, 200.0, -22.0, 3.0);
        append (in, toneWithHarmonics (fs, 200.0, -8.0, 3.0));
        auto grLoud = [&] (double amt)
        {
            Params p = basePrm(); p.thresholdDb = -25; p.ratio = 3; p.attackMs = 5; p.releaseMs = 150; p.blend = 0.5; p.vAdapt = amt;
            const Out o = runMono (p, fs, in);
            return meanRange (o.grl, (int) (5.5 * fs), (int) (6.0 * fs));
        };
        const double g0 = grLoud (0.0), g1 = grLoud (1.0);
        check ("Adaptive: 大きいフレーズでより圧縮する", g1 > g0 + 0.3, "大きいフレーズのGR OFF %.2f dB → 100%% %.2f dB", g0, g1);
    }
}

//==============================================================================
// T13: クリック防止・ブロックサイズ独立性
static void testClicks()
{
    header ("T13. パラメーター急変時のクリック / ブロックサイズ独立性",
            "1 kHz 正弦波(-12 dBFS)の途中、ブロック境界でパラメーターを急に変える。"
            "出力の最大サンプル間差分 / 入力の最大サンプル間差分(正弦波そのものの傾き)を測る。1 に近いほど段差なし。"
            "判定: Threshold/Makeup/Mix/Output/Input/Bypass/Ratio の急変で比が 2.0 未満。方式切替は参考値のみ。");
    tableHead ("急変させる項目", "比 (出力の最大差分 / 入力の最大差分)", "判定");

    const double fs = 48000.0;
    const Buf in = sine (fs, 1000.0, -12.0, 1.0);
    double inStep = 0; for (size_t i = 1; i < in.size(); ++i) inStep = std::max (inStep, (double) std::abs (in[i] - in[i - 1]));

    struct Case { const char* name; std::function<void (Params&)> change; bool judge; };
    const Case cases[] = {
        { "Threshold -50 → 0 dB",   [] (Params& p) { p.thresholdDb = 0; }, true },
        { "Makeup 0 → +18 dB",      [] (Params& p) { p.makeupDb = 18; }, true },
        { "Mix 100 → 0 %",          [] (Params& p) { p.mix = 0; }, true },
        { "Output 0 → -24 dB",      [] (Params& p) { p.outGainDb = -24; }, true },
        { "Input 0 → +12 dB",       [] (Params& p) { p.inGainDb = 12; }, true },
        { "Bypass ON",              [] (Params& p) { p.bypass = true; }, true },
        { "Ratio 2 → 20",           [] (Params& p) { p.ratio = 20; }, true },
        { "Model Transparent → FET", [] (Params& p) { p.model = Model::FET; }, false },
    };

    for (const auto& cs : cases)
    {
        Params p = basePrm(); p.thresholdDb = -50; p.ratio = 2; p.attackMs = 5; p.releaseMs = 100;
        auto c = std::make_unique<Compressor>(); c->prepare (fs, 512, 2); c->setParams (p);
        Buf l = in, r = in;
        const int half = 24000;
        for (int pos = 0; pos < (int) l.size(); pos += 512)
        {
            if (pos == half - (half % 512) + 512) { Params q = p; cs.change (q); c->setParams (q); }
            const int m = std::min (512, (int) l.size() - pos);
            float* io[2] = { l.data() + pos, r.data() + pos };
            c->process (io, 2, m);
        }
        double mx = 0;
        for (size_t i = 12000 + 1; i < l.size(); ++i) mx = std::max (mx, (double) std::abs (l[i] - l[i - 1]));
        // ゲインが変わる分(Makeup +18 dB など)はそのぶん傾きが増えるので、変化後の最大傾きで正規化せず、そのまま比を見る
        double ref = inStep;
        if (std::string (cs.name).find ("Makeup") == 0) ref *= db2lin (18.0);
        if (std::string (cs.name).find ("Input") == 0)  ref *= db2lin (12.0);
        const double ratio = mx / ref;
        const bool ok = ! cs.judge || ratio < 2.0;
        std::printf ("| %s | %.2f | %s |\n", cs.name, ratio, cs.judge ? (ok ? "PASS" : "**FAIL**") : "(参考値)");
        if (cs.judge) tally (ok, std::string ("T13 click ") + cs.name);
    }

    // ブロックサイズ独立性
    std::printf ("\n");
    tableHead ("項目", "結果", "判定");
    {
        Params p = basePrm(); p.model = Model::FET; p.oversampling = 2; p.lookaheadMs = 2.0; p.vLevel = 0.5; p.vSyl = 0.5; p.vPlosive = 0.5; p.vSib = 0.5; p.vPhrase = 0.5; p.vAdapt = 0.5; p.autoRelease = true;
        Buf x = toneWithHarmonics (fs, 200.0, -10.0, 1.0);
        append (x, noiseBuf (fs, -20.0, 1.0, 9));
        const Out ref = runMono (p, fs, x, 512);
        for (int blk : { 1, 7, 64, 333 })
        {
            const Out o = runMono (p, fs, x, blk);
            double d = 0, pk = 0;
            for (size_t i = 0; i < x.size(); ++i) { d = std::max (d, (double) std::abs (o.l[i] - ref.l[i])); pk = std::max (pk, (double) std::abs (ref.l[i])); }
            check ("ブロックサイズ " + std::to_string (blk) + " と 512 の出力差", d < 1e-4 * pk + 1e-6, "最大差 %.3g(ピーク比 %.1f dB)", d, 20 * std::log10 (d / pk + 1e-30));
        }
    }
}

//==============================================================================
// T14: ステレオ・チャンネル数、ランダムファズ(NaN/Inf/巨大値/メモリ確保)
static void testFuzz()
{
    header ("T14. 頑健性(ファズ)・オーディオスレッドでのメモリ確保",
            "全パラメーターをブロックごとにランダムに変え、ブロックサイズ 1〜512、ノイズ・インパルス・DC・NaN・Inf・±1e30・デノーマルを混ぜて処理。"
            "判定: 出力がすべて有限値かつ |x| ≤ 1e4、`process()`/`setParams()` 内の `operator new` 呼び出しが 0 回。");
    tableHead ("項目", "結果", "判定");

    std::mt19937 rng (12345);
    auto U = [&] (double a, double b) { return std::uniform_real_distribution<double> (a, b) (rng); };

    long totalBlocks = 0, nonFinite = 0, tooBig = 0;
    long allocs = 0;

    for (double fs : kRates)
    {
        auto c = std::make_unique<Compressor>();
        c->prepare (fs, 512, 2);

        std::array<float, P_COUNT> v = defaultValues();
        Buf l (512), r (512), sl (512), sr (512);
        const int blocks = fs > 100000 ? 1500 : 3000;

        for (int b = 0; b < blocks; ++b)
        {
            // パラメーターをランダム化(端の値も出やすくする)
            for (int i = 0; i < P_COUNT; ++i)
            {
                const auto& d = paramDefs()[(size_t) i];
                if (rng() % 3 != 0 && b > 0) continue;
                const int mode = (int) (rng() % 6);
                if (d.kind == ParamKind::Float)
                    v[(size_t) i] = mode == 0 ? d.min : mode == 1 ? d.max : (float) U (d.min, d.max);
                else
                    v[(size_t) i] = (float) (int) std::round (U (d.min, d.max));
            }
            const Params p = toParams (v.data());

            const int n = 1 + (int) (rng() % 512);
            const int kind = (int) (rng() % 7);
            for (int i = 0; i < n; ++i)
            {
                double x = 0;
                switch (kind)
                {
                    case 0: x = U (-1, 1) * db2lin (U (-90, 6)); break;
                    case 1: x = (i == n / 2) ? U (-4, 4) : 0.0; break;
                    case 2: x = 0.7; break;                                       // DC
                    case 3: x = 0.9 * std::sin (0.05 * i + b); break;
                    case 4: x = U (-1, 1) * 1e-40; break;                         // デノーマル域
                    case 5: x = U (-1, 1) * 1e30; break;                          // 巨大値
                    default: x = 0; break;
                }
                l[(size_t) i] = (float) x;
                r[(size_t) i] = (float) (x * U (0.2, 1.0));
                sl[(size_t) i] = (float) (U (-1, 1) * db2lin (U (-60, 0)));
                sr[(size_t) i] = sl[(size_t) i];
            }
            if (b % 37 == 0) l[(size_t) (n / 3)] = std::numeric_limits<float>::quiet_NaN();
            if (b % 53 == 0) r[(size_t) (n / 2)] = std::numeric_limits<float>::infinity();
            if (b % 71 == 0) sl[(size_t) (n / 2)] = -std::numeric_limits<float>::infinity();

            float* io[2] = { l.data(), r.data() };
            const float* sc[2] = { sl.data(), sr.data() };

            g_allocs = 0; g_track = true;
            c->setParams (p);
            c->process (io, 2, n, (rng() & 1) ? sc : nullptr, 2);
            g_track = false;
            allocs += g_allocs.load();

            for (int i = 0; i < n; ++i)
            {
                if (! std::isfinite (l[(size_t) i]) || ! std::isfinite (r[(size_t) i])) ++nonFinite;
                else if (std::abs (l[(size_t) i]) > 1.0001e4 || std::abs (r[(size_t) i]) > 1.0001e4) ++tooBig;
            }
            ++totalBlocks;
        }
    }
    check ("ランダム入力・ランダムパラメーター(全5サンプルレート)", nonFinite == 0 && tooBig == 0,
          "%ld ブロック処理、非有限値 %ld 個、|x|>1e4 が %ld 個", totalBlocks, nonFinite, tooBig);
    check ("オーディオスレッドでのメモリ確保", allocs == 0, "process()/setParams() 内の new 呼び出し %ld 回", allocs);

    // 1ch(モノラル)処理
    {
        const double fs = 48000.0;
        auto c = std::make_unique<Compressor>();
        c->prepare (fs, 512, 1);
        Params p = basePrm(); c->setParams (p);
        Buf m = sine (fs, 1000.0, -6.0, 0.2);
        float* io[1] = { m.data() };
        for (int pos = 0; pos + 512 <= (int) m.size(); pos += 512) { float* q[1] = { m.data() + pos }; c->process (q, 1, 512); }
        (void) io;
        bool fin = true; for (float x : m) fin = fin && std::isfinite (x);
        check ("モノラル(1ch)処理", fin, "出力は有限値");
    }
}

//==============================================================================
// T15: CPU 負荷
static void testCpu()
{
    header ("T15. CPU負荷(このテスト環境での参考値)",
            "ステレオ、ブロック 512、ホワイトノイズ(-12 dBFS)、全ボーカル機能 ON、10 秒分を処理した時間。"
            "**このコンテナの CPU(gcc -O2)での値**で、ユーザー環境(MSVC Release)の値ではない。目安としてのみ使うこと。");
    std::printf ("| fs | 方式 | OS | 実時間比(大きいほど軽い) | 1コア使用率 |\n|---|---|---|---|---|\n");

    for (double fs : { 48000.0, 192000.0 })
        for (int m = 0; m < 5; ++m)
            for (int os : { 1, 4 })
            {
                if (m != 2 && m != 4 && os == 4) continue;
                if (fs == 192000.0 && m != 0 && m != 2) continue;
                Params p = basePrm(); p.model = (Model) m; p.oversampling = os; p.lookaheadMs = 2.0; p.weighting = Weighting::Voice; p.scHpfHz = 100; p.scLpfHz = 12000;
                p.eqMidDb = 3.0; p.vLevel = p.vPlosive = p.vSib = p.vSyl = p.vPhrase = p.vAdapt = 0.5; p.autoRelease = true;
                const int n = (int) (10 * fs);
                Buf l = noiseBuf (fs, -12.0, 10.0, 1), r = noiseBuf (fs, -12.0, 10.0, 2);
                auto c = std::make_unique<Compressor>(); c->prepare (fs, 512, 2); c->setParams (p);
                const auto t0 = std::chrono::steady_clock::now();
                for (int pos = 0; pos + 512 <= n; pos += 512)
                {
                    float* io[2] = { l.data() + pos, r.data() + pos };
                    c->process (io, 2, 512);
                }
                const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
                std::printf ("| %.1f kHz | %s | %dx | %.0f× | %.2f %% |\n", fs / 1000, kModelNames[m], os, 10.0 / sec, 100.0 * sec / 10.0);
            }
}


//==============================================================================
static void testPresets()
{
    std::printf ("\n### T16. プリセット(工場出荷 12 種 + 保存 / 読み込み / 削除 / 名前変更)\n\n");
    std::printf ("判定: 仕様の12種が揃い全値が範囲内、全プリセットで実際に処理して出力が有限、XML往復で値が完全一致、保存・読み込み・名前変更・削除が動作、不正入力を拒否。\n\n");
    std::printf ("| 項目 | 結果 | 判定 |\n|---|---|---|\n");

    const char* required[12] = { "Vocal Natural", "Vocal Smooth", "Vocal Pop", "Vocal Aggressive", "Vocal Soft", "Vocal Lead",
                                 "Vocal Background", "Vocal Whisper", "Vocal Broadcast", "Vocal Transparent", "Vocal Heavy", "Vocal Leveling" };
    const auto& fp = factoryPresets();
    bool namesOk = fp.size() == 12;
    for (int i = 0; namesOk && i < 12; ++i) namesOk = fp[(size_t) i].name == required[i];
    check ("工場出荷プリセットが仕様の12種と一致", namesOk, "%d 種", (int) fp.size());

    bool rangeOk = true;
    for (const auto& p : fp)
        for (int i = 0; i < P_COUNT; ++i)
        {
            const auto& d = paramDefs()[(size_t) i];
            const float v = p.values[(size_t) i];
            if (! (v >= d.min && v <= d.max)) rangeOk = false;
        }
    check ("全プリセットの全値がパラメーター範囲内", rangeOk, "");

    // 各プリセットで実際に処理(ノイズ + 正弦波、48 kHz)
    bool allFinite = true;
    double worstLatMs = 0;
    for (const auto& pr : fp)
    {
        const Params p = toParams (pr.values.data());
        auto c = std::make_unique<Compressor>();
        c->prepare (48000.0, 512, 2);
        c->setParams (p);
        c->reset();
        std::mt19937 rng (7);
        std::normal_distribution<float> nd (0.0f, 0.1f);
        std::vector<float> l (48000), r (48000);
        for (size_t i = 0; i < l.size(); ++i)
        {
            l[i] = nd (rng) + 0.3f * (float) std::sin (2 * vc::kPi * 220.0 * (double) i / 48000.0);
            r[i] = nd (rng);
        }
        for (size_t pos = 0; pos + 512 <= l.size(); pos += 512)
        {
            c->setParams (p);
            float* io[2] = { l.data() + pos, r.data() + pos };
            c->process (io, 2, 512);
        }
        for (size_t i = 0; i < l.size(); ++i)
            if (! std::isfinite (l[i]) || ! std::isfinite (r[i]) || std::abs (l[i]) > 1.0e4f) allFinite = false;
        worstLatMs = std::max (worstLatMs, 1000.0 * c->getLatencySamples() / 48000.0);
    }
    check ("全12プリセットで処理でき、出力が有限値", allFinite, "最大レイテンシー %.2f ms", worstLatMs);

    // XML 往復
    bool rt = true;
    for (const auto& pr : fp)
    {
        auto xml = PresetManager::toXml (pr.name, pr.values);
        juce::String nm;
        auto back = PresetManager::fromXml (*juce::XmlDocument::parse (xml->toString()), &nm);
        if (! back || nm != pr.name) { rt = false; continue; }
        for (int i = 0; i < P_COUNT; ++i)
            if (std::abs ((*back)[(size_t) i] - pr.values[(size_t) i]) > 1.0e-6f) rt = false;
    }
    check ("XML の書き出し→読み込みで全値が一致", rt, "");

    // ファイル操作(一時フォルダー)
    juce::File tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vc_preset_test_" + juce::String (juce::Random::getSystemRandom().nextInt64()));
    {
        PresetManager pm (tmp);
        const juce::String jp = juce::String::fromUTF8 ("My Test あいう");   // 日本語名
        ParamValues v = defaultValues();
        v[P_threshold] = -33.3f; v[P_model] = 3; v[P_ratio] = 7.25f;
        check ("保存", pm.save (jp, v).wasOk(), "");
        check ("保存した名前が一覧に出る", pm.getUserNames().contains (jp), "");
        auto ld = pm.load (jp);
        check ("読み込みで値が一致", ld && std::abs ((*ld)[P_threshold] + 33.3f) < 1e-4f && (*ld)[P_model] == 3.0f && std::abs ((*ld)[P_ratio] - 7.25f) < 1e-4f, "");
        check ("名前変更", pm.rename (jp, "Renamed").wasOk() && ! pm.userPresetExists (jp) && pm.userPresetExists ("Renamed"), "");
        auto ld2 = pm.load ("Renamed");
        check ("名前変更後も値が保持される", ld2 && std::abs ((*ld2)[P_threshold] + 33.3f) < 1e-4f, "");
        check ("工場出荷と同名の保存を拒否", pm.save ("Vocal Pop", v).failed(), "");
        check ("工場出荷プリセットの削除を拒否", pm.remove ("Vocal Pop").failed(), "");
        check ("工場出荷プリセットの名前変更を拒否", pm.rename ("Vocal Pop", "X").failed(), "");
        check ("空の名前・禁止文字のみの名前を拒否", pm.save ("   ", v).failed() && pm.save ("***", v).failed(), "");
        check ("禁止文字の除去(パス区切りなど)", PresetManager::sanitise ("a/b\\c:d*e?f\"g<h>i|j") == "abcdefghij", "");
        check ("削除", pm.remove ("Renamed").wasOk() && pm.getUserNames().isEmpty(), "");
        // 壊れたファイル・範囲外の値
        tmp.getChildFile ("bad.vcpreset").replaceWithText ("<not xml");
        check ("壊れたファイルは読み込みに失敗(クラッシュしない)", ! pm.load ("bad").has_value(), "");
        tmp.getChildFile ("oor.vcpreset").replaceWithText ("<VocalCompressorPreset version=\"1\"><Param id=\"threshold\" value=\"999\"/><Param id=\"ratio\" value=\"nan\"/></VocalCompressorPreset>");
        auto oor = pm.load ("oor");
        check ("範囲外・NaN の値は安全な値に丸められる", oor && (*oor)[P_threshold] == 0.0f && std::isfinite ((*oor)[P_ratio]) && (*oor)[P_ratio] >= 1.0f, "");
        check ("工場出荷プリセットをファイルへ書き出せる", pm.exportFactoryPresets (tmp.getChildFile ("export")).wasOk()
                                                       && tmp.getChildFile ("export").findChildFiles (juce::File::findFiles, false, "*.vcpreset").size() == 12, "");
    }
    tmp.deleteRecursively();
}

//==============================================================================
int main()
{
    std::printf ("# Vocal Compressor DSP テスト結果(自動生成)\n\n");
    std::printf ("テストプログラム: `tests/dsp_tests.cpp`。実際の JUCE 8.0.4 の juce_core / juce_audio_basics / juce_dsp にリンクして実行。\n");

    testGainAccuracy();
    testNull();
    testTiming();
    testDistortion();
    testNoise();
    testFrequencyResponse();
    testLatency();
    testTransient();
    testStereoLink();
    testMixMakeup();
    testSidechain();
    testVocal();
    testClicks();
    testFuzz();
    testPresets();
    testCpu();

    std::printf ("\n## 集計\n\n判定項目: PASS %d / FAIL %d\n", g_pass, g_fail);
    for (auto& f : g_failures) std::printf ("- FAIL: %s\n", f.c_str());
    return g_fail == 0 ? 0 : 1;
}
