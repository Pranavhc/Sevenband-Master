#pragma once
#include <array>

// Starting-point genre presets. All values are estimates meant to be tuned by ear.
//
// targetRelDb      : target band level (dB RMS) relative to the Mid band (500-2k)
//                    order: Sub, Bass, Low-Mid, Mid, Up-Mid, Presence, Air
// ratio            : per-band compression ratio
// thresholdOffsetDb: compressor threshold = band's running average level + this offset
//                    (smaller = more compression, since more of the signal is above it)
// attackScale /
// releaseScale     : multipliers on the per-band base attack/release times
struct GenrePreset
{
    const char*          name;
    std::array<float, 7> targetRelDb;
    float                ratio;
    float                thresholdOffsetDb;
    float                attackScale;
    float                releaseScale;
};

inline constexpr std::array<GenrePreset, 4> kGenres {{
    // House / UKG: strong sub + bass, tight and punchy
    { "House / UKG", {{ -5.0f,  2.0f, -2.0f, 0.0f, -6.0f, -11.0f, -18.0f }}, 2.0f, 1.5f, 0.8f, 0.8f },
    // Hip-Hop: heavy low end, dark top, firm control of the lows
    { "Hip-Hop",     {{ -3.0f,  3.0f,  0.0f, 0.0f, -7.0f, -13.0f, -20.0f }}, 2.2f, 2.0f, 1.2f, 1.0f },
    // Lo-fi / R&B: warm, soft highs, gentle and slow dynamics
    { "Lo-fi / R&B", {{ -9.0f,  0.0f,  1.0f, 0.0f, -8.0f, -15.0f, -23.0f }}, 1.6f, 3.0f, 1.5f, 1.5f },
    // Pop: bright and forward, balanced low end
    { "Pop",         {{ -8.0f,  0.0f, -1.0f, 0.0f, -5.0f, -10.0f, -17.0f }}, 2.0f, 1.5f, 1.0f, 1.0f },
}};
