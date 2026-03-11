---
phase: 03-modulation-and-performance
verified: 2026-03-11T03:49:18Z
status: passed
score: 8/8 must-haves verified
---

# Phase 3: Modulation and Performance Verification Report

**Phase Goal:** The synth has its full Verbos character -- FM synthesis, envelope shaping, legato behavior, glide, and tuning controls all work with smooth parameter changes
**Verified:** 2026-03-11T03:49:18Z
**Status:** passed
**Re-verification:** No -- initial verification

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | FM LFO modulates pitch via both linear (additive) and exponential (multiplicative) paths | VERIFIED | HarmonicEngine.cpp:119 `linFmAmount = linFmDepth * freq * fmLfo` (linear), line 124 `expFmAmount = pow(2, expFmDepth * fmLfo)` (exponential), combined at line 127 |
| 2 | Coarse tuning shifts pitch in semitone steps, fine tuning shifts in cents | VERIFIED | HarmonicEngine.cpp:101-105 `coarseTune * 100 + fineTune` in cents, applied via `pow(2, cents/1200)` ratio. Coarse read directly (integer-stepped), fine read through SmoothedValue |
| 3 | AR envelope shapes amplitude with exponential curves, replacing the 5ms gate ramp | VERIFIED | HarmonicEngine.cpp:229-239 single-pole exponential follower with attack/release coefficients via `exp(-1/(ms*0.001*sr))`. No gateLevel_/gateAttackCoeff_/gateReleaseCoeff_ references remain anywhere in src/ |
| 4 | DC blocker is applied after envelope (matching gen~ order) | VERIFIED | HarmonicEngine.cpp:246-254 envelope applied first (`rawMix = harmonicMix * envLevel_ * masterLvl`), then DC blocker on rawMix |
| 5 | Overlapping MIDI notes glide to new pitch without retriggering the envelope (legato) | VERIFIED | handleNoteOn increments gateCount_ (line 271), sets targetNote_ but does NOT reset envLevel_. handleNoteOff decrements gateCount_ (line 287), derives gateOpen_ from count > 0 (line 288) |
| 6 | Portamento produces smooth pitch glide between notes at adjustable speed | VERIFIED | renderSample lines 91-98: slide~ exponential follower `glidedNote_ += (targetNote_ - glidedNote_) / glideSamples`, then mtof conversion to baseFreq |
| 7 | Moving any continuous parameter produces no zipper noise (parameter smoothing active) | VERIFIED | 12 SmoothedValue getNextValue() calls per sample covering all continuous params. Only coarse_tune and output_select read directly (correct -- discrete/stepped) |
| 8 | First note played does not glide from a default pitch | VERIFIED | handleNoteOn lines 277-281: `if (!hasPlayedNote_)` sets glidedNote_ immediately to midiNote |

