# Project Research Summary

**Project:** Harmonic Oscillator (JUCE additive synth plugin)
**Domain:** Desktop audio plugin -- monophonic additive synthesizer (Verbos Harmonic Oscillator translation from Max/MSP gen~ to C++)
**Researched:** 2026-03-10
**Confidence:** MEDIUM

## Executive Summary

Harmonic Oscillator is a focused, monophonic additive synthesizer plugin translating the Verbos Electronics Harmonic Oscillator eurorack module from a Max/MSP gen~ codebox implementation to a native VST3/AU plugin. The expert approach for this type of project is straightforward: JUCE is the only viable framework for cross-format C++ audio plugins, CMake is the modern build system, and the DSP should be translated line-by-line from the gen~ codebox to ensure sample-accurate fidelity. The bounded scope (8 harmonics, monophonic, fixed signal flow) makes this a well-defined project with no ambiguity about what to build.

The recommended approach is a flat architecture: skip JUCE's polyphonic `Synthesiser` class entirely and own a single `HarmonicEngine` DSP object directly in the `AudioProcessor`. All parameters go through `AudioProcessorValueTreeState` for thread-safe host automation and state persistence. The gen~ codebox maps naturally to this -- one monolithic DSP block processing sample-by-sample with a shared phase accumulator driving all waveform generators. Build DSP first (testable with the DAW's generic parameter UI), then layer on the custom GUI last.

The primary risks are subtle DSP correctness issues: float-vs-double precision in phase accumulators (gen~ uses 64-bit internally), parameter smoothing omission causing zipper noise (Max/MSP smooths implicitly via `slide~`), and PolyBLEP edge cases in the triangle waveform's leaky integrator. Audio-rate FM is the one area extending beyond the original patch and needs independent validation. None of these are architectural risks -- they are implementation details that unit testing and A/B comparison against the Max/MSP reference will catch.

## Key Findings

### Recommended Stack

JUCE 8.x with CMake is the only sensible choice. No alternative framework supports both VST3 and AU with the GUI toolkit needed for this project's 5-section interface. The DSP is entirely custom (no library provides Verbos-style scan envelopes), with `juce::dsp` used only for utilities: DC blocking filter and `SmoothedValue` for parameter interpolation.

**Core technologies:**
- **JUCE 8.x (C++20, CMake 3.24+):** Plugin framework -- handles format wrapping, parameter management, GUI, audio I/O
- **Custom DSP (no external library):** Additive synthesis, PolyBLEP, scan/tilt, FM -- translated directly from gen~ codebox
- **Catch2 v3 + pluginval:** Testing -- unit tests for DSP accuracy, plugin validation for host compatibility
- **Ninja generator:** Fast iterative builds during development

### Expected Features

The feature set is already minimal by design -- it is a faithful translation of a specific, bounded hardware module. Every feature maps to the Verbos module's functionality.

