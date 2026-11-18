#include "CompressorDSP.h"

namespace vc
{
namespace
{
constexpr double kDbToNeper = 0.11512925464970229;   // ln(10) / 20

inline double sanitize (float v) noexcept
{
    return std::isfinite (v) ? (double) v : 0.0;
}

// 非線形(飽和)ステージ: 非対称ソフトクリップ。小信号ゲインは 1 に正規化。
//   y = (tanh(k (x + b)) - tanh(k b)) / (k sech^2(k b))
//   b>0 で偶数次高調波、k は GR が深いほど大きく(= 歪みが増える)
struct Shaper { double k0, k1, b; };
constexpr Shaper kFetShaper   { 0.35, 0.040, 0.08 };
constexpr Shaper kVariMuShaper { 0.30, 0.030, 0.12 };

inline double shape (double x, double grDb, const Shaper& s) noexcept
{
    const double k = s.k0 + s.k1 * grDb;
    const double tb = std::tanh (k * s.b);
    const double norm = k * (1.0 - tb * tb);
    return (std::tanh (k * (x + s.b)) - tb) / norm;
}

inline double rmsWindowMs (Model m) noexcept
{
    switch (m)
    {
        case Model::Opto:   return 50.0;
        case Model::FET:    return 1.0;
        case Model::VCA:    return 5.0;
        case Model::VariMu: return 30.0;
        default:            return 10.0;
    }
}

inline bool isNonlinear (Model m) noexcept { return m == Model::FET || m == Model::VariMu; }
}

//==============================================================================
double effectiveKnee (Model m, double kneeDb) noexcept
{
    switch (m)
    {
        case Model::Opto:   return std::max (kneeDb, 10.0);   // 光学素子は本質的にソフトニー
        case Model::VariMu: return std::max (kneeDb, 12.0);   // 可変μ管もソフトニー
        case Model::FET:    return kneeDb * 0.35;             // FETは比較的ハードニー
        default:            return kneeDb;
    }
}

double computeGainReduction (double levelDb, double thresholdDb, double ratio, double kneeDb, Model m) noexcept
{
    if (ratio <= 1.0001)
        return 0.0;                                           // 1:1 では完全に無効

    const double w = kneeDb;
    const double o = levelDb - thresholdDb + 0.5 * w;         // ニーの開始点からの距離
    if (o <= 0.0)
        return 0.0;

    double reff = ratio;
    if (m == Model::VariMu)
        reff = 1.0 + (ratio - 1.0) * (1.0 - std::exp (-o / 12.0));   // 深いほどレシオが上がる

    const double k = 1.0 - 1.0 / reff;
    if (w < 1.0e-3)
        return k * std::max (levelDb - thresholdDb, 0.0);     // ハードニー
    if (o <= w)
        return k * o * o / (2.0 * w);                         // ニー内(2次曲線)
    return k * (o - 0.5 * w);                                 // ニーより上(直線)
}

double autoMakeupDb (const Params& p) noexcept
{
    // 基準レベル -12 dBFS で生じる静的ゲインリダクションを補償
    const double gr = computeGainReduction (-12.0, p.thresholdDb, p.ratio,
                                            effectiveKnee (p.model, p.kneeDb), p.model);
    return std::clamp (gr, 0.0, 24.0);
}

//==============================================================================
void DelayLine::prepare (int maxDelay)
{
    int cap = 1;
    while (cap <= maxDelay + 2)
        cap <<= 1;
    buf.assign ((size_t) cap, 0.0);
    mask = cap - 1;
    w = 0;
}

void DelayLine::reset() noexcept
{
    std::fill (buf.begin(), buf.end(), 0.0);
    w = 0;
}

void SlidingMax::prepare (int maxWindow)
{
    unsigned long long cap = 1;
    while (cap < (unsigned long long) maxWindow + 4)
        cap <<= 1;
    v.assign ((size_t) cap, 0.0);
    pos.assign ((size_t) cap, 0);
    mask = cap - 1;
    reset();
}

void SlidingMax::reset() noexcept
{
    head = tail = n = 0;
    std::fill (v.begin(), v.end(), 0.0);
    std::fill (pos.begin(), pos.end(), 0ull);
}

double SlidingMax::push (double x, int window) noexcept
{
    while (tail != head && v[(size_t) ((tail - 1) & mask)] <= x)
        --tail;
    v[(size_t) (tail & mask)] = x;
    pos[(size_t) (tail & mask)] = n;
    ++tail;

    const unsigned long long w = (unsigned long long) std::max (window, 1);
    while (n >= w && pos[(size_t) (head & mask)] <= n - w)
        ++head;

    ++n;
    return v[(size_t) (head & mask)];
}

void Leveler::prepare (double sampleRate)
{
    sr = sampleRate;
    cMs = coefFromMs (250.0, sr);   // 約250 msのRMS窓
    reset();
}

void Leveler::reset() noexcept
{
    ms = 0.0;
    gDb = 0.0;
    gain = 1.0;
    count = 0;
}

double Leveler::process (double meanSquare) noexcept
{
    ms += cMs * (meanSquare - ms);

    if (++count >= 16)
    {
        count = 0;
        constexpr double gateDb = -55.0;   // これより小さい信号ではゲインを動かさない(ノイズを持ち上げない)
        const double levelDb = 10.0 * std::log10 (ms + 1.0e-12);

        if (amt <= 0.0)
        {
            gDb += (1.0 - std::exp (-0.001)) * (0.0 - gDb);   // 無効時はゆっくり0 dBへ
        }
        else if (levelDb > gateDb)
        {
            const double target = std::clamp (tgtDb - levelDb, -12.0, 12.0) * amt;
            const double dt = 16.0 / sr;
            const double tau = target < gDb ? 0.3 : 0.8;       // 下げるのは速め、上げるのは遅め
            gDb += (1.0 - std::exp (-dt / tau)) * (target - gDb);
        }
        gain = std::pow (10.0, gDb * 0.05);
    }
    return gain;
}

//==============================================================================
Compressor::Compressor() = default;

void Compressor::prepare (double sr, int maxBlockSize, int numChannels)
{
    sampleRate = sr;
    maxBlock = std::max (maxBlockSize, 1);
    numCh = std::clamp (numChannels, 1, kMaxChannels);

    maxLookahead = (int) std::ceil (0.010 * sr) + 2;
    constexpr int osHeadroom = 512;

    for (auto& c : ch)
    {
        c.det.prepare (sr);
        c.voc.prepare (sr);
        c.smax.prepare (maxLookahead + 2);
        c.rawDelay.prepare (maxLookahead + osHeadroom);
        c.wetDelay.prepare (maxLookahead);
        c.compDelay.prepare (osHeadroom);
        c.dc.prepare (sr);
    }
    scProc.prepare (sr);
    leveler.prepare (sr);

    for (auto* sm : { &inGainSm, &outGainSm, &makeupSm, &mixSm, &thrSm, &ratioSm, &kneeSm })
        sm->reset (sr, 0.02);
    bypassSm.reset (sr, 0.01);

    for (auto* b : { &bufXg, &bufDry, &bufRaw, &bufGs, &bufMon, &bufWet })
        for (auto& v : *b)
            v.assign ((size_t) maxBlock, 0.0);

    osBuffer.setSize (numCh, maxBlock);
    for (int s = 1; s <= 2; ++s)
    {
        os[(size_t) s] = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) numCh, (size_t) s,
                             juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
        os[(size_t) s]->initProcessing ((size_t) maxBlock);
        osLatency[s] = (int) std::lround (os[(size_t) s]->getLatencyInSamples());
    }

