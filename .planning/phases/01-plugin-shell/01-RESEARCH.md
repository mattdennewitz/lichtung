# Phase 1: Plugin Shell - Research

**Researched:** 2026-03-10
**Domain:** JUCE 8.x CMake project scaffolding, VST3/AU plugin formats, APVTS parameter registration
**Confidence:** HIGH

## Summary

Phase 1 creates a loadable VST3/AU plugin shell with all synth parameters registered in an AudioProcessorValueTreeState (APVTS) and host state save/restore working. No DSP processing and no custom GUI -- the plugin uses JUCE's GenericAudioProcessorEditor so parameters are visible and automatable in any DAW.

JUCE 8.0.12 (released December 2025) is the latest stable release and should be pinned via CMake FetchContent. The CMake API is well-documented and stable. All parameter types needed (float with skew, integer-step, choice/enum) are supported by APVTS through AudioParameterFloat, AudioParameterInt, and AudioParameterChoice. State save/restore uses the standard copyState/replaceState XML pattern.

**Primary recommendation:** Pin JUCE 8.0.12 via FetchContent, use a flat ParameterLayout with AudioProcessorParameterGroup for logical grouping, and use NormalisableRange with skew = 1.0f/3.0f for the four exponent-3.0 parameters.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Plugin name: "Harmonic Oscillator"
- Manufacturer: "Die stille Erde"
- Category: Instrument / Synthesizer
- Plugin unique IDs: Claude's discretion (generate unique 4-char codes for manufacturer + plugin)
- All parameter ranges, defaults, and scaling as specified in CONTEXT.md (see Parameter Ranges section)
- Parameters with exponent 3.0 skew: Attack, Release, Glide, FM Rate
- Coarse tune: integer steps
- Output select: enum with 4 values [Harmonic Mix, Triangle, Sawtooth, Square]
- No DSP, no GUI editor in this phase

### Claude's Discretion
- JUCE version selection and pinning strategy (FetchContent vs submodule)
- CMake project structure and directory layout
- 4-char unique ID codes for VST3/AU
- Parameter group organization in APVTS
- Build configuration details (Debug/Release, compiler flags)

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| FMT-01 | VST3 format | juce_add_plugin FORMATS includes VST3; verified in JUCE CMake API |
| FMT-02 | AU format | juce_add_plugin FORMATS includes AU; auto-enabled on macOS |
| FMT-04 | Plugin state save/restore via APVTS | copyState/replaceState XML pattern documented in JUCE tutorials |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| JUCE | 8.0.12 | Audio plugin framework | Industry standard for VST3/AU plugins, latest stable release |
| CMake | >= 3.22 | Build system | Required by JUCE 8.x CMake API |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| juce_audio_utils | (bundled) | APVTS, GenericAudioProcessorEditor | Always -- core parameter management module |
| juce_audio_processors | (bundled) | AudioProcessor base class | Always -- implicit dependency of juce_audio_utils |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| FetchContent | Git submodule | FetchContent is cleaner for pinning versions; submodule requires manual init |
| GenericAudioProcessorEditor | No editor (hasEditor=false) | Generic editor lets you visually verify parameters; hasEditor=false has known issues with some hosts |

