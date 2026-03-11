---
phase: 03-modulation-and-performance
plan: 01
subsystem: dsp
tags: [fm-synthesis, lfo, tuning, ar-envelope, polyblep, additive-synthesis]

requires:
  - phase: 02-waveform-engine
    provides: "HarmonicEngine with 8-harmonic sine bank, PolyBLEP waveforms, scan/tilt"
provides:
  - "FM LFO with linear and exponential modulation paths"
  - "Coarse/fine tuning (semitones/cents)"
  - "Exponential AR envelope replacing 5ms gate ramp"
  - "DC blocker in correct post-envelope position"
affects: [04-gui, 05-validation]

tech-stack:
  added: []
  patterns:
    - "Single-pole exponential envelope follower (gen~ style)"
    - "FM: linear additive + exponential multiplicative"
    - "Tuning via pow(2, cents/1200) ratio"

key-files:
  created: []
  modified:
    - src/dsp/HarmonicEngine.h
    - src/dsp/HarmonicEngine.cpp

key-decisions:
  - "AR envelope uses single-pole exponential follower matching gen~ section 7"
  - "DC blocker moved after envelope to match gen~ signal flow order"
  - "Envelope target incorporates velocity directly (envTarget = gateOpen ? velocity : 0)"

patterns-established:
  - "Phase 3 parameters cached in prepare() alongside Phase 2 params"
  - "Tuning -> FM -> phase accumulator -> harmonic bank -> envelope -> DC blocker -> output"

requirements-completed: [FM-01, FM-02, FM-03, PERF-01, PERF-02, PERF-04]

duration: 2min
completed: 2026-03-11
---

# Phase 3 Plan 1: FM, Tuning, and AR Envelope Summary

**FM synthesis with linear+exponential paths, coarse/fine tuning, and exponential AR envelope replacing 5ms gate ramp**

## Performance

- **Duration:** 2 min
- **Started:** 2026-03-11T03:39:56Z
- **Completed:** 2026-03-11T03:41:42Z
- **Tasks:** 1
- **Files modified:** 2

## Accomplishments
- FM LFO drives both linear (Hz offset proportional to carrier) and exponential (pitch scaling via pow2) modulation
- Coarse tuning in semitone steps and fine tuning in cents applied via pow(2, cents/1200) ratio
- AR envelope with per-sample exponential coefficient computation replaces the 5ms anti-click gate ramp
- DC blocker repositioned after envelope application to match gen~ codebox signal flow
- All gate ramp variables (gateLevel_, gateAttackCoeff_, gateReleaseCoeff_) fully removed

## Task Commits

Each task was committed atomically:

1. **Task 1: Add FM LFO, tuning, and AR envelope to HarmonicEngine** - `0a48d78` (feat)

## Files Created/Modified
- `src/dsp/HarmonicEngine.h` - Added fmPhase_, envLevel_, and 7 new APVTS param pointers; removed gate ramp vars
- `src/dsp/HarmonicEngine.cpp` - Added tuning+FM+AR envelope blocks in renderSample; updated prepare() for Phase 3 params

## Decisions Made
- AR envelope uses single-pole exponential follower (envTarget + coeff * (envLevel - envTarget)) matching gen~ section 7 exactly
- DC blocker moved to after envelope application, matching gen~ signal flow order
- Envelope incorporates velocity directly into target rather than separate multiplication
- FM frequency clamped to [0.01, nyquist*0.98] for stability

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- FM, tuning, and AR envelope active in HarmonicEngine
- Ready for Plan 03-02 (SIMD/performance optimization or remaining modulation features)
- All parameter pointers cached and connected to existing APVTS layout

## Self-Check: PASSED

- FOUND: src/dsp/HarmonicEngine.h
- FOUND: src/dsp/HarmonicEngine.cpp
- FOUND: 03-01-SUMMARY.md
- FOUND: commit 0a48d78

---
*Phase: 03-modulation-and-performance*
*Completed: 2026-03-11*