    cFastGr = coefFromMs (20.0, sr) ;   // 更新周期は kCtl だが、係数は1サンプル換算(後で補正)
    const double dt = (double) kCtl / sr;
    cFastGr = 1.0 - std::exp (-dt / 0.020);
    cSlowGr = 1.0 - std::exp (-dt / 0.400);
    cMem    = 1.0 - std::exp (-dt / 2.0);

    paramsInitialised = false;
    reset();
}

void Compressor::reset()
{
    for (auto& c : ch)
    {
        c.det.reset(); c.voc.reset(); c.smax.reset();
        c.rawDelay.reset(); c.wetDelay.reset(); c.compDelay.reset();
        c.dc.reset();
        c.gd = c.gs = 0.0;
        c.fastGr = c.slowGr = c.mem = 0.0;
        c.yPrev = 0.0;
    }
    scProc.reset();
    leveler.reset();
    for (auto& o : os)
        if (o) o->reset();
    ctlCounter = 0;
}

void Compressor::setParams (const Params& np)
{
    p = np;

    const double mk = p.autoMakeup ? autoMakeupDb (p) : p.makeupDb;
    const double targets[] = { dbToLin (p.inGainDb), dbToLin (p.outGainDb), dbToLin (mk), p.mix,
                               p.bypass ? 1.0 : 0.0, p.thresholdDb, p.ratio, p.kneeDb };
    juce::SmoothedValue<double>* sms[] = { &inGainSm, &outGainSm, &makeupSm, &mixSm, &bypassSm, &thrSm, &ratioSm, &kneeSm };

    for (int i = 0; i < 8; ++i)
    {
        if (! paramsInitialised)
            sms[i]->setCurrentAndTargetValue (targets[i]);
        else
            sms[i]->setTargetValue (targets[i]);
    }
    paramsInitialised = true;

    updateDerived();
}

