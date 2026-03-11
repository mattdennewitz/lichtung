# Roadmap: Harmonic Oscillator

## Overview

Translate the Verbos Harmonic Oscillator from a Max/MSP gen~ codebox into a native JUCE audio plugin (VST3/AU). The path goes from an empty project to a loadable plugin shell, then builds all waveform generation (the core product), layers on modulation and performance controls (scan, tilt, FM, envelope, glide), adds a custom GUI matching the Max patch layout, and finishes with universal binary builds and A/B validation against the reference implementation.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: Plugin Shell** - JUCE project scaffolding, VST3/AU builds, APVTS parameter registration
- [ ] **Phase 2: Waveform Engine** - 8-harmonic sine bank, PolyBLEP waveforms, output selector, DC blocker, MIDI playability
- [ ] **Phase 3: Modulation and Performance** - Scan/tilt, FM synthesis, AR envelope with legato, glide, tuning, parameter smoothing
- [ ] **Phase 4: Custom GUI** - 5-section editor layout with harmonic faders, knobs, and output selector
- [ ] **Phase 5: Validation and Universal Build** - macOS universal binary, A/B testing against Max/MSP reference, pluginval/auval

## Phase Details

### Phase 1: Plugin Shell
**Goal**: A loadable VST3/AU plugin with all parameters registered and host state save/restore working
**Depends on**: Nothing (first phase)
**Requirements**: FMT-01, FMT-02, FMT-04
**Success Criteria** (what must be TRUE):
  1. Plugin loads in a DAW (e.g., REAPER or Ableton) as both VST3 and AU without errors
  2. All synth parameters appear in the DAW's generic parameter UI and can be automated
  3. Plugin state saves and restores correctly when reopening a DAW project
**Plans**: 1 plan

Plans:
- [ ] 01-01-PLAN.md — CMake build system, APVTS parameter registration, state save/restore, build all targets

### Phase 2: Waveform Engine
**Goal**: All four waveform outputs (harmonic mix, saw, square, triangle) are playable via MIDI with correct pitch tracking and clean output
**Depends on**: Phase 1
**Requirements**: DSP-01, DSP-02, DSP-03, DSP-04, DSP-05, DSP-06, WAVE-01, WAVE-02, WAVE-03, WAVE-04, PERF-06, PERF-07
**Success Criteria** (what must be TRUE):
  1. Playing a MIDI note produces sound through all four output modes (harmonic mix, saw, square, triangle) selectable via the output switch
  2. The 8-harmonic sine bank produces audibly correct harmonics with individual level control per harmonic
  3. Scan (Gaussian window) and tilt controls reshape the harmonic spectrum in real time
  4. PolyBLEP waveforms sound clean (no audible aliasing) across the MIDI note range
  5. No DC offset on the harmonic mix output (DC blocker active)
**Plans**: 2 plans

Plans:
- [ ] 02-01-PLAN.md — HarmonicEngine DSP class: 8-harmonic sine bank, PolyBLEP waveforms, scan/tilt, DC blocker, output selector
- [ ] 02-02-PLAN.md — Wire HarmonicEngine into PluginProcessor with MIDI handling, build and verify

### Phase 3: Modulation and Performance
**Goal**: The synth has its full Verbos character -- FM synthesis, envelope shaping, legato behavior, glide, and tuning controls all work with smooth parameter changes
**Depends on**: Phase 2
**Requirements**: FM-01, FM-02, FM-03, PERF-01, PERF-02, PERF-03, PERF-04, PERF-05, PERF-08
**Success Criteria** (what must be TRUE):
  1. Linear and exponential FM from the internal LFO produce audible modulation across the full range (0.1Hz through audio rate)
  2. AR envelope shapes the amplitude with exponential curves, and overlapping MIDI notes glide without retriggering the envelope (legato)
  3. Coarse tuning shifts pitch in semitone steps and fine tuning shifts in cents, both audibly correct
  4. Portamento produces smooth pitch glide between notes at adjustable speed
  5. Moving any continuous parameter (knobs, faders) produces no zipper noise or clicks (parameter smoothing active)
**Plans**: TBD

Plans:
- [ ] 03-01: TBD

### Phase 4: Custom GUI
**Goal**: A custom plugin editor with the 5-section layout matching the Max patch, replacing the generic parameter UI
**Depends on**: Phase 3
**Requirements**: GUI-01, GUI-02, GUI-03, GUI-04, GUI-05
**Success Criteria** (what must be TRUE):
  1. Plugin opens a custom editor window (~700px wide) with five visible sections: Pitch, Harmonics, Scan/Tilt, FM, Output
  2. Eight vertical faders control harmonic levels with immediate audible response
  3. All rotary knobs (scan, tilt, FM, envelope, tuning) control their parameters and reflect current values
  4. Output mode selector switches between Harmonic Mix, Triangle, Saw, Square
**Plans**: TBD

Plans:
- [ ] 04-01: TBD

### Phase 5: Validation and Universal Build
**Goal**: Plugin passes format validation, builds as universal binary, and DSP output matches the Max/MSP reference implementation
**Depends on**: Phase 4
**Requirements**: FMT-03
**Success Criteria** (what must be TRUE):
  1. Plugin builds as macOS universal binary (runs natively on both Apple Silicon and Intel)
  2. pluginval passes with no errors for both VST3 and AU formats
  3. A/B comparison against the Max/MSP gen~ patch confirms matching behavior for harmonic mix, scan, tilt, and PolyBLEP waveforms at standard sample rates (44.1k, 48k, 96k)
**Plans**: TBD

Plans:
- [ ] 05-01: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3 -> 4 -> 5

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Plugin Shell | 1/1 | Complete | 2026-03-11 |
| 2. Waveform Engine | 0/2 | Planned | - |
| 3. Modulation and Performance | 0/? | Not started | - |
| 4. Custom GUI | 0/? | Not started | - |
| 5. Validation and Universal Build | 0/? | Not started | - |
