# Sevenband Master

Per-band adaptive mastering for the master bus or a sub-bus. Tonal balance + multiband dynamics only:
no loudness metering, no limiter. Do peaks/loudness with whatever limiter you already like.


Delete the `build` folder first when upgrading. Parameters were removed (LUFS Offset, Auto Gain, Limiter,
Ceiling), so old FL projects load with defaults.

## Signal flow

1. **7-band LR4 crossover** (60 / 250 / 500 / 2k / 4k / 8k Hz), phase-aligned so the bands sum flat.
2. **Analysis** of each band's long-term level (Analysis knob), measured before any correction.
3. **Per-band dynamics**: linked-stereo compressor whose threshold follows the band's own running
   average (+ a genre offset), so it tames spiky bands relative to the song itself.
4. **Per-band tonal balance**: slow gain toward the genre's target curve.
   Mid (500-2k) is the reference and is never moved.
5. Output trim.

## Controls

- **Vertical sliders** (under each band): orange = Balance amount, red = Dynamics amount. 0% leaves that
  band alone for that process. Mid has no Balance slider (it is the reference).
- **Balance All / Dynamics All**: master scalers on top of the per-band sliders.
- **Max Cut / Max Boost**: limits for tonal corrections (boost is smaller by default).
- **Roll-off guard**: a band far below target (>6 dB, gone at 12 dB) has its boost faded out, so
  intentionally dark / low-passed masters are not brightened.
- **Analysis**: how slowly the plugin learns the song's average spectrum.
- **Genre**: House/UKG, Hip-Hop, Lo-fi/R&B, Pop. Edit `Source/Genres.h` to tune curves and dynamics.

Meters: blue = band level vs. target (up = too loud), orange = balance correction applied,
red = compression gain reduction.