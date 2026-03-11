cmake_minimum_required(VERSION 3.22)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0")
project(Lichtung VERSION 0.1.0)

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

juce_add_plugin(Lichtung
    COMPANY_NAME "Die stille Erde"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE DsEr
    PLUGIN_CODE Lcht
    BUNDLE_ID "com.diestilleerde.lichtung"
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Lichtung"
)

target_sources(Lichtung
    PRIVATE
        src/PluginProcessor.cpp
        src/PluginEditor.cpp
        src/dsp/HarmonicEngine.cpp
        src/ui/CustomLookAndFeel.cpp
)

target_compile_definitions(Lichtung
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(Lichtung
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
