# Phase 1: Plugin Shell - Context

**Gathered:** 2026-03-10
**Status:** Ready for planning

<domain>
## Phase Boundary

JUCE project scaffolding — CMake build system, VST3/AU format wrappers, all synth parameters registered in APVTS with correct ranges/defaults/scaling, plugin state save/restore. No DSP, no GUI editor — just a loadable plugin shell with parameters visible in the host's generic UI.

</domain>

<decisions>
## Implementation Decisions

### Plugin Identity
- Plugin name: "Harmonic Oscillator"
- Manufacturer: "Die stille Erde"
- Plugin unique IDs: Claude's discretion (generate unique 4-char codes for manufacturer + plugin)
- Category: Instrument / Synthesizer

### Parameter Ranges (match Max patch exactly)
- **Coarse tune**: -24 to +24 (semitones), default 0, integer steps
- **Fine tune**: -100 to +100 (cents), default 0
- **Glide**: 0 to 2000 (ms), default 0, exponent 3.0 skew
- **Master level**: 0 to 1, default 0.8
- **Attack**: 1 to 2000 (ms), default 10, exponent 3.0 skew
- **Release**: 1 to 5000 (ms), default 100, exponent 3.0 skew
- **Lin FM depth**: 0 to 1, default 0
- **Exp FM depth**: -1 to +1, default 0
- **FM Rate**: 0.1 to 20000 (Hz), default 1.0, exponent 3.0 skew (extended from Max patch's 100Hz cap to full audio rate)
- **Harm 1-8**: 0 to 1 each, defaults: Harm 1 = 1.0, Harm 2-8 = 0.0
- **Scan center**: 0 to 1, default 0.5
- **Scan width**: 0 to 1, default 1.0
- **Spectral tilt**: -1 to +1, default 0
- **Output select**: enum [Harmonic Mix, Triangle, Sawtooth, Square], default Harmonic Mix

### Parameter Scaling
- Attack, Release, Glide, FM Rate: skewed with exponent 3.0 (more resolution at low values)
- All other continuous params: linear
- Coarse tune: integer steps only

### Claude's Discretion
- JUCE version selection and pinning strategy (FetchContent vs submodule)
- CMake project structure and directory layout
- 4-char unique ID codes for VST3/AU
- Parameter group organization in APVTS
- Build configuration details (Debug/Release, compiler flags)

</decisions>

<specifics>
## Specific Ideas

- Reference implementation lives at `~/src/harmonic-oscillator-plugin/HarmonicOscillator.maxpat`
- The Max patch uses `live.dial`, `live.slider`, and `live.menu` objects with the exact ranges listed above
- All parameters should be automatable by the host
- No custom GUI editor in this phase — DAW's generic parameter UI is sufficient for testing

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- None — greenfield project, empty repository

### Established Patterns
- None yet — Phase 1 establishes the patterns

### Integration Points
- APVTS parameter tree created here will be consumed by DSP engine (Phase 2) and GUI (Phase 4)
- CMake build targets created here must support VST3 + AU formats

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 01-plugin-shell*
*Context gathered: 2026-03-10*