**Installation:**
```bash
# No manual install needed -- CMake FetchContent downloads JUCE automatically
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

## Architecture Patterns

### Recommended Project Structure
```
harmonic-oscillator/
├── CMakeLists.txt              # Root CMake with FetchContent for JUCE
├── src/
│   ├── PluginProcessor.h       # AudioProcessor subclass declaration
│   ├── PluginProcessor.cpp     # AudioProcessor implementation + APVTS
│   ├── PluginEditor.h          # GenericAudioProcessorEditor wrapper
│   └── PluginEditor.cpp        # Editor implementation (thin wrapper)
└── .planning/                  # Planning docs (not built)
```

### Pattern 1: CMakeLists.txt with FetchContent
**What:** Root CMake file that fetches JUCE 8.0.12 and configures the plugin target
**When to use:** Always -- this is the project entry point

```cmake
cmake_minimum_required(VERSION 3.22)
project(HarmonicOscillator VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.12
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(HarmonicOscillator
    COMPANY_NAME "Die stille Erde"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE DsEr
    PLUGIN_CODE Hrmo
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Harmonic Oscillator"
)

target_sources(HarmonicOscillator
    PRIVATE
        src/PluginProcessor.cpp
        src/PluginEditor.cpp
)

target_compile_definitions(HarmonicOscillator
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(HarmonicOscillator
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
```

**Notes on IDs:**
- `PLUGIN_MANUFACTURER_CODE`: "DsEr" -- 4 chars, matches existing "Die stille Erde" plugins (three-sisters, multi-tap-delay)
- `PLUGIN_CODE`: "Hrmo" -- 4 chars, exactly one uppercase (first letter), derived from "Harmonic Oscillator". GarageBand requires first letter uppercase, rest lowercase.

### Pattern 2: APVTS Parameter Registration with createParameterLayout
**What:** Static function that creates all parameters with correct types, ranges, defaults, and skewing
**When to use:** Called once in the PluginProcessor constructor

```cpp
static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // --- Helper: create a skewed range (exponent 3.0 = JUCE skew 1/3) ---
    // JUCE skew math: convertFrom0to1 applies exp(log(p) / skew)
    // For exponent 3.0 (more resolution at low values): skew = 1.0f / 3.0f
    auto skewedRange = [] (float start, float end, float interval = 0.0f)
    {
        return juce::NormalisableRange<float> (start, end, interval, 1.0f / 3.0f);
    };

    // --- Pitch group ---
    auto pitchGroup = std::make_unique<juce::AudioProcessorParameterGroup> (
        "pitch", "Pitch", "|");

    pitchGroup->addChild (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { "coarse_tune", 1 }, "Coarse Tune", -24, 24, 0));

    pitchGroup->addChild (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fine_tune", 1 }, "Fine Tune",
        juce::NormalisableRange<float> (-100.0f, 100.0f, 0.01f), 0.0f));

    pitchGroup->addChild (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "glide", 1 }, "Glide",
        skewedRange (0.0f, 2000.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    layout.add (std::move (pitchGroup));

    // --- Harmonics group ---
    auto harmonicsGroup = std::make_unique<juce::AudioProcessorParameterGroup> (
        "harmonics", "Harmonics", "|");

    for (int i = 1; i <= 8; ++i)
    {
        float defaultVal = (i == 1) ? 1.0f : 0.0f;
        harmonicsGroup->addChild (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "harm_" + juce::String (i), 1 },
            "Harmonic " + juce::String (i),
            juce::NormalisableRange<float> (0.0f, 1.0f), defaultVal));
    }

    layout.add (std::move (harmonicsGroup));

    // --- Scan/Tilt group ---
    auto scanGroup = std::make_unique<juce::AudioProcessorParameterGroup> (
        "scan", "Scan/Tilt", "|");

    scanGroup->addChild (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "scan_center", 1 }, "Scan Center",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f));

    scanGroup->addChild (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "scan_width", 1 }, "Scan Width",
        juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f));

    scanGroup->addChild (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "spectral_tilt", 1 }, "Spectral Tilt",
        juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f));

    layout.add (std::move (scanGroup));

    // --- FM group ---
    auto fmGroup = std::make_unique<juce::AudioProcessorParameterGroup> (
        "fm", "FM", "|");

    fmGroup->addChild (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "lin_fm_depth", 1 }, "Lin FM Depth",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));

    fmGroup->addChild (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "exp_fm_depth", 1 }, "Exp FM Depth",
        juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f));

    fmGroup->addChild (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "fm_rate", 1 }, "FM Rate",
        skewedRange (0.1f, 20000.0f), 1.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    layout.add (std::move (fmGroup));

    // --- Envelope group ---
    auto envGroup = std::make_unique<juce::AudioProcessorParameterGroup> (
        "envelope", "Envelope", "|");

    envGroup->addChild (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "attack", 1 }, "Attack",
        skewedRange (1.0f, 2000.0f), 10.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    envGroup->addChild (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "release", 1 }, "Release",
        skewedRange (1.0f, 5000.0f), 100.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    layout.add (std::move (envGroup));

    // --- Output group ---
    auto outputGroup = std::make_unique<juce::AudioProcessorParameterGroup> (
        "output", "Output", "|");

    outputGroup->addChild (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "master_level", 1 }, "Master Level",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.8f));

    outputGroup->addChild (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "output_select", 1 }, "Output Select",
        juce::StringArray { "Harmonic Mix", "Triangle", "Sawtooth", "Square" }, 0));

    layout.add (std::move (outputGroup));

    return layout;
}
```

### Pattern 3: State Save/Restore (getStateInformation / setStateInformation)
**What:** Standard APVTS XML serialization pattern
**When to use:** Required for FMT-04 (state persistence)

```cpp
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}
```

### Pattern 4: GenericAudioProcessorEditor (no custom GUI)
**What:** Use JUCE's built-in parameter editor for Phase 1
**When to use:** Phase 1 only -- replaced by custom editor in Phase 4

```cpp
// PluginProcessor.h
bool hasEditor() const override { return true; }