**Score:** 8/8 truths verified

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/dsp/HarmonicEngine.h` | FM LFO state, AR envelope state, tuning param pointers, gate counting, glide state, SmoothedValues | VERIFIED | 94 lines. Contains fmPhase_, envLevel_, gateCount_, targetNote_, glidedNote_, hasPlayedNote_, all 8 APVTS pointers, all 12 SmoothedValue members |
| `src/dsp/HarmonicEngine.cpp` | FM + tuning + AR envelope + legato + glide + smoothing in renderSample | VERIFIED | 290 lines. Complete signal chain: glide -> tuning -> FM -> phase accumulator -> waveforms -> scan/tilt -> harmonics -> AR envelope -> DC blocker -> output |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| renderSample() tuning block | renderSample() FM block | tuned freq feeds into linear FM scaling | WIRED | Line 105: `freq = baseFreq * tuneRatio`, line 119: `linFmAmount = linFmDepth * freq * fmLfo` |
| renderSample() AR envelope | renderSample() output section | envLevel_ replaces gateLevel_ for amplitude shaping | WIRED | Line 239: `envLevel_ = envTarget + envCoeff * (envLevel_ - envTarget)`, line 246: `rawMix = harmonicMix * envLevel_ * masterLvl` |
| handleNoteOn gate counting | renderSample AR envelope | gateOpen_ derived from gateCount_ > 0 | WIRED | Line 271: `gateCount_++`, line 288: `gateOpen_ = (gateCount_ > 0)`, line 237: `envTarget = gateOpen_ ? currentVelocity_ : 0.0` |
| renderSample glide processor | renderSample tuning block | glidedNote_ feeds mtof conversion before tuning | WIRED | Line 95: `glidedNote_ += ...`, line 98: `baseFreq = 440 * pow(2, (glidedNote_ - 69) / 12)`, line 105: `freq = baseFreq * tuneRatio` |
| SmoothedValue targets set per-block | getNextValue() called per-sample | scan, tilt, FM params read through SmoothedValue | WIRED | 12 setTargetValue/getNextValue pairs confirmed in renderSample covering all continuous params |

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|----------|
| FM-01 | 03-01 | Linear FM with internal sine LFO | SATISFIED | HarmonicEngine.cpp:116-119 linear FM path |
| FM-02 | 03-01 | Exponential FM with internal sine LFO | SATISFIED | HarmonicEngine.cpp:121-124 exponential FM path |
| FM-03 | 03-01 | FM LFO rate control (0.1Hz to audio rate ~20kHz) | SATISFIED | HarmonicEngine.cpp:108-114 fmRate from smoothed param drives LFO phase |
| PERF-01 | 03-01 | Coarse tuning (+/-24 semitones) | SATISFIED | HarmonicEngine.cpp:101 coarseTune read directly |
| PERF-02 | 03-01 | Fine tuning (+/-100 cents) | SATISFIED | HarmonicEngine.cpp:102-103 fineTune via SmoothedValue |
| PERF-03 | 03-02 | Portamento/glide (0-2000ms) | SATISFIED | HarmonicEngine.cpp:91-98 slide~ exponential follower |
| PERF-04 | 03-01 | AR envelope (attack 1-2000ms, release 1-5000ms, exponential curves) | SATISFIED | HarmonicEngine.cpp:229-239 single-pole exponential envelope |
| PERF-05 | 03-02 | Monophonic with legato (gate counting, no retrigger on overlapping notes) | SATISFIED | HarmonicEngine.cpp:269-289 gateCount_ increment/decrement logic |
| PERF-08 | 03-02 | Parameter smoothing on all continuously-variable parameters | SATISFIED | 12 SmoothedValue instances covering all continuous params, only discrete params read directly |

All 9 requirements from phase plans are satisfied. No orphaned requirements found -- REQUIREMENTS.md maps exactly these 9 IDs to Phase 3.

### Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| (none) | - | - | - | No anti-patterns detected |

No TODO/FIXME/placeholder comments, no empty implementations, no stub returns found in modified files.

### Human Verification Required

### 1. FM Modulation Audio Quality

**Test:** Load plugin in DAW, play a sustained note, sweep lin_fm_depth and exp_fm_depth while adjusting fm_rate from sub-audio (0.1Hz) through audio rate (1kHz+)
**Expected:** Linear FM produces vibrato at low rates and sideband-rich timbres at audio rates. Exponential FM produces pitch-proportional modulation. No clicks or discontinuities.
**Why human:** Audio quality and timbral character require listening evaluation

### 2. Legato Behavior

**Test:** Hold one note, play a second overlapping note, release first note
**Expected:** Envelope does NOT retrigger on second note -- amplitude stays continuous. Pitch glides to new note (if glide > 0). Releasing first note keeps gate open.
**Why human:** Requires real-time MIDI input and listening to verify smooth envelope continuity

### 3. Portamento Smoothness

**Test:** Play sequential notes with glide set to ~200ms
**Expected:** Pitch glides smoothly between notes. First note after plugin load plays at correct pitch with no glide from A4.
**Why human:** Pitch glide smoothness requires listening evaluation

### 4. Parameter Smoothing (Zipper-Free)

**Test:** While a note is sustained, rapidly sweep scan_center, spectral_tilt, and FM depth knobs
**Expected:** No zipper noise, clicks, or staircase artifacts -- all parameter changes are smooth
**Why human:** Zipper noise detection requires critical listening

### Gaps Summary

No gaps found. All 8 observable truths verified. All 9 requirements satisfied. Build succeeds cleanly. Signal flow order matches gen~ codebox (glide -> tuning -> FM -> phase accumulator -> waveforms -> scan/tilt -> harmonics -> AR envelope -> DC blocker -> output). The 5ms gate ramp is fully removed with no remnants. All continuous parameters use SmoothedValue for zipper-free operation.

---

_Verified: 2026-03-11T03:49:18Z_
_Verifier: Claude (gsd-verifier)_
