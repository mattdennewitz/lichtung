# Technology Stack

**Project:** Harmonic Oscillator (JUCE additive synth plugin)
**Researched:** 2026-03-10
**Overall Confidence:** MEDIUM (versions need verification -- web tools unavailable during research)

## Recommended Stack

### Core Framework

| Technology | Version | Purpose | Why | Confidence |
|------------|---------|---------|-----|------------|
| JUCE | 8.x (verify latest on GitHub) | Audio plugin framework | Industry standard for VST3/AU/standalone. Handles plugin format wrapping, parameter management, GUI, audio I/O. No serious alternative exists for cross-format C++ plugins. | HIGH (framework choice), MEDIUM (exact version) |
| C++ | C++17 minimum (C++20 preferred) | Implementation language | JUCE 7+ requires C++17. C++20 adds concepts, ranges, and `std::numbers::pi` which clean up DSP code. JUCE 8 supports C++20. | HIGH |
| CMake | 3.24+ | Build system | JUCE's official build system since JUCE 6. Projucer is deprecated for new projects. CMake enables IDE-agnostic builds, CI integration, and dependency management via FetchContent. | HIGH |

### Build Toolchain

| Technology | Version | Purpose | Why | Confidence |
|------------|---------|---------|-----|------------|
| Xcode | 15+ (latest stable) | macOS compiler/IDE | Required for AU validation, codesigning, and universal binaries. Apple Clang is the only supported compiler for AU on macOS. | HIGH |
| Xcode Command Line Tools | Latest | Compiler toolchain | Provides clang, linker, macOS SDK. Required even for CMake-only builds. | HIGH |
| Ninja | Latest | CMake generator | Faster than Xcode generator for iterative builds. Use Xcode generator only for debugging/profiling. | HIGH |

### Plugin SDKs

| Technology | Version | Purpose | Why | Confidence |
|------------|---------|---------|-----|------------|
| VST3 SDK | Bundled with JUCE | VST3 format support | JUCE bundles the VST3 SDK headers. No separate download needed since JUCE 6. Do NOT download the Steinberg SDK separately. | HIGH |
| Apple Audio Unit SDK | macOS system | AU format support | Provided by macOS/Xcode. JUCE wraps it via the AudioUnitv3 API (AUv3) internally. No separate download. | HIGH |

### DSP Libraries

| Library | Version | Purpose | When to Use | Confidence |
|---------|---------|---------|-------------|------------|
| juce::dsp module | Built into JUCE | Oscillators, filters, IIR/FIR, FFT, convolution | Use for DC blocker (juce::dsp::IIR::Filter with high-pass). Avoid its oscillator classes -- roll custom for this project's specific needs. | HIGH |
| Custom DSP (no library) | N/A | Additive synthesis, PolyBLEP, scan/tilt, FM | The core DSP is bespoke. No library provides Verbos-style scan envelopes or this specific harmonic architecture. Translate directly from gen~ codebox. | HIGH |

### Testing

| Technology | Version | Purpose | Why | Confidence |
|------------|---------|---------|-----|------------|
| Catch2 | v3.x | Unit testing | Lightweight, header-only, widely used in JUCE projects. Perfect for testing DSP functions in isolation. JUCE's own tests use it. | HIGH |
| pluginval | Latest | Plugin validation | JUCE team's own plugin validator. Catches crashes, parameter issues, threading violations. Run in CI and before every release. Non-negotiable for plugin development. | HIGH |

### CI / Automation

| Technology | Version | Purpose | Why | Confidence |
|------------|---------|---------|-----|------------|
| GitHub Actions | N/A | CI/CD | Free for public repos. macOS runners available for AU builds. Standard for JUCE open source projects. | MEDIUM (depends on hosting choice) |

## Stack Decisions with Rationale

### Why JUCE (not alternatives)

**JUCE is the only real choice** for a multi-format C++ audio plugin in 2025/2026. Alternatives considered:

| Alternative | Why Not |
|-------------|---------|
| iPlug2 | Smaller community, less documentation, fewer format targets. Fine for simple plugins but JUCE's GUI toolkit is far superior for the 8-fader + scan/tilt UI this project needs. |
| DPF (DISTRHO Plugin Framework) | Linux-focused, minimal GUI support, no AU. |
| Rust + nih-plug | Immature ecosystem, no AU support, would require rewriting all DSP from scratch without the gen~ codebox as close reference. C++ maps 1:1 to gen~ codebox math. |
| CLAP-only (clap-helpers) | No AU, project explicitly needs VST3+AU. |
| Raw VST3/AU SDKs | Massive boilerplate. JUCE eliminates thousands of lines of format-specific code. |

### Why CMake (not Projucer)