// PluginProcessor.cpp
juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}
```

This eliminates the need for a separate PluginEditor.h/cpp in Phase 1. The GenericAudioProcessorEditor automatically renders sliders/combos for all APVTS parameters.

### Pattern 5: PluginProcessor Class Structure
**What:** Minimal AudioProcessor subclass with APVTS

```cpp
class PluginProcessor : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
```

### Anti-Patterns to Avoid
- **Creating parameters outside APVTS:** All parameters must go through APVTS for state save/restore and host automation to work correctly.
- **Using Projucer:** This project uses pure CMake. Do not generate Projucer files.
- **Hardcoding plugin metadata in code:** Use the CMake juce_add_plugin properties; JUCE generates JucePlugin_Name etc. as preprocessor defines.
- **Forgetting ParameterID version number:** JUCE 8.x uses `juce::ParameterID { "id", 1 }` (the second argument is the parameter version). Omitting it causes deprecation warnings.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Parameter management | Custom parameter system | APVTS (AudioProcessorValueTreeState) | Thread-safe, handles automation, state, undo |
| State serialization | Custom binary format | APVTS copyState/replaceState + XML | Proven pattern, handles versioning |
| Parameter UI (Phase 1) | Custom editor components | GenericAudioProcessorEditor | Automatic, shows all APVTS params |
| Build system | Makefiles or Projucer | CMake + FetchContent | JUCE 8.x native CMake support, reproducible |
| Skewed ranges | Manual math in processBlock | NormalisableRange with skew factor | Integrates with host automation curves |

**Key insight:** JUCE's APVTS handles the entire parameter lifecycle (registration, thread-safe access, automation, state persistence). Building any part of this custom is unnecessary and error-prone.

## Common Pitfalls

### Pitfall 1: Wrong Skew Factor Direction
**What goes wrong:** Using skew = 3.0 instead of 1.0/3.0, resulting in more resolution at HIGH values instead of low values
**Why it happens:** Confusion between "exponent" (from Max/MSP) and JUCE's skew factor. They are inverses.
**How to avoid:** JUCE's skew factor < 1.0 gives more resolution at low values. For "exponent 3.0" use `skew = 1.0f / 3.0f`.
**Warning signs:** Attack/Release knobs feel wrong -- tiny movements at the bottom cause huge jumps.

### Pitfall 2: Missing ParameterID Version Number
**What goes wrong:** Deprecation warnings or compilation errors in JUCE 8.x
**Why it happens:** JUCE 7.x used plain string IDs; JUCE 8.x uses `juce::ParameterID { "id", version }`.
**How to avoid:** Always use `juce::ParameterID { "param_id", 1 }` as the first argument to parameter constructors.
**Warning signs:** Compiler deprecation warnings about ParameterID constructor.

### Pitfall 3: GenericAudioProcessorEditor Empty Window
**What goes wrong:** Plugin window opens but shows no parameter controls
**Why it happens:** Known issue when parameters are not properly registered in APVTS, or when hasEditor returns false and createEditor returns nullptr.
**How to avoid:** Ensure hasEditor() returns true and createEditor() returns `new juce::GenericAudioProcessorEditor (*this)`. Verify all parameters are in the APVTS.

### Pitfall 4: FetchContent Downloads on Every Build
**What goes wrong:** CMake re-downloads JUCE on every configure step
**Why it happens:** Missing FETCHCONTENT_UPDATES_DISCONNECTED or not using GIT_SHALLOW.
**How to avoid:** Use `GIT_SHALLOW TRUE` in FetchContent_Declare. After first successful configure, optionally set `FETCHCONTENT_UPDATES_DISCONNECTED ON`.

### Pitfall 5: Plugin Does Not Appear in DAW
**What goes wrong:** Plugin builds but is not visible in the DAW plugin scanner
**Why it happens:** COPY_PLUGIN_AFTER_BUILD is FALSE, or the plugin is built to a non-standard location
**How to avoid:** Set `COPY_PLUGIN_AFTER_BUILD TRUE` in juce_add_plugin. VST3 copies to `~/Library/Audio/Plug-Ins/VST3/`, AU copies to `~/Library/Audio/Plug-Ins/Components/`.

### Pitfall 6: APVTS Constructor Order
**What goes wrong:** Crash on construction because APVTS is initialized before the AudioProcessor base class
**Why it happens:** C++ member initialization order. APVTS must be initialized in the member initializer list AFTER the base class.
**How to avoid:** Declare `apvts` after any base class data, and initialize it in the constructor initializer list:
```cpp
PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "HarmonicOscillatorParams", createParameterLayout())
{
}
```

## Code Examples

All patterns are provided in the Architecture Patterns section above. Key verified examples:

### Complete Constructor
```cpp
// Source: JUCE 8.x APVTS tutorial pattern
PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, juce::Identifier ("HarmonicOscillatorParams"),
             createParameterLayout())
{
}
```

### Stub processBlock (Phase 1 - no DSP)
```cpp
void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                     juce::MidiBuffer& /*midiMessages*/)
{
    // Phase 1: no DSP -- silence output
    buffer.clear();
}
```

### Plugin Instance Creator
```cpp
// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Projucer project generation | Pure CMake with FetchContent | JUCE 6+ (mature in 8.x) | No Projucer dependency, CI-friendly |
| String parameter IDs | ParameterID { "id", version } | JUCE 7.x | Enables parameter versioning for state compat |
| Raw AudioProcessorParameter | APVTS with typed parameters | JUCE 5+ | Thread-safe automation, state, undo |