void Compressor::updateDerived()
{
    lookaheadSamples = std::clamp ((int) std::lround (p.lookaheadMs * 0.001 * sampleRate), 0, maxLookahead);
    const int osIdx = p.oversampling >= 4 ? 2 : (p.oversampling >= 2 ? 1 : 0);
    osLat = osIdx > 0 ? osLatency[osIdx] : 0;
    latencySamples = lookaheadSamples + osLat;

    scProc.setParams (p);
    leveler.set (p.vLevel, p.thresholdDb + 4.0);

    for (auto& c : ch)
    {
        c.det.setBlend (p.blend);
        c.det.setRmsWindowMs (rmsWindowMs (p.model));
        c.voc.setAmounts (p.vPlosive, p.vSib, p.vSyl, p.vPhrase, p.vAdapt);
        c.voc.setReferenceDb (p.thresholdDb + 10.0);
    }
    for (int c = 0; c < kMaxChannels; ++c)
        updateBallistics (c);
}

//==============================================================================
// 方式ごとの時間特性
//   Transparent : 指定どおりの Attack / Release(対数領域のデカップルド・スムーズ)
//   Opto        : アタックは最低約6 ms。GRが深く長いほどリリースが遅い(2段リリースの近似)
//   FET         : アタックを1/4に短縮(最短20 µs)。GRが深いとリリースがやや長い
//   VCA         : リリースが dB 領域で直線的(一定の dB/秒で戻る)
//   Vari-Mu     : アタック最低約10 ms。GR記憶に応じてリリースが大きく伸びる
void Compressor::updateBallistics (int c) noexcept
{
    Channel& C = ch[(size_t) c];

    C.fastGr += cFastGr * (C.gs - C.fastGr);
    C.slowGr += cSlowGr * (C.gs - C.slowGr);
    C.mem    += cMem * (C.gs - C.mem);

    double tA = p.attackMs * 0.001, tR = p.releaseMs * 0.001;
    const double memN = std::clamp (C.mem / 10.0, 0.0, 1.0);

    switch (p.model)
    {
        case Model::Opto:
            tA = std::sqrt (tA * tA + 0.006 * 0.006);
            tR *= 0.5 + 2.5 * memN;
            break;
        case Model::FET:
            tA = std::max (tA * 0.25, 20.0e-6);
            tR *= 0.7 + 0.6 * std::clamp (C.gs / 12.0, 0.0, 1.0);
            break;
        case Model::VariMu:
            tA = std::sqrt (tA * tA + 0.010 * 0.010);
            tR *= 1.0 + 2.0 * memN;
            break;
        default:
            break;
    }

    if (p.autoRelease)
    {
        // 持続的なGR(= slow ≈ fast)ほど長く、瞬間的なスパイクほど短く戻す
        const double sustain = std::clamp (C.slowGr / (C.fastGr + 0.05), 0.0, 1.0);
        tR *= 0.35 + 1.65 * sustain;
    }

    const VocalControl& v = C.voc.control();
    tA *= v.attackFactor;
    tR *= v.releaseFactor;

    tA = std::max (tA, 5.0e-6);
    tR = std::max (tR, 1.0e-3);

    C.aA = std::exp (-1.0 / (tA * sampleRate));
    C.aR = std::exp (-1.0 / (tR * sampleRate));
    C.vcaRate = 10.0 / (2.3 * tR * sampleRate);   // 10 dB の回復に 2.3τ(≒90%)かかる速度
}

