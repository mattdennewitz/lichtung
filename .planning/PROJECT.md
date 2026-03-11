# Harmonic Oscillator

## What This Is

A JUCE audio plugin (VST3/AU) that faithfully translates the Verbos Harmonic Oscillator Max/MSP patch into a standalone C++ plugin. It's a monophonic additive synthesizer with 8 individually controllable harmonics, spectral scan/tilt controls, linear and exponential FM with an internal oscillator, classic PolyBLEP waveform outputs, and an AR envelope. Built for the developer's own music production use.

## Core Value

The 8-harmonic additive synthesis engine with scan and tilt must produce the same sound as the Max/MSP gen~ codebox — accurate DSP translation is everything.

## Requirements

### Validated

(None yet — ship to validate)

### Active

- [ ] 8-harmonic sine bank with individual level controls (faders)
- [ ] Scan envelope (Gaussian window) with center and width parameters
- [ ] Spectral tilt (linear-in-dB slope across harmonics, +/-24dB range)
- [ ] PolyBLEP sawtooth, square, and triangle (leaky integrated square) waveforms
- [ ] Output selector: hard switch between Harmonic Mix, Triangle, Saw, Square
- [ ] Linear FM and exponential FM with internal LFO (sine, 0.1Hz to audio rate)
- [ ] AR envelope (attack 1-2000ms, release 1-5000ms, exponential curves)
- [ ] Coarse tuning (+/-24 semitones), fine tuning (+/-100 cents)
- [ ] Portamento/glide (0-2000ms)
- [ ] Monophonic with legato (overlapping notes glide without retriggering envelope)
- [ ] Master level control
- [ ] DC blocker on harmonic mix output
- [ ] MIDI note input with velocity sensitivity
- [ ] Anti-aliasing: skip harmonics above Nyquist
- [ ] GUI replicating the Max patch layout: Pitch, Harmonics (8 vertical sliders), Scan/Tilt, FM, Output sections
- [ ] VST3 and AU plugin formats
- [ ] macOS builds (developer's platform)

### Out of Scope

- Polyphony — this is a mono synth
- External FM sidechain input — internal LFO only
- Preset browser / factory presets — host handles presets via parameter state
- Windows/Linux builds — macOS only for now
- CLAP format — VST3/AU sufficient

## Context

- Reference implementation: `~/src/harmonic-oscillator-plugin/HarmonicOscillator.maxpat` (Max 8.6, gen~ codebox with all DSP)
- The gen~ codebox contains the complete DSP algorithm — frequency/FM calculation, phase accumulator, PolyBLEP waveforms, scan envelope, spectral tilt, 8-harmonic sine bank, AR envelope, DC blocker
- Harmonic levels are stored in a shared buffer written by poke~ from UI sliders — in JUCE this becomes direct parameter access
- The patch uses `slide~` for glide, `accum` for gate counting (legato), `selector~` for output switching
- FM LFO range extended from 0.1-100Hz (patch) to 0.1Hz-to-audio-rate per user request
- All parameters use `live.dial` / `live.slider` / `live.menu` with Ableton-compatible ranges

## Constraints

- **Tech stack**: JUCE framework (C++) — industry standard for audio plugins
- **Platform**: macOS (Apple Silicon + Intel via universal binary)
- **Format**: VST3 + AU
- **DSP accuracy**: Must match the gen~ codebox behavior sample-for-sample where possible
- **Performance**: Single voice, should be extremely lightweight (<1% CPU)

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Monophonic only | Matches original patch, simpler architecture | — Pending |
| Internal FM LFO extended to audio rate | More sound design potential than patch's 100Hz cap | — Pending |
| Legato with gate counting | Matches patch's accum-based gate logic, natural for mono synth | — Pending |
| Hard output switching (no crossfade) | Matches patch's selector~ behavior | — Pending |
| JUCE framework | Industry standard, handles VST3/AU/GUI, well-documented | — Pending |

---
*Last updated: 2026-03-10 after initialization*
