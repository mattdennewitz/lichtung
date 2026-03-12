# Lichtung

A synthesizer plugin (AU/VST3/Standalone) by **Die stille Erde**, ported from a Max/MSP gen~ patch to C++/JUCE. Named for the German word meaning "clearing" — an opening in the forest where light enters — evoking the way the scan window reveals hidden structure in the harmonic series.

## Architecture

The plugin is a monophonic MIDI synth with no audio input. A single `HarmonicEngine` runs sample-by-sample inside the audio callback. The processor interleaves MIDI event handling with sample rendering so note events land at their exact sample position.

## Signal Flow

```
┌─────────────────────────────────────────────────────────────────┐
│  MIDI IN                                                        │
│    │                                                            │
│    ├── Note On/Off → gate counter (legato) → AR envelope        │
│    └── Note number → glide (exponential portamento) → pitch     │
│                                                                 │
│  ┌──────────────┐                                               │
│  │  PITCH BLOCK │                                               │
│  │              │                                               │
│  │  glided note ──→ mtof ──→ base freq                          │
│  │  coarse tune ──→ ┐                                           │
│  │  fine tune   ──→ ┴─ semitone ratio ──→ tuned freq            │
│  │                                                              │
│  │  FM LFO (sine, free-running) ──┐                             │
│  │  lin FM depth ──→ Hz offset ───┤                             │
│  │  exp FM depth ──→ pitch mult ──┴──→ final freq               │
│  └──────────────┘           │                                   │
│                             ▼                                   │
│  ┌──────────────────────────────────────────┐                   │
│  │  OSCILLATOR BLOCK                        │                   │
│  │                                          │                   │
│  │  Master phase accumulator (final freq)   │                   │
│  │    ├──→ PolyBLEP Sawtooth                │                   │
│  │    ├──→ PolyBLEP Square                  │                   │
│  │    └──→ Triangle (leaky-integrated sqr)  │                   │
│  │                                          │                   │
│  │  8 independent harmonic phase accums     │                   │
│  │    └──→ Sine bank (harmonics 1–8)        │                   │
│  │           weighted by:                   │                   │
│  │             • fader level (per-harmonic)  │                   │
│  │             • scan envelope (Gaussian)    │                   │
│  │             • spectral tilt              │                   │
│  └──────────────────────────────────────────┘                   │
│                             │                                   │
│                             ▼                                   │
│  ┌──────────────────────────────────────────┐                   │
│  │  OUTPUT BLOCK                            │                   │
│  │                                          │                   │
│  │  output select ──→ mix / tri / saw / sqr │                   │
│  │  DC blocker (on harmonic mix only)       │                   │
│  │  × AR envelope                           │                   │
│  │  × master level                          │                   │
│  │  ──→ mono out, copied to both channels   │                   │
│  └──────────────────────────────────────────┘                   │
└─────────────────────────────────────────────────────────────────┘
```

## Block Details

### Pitch

MIDI note number passes through an exponential glide (portamento), then standard `mtof` conversion. Coarse tune (semitones) and fine tune (cents) are combined into a single frequency ratio. An internal sine LFO provides both linear FM (Hz offset proportional to carrier) and exponential FM (pitch multiplication). The result is clamped below Nyquist.

### Oscillator

Two parallel generation paths share the same final frequency:

**PolyBLEP waveforms** — A single master phase drives sawtooth, square, and triangle generators. Saw and square use first-order PolyBLEP anti-aliasing. Triangle is derived by leaky-integrating the square wave.

**8-harmonic sine bank** — Eight sine oscillators at integer multiples of the fundamental (1×–8×), each with its own phase accumulator. Harmonics above Nyquist are silenced. Each harmonic's amplitude is the product of three factors:

- **Fader level** — direct per-harmonic gain (0–1), set by the user
- **Scan envelope** — a Gaussian window centered at `scan_center` with spread `scan_width`, applied across the 8 harmonics. Sweeping the center crossfades through the harmonic series.
- **Spectral tilt** — a dB-linear slope across harmonics, from ±24 dB. Positive tilt boosts upper harmonics (brighter), negative boosts lower (darker).

The bank output is scaled by a fixed normalization factor matching the original gen~ patch.

### Envelope

A one-pole AR envelope with exponential coefficients, driven by a legato gate counter. Multiple overlapping notes hold the gate open; only the last note-off closes it. Attack and release times are in milliseconds.

### Output

A 4-way selector chooses between the harmonic mix, triangle, sawtooth, or square. The harmonic mix path runs through a DC blocker (high-pass at 5 Hz). The selected signal is multiplied by the envelope and master level, then written identically to both output channels (mono synth).

### Parameter Smoothing

All continuous parameters use 64-sample smoothed values to prevent zipper noise. Coarse tune and output select are discrete (int/choice) and not smoothed.

### Random Timbre

The "?" button in the Harmonics panel header randomizes 11 timbre parameters with musically weighted distributions. Harmonic fader amplitudes are biased toward lower harmonics (`random^(n×0.5)`). Scan width is constrained to 0.3–1.0 to prevent muting, and spectral tilt stays within ±0.5 to avoid extreme filtering.

## Testing

```
cmake -B build-tests -DLICHTUNG_BUILD_TESTS=ON
cmake --build build-tests
./build-tests/LichtungTests_artefacts/Release/LichtungTests
```

27 test cases covering pitch accuracy across sample rates (44.1k–192k), envelope edge cases, FM extremes, glide safety, scan/tilt shaping, output mode switching, Nyquist safety, DC blocking, and legato behavior.

## Building

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Produces a macOS universal binary (arm64 + x86_64). JUCE 8.0.12 is fetched automatically. Output formats: AU, VST3, Standalone.

## Origin

Ported from a Max/MSP gen~ patch. The DSP structure mirrors the original section-by-section. Known minor differences: the DC blocker cutoff is slightly higher than Max's `dcblock~` default, and harmonic amplitude uses a hard Nyquist cutoff rather than a soft rolloff.
