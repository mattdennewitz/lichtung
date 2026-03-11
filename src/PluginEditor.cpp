#include "PluginEditor.h"
#include "PluginProcessor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (p),
      processorRef (p)
{
    setLookAndFeel (&customLookAndFeel);

    // --- Pitch knobs ---
    setupRotarySlider (coarseTuneSlider, 0.0);
    coarseTuneSlider.setNumDecimalPlacesToDisplay (0);
    coarseTuneAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.apvts, "coarse_tune", coarseTuneSlider);

    setupRotarySlider (fineTuneSlider, 0.0);
    fineTuneAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.apvts, "fine_tune", fineTuneSlider);

    setupRotarySlider (glideSlider, 0.0);
    glideAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.apvts, "glide", glideSlider);

    // --- Harmonic faders ---
    for (int i = 0; i < 8; ++i)
    {
        auto& slider = harmonicSliders[i];
        slider.setSliderStyle (juce::Slider::LinearVertical);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 40, 16);
        slider.setDoubleClickReturnValue (true, (i == 0) ? 1.0 : 0.0);
        addAndMakeVisible (slider);

        harmonicAttach[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processorRef.apvts, "harm_" + juce::String (i + 1), slider);

        // Harmonic number label (above fader)
        setupKnobLabel (harmonicNumLabels[i], juce::String (i + 1));

        // Frequency ratio label (below fader)
        setupKnobLabel (harmonicRatioLabels[i], juce::String (i + 1) + "x");
    }

    // --- Scan knobs ---
    setupRotarySlider (scanCenterSlider, 0.5);
    scanCenterAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.apvts, "scan_center", scanCenterSlider);

    setupRotarySlider (scanWidthSlider, 1.0);
    scanWidthAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.apvts, "scan_width", scanWidthSlider);

    setupRotarySlider (spectralTiltSlider, 0.0);
    spectralTiltAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.apvts, "spectral_tilt", spectralTiltSlider);

    // --- FM knobs ---
    setupRotarySlider (linFmDepthSlider, 0.0);
    linFmDepthAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.apvts, "lin_fm_depth", linFmDepthSlider);

    setupRotarySlider (expFmDepthSlider, 0.0);
    expFmDepthAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.apvts, "exp_fm_depth", expFmDepthSlider);

    setupRotarySlider (fmRateSlider, 1.0);
    fmRateAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.apvts, "fm_rate", fmRateSlider);

    // --- Envelope knobs ---
    setupRotarySlider (attackSlider, 10.0);
    attackAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.apvts, "attack", attackSlider);

    setupRotarySlider (releaseSlider, 100.0);
    releaseAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.apvts, "release", releaseSlider);

    // --- Output ---
    setupRotarySlider (masterLevelSlider, 0.8);
    masterLevelAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.apvts, "master_level", masterLevelSlider);

    outputSelectBox.addItemList ({ "Harmonic Mix", "Triangle", "Sawtooth", "Square" }, 1);
    addAndMakeVisible (outputSelectBox);
    outputSelectAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processorRef.apvts, "output_select", outputSelectBox);

    // --- Section header labels ---
    setupSectionLabel (pitchLabel, "PITCH");
    setupSectionLabel (harmonicsLabel, "HARMONICS");
    setupSectionLabel (scanLabel, "SCAN / TILT");
    setupSectionLabel (fmLabel, "FM");
    setupSectionLabel (outputLabel, "OUTPUT");

    // --- Knob labels ---
    setupKnobLabel (coarseTuneLabel, "Coarse");
    setupKnobLabel (fineTuneLabel, "Fine");
    setupKnobLabel (glideLabel, "Glide");
    setupKnobLabel (scanCenterLabel, "Center");
    setupKnobLabel (scanWidthLabel, "Width");
    setupKnobLabel (spectralTiltLabel, "Tilt");
    setupKnobLabel (linFmDepthLabel, "Lin Depth");
    setupKnobLabel (expFmDepthLabel, "Exp Depth");
    setupKnobLabel (fmRateLabel, "Rate");
    setupKnobLabel (attackLabel, "Attack");
    setupKnobLabel (releaseLabel, "Release");
    setupKnobLabel (masterLevelLabel, "Level");

    // --- Resize configuration ---
    setResizable (true, true);
    setResizeLimits (525, 338, 1050, 675);
    getConstrainer()->setFixedAspectRatio (700.0 / 450.0);
    setSize (700, 450);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
}

void PluginEditor::setupRotarySlider (juce::Slider& slider, double defaultValue)
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    slider.setDoubleClickReturnValue (true, defaultValue);
    addAndMakeVisible (slider);
}

void PluginEditor::setupSectionLabel (juce::Label& label, const juce::String& text)
{
    label.setText (text, juce::dontSendNotification);
    label.setFont (juce::Font (14.0f).boldened());
    label.setColour (juce::Label::textColourId, CustomLookAndFeel::labelGray);
    label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (label);
}

void PluginEditor::setupKnobLabel (juce::Label& label, const juce::String& text)
{
    label.setText (text, juce::dontSendNotification);
    label.setFont (juce::Font (11.0f));
    label.setColour (juce::Label::textColourId, CustomLookAndFeel::labelGray);
    label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (label);
}

// --- Layout area helpers ---

juce::Rectangle<int> PluginEditor::getPitchArea() const
{
    auto area = getLocalBounds().reduced (8);
    auto topRow = area.removeFromTop (static_cast<int> (area.getHeight() * 0.55f));
    topRow.removeFromRight (4);
    return topRow.removeFromLeft (static_cast<int> (topRow.getWidth() * 0.22f)).reduced (2);
}

