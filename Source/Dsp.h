#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace dsp7
{
constexpr double kPi = 3.14159265358979323846;

//==============================================================================
// Double-precision transposed direct-form II biquad
struct Biquad
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;

    void set (double B0, double B1, double B2, double A0, double A1, double A2) noexcept
    {
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0;
        a1 = A1 / A0; a2 = A2 / A0;
    }

    void reset() noexcept { z1 = z2 = 0.0; }

    inline double process (double x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

inline void makeLowpass (Biquad& f, double sr, double fc, double q)
{
    const double w = 2.0 * kPi * fc / sr, c = std::cos (w), alpha = std::sin (w) / (2.0 * q);
    f.set ((1.0 - c) * 0.5, 1.0 - c, (1.0 - c) * 0.5, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
}

inline void makeHighpass (Biquad& f, double sr, double fc, double q)
{
    const double w = 2.0 * kPi * fc / sr, c = std::cos (w), alpha = std::sin (w) / (2.0 * q);
    f.set ((1.0 + c) * 0.5, -(1.0 + c), (1.0 + c) * 0.5, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
}

inline void makeAllpass (Biquad& f, double sr, double fc, double q)
{
    const double w = 2.0 * kPi * fc / sr, c = std::cos (w), alpha = std::sin (w) / (2.0 * q);
    f.set (1.0 - alpha, -2.0 * c, 1.0 + alpha, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
}

//==============================================================================
// 7-band Linkwitz-Riley (LR4) splitter for ONE channel.
// Bands are split sequentially; lower bands are run through allpasses matching the
// higher crossovers so that the bands sum back flat (phase-aligned).
class ChannelSplitter
{
public:
    static constexpr int numBands = 7;
    static constexpr int numXovers = numBands - 1;

    void prepare (double sr, const std::array<double, numXovers>& freqs)
    {
        const double q = 0.70710678118;
        for (int i = 0; i < numXovers; ++i)
        {
            const double f = std::min (freqs[(size_t) i], sr * 0.45);
            for (int s = 0; s < 2; ++s)
            {
                makeLowpass  (lp[i][s], sr, f, q);
                makeHighpass (hp[i][s], sr, f, q);
            }
            for (int j = i + 1; j < numXovers; ++j)
                makeAllpass (ap[i][j], sr, std::min (freqs[(size_t) j], sr * 0.45), q);
        }
        reset();
    }

    void reset()
    {
        for (int i = 0; i < numXovers; ++i)
        {
            for (int s = 0; s < 2; ++s) { lp[i][s].reset(); hp[i][s].reset(); }
            for (int j = 0; j < numXovers; ++j) ap[i][j].reset();
        }
    }

    // out must hold numBands values
    inline void process (double x, double* out) noexcept
    {
        double rest = x;
        for (int i = 0; i < numXovers; ++i)
        {
            double lo = lp[i][1].process (lp[i][0].process (rest));
            const double hi = hp[i][1].process (hp[i][0].process (rest));
            for (int j = i + 1; j < numXovers; ++j)
                lo = ap[i][j].process (lo);
            out[i] = lo;
            rest = hi;
        }
        out[numBands - 1] = rest;
    }

private:
    Biquad lp[numXovers][2], hp[numXovers][2], ap[numXovers][numXovers];
};

} // namespace dsp7