- Projucer is deprecated. JUCE team recommends CMake for all new projects.
- CMake integrates with any IDE (CLion, VS Code, Xcode).
- `juce_add_plugin()` CMake function handles all format targets declaratively.
- FetchContent can pull JUCE itself, making the project self-contained.

### Why Custom DSP (not juce::dsp oscillators)

- `juce::dsp::Oscillator` is a basic wavetable/function oscillator. It does not support additive synthesis, PolyBLEP, or the scan/tilt algorithm.
- The gen~ codebox IS the DSP spec. Translating it line-by-line to C++ ensures sample-accurate matching.
- Only use `juce::dsp` for utility functions: DC blocker (IIR high-pass), possibly `SmoothedValue` for parameter smoothing.

### Why C++17/20 (not C++14)

- JUCE 7+ dropped C++14 support.
- C++17 gives: structured bindings, `std::clamp`, `if constexpr`, `std::optional`.
- C++20 adds: `std::numbers::pi_v<float>` (no more `#define PI`), concepts for constraining DSP templates, `std::span` for buffer views.
- Recommendation: Target C++20 if Xcode version supports it (Xcode 14.3+ supports C++20). Fall back to C++17 only if CI constraints require it.

## Project Structure

```
harmonic-oscillator/
  CMakeLists.txt                    # Top-level: FetchContent JUCE, add subdirectories
  src/
    PluginProcessor.h/.cpp          # AudioProcessor: parameter layout, prepareToPlay, processBlock
    PluginEditor.h/.cpp             # AudioProcessorEditor: GUI layout
    dsp/
      HarmonicOscillator.h/.cpp     # 8-harmonic sine bank + scan + tilt
      PolyBLEP.h/.cpp               # Saw, square, triangle via PolyBLEP
      FMOperator.h/.cpp             # Internal LFO for linear/exponential FM
      AREnvelope.h/.cpp             # Attack-release envelope with exponential curves
      DCBlocker.h/.cpp              # High-pass filter (or use juce::dsp::IIR)
      Portamento.h/.cpp             # Glide/portamento logic
    gui/
      PitchSection.h/.cpp           # Coarse/fine tuning knobs
      HarmonicSection.h/.cpp        # 8 vertical sliders
      ScanTiltSection.h/.cpp        # Scan center, width, tilt controls
      FMSection.h/.cpp              # FM depth, rate, mode controls
      OutputSection.h/.cpp          # Output selector, master level, AR envelope
  test/
    dsp/
      HarmonicOscillatorTest.cpp    # Unit tests for DSP accuracy
      PolyBLEPTest.cpp
      ...
  JUCE/                             # Fetched via CMake FetchContent (or git submodule)
```

### CMakeLists.txt Skeleton

```cmake
cmake_minimum_required(VERSION 3.24)
project(HarmonicOscillator VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch JUCE
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG master  # Pin to specific release tag after verifying latest
)
FetchContent_MakeAvailable(JUCE)

# Plugin target
juce_add_plugin(HarmonicOscillator
    COMPANY_NAME "YourName"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    PLUGIN_MANUFACTURER_CODE HRMN
    PLUGIN_CODE HmOs
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Harmonic Oscillator"
    COPY_PLUGIN_AFTER_BUILD TRUE
)

target_sources(HarmonicOscillator PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    # ... DSP and GUI sources
)

target_compile_definitions(HarmonicOscillator PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0  # Requires JUCE license or GPL
)

target_link_libraries(HarmonicOscillator
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
```

## Alternatives Considered

| Category | Recommended | Alternative | Why Not |
|----------|-------------|-------------|---------|
| Framework | JUCE 8 | iPlug2 | Smaller ecosystem, less GUI power |
| Framework | JUCE 8 | nih-plug (Rust) | No AU, different language from reference impl |
| Build | CMake | Projucer | Deprecated, not scriptable, poor CI support |
| Build | CMake | Meson/Bazel | No JUCE integration |
| Testing | Catch2 v3 | Google Test | Heavier, less common in audio/JUCE projects |
| Testing | Catch2 v3 | doctest | Viable alternative but Catch2 has more JUCE community usage |
| DC Blocker | juce::dsp::IIR | Custom | No reason to hand-roll a simple 1-pole high-pass |
| Param Smoothing | juce::SmoothedValue | Custom exponential | SmoothedValue handles thread-safe ramping correctly |
| GUI | JUCE Component (native) | JUCE + WebView | Massive overkill for 5 sections of knobs/sliders |
| GUI | JUCE LookAndFeel | Figma/SVG export | Over-engineering for a personal-use plugin |

## JUCE Modules to Include

