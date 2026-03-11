#include "HarmonicEngine.h"

void HarmonicEngine::prepare (double sampleRate, int /*maxBlockSize*/,
                               juce::AudioProcessorValueTreeState& apvts)
{
    sampleRate_ = sampleRate;
    inverseSampleRate_ = 1.0 / sampleRate;

    // DC blocker coefficient (5Hz cutoff)
    dcR_ = 1.0 - (juce::MathConstants<double>::twoPi * 5.0 * inverseSampleRate_);

    // Reset all state
    phase_ = 0.0;
    harmPhases_.fill (0.0);
    triIntegrator_ = 0.0;
    dcX1_ = 0.0;
    dcY1_ = 0.0;
    scanEnv_.fill (1.0);
    tiltFactors_.fill (1.0);
    gateOpen_ = false;
    fmPhase_ = 0.0;
    envLevel_ = 0.0;
    currentFreq_ = 440.0;
    currentVelocity_ = 0.0;

    // Reset legato/glide state
    gateCount_ = 0;
    targetNote_ = 69;
    glidedNote_ = 69.0;
    hasPlayedNote_ = false;

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

    // Phase 3: FM, tuning, AR envelope parameters
    coarseTuneParam_ = apvts.getRawParameterValue ("coarse_tune");
    fineTuneParam_ = apvts.getRawParameterValue ("fine_tune");
    linFmDepthParam_ = apvts.getRawParameterValue ("lin_fm_depth");
    expFmDepthParam_ = apvts.getRawParameterValue ("exp_fm_depth");
    fmRateParam_ = apvts.getRawParameterValue ("fm_rate");
    attackParam_ = apvts.getRawParameterValue ("attack");
    releaseParam_ = apvts.getRawParameterValue ("release");
    glideParam_ = apvts.getRawParameterValue ("glide");
}

double HarmonicEngine::renderSample()
{
    // --- Glide (slide~): exponential follower on note number ---
    double glideMs = static_cast<double> (*glideParam_);
    double glideSamples = std::max (1.0, glideMs * 0.001 * sampleRate_);
    glidedNote_ += (static_cast<double> (targetNote_) - glidedNote_) / glideSamples;

    // --- Convert glided note to frequency (mtof~) ---
    double baseFreq = 440.0 * std::pow (2.0, (glidedNote_ - 69.0) / 12.0);

    // --- Section 1: Tuning (gen~ section 1) ---
    double coarseTune = static_cast<double> (*coarseTuneParam_);
    double fineTune = static_cast<double> (*fineTuneParam_);
    double tuneRatio = std::pow (2.0, (coarseTune * 100.0 + fineTune) / 1200.0);
    double freq = baseFreq * tuneRatio;

    // --- Section 1: FM LFO ---
    double fmRate = static_cast<double> (*fmRateParam_);
    double fmInc = fmRate * inverseSampleRate_;
    double fmP = fmPhase_ + fmInc;
    fmP -= std::floor (fmP);
    fmPhase_ = fmP;
    double fmLfo = std::sin (fmP * juce::MathConstants<double>::twoPi);

    // --- Section 1: Linear FM (additive: Hz offset proportional to carrier) ---
    double linFmDepth = static_cast<double> (*linFmDepthParam_);
    double linFmAmount = linFmDepth * freq * fmLfo;

    // --- Section 1: Exponential FM (multiplicative: pitch scaling) ---
    double expFmDepth = static_cast<double> (*expFmDepthParam_);
    double expFmAmount = std::pow (2.0, expFmDepth * fmLfo);

    // --- Section 1: Combine FM and clamp ---
    freq = freq * expFmAmount + linFmAmount;
    double nyquist = sampleRate_ * 0.5;
    freq = std::clamp (freq, 0.01, nyquist * 0.98);

    // --- Section 2: Master phase accumulator ---
    double phaseInc = freq * inverseSampleRate_;
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
    for (int i = 0; i < 8; ++i)
    {
        // Update SmoothedValue target from APVTS
        harmLevelSmoothed_[i].setTargetValue (*harmLevelParams_[i]);
        double fader = static_cast<double> (harmLevelSmoothed_[i].getNextValue());

        int harmNum = i + 1;
        double harmFreq = freq * harmNum;
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

    // --- Section 7: AR envelope (replaces 5ms gate ramp) ---
    double attackMs = static_cast<double> (*attackParam_);
    double releaseMs = static_cast<double> (*releaseParam_);
    double attackCoeff = std::exp (-1.0 / (std::max (1.0, attackMs) * 0.001 * sampleRate_));
    double releaseCoeff = std::exp (-1.0 / (std::max (1.0, releaseMs) * 0.001 * sampleRate_));

    double envTarget = gateOpen_ ? currentVelocity_ : 0.0;
    double envCoeff = gateOpen_ ? attackCoeff : releaseCoeff;
    envLevel_ = envTarget + envCoeff * (envLevel_ - envTarget);

    // --- Section 9: Output with corrected DC blocker order ---
    masterLevelSmoothed_.setTargetValue (*masterLevelParam_);
    double masterLvl = static_cast<double> (masterLevelSmoothed_.getNextValue());

    // Apply envelope and master level
    double rawMix = harmonicMix * envLevel_ * masterLvl;
    double rawTri = triOut * envLevel_ * masterLvl;
    double rawSaw = sawOut * envLevel_ * masterLvl;
    double rawSqr = sqrOut * envLevel_ * masterLvl;

    // DC blocker on mix only AFTER envelope (matching gen~)
    double mixDC = rawMix - dcX1_ + dcR_ * dcY1_;
    dcX1_ = rawMix;
    dcY1_ = mixDC;

    int outputMode = static_cast<int> (*outputSelectParam_);
    double selected;
    switch (outputMode)
    {
        case 0:  selected = mixDC;   break;
        case 1:  selected = rawTri;  break;
        case 2:  selected = rawSaw;  break;
        case 3:  selected = rawSqr;  break;
        default: selected = mixDC;   break;
    }
    return selected;
}

void HarmonicEngine::handleNoteOn (int midiNote, float velocity)
{
    gateCount_++;
    gateOpen_ = true;
    targetNote_ = midiNote;
    currentVelocity_ = static_cast<double> (velocity);

    // First note: set glide position immediately (no slide from default)
    if (!hasPlayedNote_)
    {
        glidedNote_ = static_cast<double> (midiNote);
        hasPlayedNote_ = true;
    }
    // Subsequent notes: glidedNote_ slides toward targetNote_ in renderSample
}

void HarmonicEngine::handleNoteOff (int /*midiNote*/)
{
    gateCount_ = std::max (0, gateCount_ - 1);
    gateOpen_ = (gateCount_ > 0);
}