juce::Rectangle<int> PluginEditor::getHarmonicsArea() const
{
    auto area = getLocalBounds().reduced (8);
    auto topRow = area.removeFromTop (static_cast<int> (area.getHeight() * 0.55f));
    topRow.removeFromLeft (static_cast<int> (topRow.getWidth() * 0.22f));
    topRow.removeFromLeft (4);
    return topRow.reduced (2);
}

juce::Rectangle<int> PluginEditor::getScanArea() const
{
    auto area = getLocalBounds().reduced (8);
    area.removeFromTop (static_cast<int> (area.getHeight() * 0.55f));
    area.removeFromTop (4);
    auto thirdWidth = area.getWidth() / 3;
    return area.removeFromLeft (thirdWidth).reduced (2);
}

juce::Rectangle<int> PluginEditor::getFmArea() const
{
    auto area = getLocalBounds().reduced (8);
    area.removeFromTop (static_cast<int> (area.getHeight() * 0.55f));
    area.removeFromTop (4);
    auto thirdWidth = area.getWidth() / 3;
    area.removeFromLeft (thirdWidth);
    return area.removeFromLeft (thirdWidth).reduced (2);
}

juce::Rectangle<int> PluginEditor::getOutputArea() const
{
    auto area = getLocalBounds().reduced (8);
    area.removeFromTop (static_cast<int> (area.getHeight() * 0.55f));
    area.removeFromTop (4);
    auto thirdWidth = area.getWidth() / 3;
    area.removeFromLeft (thirdWidth);
    area.removeFromLeft (thirdWidth);
    return area.reduced (2);
}

// --- Paint ---

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (CustomLookAndFeel::backgroundColour);

    auto cornerSize = 6.0f;
    g.setColour (CustomLookAndFeel::panelColour);

    g.fillRoundedRectangle (getPitchArea().toFloat(), cornerSize);
    g.fillRoundedRectangle (getHarmonicsArea().toFloat(), cornerSize);
    g.fillRoundedRectangle (getScanArea().toFloat(), cornerSize);
    g.fillRoundedRectangle (getFmArea().toFloat(), cornerSize);
    g.fillRoundedRectangle (getOutputArea().toFloat(), cornerSize);
}

// --- Resized ---

void PluginEditor::resized()
{
    // --- Pitch section ---
    {
        auto section = getPitchArea();
        pitchLabel.setBounds (section.removeFromTop (24));

        auto knobHeight = section.getHeight() / 3;

        auto coarseArea = section.removeFromTop (knobHeight);
        coarseTuneLabel.setBounds (coarseArea.removeFromTop (14));
        coarseTuneSlider.setBounds (coarseArea);

        auto fineArea = section.removeFromTop (knobHeight);
        fineTuneLabel.setBounds (fineArea.removeFromTop (14));
        fineTuneSlider.setBounds (fineArea);

        glideLabel.setBounds (section.removeFromTop (14));
        glideSlider.setBounds (section);
    }

    // --- Harmonics section ---
    {
        auto section = getHarmonicsArea();
        harmonicsLabel.setBounds (section.removeFromTop (24));

        auto faderWidth = section.getWidth() / 8;

        for (int i = 0; i < 8; ++i)
        {
            auto col = section.removeFromLeft (faderWidth);
            harmonicNumLabels[i].setBounds (col.removeFromTop (16));
            harmonicRatioLabels[i].setBounds (col.removeFromBottom (16));
            harmonicSliders[i].setBounds (col);
        }
    }

    // --- Scan/Tilt section ---
    {
        auto section = getScanArea();
        scanLabel.setBounds (section.removeFromTop (24));

        auto knobWidth = section.getWidth() / 3;

        auto centerCol = section.removeFromLeft (knobWidth);
        scanCenterLabel.setBounds (centerCol.removeFromTop (14));
        scanCenterSlider.setBounds (centerCol);

        auto widthCol = section.removeFromLeft (knobWidth);
        scanWidthLabel.setBounds (widthCol.removeFromTop (14));
        scanWidthSlider.setBounds (widthCol);

        spectralTiltLabel.setBounds (section.removeFromTop (14));
        spectralTiltSlider.setBounds (section);
    }

    // --- FM section ---
    {
        auto section = getFmArea();
        fmLabel.setBounds (section.removeFromTop (24));

        auto knobWidth = section.getWidth() / 3;

        auto linCol = section.removeFromLeft (knobWidth);
        linFmDepthLabel.setBounds (linCol.removeFromTop (14));
        linFmDepthSlider.setBounds (linCol);

        auto expCol = section.removeFromLeft (knobWidth);
        expFmDepthLabel.setBounds (expCol.removeFromTop (14));
        expFmDepthSlider.setBounds (expCol);

        fmRateLabel.setBounds (section.removeFromTop (14));
        fmRateSlider.setBounds (section);
    }

    // --- Output section ---
    {
        auto section = getOutputArea();
        outputLabel.setBounds (section.removeFromTop (24));

        auto topHalf = section.removeFromTop (section.getHeight() / 2);

        // Attack and Release knobs in a row
        auto envWidth = topHalf.getWidth() / 2;
        auto atkCol = topHalf.removeFromLeft (envWidth);
        attackLabel.setBounds (atkCol.removeFromTop (14));
        attackSlider.setBounds (atkCol);

        releaseLabel.setBounds (topHalf.removeFromTop (14));
        releaseSlider.setBounds (topHalf);

        // Bottom: ComboBox and master level
        auto bottomHalf = section;
        auto comboArea = bottomHalf.removeFromLeft (bottomHalf.getWidth() / 2);
        outputSelectBox.setBounds (comboArea.reduced (4).removeFromTop (24));

        masterLevelLabel.setBounds (bottomHalf.removeFromTop (14));
        masterLevelSlider.setBounds (bottomHalf);
    }
}