//==============================================================================
void Compressor::process (float* const* io, int numChannels, int numSamples,
                          const float* const* sc, int scChannels, float* const* grOut)
{
    juce::ScopedNoDenormals noDenormals;   // デノーマル数による CPU スパイクを防ぐ(DSP 側でも保証)
    const int nCh = std::clamp (std::min (numChannels, numCh), 1, kMaxChannels);

    int done = 0;
    while (done < numSamples)
    {
        const int n = std::min (maxBlock, numSamples - done);
        float* chans[kMaxChannels] = { nullptr, nullptr };
        const float* scs[kMaxChannels] = { nullptr, nullptr };
        float* grs[kMaxChannels] = { nullptr, nullptr };

        for (int c = 0; c < nCh; ++c)
        {
            chans[c] = io[c] + done;
            if (grOut != nullptr && grOut[c] != nullptr) grs[c] = grOut[c] + done;
        }
        for (int c = 0; c < std::min (scChannels, kMaxChannels); ++c)
            if (sc != nullptr && sc[c] != nullptr) scs[c] = sc[c] + done;

        processChunk (chans, nCh, n, scs, sc != nullptr ? std::min (scChannels, kMaxChannels) : 0, grs);
        done += n;
    }
}

void Compressor::processChunk (float* const* io, int nCh, int n, const float* const* scIn, int scCh, float* const* grOut)
{
    const bool useExt  = p.scSource == ScSource::External && scIn != nullptr && scCh > 0 && scIn[0] != nullptr;
    const bool fb      = p.topology == Topology::FeedBack && ! useExt;
    const bool scMono  = p.scMode == ScMode::Mono && nCh == 2;
    const int  L       = lookaheadSamples;
    const int  dTotal  = L + osLat;
    const Model model  = p.model;
    const double link  = p.stereoLink;

    double inPeak = 0.0, inSq = 0.0, grMax = 0.0;

    //--------------------------------------------------------------------------
    // パス1: サンプルごと(検出 -> ゲイン計算 -> バリスティクス -> ゲイン適用)
    for (int i = 0; i < n; ++i)
    {
        const double inG  = inGainSm.getNextValue();
        const double thr  = thrSm.getNextValue();
        const double rat  = ratioSm.getNextValue();
        const double knee = effectiveKnee (model, kneeSm.getNextValue());

        double x[kMaxChannels] = { 0.0, 0.0 }, xl[kMaxChannels] = { 0.0, 0.0 }, xd[kMaxChannels] = { 0.0, 0.0 };
        double sIn[kMaxChannels] = { 0.0, 0.0 }, tgt[kMaxChannels] = { 0.0, 0.0 };
        double sq = 0.0;

        for (int c = 0; c < nCh; ++c)
        {
            const double raw = sanitize (io[c][i]);
            const double rawD = ch[(size_t) c].rawDelay.process (raw, dTotal);
            bufRaw[(size_t) c][(size_t) i] = rawD;
            bufDry[(size_t) c][(size_t) i] = rawD * inG;

            x[c] = raw * inG;
            inPeak = std::max (inPeak, std::abs (x[c]));
            sq += x[c] * x[c];
        }
        inSq += sq;

        const double lvGain = leveler.process (sq / (double) nCh);

        for (int c = 0; c < nCh; ++c)
        {
            xl[c] = x[c] * lvGain;
            xd[c] = ch[(size_t) c].wetDelay.process (xl[c], L);
        }

        // サイドチェイン入力の選択
        for (int c = 0; c < nCh; ++c)
        {
            if (useExt)
            {
                const float* src = scIn[std::min (c, scCh - 1)];
                sIn[c] = src != nullptr ? sanitize (src[i]) : 0.0;
            }
            else
            {
                sIn[c] = fb ? ch[(size_t) c].yPrev : xl[c];
            }
        }
        if (scMono)
            sIn[0] = sIn[1] = 0.5 * (sIn[0] + sIn[1]);

        // 検出 -> ゲインコンピューター
        for (int c = 0; c < nCh; ++c)
        {
            Channel& C = ch[(size_t) c];
            const double scs = scProc.process (c, sIn[c]);
            bufMon[(size_t) c][(size_t) i] = scs;

            const double det = C.voc.process (scs);
            const double lvl = C.det.process (det);
            const double lvlDb = 20.0 * std::log10 (lvl + 1.0e-9);

            const VocalControl& v = C.voc.control();
            const double ratioEff = std::clamp (rat * v.ratioScale, 1.0, 20.0);
            tgt[c] = computeGainReduction (lvlDb, thr + v.thresholdShiftDb, ratioEff, knee, model);
        }

        // ステレオリンク(GR領域)
        if (nCh == 2)
        {
            const double m = std::max (tgt[0], tgt[1]);
            tgt[0] += link * (m - tgt[0]);
            tgt[1] += link * (m - tgt[1]);
        }

        if (++ctlCounter >= kCtl)
        {
            ctlCounter = 0;
            for (int c = 0; c < nCh; ++c)
                updateBallistics (c);
        }

        // ルックアヘッド + バリスティクス + ゲイン適用
        for (int c = 0; c < nCh; ++c)
        {
            Channel& C = ch[(size_t) c];
            double t = tgt[c];
            if (L > 0 && ! fb)
                t = C.smax.push (t, L + 1);

            if (model == Model::VCA)
                C.gd = std::max (t, C.gd - C.vcaRate);
            else
                C.gd = std::max (t, C.aR * C.gd + (1.0 - C.aR) * t);

            C.gs = C.aA * C.gs + (1.0 - C.aA) * C.gd;
            if (C.gd < 1.0e-12) C.gd = 0.0;
            if (C.gs < 1.0e-12) C.gs = 0.0;

            const double gain = std::exp (-C.gs * kDbToNeper);
            const double xg = xd[c] * gain;
            C.yPrev = xg;

            bufXg[(size_t) c][(size_t) i] = xg;
            bufGs[(size_t) c][(size_t) i] = C.gs;
            grMax = std::max (grMax, C.gs);
            if (grOut != nullptr && grOut[c] != nullptr)
                grOut[c][i] = (float) C.gs;
        }
    }

    //--------------------------------------------------------------------------
    // パス2: 非線形ステージ(FET / Vari-Mu)とオーバーサンプリング
    const int osIdx = p.oversampling >= 4 ? 2 : (p.oversampling >= 2 ? 1 : 0);
    const Shaper& shp = model == Model::VariMu ? kVariMuShaper : kFetShaper;

    if (isNonlinear (model))
    {
        if (osIdx > 0 && os[(size_t) osIdx] != nullptr)
        {
            auto& O = *os[(size_t) osIdx];
            const int factor = 1 << osIdx;

            for (int c = 0; c < nCh; ++c)
            {
                float* dst = osBuffer.getWritePointer (c);
                for (int i = 0; i < n; ++i)
                    dst[i] = (float) bufXg[(size_t) c][(size_t) i];
            }

            juce::dsp::AudioBlock<float> blk (osBuffer.getArrayOfWritePointers(), (size_t) nCh, (size_t) n);
            auto up = O.processSamplesUp (juce::dsp::AudioBlock<const float> (blk));

            for (int c = 0; c < nCh; ++c)
            {
                float* u = up.getChannelPointer ((size_t) c);
                const size_t total = up.getNumSamples();
                for (size_t j = 0; j < total; ++j)
                {
                    const double gs = bufGs[(size_t) c][std::min ((size_t) n - 1, j / (size_t) factor)];
                    u[j] = (float) shape ((double) u[j], gs, shp);
                }
            }

            O.processSamplesDown (blk);

            for (int c = 0; c < nCh; ++c)
            {
                const float* src = osBuffer.getReadPointer (c);
                for (int i = 0; i < n; ++i)
                    bufWet[(size_t) c][(size_t) i] = (double) src[i];
            }
        }
        else
        {
            for (int c = 0; c < nCh; ++c)
                for (int i = 0; i < n; ++i)
                    bufWet[(size_t) c][(size_t) i] = shape (bufXg[(size_t) c][(size_t) i], bufGs[(size_t) c][(size_t) i], shp);
        }
    }
    else
    {
        // 線形な方式は OS 不要。ただしレイテンシーを一定に保つため、同じ長さだけ遅らせる。
        for (int c = 0; c < nCh; ++c)
            for (int i = 0; i < n; ++i)
                bufWet[(size_t) c][(size_t) i] = osLat > 0 ? ch[(size_t) c].compDelay.process (bufXg[(size_t) c][(size_t) i], osLat)
                                                          : bufXg[(size_t) c][(size_t) i];
    }

    //--------------------------------------------------------------------------
    // パス3: DCブロック -> Makeup -> Mix -> Output -> バイパス -> 書き出し
    double outPeak = 0.0, outSq = 0.0;
    const bool nonlinear = isNonlinear (model);

    for (int i = 0; i < n; ++i)
    {
        const double mixv = mixSm.getNextValue();
        const double mk   = makeupSm.getNextValue();
        const double og   = outGainSm.getNextValue();
        const double byp  = bypassSm.getNextValue();

        for (int c = 0; c < nCh; ++c)
        {
            // DCブロッカーは非線形方式(偏りで直流が生じる)のみ出力に使う。
            // 線形方式では使わず、Ratio 1:1 / GRなしのときに入力と完全一致(ヌルテスト可能)にする。
            const double wIn = bufWet[(size_t) c][(size_t) i];
            const double wDc = ch[(size_t) c].dc.process (wIn);   // 状態は常に更新(方式切替時の段差を最小化)
            const double wet = (nonlinear ? wDc : wIn) * mk;
            double o = (bufDry[(size_t) c][(size_t) i] * (1.0 - mixv) + wet * mixv) * og;

            if (p.scMonitor)
                o = bufMon[(size_t) c][(size_t) i];

            o = o * (1.0 - byp) + bufRaw[(size_t) c][(size_t) i] * byp;

            if (! std::isfinite (o))
                o = 0.0;
            o = std::clamp (o, -1.0e4, 1.0e4);

            io[c][i] = (float) o;
            outPeak = std::max (outPeak, std::abs (o));
            outSq += o * o;
        }
    }

    const double denom = (double) n * (double) nCh;
    meters.inPeakDb.store ((float) linToDb (inPeak), std::memory_order_relaxed);
    meters.inRmsDb.store ((float) linToDb (std::sqrt (inSq / denom)), std::memory_order_relaxed);
    meters.outPeakDb.store ((float) linToDb (outPeak), std::memory_order_relaxed);
    meters.outRmsDb.store ((float) linToDb (std::sqrt (outSq / denom)), std::memory_order_relaxed);
    meters.grDb.store ((float) grMax, std::memory_order_relaxed);
}

} // namespace vc