| Module | Required | Purpose |
|--------|----------|---------|
| juce_audio_basics | Yes | AudioBuffer, MidiMessage, basic audio types |
| juce_audio_devices | Yes (via audio_utils) | Audio device management for standalone |
| juce_audio_formats | Yes (via audio_utils) | Not directly needed but pulled in by audio_utils |
| juce_audio_plugin_client | Yes (auto) | Plugin format wrappers (VST3, AU) |
| juce_audio_processors | Yes | AudioProcessor base class, parameters |
| juce_audio_utils | Yes | AudioProcessorEditor, keyboard component |
| juce_core | Yes | Basics, logging, files |
| juce_data_structures | Yes | ValueTree for state save/restore |
| juce_dsp | Yes | IIR filters (DC blocker), SmoothedValue, lookup tables |
| juce_events | Yes | Message thread, timers |
| juce_graphics | Yes | Graphics, fonts, colours for GUI |
| juce_gui_basics | Yes | Component, Slider, Label |
| juce_gui_extra | No | Contains web browser, code editor -- not needed |
| juce_opengl | No | Only needed for OpenGL-rendered UIs |
| juce_osc | No | OSC networking -- not needed |
| juce_video | No | Video playback -- not needed |

## Key JUCE APIs for This Project

| API | Use Case | Notes |
|-----|----------|-------|
| `juce::AudioProcessor` | Plugin entry point | Override `processBlock()`, `prepareToPlay()`, `getStateInformation()` |
| `juce::AudioProcessorValueTreeState` (APVTS) | Parameter management | Declarative parameter layout. Thread-safe. Handles host automation, state save/load. Use this, not raw AudioParameterFloat. |
| `juce::AudioProcessorEditor` | GUI | Create custom Component subclass. Attach sliders/knobs to APVTS parameters via SliderAttachment. |
| `juce::SmoothedValue<float>` | Parameter smoothing | Prevents zipper noise on knob changes. Use for all continuously-variable parameters. |
| `juce::dsp::IIR::Filter` | DC blocker | Configure as 1st-order high-pass at ~5-10Hz. Apply to harmonic mix output. |
| `juce::dsp::LookupTableTransform` | Fast sine computation | Pre-compute sine table for the 8-harmonic bank. Faster than `std::sin()` per-sample. |
| `juce::MidiBuffer` | MIDI processing | Iterate in processBlock for note on/off, pitch bend. |

## What NOT to Use

| Technology | Why Not |
|------------|---------|
| Projucer | Deprecated. Use CMake. |
| JUCE VST2 wrapper | VST2 SDK is discontinued by Steinberg. No new VST2 plugins. |
| `juce::Synthesiser` / `juce::SynthesiserVoice` | Designed for polyphonic synths with voice stealing. This is monophonic -- using it adds unnecessary complexity. Roll a simple mono voice handler. |
| `juce::dsp::Oscillator` | Too generic. Does not support PolyBLEP, additive synthesis, or scan/tilt. Custom DSP is required. |
| External DSP libraries (Faust, STK, Gamma) | Adds dependency for no benefit. The gen~ codebox is the spec; translate directly to C++. |
| JUCE's `LookAndFeel_V4` as-is | Looks generic. Customize `LookAndFeel` for the Verbos-inspired panel aesthetic. |
| Melatonin Inspector | Useful for debugging complex GUIs. This UI is simple enough to not need it. Consider only if layout issues arise. |

## Version Verification Needed

**IMPORTANT:** The following versions could not be verified against live sources during research (web tools were unavailable). Verify before starting development:

| Item | Assumed | How to Verify |
|------|---------|---------------|
| JUCE version | 8.x | `https://github.com/juce-framework/JUCE/releases` |
| CMake minimum | 3.24 | JUCE repo CMakeLists.txt |
| Catch2 version | 3.x | `https://github.com/catchorg/Catch2/releases` |
| C++20 Xcode support | Xcode 14.3+ | Apple developer docs |
| pluginval | Latest | `https://github.com/Tracktion/pluginval/releases` |

Pin JUCE to a specific git tag (e.g., `8.0.3`) in CMakeLists.txt rather than tracking `master`.

## Installation

```bash
# Prerequisites (macOS)
brew install cmake ninja

# Xcode (from App Store or xcode-select)
xcode-select --install

# Clone and build
git clone <repo>
cd harmonic-oscillator
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Plugins will be in:
# build/HarmonicOscillator_artefacts/Release/VST3/
# build/HarmonicOscillator_artefacts/Release/AU/

# For development (debug + auto-copy to plugin folders)
cmake -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug
```

## Sources

- JUCE GitHub repository: https://github.com/juce-framework/JUCE (HIGH confidence on API patterns, MEDIUM on exact current version)
- JUCE CMake API documentation: https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md (HIGH confidence)
- pluginval: https://github.com/Tracktion/pluginval (HIGH confidence)
- Catch2: https://github.com/catchorg/Catch2 (HIGH confidence on v3 recommendation)
- Training data knowledge of JUCE ecosystem patterns (MEDIUM confidence -- well-established patterns unlikely to have changed)
