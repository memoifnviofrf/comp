#include "SidechainProcessor.h"

namespace vc
{
void SidechainProcessor::prepare (double sampleRate)
{
    sr = sampleRate;
    currentWeighting = Weighting::Off;
    numWeight = 0;
    weightGain = 1.0;
    reset();
}

void SidechainProcessor::reset() noexcept
{
    for (auto& c : chain)
    {
        c.hpf.reset(); c.lpf.reset();
        for (auto& e : c.eq) e.reset();
        for (auto& w : c.wt) w.reset();
    }
}

void SidechainProcessor::setParams (const Params& p)
{
    gain = dbToLin (p.scGainDb);

    useHpf = p.scHpfHz > 20.5;
    useLpf = p.scLpfHz < 19999.0;
    useEq[0] = std::abs (p.eqLowDb)  > 0.01;
    useEq[1] = std::abs (p.eqMidDb)  > 0.01;
    useEq[2] = std::abs (p.eqHighDb) > 0.01;

    for (auto& c : chain)
    {
        if (useHpf) c.hpf.c = makeHighpass (sr, p.scHpfHz, 0.7071);
        if (useLpf) c.lpf.c = makeLowpass (sr, p.scLpfHz, 0.7071);
        if (useEq[0]) c.eq[0].c = makeLowShelf (sr, p.eqLowHz, p.eqLowDb);
        if (useEq[1]) c.eq[1].c = makePeaking (sr, p.eqMidHz, 1.0, p.eqMidDb);
        if (useEq[2]) c.eq[2].c = makeHighShelf (sr, p.eqHighHz, p.eqHighDb);
    }

    if (p.weighting != currentWeighting)
        designWeighting (p.weighting);
}

void SidechainProcessor::designWeighting (Weighting w)
{
    currentWeighting = w;
    numWeight = 0;
    weightGain = 1.0;

    auto set = [this] (int i, const BiquadCoeffs& c) { for (auto& ch : chain) ch.wt[(size_t) i].c = c; };

    if (w == Weighting::Voice)
    {
        // 声の帯域(約120 Hz〜12 kHz)を中心に、プレゼンスを少し強調
        set (0, makeHighpass (sr, 120.0, 0.7071));
        set (1, makePeaking (sr, 3000.0, 0.7, 3.0));
        set (2, makeLowpass (sr, 12000.0, 0.7071));
        numWeight = 3;
    }
    else if (w == Weighting::AWeight)
    {
        // A特性: H(s) = k s^4 / ((s+w1)^2 (s+w2)(s+w3)(s+w4)^2)(IEC 61672 の極: 20.6 / 107.7 / 737.9 / 12194 Hz)
        //   = [20.6 Hz の二重極のHP] x [107.7 Hz と 737.9 Hz の実数極をもつ2次HP] x [12194 Hz の二重極のLP]
        set (0, makeHighpass (sr, 20.6, 0.5));
        const double w2 = 107.7, w3 = 737.9;
        const double f0 = std::sqrt (w2 * w3), q = f0 / (w2 + w3);   // 実数極2つ -> Q = 0.333
        set (1, makeHighpass (sr, f0, q));
        set (2, makeLowpass (sr, 12194.0, 0.5));
        numWeight = 3;
    }

    if (numWeight > 0)
    {
        // 1 kHz で 0 dB に正規化
        double mag = 1.0;
        for (int i = 0; i < numWeight; ++i)
            mag *= biquadMagnitude (chain[0].wt[(size_t) i].c, 2.0 * kPi * 1000.0 / sr);
        weightGain = 1.0 / std::max (mag, 1.0e-9);
    }
    reset();
}

double SidechainProcessor::weightingMagnitude (double freqHz) const
{
    double mag = weightGain;
    for (int i = 0; i < numWeight; ++i)
        mag *= biquadMagnitude (chain[0].wt[(size_t) i].c, 2.0 * kPi * freqHz / sr);
    return mag;
}

} // namespace vc
