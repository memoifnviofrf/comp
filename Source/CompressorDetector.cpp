#include "CompressorDetector.h"

namespace vc
{
//==============================================================================
void LevelDetector::prepare (double sampleRate)
{
    sr = sampleRate;
    setRmsWindowMs (10.0);
    reset();
}

void LevelDetector::setRmsWindowMs (double windowMs)
{
    rmsCoef = coefFromMs (windowMs, sr);
}

//==============================================================================
namespace
{
inline double smoothstep (double a, double b, double x)
{
    const double t = std::clamp ((x - a) / (b - a), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}
}

void VocalAnalyzer::prepare (double sampleRate)
{
    sr = sampleRate;
    lowBand.c  = makeLowpass (sr, 150.0, 0.7071);
    highBand.c = makeHighpass (sr, 4000.0, 0.7071);
    hpA.c = makeHighpass (sr, 180.0, 0.5411961);   hpB.c = makeHighpass (sr, 180.0, 1.3065630);
    lpA.c = makeLowpass (sr, 4000.0, 0.5411961);   lpB.c = makeLowpass (sr, 4000.0, 1.3065630);

    cEnvA  = coefFromMs (1.0, sr);   cEnvR = coefFromMs (20.0, sr);
    cFast  = coefFromMs (2.0, sr);   cSlow = coefFromMs (40.0, sr);
    cSyl   = coefFromMs (12.0, sr);
    cPhrA  = coefFromMs (30.0, sr);  cPhrR = coefFromMs (500.0, sr);
    cAct   = coefFromMs (30.0, sr);
    cCrest = coefFromMs (400.0, sr);
    pkDecay = std::exp (-1.0 / (0.4 * sr));
    stepSec = (double) kCtl / sr;
    reset();
}

void VocalAnalyzer::reset() noexcept
{
    lowBand.reset();
    highBand.reset();
    hpA.reset(); hpB.reset(); lpA.reset(); lpB.reset();
    eTot = eLow = eHigh = eFast = eSlow = eSyl = ePhr = eAct = msCrest = pk = 0.0;
    ctlCount = 0;
    plosiveHold = onsetHold = 0;
    pState = sState = vState = oState = pActivity = 0.0;
    crestDb = 12.0;
    phraseDb = -120.0;
    ctl = VocalControl {};
}

void VocalAnalyzer::setAmounts (double plosive, double sibilance, double syllable, double phrase, double adaptive)
{
    amtP = plosive; amtS = sibilance; amtSyl = syllable; amtPhr = phrase; amtAd = adaptive;
    const bool was = active;
    active = (plosive > 0.0 || sibilance > 0.0 || syllable > 0.0 || phrase > 0.0 || adaptive > 0.0);
    if (! active && was)
        ctl = VocalControl {};
}

double VocalAnalyzer::process (double x) noexcept
{
    if (! active)
        return x;

    const double lo = lowBand.process (x);
    const double hi = highBand.process (x);
    const double ax = std::abs (x);

    eTot  += (ax > eTot ? cEnvA : cEnvR) * (ax - eTot);
    const double alo = std::abs (lo), ahi = std::abs (hi);
    eLow  += (alo > eLow ? cEnvA : cEnvR) * (alo - eLow);
    eHigh += (ahi > eHigh ? cEnvA : cEnvR) * (ahi - eHigh);
    eFast += cFast * (ax - eFast);
    eSlow += cSlow * (ax - eSlow);
    eSyl  += cSyl * (ax - eSyl);
    ePhr  += (ax > ePhr ? cPhrA : cPhrR) * (ax - ePhr);
    eAct  += (ax > eAct ? cEnvA : cAct) * (ax - eAct);
    msCrest += cCrest * (x * x - msCrest);
    pk = std::max (ax, pk * pkDecay);

    if (++ctlCount >= kCtl)
    {
        ctlCount = 0;
        updateControl();
    }

    // 検出器に渡す信号: 破裂音の間は 4次HP(180 Hz)へ、歯擦音の間は 4次LP(4 kHz)へ、状態と量に応じて滑らかに近づける。
    // (単純な「x - 低域」は位相のずれで低域が十分に消えないため、4次の分離フィルターを使う)
    const double hpx = hpB.process (hpA.process (x));
    const double lpx = lpB.process (lpA.process (x));
    return x + amtP * pState * (hpx - x) + amtS * sState * (lpx - x);
}

void VocalAnalyzer::updateControl() noexcept
{
    constexpr double floorLin = 0.0031622776601683794;   // -50 dBFS
    const double dt = stepSec;
    auto follow = [dt] (double& state, double target, double attackSec, double releaseSec)
    {
        const double tau = target > state ? attackSec : releaseSec;
        state += (1.0 - std::exp (-dt / tau)) * (target - state);
    };

    const double ratioLow  = eLow  / (eTot + 1.0e-9);
    const double ratioHigh = eHigh / (eTot + 1.0e-9);
    const double burst     = eFast / (eSlow + 1.0e-6);

    // --- 破裂音: 低域が支配的で、立ち上がりが急(持続する低音の基音とは区別) ---
    if (ratioLow > 0.6 && burst > 1.6 && eTot > floorLin)
        plosiveHold = (int) (0.040 / dt);
    else if (plosiveHold > 0)
        --plosiveHold;
    follow (pState, plosiveHold > 0 ? 1.0 : 0.0, 0.002, 0.040);

    // --- 歯擦音: 高域エネルギーの比率 ---
    const double sibTarget = smoothstep (0.35, 0.60, ratioHigh) * (eHigh > floorLin * 0.5 ? 1.0 : 0.0);
    follow (sState, sibTarget, 0.001, 0.015);

    // --- 音節: オンセット(立ち上がり)と谷(音節の切れ目) ---
    if (burst > 1.8 && eFast > floorLin)
        onsetHold = (int) (0.030 / dt);
    else if (onsetHold > 0)
        --onsetHold;
    follow (oState, onsetHold > 0 ? 1.0 : 0.0, 0.003, 0.020);

    const bool valley = (eSyl < 0.35 * ePhr) && (ePhr > floorLin);
    follow (vState, valley ? 1.0 : 0.0, 0.010, 0.030);

    // --- フレーズ: 持続的な活動度(アタック遅め=一瞬の音は無視) ---
    // 活動度は速い包絡線(リリース30 ms)で判定: 休符に入ったらすぐ「休み」と分かる。
    // 判定レベルは参照レベル(Threshold+10 dB)の 25 dB 下(Threshold の 15 dB 下)。
    const double actDb = 20.0 * std::log10 (eAct + 1.0e-9);
    follow (pActivity, actDb > refDb - 25.0 ? 1.0 : 0.0, 0.150, 0.040);
    // フレーズの大きさ(適応用)は遅い包絡線
    const double phraseDbNow = 20.0 * std::log10 (ePhr + 1.0e-9);
    phraseDb = phraseDbNow;

    // --- 波高率(クレスト)---
    const double rms = std::sqrt (msCrest) + 1.0e-9;
    crestDb += (1.0 - std::exp (-dt / 0.4)) * (20.0 * std::log10 ((pk + 1.0e-9) / rms) - crestDb);

    // --- 制御値の合成 ---
    ctl.attackFactor = 1.0 + 0.8 * amtSyl * oState;                      // オンセットは通す(アタックを遅く)
    double rel = 1.0 - 0.7 * amtSyl * vState;                            // 音節の谷では速く戻す
    rel *= 1.0 + amtPhr * (0.5 * pActivity - 0.5 * (1.0 - pActivity));   // 歌唱中は遅く、休符では速く
    ctl.releaseFactor = std::clamp (rel, 0.2, 3.0);

    // 適応: フレーズが大きいほどしきい値を下げる(=大きいフレーズをより圧縮)
    ctl.thresholdShiftDb = -amtAd * std::clamp (0.5 * (phraseDb - refDb), -6.0, 6.0) * pActivity;
    ctl.ratioScale = 1.0 + 0.3 * amtAd * std::clamp ((crestDb - 12.0) / 12.0, -1.0, 1.0);
}

} // namespace vc
