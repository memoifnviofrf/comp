#include "Metering.h"
#include <algorithm>
#include <cmath>

namespace vc
{
void MeterBallistics::reset (float floorDb)
{
    peak = peakHold = rms = floorDb;
    holdTimer = 0.0f;
}

void MeterBallistics::update (float peakDb, float rmsDb, float dtSec)
{
    // ピーク: 上昇は即時、下降は一定速度(dB/s)
    peak = peakDb >= peak ? peakDb : std::max (peakDb, peak - fall * dtSec);

    // ピークホールド
    if (peakDb >= peakHold)
    {
        peakHold = peakDb;
        holdTimer = hold;
    }
    else if (holdTimer > 0.0f)
    {
        holdTimer -= dtSec;
    }
    else
    {
        peakHold = std::max (peakDb, peakHold - fall * dtSec);
    }

    // RMS: 約 300 ms の積分(パワー領域ではなく dB 領域で簡易平滑)
    const float k = 1.0f - std::exp (-dtSec / 0.3f);
    rms += k * (rmsDb - rms);
}

} // namespace vc
