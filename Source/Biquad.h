#pragma once

// 小さなDSP部品: 双2次フィルター(RBJ)、DCブロッカー、時定数の係数計算。
// すべて double 精度・メモリ確保なし。

#include <algorithm>
#include <cmath>

namespace vc
{
constexpr double kPi = 3.14159265358979323846;

struct BiquadCoeffs { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };

inline double clampFreq (double f, double fs) { return std::clamp (f, 5.0, fs * 0.49); }

inline BiquadCoeffs normalise (double b0, double b1, double b2, double a0, double a1, double a2)
{
    return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
}

inline BiquadCoeffs makeLowpass (double fs, double f, double q)
{
    const double w = 2.0 * kPi * clampFreq (f, fs) / fs, cw = std::cos (w), al = std::sin (w) / (2.0 * q);
    return normalise ((1 - cw) * 0.5, 1 - cw, (1 - cw) * 0.5, 1 + al, -2 * cw, 1 - al);
}

inline BiquadCoeffs makeHighpass (double fs, double f, double q)
{
    const double w = 2.0 * kPi * clampFreq (f, fs) / fs, cw = std::cos (w), al = std::sin (w) / (2.0 * q);
    return normalise ((1 + cw) * 0.5, -(1 + cw), (1 + cw) * 0.5, 1 + al, -2 * cw, 1 - al);
}

inline BiquadCoeffs makeBandpass (double fs, double f, double q)   // 0 dB peak gain
{
    const double w = 2.0 * kPi * clampFreq (f, fs) / fs, cw = std::cos (w), al = std::sin (w) / (2.0 * q);
    return normalise (al, 0, -al, 1 + al, -2 * cw, 1 - al);
}

inline BiquadCoeffs makePeaking (double fs, double f, double q, double gainDb)
{
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w = 2.0 * kPi * clampFreq (f, fs) / fs, cw = std::cos (w), al = std::sin (w) / (2.0 * q);
    return normalise (1 + al * A, -2 * cw, 1 - al * A, 1 + al / A, -2 * cw, 1 - al / A);
}

inline BiquadCoeffs makeLowShelf (double fs, double f, double gainDb)   // Q = 0.707
{
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w = 2.0 * kPi * clampFreq (f, fs) / fs, cw = std::cos (w), sw = std::sin (w);
    const double k = 2.0 * std::sqrt (A) * (sw / (2.0 * 0.7071067811865476));
    return normalise (A * ((A + 1) - (A - 1) * cw + k), 2 * A * ((A - 1) - (A + 1) * cw), A * ((A + 1) - (A - 1) * cw - k),
                      (A + 1) + (A - 1) * cw + k, -2 * ((A - 1) + (A + 1) * cw), (A + 1) + (A - 1) * cw - k);
}

inline BiquadCoeffs makeHighShelf (double fs, double f, double gainDb)  // Q = 0.707
{
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w = 2.0 * kPi * clampFreq (f, fs) / fs, cw = std::cos (w), sw = std::sin (w);
    const double k = 2.0 * std::sqrt (A) * (sw / (2.0 * 0.7071067811865476));
    return normalise (A * ((A + 1) + (A - 1) * cw + k), -2 * A * ((A - 1) + (A + 1) * cw), A * ((A + 1) + (A - 1) * cw - k),
                      (A + 1) - (A - 1) * cw + k, 2 * ((A - 1) - (A + 1) * cw), (A + 1) - (A - 1) * cw - k);
}

// |H(e^jw)| (線形)
inline double biquadMagnitude (const BiquadCoeffs& c, double w)
{
    const double cw = std::cos (w), sw = std::sin (w), c2 = std::cos (2 * w), s2 = std::sin (2 * w);
    const double nr = c.b0 + c.b1 * cw + c.b2 * c2, ni = -(c.b1 * sw + c.b2 * s2);
    const double dr = 1.0 + c.a1 * cw + c.a2 * c2,   di = -(c.a1 * sw + c.a2 * s2);
    return std::sqrt ((nr * nr + ni * ni) / (dr * dr + di * di + 1.0e-300));
}

// 転置直接型II
struct Biquad
{
    BiquadCoeffs c;
    double z1 = 0.0, z2 = 0.0;

    void reset() noexcept { z1 = z2 = 0.0; }

    double process (double x) noexcept
    {
        const double y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        return y;
    }
};

// 1次のDCブロッカー: y[n] = x[n] - x[n-1] + R y[n-1]
struct DcBlocker
{
    double R = 0.9995, x1 = 0.0, y1 = 0.0;

    void prepare (double fs, double fc = 3.0) { R = std::exp (-2.0 * kPi * fc / fs); reset(); }
    void reset() noexcept { x1 = y1 = 0.0; }

    double process (double x) noexcept
    {
        const double y = x - x1 + R * y1;
        x1 = x;
        y1 = y;
        return y;
    }
};

// 1次ローパス(指数平滑)の係数: 時定数 ms
inline double coefFromMs (double ms, double fs)
{
    return 1.0 - std::exp (-1.0 / (std::max (ms, 1.0e-6) * 0.001 * fs));
}

inline double dbToLin (double db) { return std::pow (10.0, db * 0.05); }
inline double linToDb (double lin) { return 20.0 * std::log10 (lin + 1.0e-12); }

} // namespace vc
