# Requirements: Harmonic Oscillator

**Defined:** 2026-03-10
**Core Value:** 8-harmonic additive synthesis engine with scan and tilt that matches the Max/MSP gen~ codebox

## v1 Requirements

### DSP Core

- [x] **DSP-01**: 8-harmonic sine bank with individual level controls (0-1 per harmonic)
- [x] **DSP-02**: Scan envelope (Gaussian window) with center (0-1) and width (0-1) parameters
- [x] **DSP-03**: Spectral tilt (linear-in-dB slope, +/-24dB range across harmonics)
- [x] **DSP-04**: DC blocker on harmonic mix output (~5Hz highpass)
- [x] **DSP-05**: Anti-aliasing — skip harmonics above Nyquist
- [x] **DSP-06**: Double-precision phase accumulator to prevent pitch drift

### Waveforms

- [x] **WAVE-01**: PolyBLEP sawtooth waveform
- [x] **WAVE-02**: PolyBLEP square waveform
- [x] **WAVE-03**: Triangle waveform via leaky integrated square
- [x] **WAVE-04**: Output selector — hard switch between Harmonic Mix, Triangle, Saw, Square

### FM

- [ ] **FM-01**: Linear FM with internal sine LFO
- [ ] **FM-02**: Exponential FM with internal sine LFO
- [ ] **FM-03**: FM LFO rate control (0.1Hz to audio rate ~20kHz)

### Pitch & Performance

- [ ] **PERF-01**: Coarse tuning (+/-24 semitones)
- [ ] **PERF-02**: Fine tuning (+/-100 cents)
- [ ] **PERF-03**: Portamento/glide (0-2000ms)
- [ ] **PERF-04**: AR envelope (attack 1-2000ms, release 1-5000ms, exponential curves)
- [ ] **PERF-05**: Monophonic with legato (gate counting, no retrigger on overlapping notes)
- [x] **PERF-06**: MIDI note input with velocity sensitivity
- [x] **PERF-07**: Master level control (0-1)
- [ ] **PERF-08**: Parameter smoothing on all continuously-variable parameters

### GUI

- [ ] **GUI-01**: 5-section layout: Pitch, Harmonics, Scan/Tilt, FM, Output
- [ ] **GUI-02**: 8 vertical faders for harmonic levels
- [ ] **GUI-03**: Rotary knobs for scan center, scan width, tilt, FM controls, envelope, tuning
- [ ] **GUI-04**: Output mode selector (menu or segmented control)
- [ ] **GUI-05**: Sensibly sized fixed window (~700px wide)

### Plugin Format

- [x] **FMT-01**: VST3 format
- [x] **FMT-02**: AU format
- [ ] **FMT-03**: macOS universal binary (Apple Silicon + Intel)
- [x] **FMT-04**: Plugin state save/restore via APVTS

## v2 Requirements

None — scope is deliberately bounded to the Verbos module translation.

## Out of Scope

| Feature | Reason |
|---------|--------|
| Polyphony | Mono synth by design, matches original module |
| External FM sidechain | Internal LFO only, keeps architecture simple |
| Preset browser | Host handles presets via APVTS state |
| Windows/Linux | macOS only for now |
| CLAP format | VST3/AU sufficient |
| Built-in effects | Not part of original module |
| Visual feedback (spectrum/scope) | Adds complexity without sonic value |
| Oversampling | 8 harmonics at standard rates shouldn't need it |
| MPE / microtuning | Out of scope for v1 |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| DSP-01 | Phase 2 | Complete |
| DSP-02 | Phase 2 | Complete |
| DSP-03 | Phase 2 | Complete |
| DSP-04 | Phase 2 | Complete |
| DSP-05 | Phase 2 | Complete |
| DSP-06 | Phase 2 | Complete |
| WAVE-01 | Phase 2 | Complete |
| WAVE-02 | Phase 2 | Complete |
| WAVE-03 | Phase 2 | Complete |
| WAVE-04 | Phase 2 | Complete |
| FM-01 | Phase 3 | Pending |
| FM-02 | Phase 3 | Pending |
| FM-03 | Phase 3 | Pending |
| PERF-01 | Phase 3 | Pending |
| PERF-02 | Phase 3 | Pending |
| PERF-03 | Phase 3 | Pending |
| PERF-04 | Phase 3 | Pending |
| PERF-05 | Phase 3 | Pending |
| PERF-06 | Phase 2 | Complete |
| PERF-07 | Phase 2 | Complete |
| PERF-08 | Phase 3 | Pending |
| GUI-01 | Phase 4 | Pending |
| GUI-02 | Phase 4 | Pending |
| GUI-03 | Phase 4 | Pending |
| GUI-04 | Phase 4 | Pending |
| GUI-05 | Phase 4 | Pending |
| FMT-01 | Phase 1 | Complete |
| FMT-02 | Phase 1 | Complete |
| FMT-03 | Phase 5 | Pending |
| FMT-04 | Phase 1 | Complete |

**Coverage:**
- v1 requirements: 30 total
- Mapped to phases: 30
- Unmapped: 0

---
*Requirements defined: 2026-03-10*
*Last updated: 2026-03-10 after roadmap creation*
