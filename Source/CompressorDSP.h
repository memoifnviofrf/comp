#pragma once

// コンプレッサー本体(Phase 1〜5 の DSP を統合)
//
// 信号の流れ(1サンプルごと、ステレオ):
//   入力 -> Input Gain -> [Vocal Leveling(遅いゲインライダー)]
//        -> サイドチェイン(Int/Ext -> Gain -> HPF -> LPF -> 3バンドEQ -> 重み付け)
//        -> ボーカル解析(破裂音/歯擦音の寄与を除去) -> 検出器(Peak/RMS)
//        -> ゲインコンピューター(ソフトニー) -> ステレオリンク -> ルックアヘッド(スライディング最大値)
//        -> バリスティクス(Attack/Release、方式ごとの時間特性) -> ゲイン適用
//        -> [非線形ステージ(FET/Vari-Mu、必要ならオーバーサンプリング)]
//        -> DCブロッカー -> Makeup -> Mix(遅延補償済みのDry) -> Output Gain
//
// オーディオスレッドでメモリ確保・ファイルI/Oは行いません(prepare() で全て確保)。

#include <array>
#include <memory>
#include <vector>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "Biquad.h"
#include "CompressorDetector.h"
#include "CompressorParameters.h"
#include "Metering.h"
#include "SidechainProcessor.h"

namespace vc
{
//==============================================================================
// ゲインコンピューター(静的カーブ)。GUI のカーブ表示・オートメイクアップからも使う。
//   戻り値: ゲインリダクション量 [dB, >= 0]
double effectiveKnee (Model m, double kneeDb) noexcept;
double computeGainReduction (double levelDb, double thresholdDb, double ratio, double kneeDb, Model m) noexcept;
double autoMakeupDb (const Params& p) noexcept;

//==============================================================================
class DelayLine
{
public:
    void prepare (int maxDelay);
    void reset() noexcept;
    double process (double x, int delaySamples) noexcept
    {
        buf[(size_t) w] = x;
        const double y = buf[(size_t) ((w - std::min (delaySamples, mask)) & mask)];
        w = (w + 1) & mask;
        return y;
    }

private:
    std::vector<double> buf;
    int mask = 0, w = 0;
};

// スライディング最大値(単調 deque、O(1) 償却)。ルックアヘッド用。
class SlidingMax
{
public:
    void prepare (int maxWindow);
    void reset() noexcept;
    double push (double x, int window) noexcept;

private:
    std::vector<double> v;
    std::vector<unsigned long long> pos;
    unsigned long long head = 0, tail = 0, n = 0;
    unsigned long long mask = 0;
};

// Vocal Leveling: 遅いゲインライダー(目標レベルへ緩やかに追従。無音時は保持)
class Leveler
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;
    void set (double amount, double targetDb) noexcept { amt = amount; tgtDb = targetDb; }
    double process (double meanSquare) noexcept;   // 戻り値: 線形ゲイン
    double currentGainDb() const noexcept { return gDb; }

private:
    double sr = 44100.0, amt = 0.0, tgtDb = -16.0, ms = 0.0, gDb = 0.0, gain = 1.0, cMs = 0.0;
    int count = 0;
};

//==============================================================================
class Compressor
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr int kCtl = 16;          // 時定数係数の更新周期(サンプル)

    Compressor();

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    void setParams (const Params& p);        // ブロックの先頭で呼ぶ
    const Params& getParams() const noexcept { return p; }
    int getLatencySamples() const noexcept { return latencySamples; }

    // io: 各チャンネルのポインター(その場で処理)
    // sc: 外部サイドチェイン(無ければ nullptr)
    // grOut: 各チャンネルのサンプルごとのゲインリダクション[dB](不要なら nullptr)。テスト・表示用。
    void process (float* const* io, int numChannels, int numSamples,
                  const float* const* sc = nullptr, int scChannels = 0, float* const* grOut = nullptr);

    MeterData meters;

private:
    struct Channel
    {
        LevelDetector det;
        VocalAnalyzer voc;
        SlidingMax smax;
        DelayLine rawDelay, wetDelay, compDelay;
        DcBlocker dc;

        double gd = 0.0, gs = 0.0;               // バリスティクスの状態 [dB]
        double aA = 0.0, aR = 0.0, vcaRate = 0.0;
        double fastGr = 0.0, slowGr = 0.0, mem = 0.0;
        double yPrev = 0.0;                      // フィードバック用(前サンプルの出力)
    };

    void processChunk (float* const* io, int nCh, int n, const float* const* sc, int scCh, float* const* grOut);
    void updateBallistics (int c) noexcept;
    void updateDerived();

    Params p;
    bool paramsInitialised = false;
    double sampleRate = 44100.0;
    int maxBlock = 512, numCh = 2;
    int lookaheadSamples = 0, osLatency[3] = { 0, 0, 0 }, osLat = 0, latencySamples = 0, maxLookahead = 0;
    int ctlCounter = 0;
    double cFastGr = 0, cSlowGr = 0, cMem = 0;

    std::array<Channel, kMaxChannels> ch;
    SidechainProcessor scProc;
    Leveler leveler;

    juce::SmoothedValue<double> inGainSm, outGainSm, makeupSm, mixSm, bypassSm, thrSm, ratioSm, kneeSm;

    // ブロック作業用バッファ(prepare で確保)
    std::array<std::vector<double>, kMaxChannels> bufXg, bufDry, bufRaw, bufGs, bufMon, bufWet;

    // オーバーサンプリング(index: 0=未使用, 1=2x, 2=4x)
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 3> os;
    juce::AudioBuffer<float> osBuffer;
};

} // namespace vc
