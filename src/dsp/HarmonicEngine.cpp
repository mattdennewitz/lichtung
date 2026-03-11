#include "HarmonicEngine.h"

void HarmonicEngine::prepare (double sampleRate, int /*maxBlockSize*/,
                               juce::AudioProcessorValueTreeState& apvts)
{
    sampleRate_ = sampleRate;
    inverseSampleRate_ = 1.0 / sampleRate;

    // DC blocker coefficient (5Hz cutoff)
    dcR_ = 1.0 - (juce::MathConstants<double>::twoPi * 5.0 * inverseSampleRate_);

    // Gate ramp coefficients for 5ms anti-click
    gateAttackCoeff_ = 1.0 / (0.005 * sampleRate);
    gateReleaseCoeff_ = 1.0 / (0.005 * sampleRate);

    // Reset all state
    phase_ = 0.0;
    harmPhases_.fill (0.0);
    triIntegrator_ = 0.0;
    dcX1_ = 0.0;
    dcY1_ = 0.0;
    scanEnv_.fill (1.0);
    tiltFactors_.fill (1.0);
    gateOpen_ = false;
    gateLevel_ = 0.0;
    currentFreq_ = 440.0;
    currentVelocity_ = 0.0;

    // Initialize SmoothedValue for master level
    masterLevelSmoothed_.reset (sampleRate, 64.0 / sampleRate);
    masterLevelSmoothed_.setCurrentAndTargetValue (0.8f);

    // Initialize SmoothedValue for harmonic faders
    for (int i = 0; i < 8; ++i)
    {
        harmLevelSmoothed_[i].reset (sampleRate, 64.0 / sampleRate);
        float defaultVal = (i == 0) ? 1.0f : 0.0f;
        harmLevelSmoothed_[i].setCurrentAndTargetValue (defaultVal);
    }

    // Cache APVTS parameter pointers
    for (int i = 0; i < 8; ++i)
        harmLevelParams_[i] = apvts.getRawParameterValue ("harm_" + juce::String (i + 1));

    scanCenterParam_ = apvts.getRawParameterValue ("scan_center");
    scanWidthParam_ = apvts.getRawParameterValue ("scan_width");
    spectralTiltParam_ = apvts.getRawParameterValue ("spectral_tilt");
    masterLevelParam_ = apvts.getRawParameterValue ("master_level");
    outputSelectParam_ = apvts.getRawParameterValue ("output_select");
}

