#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "ui/CustomLookAndFeel.h"

class PluginProcessor;

class PluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor (PluginProcessor& p);
    ~PluginEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    PluginProcessor& processorRef;

    // LookAndFeel declared BEFORE child components (destroyed AFTER them)
    CustomLookAndFeel customLookAndFeel;

    // --- Sliders ---
    // Pitch
    juce::Slider coarseTuneSlider, fineTuneSlider, glideSlider;
    // Harmonics
    juce::Slider harmonicSliders[8];
    // Scan
    juce::Slider scanCenterSlider, scanWidthSlider, spectralTiltSlider;
    // FM
    juce::Slider linFmDepthSlider, expFmDepthSlider, fmRateSlider;
    // Envelope
    juce::Slider attackSlider, releaseSlider;
    // Output
    juce::Slider masterLevelSlider;

    // ComboBox
    juce::ComboBox outputSelectBox;

    // Random timbre button
    juce::TextButton randomTimbreButton;

    // --- Section header labels ---
    juce::Label pitchLabel, harmonicsLabel, scanLabel, fmLabel, outputLabel;

    // --- Harmonic labels ---
    juce::Label harmonicNumLabels[8];   // "1" through "8"
    juce::Label harmonicRatioLabels[8]; // "1x" through "8x"

    // --- Knob labels ---
    juce::Label coarseTuneLabel, fineTuneLabel, glideLabel;
    juce::Label scanCenterLabel, scanWidthLabel, spectralTiltLabel;
    juce::Label linFmDepthLabel, expFmDepthLabel, fmRateLabel;
    juce::Label attackLabel, releaseLabel, masterLevelLabel;

    // --- Attachments (declared AFTER the controls) ---
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> coarseTuneAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fineTuneAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> glideAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> harmonicAttach[8];
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> scanCenterAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> scanWidthAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> spectralTiltAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> linFmDepthAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> expFmDepthAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fmRateAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterLevelAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> outputSelectAttach;

    // Helper to set up a rotary knob
    void setupRotarySlider (juce::Slider& slider, double defaultValue);
    void setupSectionLabel (juce::Label& label, const juce::String& text);
    void setupKnobLabel (juce::Label& label, const juce::String& text);

    // Layout helpers
    juce::Rectangle<int> getPitchArea() const;
    juce::Rectangle<int> getHarmonicsArea() const;
    juce::Rectangle<int> getScanArea() const;
    juce::Rectangle<int> getFmArea() const;
    juce::Rectangle<int> getOutputArea() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
