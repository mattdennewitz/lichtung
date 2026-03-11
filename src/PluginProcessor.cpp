#include "PluginProcessor.h"
#include "PluginEditor.h"

PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, juce::Identifier ("LichtungParams"),
             createParameterLayout())
{
}

PluginProcessor::~PluginProcessor()
{
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // No input buses expected (synth)
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled())
        return false;

    const auto& outSet = layouts.getMainOutputChannelSet();
    return outSet == juce::AudioChannelSet::mono()
        || outSet == juce::AudioChannelSet::stereo();
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Helper: create a skewed range (exponent 3.0 = JUCE skew 1/3)
    // JUCE skew < 1.0 gives more resolution at low values
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

void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::AudioProcessor::prepareToPlay (sampleRate, samplesPerBlock);

    // Bridge APVTS to decoupled EngineParams
    for (int i = 0; i < 8; ++i)
        engineParams_.harmLevels[i] = apvts.getRawParameterValue ("harm_" + juce::String (i + 1));

    engineParams_.scanCenter    = apvts.getRawParameterValue ("scan_center");
    engineParams_.scanWidth     = apvts.getRawParameterValue ("scan_width");
    engineParams_.spectralTilt  = apvts.getRawParameterValue ("spectral_tilt");
    engineParams_.masterLevel   = apvts.getRawParameterValue ("master_level");
    engineParams_.outputSelect  = apvts.getRawParameterValue ("output_select");
    engineParams_.coarseTune    = apvts.getRawParameterValue ("coarse_tune");
    engineParams_.fineTune      = apvts.getRawParameterValue ("fine_tune");
    engineParams_.linFmDepth    = apvts.getRawParameterValue ("lin_fm_depth");
    engineParams_.expFmDepth    = apvts.getRawParameterValue ("exp_fm_depth");
    engineParams_.fmRate        = apvts.getRawParameterValue ("fm_rate");
    engineParams_.attack        = apvts.getRawParameterValue ("attack");
    engineParams_.release       = apvts.getRawParameterValue ("release");
    engineParams_.glide         = apvts.getRawParameterValue ("glide");

    engine_.prepare (sampleRate, samplesPerBlock, engineParams_);
}

void PluginProcessor::releaseResources()
{
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                     juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int numChannels = buffer.getNumChannels();
    auto* leftChannel = buffer.getWritePointer (0);
    auto* rightChannel = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    int currentSample = 0;

    for (const auto metadata : midiMessages)
    {
        // Render samples up to this MIDI event
        for (int s = currentSample; s < metadata.samplePosition; ++s)
        {
            float sample = static_cast<float> (engine_.renderSample());
            leftChannel[s] = sample;
            if (rightChannel != nullptr)
                rightChannel[s] = sample;
        }
        currentSample = metadata.samplePosition;

        // Handle MIDI event
        auto msg = metadata.getMessage();
        if (msg.isNoteOn())
            engine_.handleNoteOn (msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isNoteOff())
            engine_.handleNoteOff (msg.getNoteNumber());
        else if (msg.isPitchWheel())
            engine_.handlePitchBend (msg.getPitchWheelValue());
    }

    // Render remaining samples after last MIDI event
    for (int s = currentSample; s < buffer.getNumSamples(); ++s)
    {
        float sample = static_cast<float> (engine_.renderSample());
        leftChannel[s] = sample;
        if (rightChannel != nullptr)
            rightChannel[s] = sample;
    }
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state ("LichtungState");
    state.setProperty ("version", 1, nullptr);

    state.appendChild (apvts.copyState(), nullptr);

    juce::ValueTree uiState ("UIState");
    uiState.setProperty ("width", lastUIWidth, nullptr);
    uiState.setProperty ("height", lastUIHeight, nullptr);
    state.appendChild (uiState, nullptr);

    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState == nullptr)
        return;

    // Versioned format (v1+)
    if (xmlState->hasTagName ("LichtungState"))
    {
        juce::ValueTree state = juce::ValueTree::fromXml (*xmlState);

        auto paramState = state.getChildWithName (apvts.state.getType());
        if (paramState.isValid())
            apvts.replaceState (paramState);

        auto uiState = state.getChildWithName ("UIState");
        if (uiState.isValid())
        {
            lastUIWidth  = uiState.getProperty ("width", 700);
            lastUIHeight = uiState.getProperty ("height", 450);
        }
    }
    // Legacy format (pre-v1): bare APVTS state
    else if (xmlState->hasTagName (apvts.state.getType()))
    {
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
    }
}

// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