**Deprecated/outdated:**
- `AudioParameterFloat (String, String, float, float, float)` constructor -- use the NormalisableRange version
- Projucer-based builds -- CMake is the recommended approach for JUCE 8.x
- `createParameterLayout` returning `{ params... }` braced init list -- use `layout.add()` for clarity

## Open Questions

1. **Exact skew factor mapping to Max/MSP exponent behavior**
   - What we know: JUCE skew = 1/3 produces `p^3` mapping via `exp(log(p) * 3)`, giving more resolution at low values
   - What's unclear: Whether Max/MSP's "exponent 3.0" on live.dial produces exactly the same curve as JUCE's `skew = 1/3`
   - Recommendation: Use skew = 1/3 -- it produces the correct qualitative behavior (more resolution at low values). Exact curve matching can be verified in Phase 5 validation.

2. **COPY_PLUGIN_AFTER_BUILD with system permissions**
   - What we know: JUCE copies to ~/Library/Audio/Plug-Ins/ by default
   - What's unclear: Whether sandboxed build environments or CI might block this
   - Recommendation: Enable it for local development. Disable for CI if needed.

## Sources

### Primary (HIGH confidence)
- [JUCE GitHub releases](https://github.com/juce-framework/JUCE/releases/tag/8.0.12) - JUCE 8.0.12 release tag, December 2025
- [JUCE CMake AudioPlugin example](https://github.com/juce-framework/JUCE/blob/master/examples/CMake/AudioPlugin/CMakeLists.txt) - Official juce_add_plugin example
- [JUCE CMake API docs](https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md) - Full CMake API reference
- [JUCE NormalisableRange source](https://github.com/juce-framework/JUCE/blob/master/modules/juce_core/maths/juce_NormalisableRange.h) - Skew factor math verified from source code
- [JUCE APVTS tutorial](https://juce.com/tutorials/tutorial_audio_processor_value_tree_state/) - State save/restore pattern

### Secondary (MEDIUM confidence)
- [JUCE NormalisableRange docs](https://docs.juce.com/master/classNormalisableRange.html) - Constructor signatures and skew documentation
- [JUCE GenericAudioProcessorEditor docs](https://docs.juce.com/master/classGenericAudioProcessorEditor.html) - No-editor pattern
- [Melatonin CMake + JUCE guide](https://melatonin.dev/blog/how-to-use-cmake-with-juce/) - Community best practices

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - JUCE 8.0.12 verified on GitHub, CMake API well-documented
- Architecture: HIGH - Patterns sourced from official JUCE examples and tutorials
- Pitfalls: HIGH - Known issues documented in JUCE forums and verified against source code

**Research date:** 2026-03-10
**Valid until:** 2026-06-10 (JUCE releases are infrequent, API is stable)