double HarmonicEngine::renderSample()
{
    // --- Section 2: Master phase accumulator ---
    double phaseInc = currentFreq_ * inverseSampleRate_;
    double p = phase_ + phaseInc;
    p -= std::floor (p);
    phase_ = p;

    // --- Section 3: PolyBLEP Sawtooth ---
    double sawRaw = 2.0 * p - 1.0;
    double dt = phaseInc;
    double sawBlep = 0.0;
    if (p < dt)
    {
        double t = p / dt;
        sawBlep = t + t - t * t - 1.0;
    }
    if (p > 1.0 - dt)
    {
        double t = (p - 1.0) / dt;
        sawBlep = t * t + t + t + 1.0;
    }
    double sawOut = sawRaw - sawBlep;

    // --- Section 3: PolyBLEP Square ---
    double sqrRaw = (p < 0.5) ? 1.0 : -1.0;
    double sqrBlep = 0.0;
    if (p < dt)
    {
        double t = p / dt;
        sqrBlep += t + t - t * t - 1.0;
    }
    else if (p > 1.0 - dt)
    {
        double t = (p - 1.0) / dt;
        sqrBlep += t * t + t + t + 1.0;
    }
    if (p >= 0.5 && p < 0.5 + dt)
    {
        double t = (p - 0.5) / dt;
        sqrBlep -= t + t - t * t - 1.0;
    }
    else if (p > 0.5 - dt && p < 0.5)
    {
        double t = (p - 0.5) / dt;
        sqrBlep -= t * t + t + t + 1.0;
    }
    double sqrOut = sqrRaw + sqrBlep;

    // --- Section 3: Triangle (leaky integrated square, fixed leak 0.999) ---
    double triRaw = triIntegrator_ * 0.999 + sqrOut * phaseInc * 4.0;
    triIntegrator_ = triRaw;
    double triOut = std::clamp (triRaw, -1.0, 1.0);

    // --- Section 4: Scan Gaussian envelope ---
    double centerPos = static_cast<double> (*scanCenterParam_) * 7.0;
    double sigma = 0.3 + static_cast<double> (*scanWidthParam_) * 9.7;
    for (int i = 0; i < 8; ++i)
    {
        double dist = (static_cast<double> (i) - centerPos) / sigma;
        scanEnv_[i] = std::exp (-0.5 * dist * dist);
    }

    // --- Section 5: Spectral tilt ---
    double tiltParam = static_cast<double> (*spectralTiltParam_);
    for (int i = 0; i < 8; ++i)
    {
        double normPos = static_cast<double> (i) / 7.0;
        double tiltDb = tiltParam * (normPos - 0.5) * 24.0;
        tiltFactors_[i] = std::pow (10.0, tiltDb / 20.0);
    }

    // --- Section 6: 8-harmonic sine bank with per-harmonic phase accumulators ---
    double harmonicMix = 0.0;
    double nyquist = sampleRate_ * 0.5;
    for (int i = 0; i < 8; ++i)
    {
        // Update SmoothedValue target from APVTS
        harmLevelSmoothed_[i].setTargetValue (*harmLevelParams_[i]);
        double fader = static_cast<double> (harmLevelSmoothed_[i].getNextValue());

        int harmNum = i + 1;
        double harmFreq = currentFreq_ * harmNum;
        if (harmFreq < nyquist)
        {
            double hPhase = harmPhases_[i] + harmFreq * inverseSampleRate_;
            hPhase -= std::floor (hPhase);
            harmPhases_[i] = hPhase;
            double amp = fader * scanEnv_[i] * tiltFactors_[i];
            harmonicMix += std::sin (hPhase * juce::MathConstants<double>::twoPi) * amp;
        }
        else
        {
            harmPhases_[i] = 0.0;
        }
    }
    harmonicMix *= 0.25;  // Normalization factor from gen~

    // --- Section 8: DC blocker (5Hz highpass) ---
    double mixDC = harmonicMix - dcX1_ + dcR_ * dcY1_;
    dcX1_ = harmonicMix;
    dcY1_ = mixDC;

    // --- Section 9: Gate, master level, output selector ---

    // 5ms anti-click gate ramp
    double gateTarget = gateOpen_ ? 1.0 : 0.0;
    if (gateLevel_ < gateTarget)
        gateLevel_ = std::min (gateLevel_ + gateAttackCoeff_, gateTarget);
    else if (gateLevel_ > gateTarget)
        gateLevel_ = std::max (gateLevel_ - gateReleaseCoeff_, gateTarget);

    double env = gateLevel_ * currentVelocity_;

    // Smoothed master level
    masterLevelSmoothed_.setTargetValue (*masterLevelParam_);
    double masterLvl = static_cast<double> (masterLevelSmoothed_.getNextValue());

    int outputMode = static_cast<int> (*outputSelectParam_);
    double selected;
    switch (outputMode)
    {
        case 0:  selected = mixDC;   break;
        case 1:  selected = triOut;  break;
        case 2:  selected = sawOut;  break;
        case 3:  selected = sqrOut;  break;
        default: selected = mixDC;   break;
    }
    return selected * env * masterLvl;
}

void HarmonicEngine::handleNoteOn (int midiNote, float velocity)
{
    currentFreq_ = juce::MidiMessage::getMidiNoteInHertz (midiNote);
    currentVelocity_ = velocity;
    gateOpen_ = true;
}

void HarmonicEngine::handleNoteOff (int /*midiNote*/)
{
    // Simple: any note-off closes gate (proper gate counting in Phase 3)
    gateOpen_ = false;
}
