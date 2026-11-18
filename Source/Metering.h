#pragma once

// メーター
//   MeterData        : オーディオスレッド -> GUI の共有値(ロックなしの atomic)
//   MeterBallistics  : GUI側の表示用バリスティクス(ピークホールド・RMS平滑)

#include <atomic>

namespace vc
{
struct MeterData
{
    std::atomic<float> inPeakDb { -120.0f }, inRmsDb { -120.0f };
    std::atomic<float> outPeakDb { -120.0f }, outRmsDb { -120.0f };
    std::atomic<float> grDb { 0.0f };          // 正の値(= ゲインリダクション量 dB)
};

class MeterBallistics
{
public:
    // peakFallDbPerSec: ピークの落下速度 / holdSec: ピークを保持する時間
    explicit MeterBallistics (float peakFallDbPerSec = 24.0f, float holdSec = 1.0f)
        : fall (peakFallDbPerSec), hold (holdSec) {}

    void reset (float floorDb = -120.0f);
    void update (float peakDb, float rmsDb, float dtSec);

    float getPeak() const noexcept { return peak; }
    float getPeakHold() const noexcept { return peakHold; }
    float getRms() const noexcept { return rms; }

private:
    float fall, hold;
    float peak = -120.0f, peakHold = -120.0f, rms = -120.0f, holdTimer = 0.0f;
};

} // namespace vc