**Must have (table stakes):**
- 8-harmonic sine bank with individual level controls (the instrument's identity)
- Scan envelope (Gaussian window) + spectral tilt (signature Verbos controls)
- PolyBLEP saw/square/triangle with hard output selector
- Linear and exponential FM with internal oscillator (extended to audio rate)
- AR envelope with gate-counting legato, portamento/glide
- Coarse/fine tuning, MIDI input, master level, DC blocker
- VST3 + AU formats, macOS universal binary
- Plugin state save/restore via APVTS
- GUI with 5 sections matching Max patch layout

**Should have (differentiators):**
- Scan + tilt interaction as primary spectral macro control (no other plugin does this)
- Audio-rate FM on an additive synth (unique timbral space)
- Extremely low CPU (<1% -- simplicity is the advantage)

**Defer (v2+):**
- Polyphony, preset browser, built-in effects, visual feedback (spectrum/scope), CLAP format, Windows/Linux builds, MPE, microtuning, oversampling -- all explicitly ruled out as anti-features for v1

### Architecture Approach

Flat, direct architecture: `PluginProcessor` owns a single `HarmonicEngine` that processes audio in `processBlock`. A shared double-precision phase accumulator drives all waveform generators (harmonics and PolyBLEP). MIDI is processed sample-accurately by splitting the audio buffer at event timestamps. All parameters flow through APVTS with `SmoothedValue` interpolation on the audio thread.

**Major components:**
1. **HarmonicEngine** -- Top-level DSP coordinator: receives MIDI + parameter snapshots, renders audio
2. **HarmonicBank** -- 8 sine oscillators with scan envelope and spectral tilt
3. **PolyBLEPOscillator** -- Anti-aliased saw/square/triangle from shared master phase
4. **FMProcessor** -- Internal LFO with linear/exponential FM modes
5. **AREnvelope** -- Attack-release with exponential curves and gate counting for legato
6. **PluginEditor** -- 5-section GUI with APVTS parameter attachments

### Critical Pitfalls

1. **Audio thread memory allocation** -- Pre-allocate everything in `prepareToPlay()`. Zero heap operations, locks, or logging in `processBlock()`. Use atomics and APVTS for parameter communication. Violating this causes intermittent, hard-to-debug glitches.
2. **Float precision in phase accumulators** -- Use `double` for all phase and frequency calculations. gen~ uses 64-bit internally. Float causes pitch drift on sustained notes, especially audible on harmonics 4-8 at higher fundamentals.
3. **Missing parameter smoothing** -- Every continuous parameter needs `SmoothedValue`. Max/MSP smooths implicitly via `slide~`; JUCE does not. Without it, every knob movement produces zipper noise. Bake this in from day one.
4. **PolyBLEP edge cases** -- Square needs correction at both phase transitions (0 and 0.5). Triangle's leaky integrator needs frequency-dependent coefficient and gain compensation. FM with negative phase increments needs `abs(phaseIncrement)` for PolyBLEP threshold.
5. **Audio-rate FM is uncharted territory** -- The original patch caps at 100Hz. Extending to audio rate means FM sidebands will exceed Nyquist. Match sub-audio behavior against the reference first, then validate audio-rate independently.

## Implications for Roadmap

Based on research, suggested phase structure:

### Phase 1: Project Scaffolding and Plugin Shell
**Rationale:** Everything depends on a working build system and plugin host connection. PITFALLS.md flags CMake/JUCE setup as a phase-0 concern. Get a plugin that loads in a DAW before writing any DSP.
**Delivers:** CMake project with JUCE via FetchContent, plugin builds as VST3/AU/Standalone, loads in DAW, APVTS with all parameters registered, generic parameter UI works.
**Addresses:** Plugin format support, parameter state save/restore, project structure.
**Avoids:** Build system complexity pitfall (Pitfall 14), state save/restore pitfall (Pitfall 8).

### Phase 2: Core DSP Engine
**Rationale:** The DSP is the product. All waveform generation depends on the phase accumulator. Architecture research shows a clear dependency chain: phase accumulator -> harmonic bank -> PolyBLEP -> output selector -> DC blocker -> master level.
**Delivers:** All 4 waveform outputs (harmonic mix, saw, square, triangle), output selector, DC blocker, master level. Playable via MIDI with basic note on/off.
**Addresses:** 8-harmonic sine bank, PolyBLEP waveforms, output selector, DC blocker, anti-aliasing.
**Avoids:** Float precision pitfall (use double from start), PolyBLEP edge cases (unit test each waveform), variable buffer size pitfall, audio thread allocation pitfall.

### Phase 3: Modulation and Performance Controls
**Rationale:** Scan/tilt, FM, envelope, and glide all modulate the base waveforms -- they require working waveform generators underneath. This phase adds the Verbos character.
**Delivers:** Scan envelope, spectral tilt, linear/exponential FM, AR envelope with legato, portamento/glide, coarse/fine tuning, parameter smoothing on all controls.
**Addresses:** Scan + tilt (core differentiator), FM synthesis, AR envelope, legato, portamento, pitch controls.
**Avoids:** Parameter smoothing pitfall (SmoothedValue on everything), gate counting edge cases (Pitfall 11), FM audio-rate issues (Pitfall 6), AR envelope retrigger glitches (Pitfall 15).

### Phase 4: GUI
**Rationale:** Architecture research is clear: build GUI after DSP. The DSP defines what parameters exist. JUCE's generic parameter editor is sufficient for testing until a custom GUI is needed. GUI is the most time-consuming non-DSP work.
**Delivers:** Custom plugin editor with 5 sections (Pitch, Harmonics, Scan/Tilt, FM, Output/Envelope), 8 vertical harmonic faders, rotary knobs, output mode selector.
**Addresses:** Visual layout, 8 harmonic faders, knobs for all parameters, output selector UI.
**Avoids:** GUI thread contention pitfall (Pitfall 10) -- use APVTS attachments exclusively.

### Phase 5: Validation and Polish
**Rationale:** A/B testing against the gen~ reference requires the complete signal chain. AU validation, universal binary builds, and cross-sample-rate testing are final checks.
**Delivers:** Verified DSP accuracy vs Max/MSP reference, AU validation passing, universal binary, tested at 44.1k/48k/96k, pluginval clean.
**Addresses:** Sample-rate aware DSP, macOS universal binary, AU format validation.
**Avoids:** AU validation pitfall (Pitfall 13), sample rate handling pitfall (Pitfall 12).

### Phase Ordering Rationale

- Build system and parameter registration must come first because every subsequent phase depends on a loadable plugin with APVTS.
- DSP before modulation because scan/tilt/FM/envelope all modify the base waveforms -- you need to hear the base to verify modulations work.
- DSP before GUI because the parameter set must be finalized before building UI, and the DAW's generic editor suffices for DSP testing.
- Validation last because A/B testing requires the complete chain, and AU validation is a packaging concern.

### Research Flags

Phases likely needing deeper research during planning:
- **Phase 2 (Core DSP):** PolyBLEP implementation details, especially triangle via leaky integrator. The gen~ codebox should be the primary reference -- verify the exact algorithm used.
- **Phase 3 (Modulation):** Audio-rate FM is beyond the original patch's tested range. Needs independent validation. Scan Gaussian window math should be verified against the gen~ implementation.

Phases with standard patterns (skip research-phase):
- **Phase 1 (Scaffolding):** JUCE CMake plugin setup is thoroughly documented. Follow the JUCE CMake API docs.
- **Phase 4 (GUI):** Standard JUCE Component/Slider/APVTS attachment patterns. Well-documented.
- **Phase 5 (Validation):** pluginval and auval are standard tools with clear pass/fail criteria.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | JUCE is the only real option. CMake, Catch2, pluginval are standard. Exact version numbers need verification. |
| Features | HIGH | Feature set is bounded by the Verbos module spec. No ambiguity about what to build. |
| Architecture | MEDIUM-HIGH | Flat engine pattern is well-established for mono synths. APVTS patterns are stable. Web verification was unavailable. |
| Pitfalls | MEDIUM-HIGH | Audio thread constraints and gen~-to-C++ translation issues are well-known. PolyBLEP and FM edge cases need implementation-time attention. |

**Overall confidence:** MEDIUM -- all four research files note that web verification was unavailable. The recommendations are sound (based on well-established patterns), but exact JUCE 8.x API details and version numbers should be verified before starting implementation.

### Gaps to Address

- **JUCE version pinning:** Verify latest JUCE 8.x release tag on GitHub before starting. Pin in CMakeLists.txt.
- **gen~ codebox reference:** The actual gen~ codebox source should be the ground truth for DSP algorithms. Research inferred behavior from PROJECT.md descriptions -- verify scan Gaussian formula, tilt slope calculation, and FM scaling against the actual code.
- **Audio-rate FM behavior:** No reference implementation exists above 100Hz. Needs independent testing and ear-validation during Phase 3.
- **Triangle waveform via leaky integrator:** Multiple valid approaches exist. Pick one, unit test it, and verify spectral content matches expectations.
- **dcblock~ default cutoff:** Research suggests ~40Hz but this should be verified against Cycling '74 documentation or the actual gen~ patch settings.

## Sources

### Primary (HIGH confidence)
- JUCE AudioProcessorValueTreeState patterns -- stable API, well-established
- PolyBLEP algorithm (Valimaki & Franck) -- well-documented DSP technique
- Audio thread real-time constraints -- universally established in audio programming
- Gate counting for monophonic legato -- standard technique across frameworks

### Secondary (MEDIUM confidence)
- JUCE 8.x CMake API -- based on training data, likely stable but version-specific details unverified
- gen~ codebox behavior -- inferred from PROJECT.md, not from direct code inspection
- JUCE Synthesiser class behavior -- training data, patterns stable since JUCE 5

### Tertiary (LOW confidence)
- Exact JUCE 8.x release version -- needs live verification
- Catch2 v3 / pluginval current release versions -- needs live verification
- C++20 support in current Xcode -- needs verification against Apple developer docs

---
*Research completed: 2026-03-10*
*Ready for roadmap: yes*
