---
phase: 03-modulation-and-performance
plan: 02
subsystem: dsp
tags: [legato, portamento, glide, parameter-smoothing, SmoothedValue, gate-counting]

# Dependency graph
requires:
  - phase: 03-modulation-and-performance/plan-01
    provides: "FM modulation, tuning, AR envelope in HarmonicEngine"
provides:
  - "Legato gate counting (overlapping notes don't retrigger envelope)"
  - "Portamento/glide via slide~ exponential follower on MIDI note numbers"
  - "SmoothedValue on all 10 remaining continuous parameters (zipper-free)"
affects: [04-gui, 05-validation]

# Tech tracking
tech-stack:
  added: []
  patterns: [SmoothedValue per-sample setTargetValue/getNextValue, slide~ exponential follower, gate counting for legato]

key-files:
  created: []
  modified:
    - src/dsp/HarmonicEngine.h
    - src/dsp/HarmonicEngine.cpp

key-decisions:
  - "Glide operates on MIDI note numbers before mtof (matching Max slide~ placement)"
  - "First note sets glidedNote_ immediately to avoid sweep from default A4"
  - "coarse_tune and output_select kept as direct reads (discrete/stepped params)"

patterns-established:
  - "SmoothedValue pattern: setTargetValue per-sample, getNextValue for read"
  - "Gate counting: gateCount_++ on noteOn, max(0, --) on noteOff, gateOpen_ derived"

requirements-completed: [PERF-03, PERF-05, PERF-08]

# Metrics
duration: 3min
completed: 2026-03-11
---

# Phase 3 Plan 2: Legato, Portamento, and Parameter Smoothing Summary

**Monophonic legato with gate counting, exponential pitch glide (slide~ translation), and SmoothedValue on all 10 remaining continuous parameters for zipper-free operation**

## Performance

- **Duration:** 3 min
- **Started:** 2026-03-11T03:44:04Z
- **Completed:** 2026-03-11T03:47:05Z
- **Tasks:** 2
- **Files modified:** 2

## Accomplishments
- Legato gate counting replaces simple gate -- overlapping MIDI notes flow without envelope retrigger
- Exponential pitch glide (slide~ translation) provides smooth portamento between notes at adjustable speed
- All 10 remaining continuous parameters read through SmoothedValue, eliminating zipper noise
- First note played sets pitch immediately (no glide from default A4)

## Task Commits

Each task was committed atomically:

1. **Task 1: Add legato gate counting and portamento glide** - `56a22cf` (feat)
2. **Task 2: Add SmoothedValue for all remaining continuous parameters** - `61cfe3f` (feat)

## Files Created/Modified
- `src/dsp/HarmonicEngine.h` - Added gate counting state, glide state, glideParam_ pointer, 10 SmoothedValue members
- `src/dsp/HarmonicEngine.cpp` - Legato noteOn/noteOff, slide~ glide processor, smoothed reads for all continuous params

## Decisions Made
- Glide operates on MIDI note numbers before mtof conversion, matching Max/MSP slide~ placement in signal flow
- First note sets glidedNote_ immediately to prevent audible sweep from default A4 on plugin load
- coarse_tune and output_select kept as direct atomic reads (discrete/stepped parameters need no smoothing)

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Full Phase 3 DSP feature set complete: FM modulation, tuning, AR envelope (plan 01) + legato, glide, parameter smoothing (plan 02)
- HarmonicEngine has complete Verbos Harmonic Oscillator character
- Ready for Phase 4 (GUI) or Phase 5 (validation)

---
*Phase: 03-modulation-and-performance*
*Completed: 2026-03-11*
