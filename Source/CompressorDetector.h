#pragma once

// 検出器
//   LevelDetector : ピーク / RMS / ブレンド
//   VocalAnalyzer : ボーカル専用の解析(破裂音・歯擦音・音節・フレーズ・適応)
//
// どちらも「サイドチェイン信号 -> 検出レベル / 制御値」を作るだけで、
// ゲインの適用は CompressorDSP が行います。

#include "Biquad.h"
#include "CompressorParameters.h"

namespace vc
{
//==============================================================================
class LevelDetector
{
public:
    void prepare (double sampleRate);
    void reset() noexcept { ms = 0.0; }
    void setRmsWindowMs (double windowMs);
    void setBlend (double b) noexcept { blend = std::clamp (b, 0.0, 1.0); }

    // 戻り値: 検出レベル(線形)。RMS は √2 倍して「正弦波ならピークと同じ値」に校正してある。
    double process (double x) noexcept
    {
        const double a = std::abs (x);
        ms += rmsCoef * (x * x - ms);
        const double rms = std::sqrt (ms) * 1.4142135623730951;
        return (1.0 - blend) * a + blend * rms;
    }

private:
    double sr = 44100.0, rmsCoef = 0.0, blend = 0.5, ms = 0.0;
};

//==============================================================================
// ボーカル解析の結果(CompressorDSP が時定数・しきい値・レシオを調整するために使う)
struct VocalControl
{
    double attackFactor = 1.0;      // アタック時間への係数
    double releaseFactor = 1.0;     // リリース時間への係数
    double thresholdShiftDb = 0.0;  // しきい値への加算 [dB]
    double ratioScale = 1.0;        // レシオへの係数
};

class VocalAnalyzer
{
public:
    static constexpr int kCtl = 16; // 解析の更新周期(サンプル)

    void prepare (double sampleRate);
    void reset() noexcept;
    void setAmounts (double plosive, double sibilance, double syllable, double phrase, double adaptive);
    void setReferenceDb (double db) noexcept { refDb = db; }

    // x: サイドチェイン信号。戻り値: 検出器に渡す信号(破裂音・歯擦音の寄与を除去したもの)
    double process (double x) noexcept;

    const VocalControl& control() const noexcept { return ctl; }
    bool isActive() const noexcept { return active; }

    // 表示・テスト用
    double plosiveState() const noexcept { return pState; }
    double sibilanceState() const noexcept { return sState; }
    double valleyState() const noexcept { return vState; }
    double phraseActivity() const noexcept { return pActivity; }

private:
    void updateControl() noexcept;

    double sr = 44100.0;
    bool active = false;
    double amtP = 0, amtS = 0, amtSyl = 0, amtPhr = 0, amtAd = 0, refDb = -10.0;

    Biquad lowBand, highBand;   // 検出用: 150 Hz 以下 / 4 kHz 以上(2次)
    // 検出器に渡す信号を作るための4次バターワース(2次×2): 破裂音時は HP 180 Hz、歯擦音時は LP 4 kHz へ近づける
    Biquad hpA, hpB, lpA, lpB;

    // 包絡線(サンプル周期で更新)
    double eTot = 0, eLow = 0, eHigh = 0, eFast = 0, eSlow = 0, eSyl = 0, ePhr = 0, eAct = 0, msCrest = 0, pk = 0;
    double cEnvA = 0, cEnvR = 0, cFast = 0, cSlow = 0, cSyl = 0, cPhrA = 0, cPhrR = 0, cAct = 0, cCrest = 0, pkDecay = 0;

    // 制御周期の状態
    int ctlCount = 0;
    double stepSec = 0.0;
    int plosiveHold = 0, onsetHold = 0;
    double pState = 0, sState = 0, vState = 0, oState = 0, pActivity = 0, crestDb = 12.0, phraseDb = -120.0;
    VocalControl ctl;
};

} // namespace vc
