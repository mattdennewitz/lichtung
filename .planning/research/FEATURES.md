# Feature Landscape

**Domain:** Additive synthesizer plugin (Verbos Harmonic Oscillator translation)
**Researched:** 2026-03-10
**Confidence:** MEDIUM (domain expertise, no web verification available)

## Table Stakes

Features users expect from any synthesizer plugin. Missing = product feels broken or incomplete.

### DSP Core

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| 8-harmonic sine bank with individual level controls | Defines the instrument. The Verbos HO's identity is 8 individually controllable harmonics. Without this, it is not a Harmonic Oscillator. | High | Core DSP engine. Each harmonic is a sine at integer multiple of fundamental. Must handle phase accumulation, anti-aliasing (skip harmonics above Nyquist). This is the most complex single feature. |
| Scan envelope (Gaussian window) | Signature Verbos feature. Center + width parameters sweep a window across harmonics, the primary performance control. | Medium | Gaussian distribution centered on a harmonic index, width controls spread. Modulates individual harmonic amplitudes multiplicatively. |
| Spectral tilt | Signature Verbos feature. Tilts the harmonic spectrum bright or dark with a linear-in-dB slope. | Low | Simple linear slope in dB across 8 harmonics. +/-24dB range. Applied multiplicatively with scan. |
| Fundamental frequency control (coarse + fine tuning) | Every synth needs pitch control. Coarse +/-24 semitones, fine +/-100 cents. | Low | Standard MIDI-to-frequency with semitone/cent offsets. |
| MIDI note input with velocity sensitivity | Plugin must respond to MIDI. Velocity mapped to amplitude is the minimum expectation. | Low | JUCE MidiBuffer processing. Map velocity to amplitude scaling. |
| Monophonic voice with legato | Matches the original module (single voice). Legato means overlapping notes glide without retriggering the envelope. | Medium | Gate counting (matching Max patch's accum logic). Must track note stack for last-note priority. |
| AR envelope | Every synth needs an amplitude envelope. AR (Attack-Release) matches the Verbos module's simplicity. | Low-Medium | Exponential curves. Attack 1-2000ms, release 1-5000ms. Gate counting for legato retrigger logic. |
| Master level control | Basic gain staging. | Low | Simple amplitude multiplier. |
| DC blocker on harmonic mix output | Additive synthesis with asymmetric harmonic levels produces DC offset. Without blocking, output drifts and causes clicks/pops downstream. | Low | Standard single-pole highpass at ~5-20Hz. Essential for additive synthesis. |
| Plugin state save/restore | Host must be able to save and recall all parameter values with the session. | Low | JUCE AudioProcessorValueTreeState handles this automatically if parameters are registered correctly. No preset browser needed -- host manages presets. |
| Sample-rate aware DSP | Must work at 44.1k, 48k, 88.2k, 96k without aliasing artifacts or pitch errors. | Low | Phase increment calculation uses sample rate. Anti-aliasing thresholds adapt. |

### Classic Waveforms

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| PolyBLEP sawtooth | The Verbos HO provides classic waveforms alongside the harmonic mix. Saw is the most useful single waveform. | Medium | PolyBLEP anti-aliasing is well-documented but must be implemented correctly. Polynomial bandlimited step correction at discontinuities. |
| PolyBLEP square | Standard waveform. | Medium | Built from two PolyBLEP sawtooth phases OR direct implementation. |
| Triangle (leaky integrated square) | Matches Verbos module. | Medium | Leaky integrator on square wave. Gain compensation needed as frequency changes. |
| Output selector (hard switch) | Matches Verbos module's selector behavior. Four positions: Harmonic Mix, Triangle, Saw, Square. | Low | selector~-style hard switch, no crossfade. Matches original patch behavior. |

### FM Synthesis

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Linear FM with internal oscillator | Core Verbos feature. Linear FM preserves harmonic relationships. Internal sine LFO as modulator. | Medium | FM modulation applied to fundamental frequency before phase accumulation. Linear FM = direct frequency addition. |
| Exponential FM with internal oscillator | Core Verbos feature. Exponential FM creates inharmonic, bell-like timbres. | Medium | Exponential FM = multiply frequency by 2^(mod_depth * mod_signal). Different character from linear FM. |
| FM LFO rate control (0.1Hz to audio rate) | Extended from original patch (0.1-100Hz). Audio-rate FM is a key sound design capability. | Low | Single sine oscillator with wide range. The extension to audio rate was explicitly requested. |
| FM depth control | Must be able to control modulation intensity. | Low | Simple depth multiplier on modulator output. |

### Pitch & Performance

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Portamento / glide (0-2000ms) | Standard mono synth feature. Matches Verbos patch's slide~ behavior. | Low | Exponential or linear interpolation between current and target frequency. |
| Anti-aliasing (skip harmonics above Nyquist) | Without this, harmonics above Nyquist fold back as inharmonic aliasing artifacts. Sounds terrible. | Low | Simple check: if harmonic_number * fundamental > sampleRate/2, set that harmonic's amplitude to zero. |

### GUI

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Visual layout matching Max patch sections | Plugin must have a usable GUI. Following the Max patch layout (Pitch, Harmonics, Scan/Tilt, FM, Output) provides logical grouping. | High | JUCE GUI development is the most time-consuming non-DSP feature. Five logical sections need layout. |
| 8 vertical harmonic faders | The primary interaction for setting harmonic balance. Faders are the natural control for levels. | Medium | JUCE Slider components in vertical orientation. Must be visually grouped and clearly labeled (1-8). |
| Knobs for scan, tilt, FM, envelope parameters | Standard synth UI controls. | Medium | JUCE RotarySlider components. Need reasonable default ranges and labels. |
| Output mode selector | Visual indication of which waveform is active. | Low | ComboBox or segmented button. |
| Resizable or sensibly sized window | Users expect plugins to fit their screen. | Low | Fixed size is fine for v1. 600-800px wide is standard for a mono synth. |

### Plugin Format

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| VST3 format | Industry standard. Required for most DAWs except Logic. | Low | JUCE handles this via build configuration. |
| AU format | Required for Logic Pro, which is the dominant DAW on macOS. | Low | JUCE handles this via build configuration. |
| macOS universal binary (ARM + Intel) | Must run on both Apple Silicon and Intel Macs. | Low | CMake/Xcode configuration. JUCE supports this. |

## Differentiators

Features that set this plugin apart. Not expected in every synth, but valued in this specific product.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Scan + Tilt interaction as primary spectral control | Most additive synths give you individual harmonic sliders and nothing else. The scan/tilt combination provides a performance-oriented macro control over the spectrum that is distinctly "Verbos." This is the core differentiator. | Medium | The interaction between scan (Gaussian window position/width) and tilt (spectral slope) creates a 3-parameter spectral control (center, width, tilt) that is more playable than 8 individual sliders. |
| Audio-rate FM on an additive synth | Most additive synths do not include FM. Combining additive harmonics with FM creates a unique timbral space -- you can FM the summed harmonic output or the fundamental, producing complex spectra not available from either technique alone. | Low | Already part of the design. The extension to audio rate (beyond the original 100Hz cap) is the differentiator. |
| Faithful Verbos character | The Verbos Harmonic Oscillator has a specific sonic character loved in the eurorack community. A plugin version does not exist. This fills a gap. | N/A | This is a product-level differentiator, not a feature. Achieved through accurate DSP translation. |
| Extremely low CPU usage | A single monophonic voice with 8 harmonics should be nearly free (<1% CPU). Most additive synths are CPU-heavy because they support hundreds of partials and polyphony. | Low | Simplicity is the advantage. No polyphony, no FFT, no resynthesis -- just 8 sine oscillators and basic math. |
| Hard output switching (no crossfade) | Matches the Verbos module's abrupt selector behavior. Can be used as a performance effect -- switching between harmonic mix and saw mid-note creates distinctive timbral jumps. | Low | Already specified. The deliberate choice to NOT crossfade is the differentiator. |

## Anti-Features

Features to explicitly NOT build. These would add complexity without serving the project's goals.

| Anti-Feature | Why Avoid | What to Do Instead |
|--------------|-----------|-------------------|
| Polyphony | The Verbos HO is monophonic. Adding polyphony changes the instrument's character, multiplies CPU cost, and complicates legato/glide logic. Mono is a design choice, not a limitation. | Keep monophonic. Users wanting polyphony should layer multiple instances. |
| Preset browser / factory presets | Adds significant UI complexity. DAW hosts already save/restore parameter state. The instrument has few enough parameters that presets are not essential. | Let the host handle presets via AudioProcessorValueTreeState serialization. |
| External FM sidechain input | Would require audio routing that complicates the plugin architecture. The internal LFO covers the Verbos module's functionality and extends it to audio rate. | Internal FM oscillator only. Covers 99% of use cases. |
| Wavetable / sample-based oscillators | This is an additive synthesizer. Adding wavetables changes the fundamental architecture and product identity. | Stick to sine bank + PolyBLEP classics. |
| Built-in effects (reverb, delay, filter) | The Verbos module has no effects. Adding them bloats scope and the plugin loses its identity as a focused sound source. | Output clean signal. Let users apply their own effects chain. |
| Modulation matrix / complex routing | The Verbos module has fixed signal flow. A modulation matrix turns this into a generic synth and explodes complexity. | Fixed signal flow matching the original module. |
| MPE / per-note expression | Monophonic instrument. MPE is meaningless for a single voice. | Standard MIDI only. |
| Microtuning / scale systems | Out of scope for v1. Adds UI complexity for a niche need. | Standard 12-TET tuning. Could be a future addition. |
| CLAP plugin format | VST3 + AU covers all major macOS DAWs. CLAP adoption is still growing. | VST3 + AU only. CLAP can be added later if needed -- JUCE does not natively support it anyway (requires third-party wrapper). |
| Windows / Linux builds | Developer uses macOS. Cross-platform builds add CI complexity and testing burden. | macOS only. JUCE code is portable, so cross-platform can be added later with minimal DSP changes. |
| Visual feedback (spectrum analyzer, oscilloscope) | Nice-to-have but adds significant GUI complexity. The 8 harmonic faders already provide visual feedback of the spectral balance. | Defer to v2 if desired. The faders themselves serve as a visual representation. |
| Oversampling | With only 8 harmonics and proper Nyquist checks, aliasing is already handled by skipping above-Nyquist harmonics. PolyBLEP handles waveform anti-aliasing. Oversampling would add CPU cost for negligible benefit. | Anti-alias via harmonic skipping and PolyBLEP. |

## Feature Dependencies

```
MIDI Input → Frequency Calculation → Phase Accumulator → All Waveform Generation
                                          |
                                          ├─→ 8-Harmonic Sine Bank → Scan Envelope → Spectral Tilt → Harmonic Mix Output
                                          |                                                                    |
                                          ├─→ PolyBLEP Saw Output                                             ├─→ Output Selector → AR Envelope → DC Blocker → Master Level → Audio Output
                                          ├─→ PolyBLEP Square Output                                          |
                                          └─→ Triangle (integrated square) Output ─────────────────────────────┘

FM Oscillator → Linear FM ──┐
                             ├─→ Frequency Calculation (modulates fundamental)
FM Oscillator → Exponential FM┘

Portamento/Glide → Frequency Calculation (smooths pitch transitions)

Legato Detection → AR Envelope (controls retrigger behavior)
                 → Portamento (enables glide on overlapping notes)
```

### Build Order Dependencies

```
1. Phase Accumulator + Frequency Calc (foundation for everything)
2. 8-Harmonic Sine Bank (core identity feature)
3. PolyBLEP Waveforms (independent of sine bank, can parallel)
4. Output Selector (needs all waveform sources)
5. Scan + Tilt (requires working sine bank)
6. AR Envelope (requires gate/trigger input)
7. FM (requires working frequency path)
8. Legato + Glide (requires working note handling)
9. DC Blocker + Master Level (final output chain)
10. GUI (can start in parallel once parameters are defined, but needs all parameters finalized)
```

## MVP Recommendation

The MVP is effectively the complete feature set because this is a translation of a specific, bounded instrument. However, if phasing is needed:

**Phase 1 -- Sound Engine (get sound out):**
1. MIDI input + frequency calculation
2. 8-harmonic sine bank (fixed equal levels, no scan/tilt yet)
3. PolyBLEP saw/square/triangle
4. Output selector
5. AR envelope
6. Master level + DC blocker
7. Minimal GUI (just enough to test)

**Phase 2 -- Verbos Character (what makes it special):**
1. Scan envelope (Gaussian window)
2. Spectral tilt
3. Individual harmonic level faders
4. Linear + exponential FM with internal oscillator

**Phase 3 -- Performance & Polish:**
1. Legato with gate counting
2. Portamento/glide
3. Coarse/fine tuning controls
4. Full GUI matching Max patch layout
5. Plugin state save/restore validation

**Defer:** Nothing. The feature set is already minimal by design. Every active requirement in PROJECT.md maps to the Verbos module's functionality and should ship in v1.

## Complexity Assessment

| Feature Group | Estimated Effort | Risk |
|--------------|-----------------|------|
| Sine bank + anti-aliasing | Medium-High | Core DSP accuracy is the project's success criterion |
| Scan + Tilt | Medium | Algorithm is straightforward but interaction testing matters |
| PolyBLEP waveforms | Medium | Well-documented algorithms, but triangle (leaky integrator) needs care |
| FM synthesis | Medium | Linear vs exponential FM are different math; audio-rate FM needs stability testing |
| AR envelope + legato | Medium | Gate counting logic must match Max patch exactly |
| GUI | High | JUCE GUI is always the most time-consuming part of plugin development |
| Plugin formats + build | Low | JUCE handles this; mainly configuration |

## Sources

- Domain knowledge of additive synthesis, Verbos Electronics Harmonic Oscillator eurorack module, JUCE plugin development
- PROJECT.md reference implementation details (Max/MSP gen~ codebox)
- Note: Web search was unavailable during research. Confidence is MEDIUM based on domain expertise. Verbos module specifications and competitive landscape claims should be verified.
