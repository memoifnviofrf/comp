#pragma once

// サイドチェインの信号処理: ゲイン -> HPF -> LPF -> 3バンドEQ -> 周波数重み付け
// 検出専用の経路です(音声そのものには掛かりません)。

#include <array>
#include "Biquad.h"
#include "CompressorParameters.h"

namespace vc
{
class SidechainProcessor
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;
    void setParams (const Params& p);

    // ch: 0 or 1
    double process (int ch, double x) noexcept
    {
        auto& c = chain[(size_t) ch];
        x *= gain;
        if (useHpf) x = c.hpf.process (x);
        if (useLpf) x = c.lpf.process (x);
        if (useEq[0]) x = c.eq[0].process (x);
        if (useEq[1]) x = c.eq[1].process (x);
        if (useEq[2]) x = c.eq[2].process (x);
        for (int i = 0; i < numWeight; ++i)
            x = c.wt[(size_t) i].process (x);
        return x * weightGain;
    }

    // 周波数重み付けの応答(線形)。テスト用。
    double weightingMagnitude (double freqHz) const;

private:
    static constexpr int kMaxWeight = 4;
    struct Chain { Biquad hpf, lpf, eq[3]; std::array<Biquad, kMaxWeight> wt; };

    void designWeighting (Weighting w);

    double sr = 44100.0, gain = 1.0, weightGain = 1.0;
    bool useHpf = false, useLpf = false;
    bool useEq[3] = { false, false, false };
    int numWeight = 0;
    Weighting currentWeighting = Weighting::Off;
    std::array<Chain, 2> chain;
};

} // namespace vc
