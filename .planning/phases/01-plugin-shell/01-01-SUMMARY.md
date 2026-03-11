---
phase: 01-plugin-shell
plan: 01
subsystem: audio
tags: [juce, vst3, au, apvts, cmake, audio-plugin]

# Dependency graph
requires: []
provides:
  - JUCE 8.0.12 CMake project with FetchContent
  - VST3/AU/Standalone plugin targets
  - AudioProcessor subclass with 21 APVTS parameters across 6 groups
  - State save/restore via XML serialization
  - GenericAudioProcessorEditor for parameter UI
affects: [02-waveform-engine, 03-modulation, 04-gui, 05-validation]

# Tech tracking
tech-stack:
  added: [JUCE 8.0.12, CMake 3.22+]
  patterns: [FetchContent for dependency management, APVTS parameter registration, XML state serialization]

key-files:
  created: [CMakeLists.txt, src/PluginProcessor.h, src/PluginProcessor.cpp]
  modified: []

key-decisions:
  - "Used juce module includes instead of JuceHeader.h (CMake-native approach)"
  - "Added BUNDLE_ID to avoid spaces in auto-generated com.Company.Plugin ID"

patterns-established:
  - "APVTS parameter groups with ParameterID version 1"
  - "Skewed NormalisableRange with 1/3 factor for exponential controls"
  - "GenericAudioProcessorEditor as placeholder until custom GUI phase"

requirements-completed: [FMT-01, FMT-02, FMT-04]

# Metrics
duration: 4min
completed: 2026-03-11
---

# Phase 1 Plan 1: Plugin Shell Summary

**JUCE 8.0.12 synth plugin with 21 APVTS parameters, VST3/AU/Standalone targets, skewed ranges for time controls, and XML state persistence**

## Performance

- **Duration:** 4 min
- **Started:** 2026-03-11T01:47:52Z
- **Completed:** 2026-03-11T01:51:44Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments
- Complete JUCE plugin shell building as VST3, AU, and Standalone from a single CMakeLists.txt
- 21 parameters across 6 groups (Pitch, Harmonics, Scan/Tilt, FM, Envelope, Output) with correct types, ranges, defaults, and skew
- State save/restore via APVTS XML pattern (copyState/replaceState)
- All plugin artifacts installed to ~/Library/Audio/Plug-Ins/ and Standalone launches successfully

## Task Commits

Each task was committed atomically:

1. **Task 1: Create CMake build system and plugin processor** - `bf2f891` (feat)
2. **Task 2: Build all targets and verify plugin artifacts** - verification only, no code changes

## Files Created/Modified
- `CMakeLists.txt` - Root CMake with JUCE 8.0.12 FetchContent, VST3/AU/Standalone plugin targets
- `src/PluginProcessor.h` - AudioProcessor subclass with APVTS member declaration
- `src/PluginProcessor.cpp` - Full implementation with createParameterLayout (21 params), state save/restore, stub processBlock
- `.gitignore` - Excludes build directory

## Decisions Made
- Used `#include <juce_audio_processors/juce_audio_processors.h>` and `<juce_audio_utils/juce_audio_utils.h>` instead of `<JuceHeader.h>` (JuceHeader.h is Projucer-only; CMake uses direct module includes)
- Added explicit `BUNDLE_ID "com.diestilleerde.harmonicoscillator"` to avoid JUCE warning about spaces in auto-generated bundle ID from company name

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Fixed JuceHeader.h include for CMake builds**
- **Found during:** Task 1 (Build verification)
- **Issue:** `#include <JuceHeader.h>` does not exist in CMake-based JUCE projects (it is a Projucer artifact)
- **Fix:** Changed to direct module includes: `juce_audio_processors.h` and `juce_audio_utils.h`
- **Files modified:** src/PluginProcessor.h
- **Verification:** VST3 target builds without errors
- **Committed in:** bf2f891 (Task 1 commit)

**2. [Rule 1 - Bug] Added BUNDLE_ID to fix CMake warning**
- **Found during:** Task 1 (CMake configure)
- **Issue:** Auto-generated bundle ID `com.Die stille Erde.HarmonicOscillator` contains spaces, triggering CMake warning
- **Fix:** Added explicit `BUNDLE_ID "com.diestilleerde.harmonicoscillator"` to juce_add_plugin
- **Files modified:** CMakeLists.txt
- **Verification:** CMake configure runs with no warnings
- **Committed in:** bf2f891 (Task 1 commit)

---

**Total deviations:** 2 auto-fixed (2 bugs)
**Impact on plan:** Both fixes necessary for correct builds. No scope creep.

## Issues Encountered
None beyond the auto-fixed deviations above.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Plugin shell complete with all 21 parameters registered and buildable
- Ready for Phase 2 (Waveform Engine) to implement DSP in processBlock
- GenericAudioProcessorEditor provides immediate parameter visibility for testing
- State save/restore ready for DAW testing

## Self-Check: PASSED

All files exist, commit bf2f891 verified, VST3 and AU artifacts installed.

---
*Phase: 01-plugin-shell*
*Completed: 2026-03-11*
