#include "HarmonicEngine.h"

void HarmonicEngine::prepare (double sampleRate, int /*maxBlockSize*/,
                               const EngineParams& params)
{
    sampleRate_ = sampleRate;
    inverseSampleRate_ = 1.0 / sampleRate;
    params_ = params;

    // DC blocker coefficient (5Hz cutoff)
    dcR_ = 1.0 - (juce::MathConstants<double>::twoPi * 5.0 * inverseSampleRate_);

    // Triangle integrator leak: ~7Hz equivalent at any sample rate
    triLeak_ = 1.0 - (juce::MathConstants<double>::twoPi * 7.0 * inverseSampleRate_);

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
    currentVelocity_ = 0.0;
    attackCoeff_ = 0.0;
    releaseCoeff_ = 0.0;
    controlCounter_ = 0;
    pitchBendSemitones_ = 0.0;

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

    // Initialize SmoothedValues for all continuous parameters
    double smoothTime = 64.0 / sampleRate;
    scanCenterSmoothed_.reset (sampleRate, smoothTime);
    scanWidthSmoothed_.reset (sampleRate, smoothTime);
    spectralTiltSmoothed_.reset (sampleRate, smoothTime);
    linFmSmoothed_.reset (sampleRate, smoothTime);
    expFmSmoothed_.reset (sampleRate, smoothTime);
    fmRateSmoothed_.reset (sampleRate, smoothTime);
    attackSmoothed_.reset (sampleRate, smoothTime);
    releaseSmoothed_.reset (sampleRate, smoothTime);
    glideSmoothed_.reset (sampleRate, smoothTime);
    fineTuneSmoothed_.reset (sampleRate, smoothTime);

    scanCenterSmoothed_.setCurrentAndTargetValue (*params_.scanCenter);
    scanWidthSmoothed_.setCurrentAndTargetValue (*params_.scanWidth);
    spectralTiltSmoothed_.setCurrentAndTargetValue (*params_.spectralTilt);
    linFmSmoothed_.setCurrentAndTargetValue (*params_.linFmDepth);
    expFmSmoothed_.setCurrentAndTargetValue (*params_.expFmDepth);
    fmRateSmoothed_.setCurrentAndTargetValue (*params_.fmRate);
    attackSmoothed_.setCurrentAndTargetValue (*params_.attack);
    releaseSmoothed_.setCurrentAndTargetValue (*params_.release);
    glideSmoothed_.setCurrentAndTargetValue (*params_.glide);
    fineTuneSmoothed_.setCurrentAndTargetValue (*params_.fineTune);
}

double HarmonicEngine::renderSample()
{
    // --- Glide (slide~): exponential follower on note number ---
    glideSmoothed_.setTargetValue (*params_.glide);
    double glideMs = static_cast<double> (glideSmoothed_.getNextValue());
    double glideSamples = std::max (1.0, glideMs * 0.001 * sampleRate_);
    glidedNote_ += (static_cast<double> (targetNote_) - glidedNote_) / glideSamples;

    // --- Apply pitch bend and convert to frequency (mtof~) ---
    double pitchedNote = glidedNote_ + pitchBendSemitones_;
    double baseFreq = 440.0 * std::pow (2.0, (pitchedNote - 69.0) / 12.0);

    // --- Section 1: Tuning (gen~ section 1) ---
    double coarseTune = static_cast<double> (*params_.coarseTune);
    fineTuneSmoothed_.setTargetValue (*params_.fineTune);
    double fineTune = static_cast<double> (fineTuneSmoothed_.getNextValue());
    double tuneRatio = std::pow (2.0, (coarseTune * 100.0 + fineTune) / 1200.0);
    double freq = baseFreq * tuneRatio;

    // --- Section 1: FM LFO ---
    fmRateSmoothed_.setTargetValue (*params_.fmRate);
    double fmRate = static_cast<double> (fmRateSmoothed_.getNextValue());
    double fmInc = fmRate * inverseSampleRate_;
    double fmP = fmPhase_ + fmInc;
    fmP -= std::floor (fmP);
    fmPhase_ = fmP;
    double fmLfo = std::sin (fmP * juce::MathConstants<double>::twoPi);

    // --- Section 1: Linear FM (additive: Hz offset proportional to carrier) ---
    linFmSmoothed_.setTargetValue (*params_.linFmDepth);
    double linFmDepth = static_cast<double> (linFmSmoothed_.getNextValue());
    double linFmAmount = linFmDepth * freq * fmLfo;

    // --- Section 1: Exponential FM (multiplicative: pitch scaling) ---
    expFmSmoothed_.setTargetValue (*params_.expFmDepth);
    double expFmDepth = static_cast<double> (expFmSmoothed_.getNextValue());
    double expFmAmount = std::pow (2.0, expFmDepth * fmLfo);

    // --- Section 1: Combine FM and clamp ---
    freq = freq * expFmAmount + linFmAmount;
    double nyquist = sampleRate_ * 0.5;
    freq = std::clamp (freq, 0.01, nyquist * 0.98);

    // Read output mode early to skip PolyBLEP when only harmonic mix is needed
    int outputMode = static_cast<int> (*params_.outputSelect);

    // --- Section 2: Master phase accumulator ---
    double phaseInc = freq * inverseSampleRate_;
    double p = phase_ + phaseInc;
    p -= std::floor (p);
    phase_ = p;

    // --- Section 3: PolyBLEP waveforms (only computed for output modes 1-3) ---
    double sawOut = 0.0;
    double sqrOut = 0.0;
    double triOut = 0.0;

    if (outputMode != 0)
    {
        // PolyBLEP Sawtooth
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
        sawOut = sawRaw - sawBlep;

        // PolyBLEP Square
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
        sqrOut = sqrRaw + sqrBlep;

        // Triangle (leaky integrated square, sample-rate-derived leak)
        double triRaw = triIntegrator_ * triLeak_ + sqrOut * phaseInc * 4.0;
        triIntegrator_ = triRaw;
        triOut = std::clamp (triRaw, -1.0, 1.0);
    }
    else
    {
        // Leak integrator toward zero for clean transition when switching modes
        triIntegrator_ *= triLeak_;
    }

    // --- Sections 4-5 & envelope coefficients: control-rate update ---
    scanCenterSmoothed_.setTargetValue (*params_.scanCenter);
    scanWidthSmoothed_.setTargetValue (*params_.scanWidth);
    spectralTiltSmoothed_.setTargetValue (*params_.spectralTilt);
    attackSmoothed_.setTargetValue (*params_.attack);
    releaseSmoothed_.setTargetValue (*params_.release);

    float scanCenterVal = scanCenterSmoothed_.getNextValue();
    float scanWidthVal = scanWidthSmoothed_.getNextValue();
    float spectralTiltVal = spectralTiltSmoothed_.getNextValue();
    float attackVal = attackSmoothed_.getNextValue();
    float releaseVal = releaseSmoothed_.getNextValue();

    if (--controlCounter_ <= 0)
    {
        controlCounter_ = kControlRate;

        // Section 4: Scan Gaussian envelope
        double centerPos = static_cast<double> (scanCenterVal) * 7.0;
        double sigma = 0.3 + static_cast<double> (scanWidthVal) * 9.7;
        for (int i = 0; i < 8; ++i)
        {
            double dist = (static_cast<double> (i) - centerPos) / sigma;
            scanEnv_[i] = std::exp (-0.5 * dist * dist);
        }

        // Section 5: Spectral tilt
        double tiltP = static_cast<double> (spectralTiltVal);
        for (int i = 0; i < 8; ++i)
        {
            double normPos = static_cast<double> (i) / 7.0;
            double tiltDb = tiltP * (normPos - 0.5) * 24.0;
            tiltFactors_[i] = std::pow (10.0, tiltDb / 20.0);
        }

        // Envelope coefficients
        double attackMs = static_cast<double> (attackVal);
        double releaseMs = static_cast<double> (releaseVal);
        attackCoeff_ = std::exp (-1.0 / (std::max (1.0, attackMs) * 0.001 * sampleRate_));
        releaseCoeff_ = std::exp (-1.0 / (std::max (1.0, releaseMs) * 0.001 * sampleRate_));
    }

    // --- Section 6: 8-harmonic sine bank with per-harmonic phase accumulators ---
    double harmonicMix = 0.0;
    for (int i = 0; i < 8; ++i)
    {
        harmLevelSmoothed_[i].setTargetValue (*params_.harmLevels[i]);
        double fader = static_cast<double> (harmLevelSmoothed_[i].getNextValue());

        int harmNum = i + 1;
        double harmFreq = freq * harmNum;

        // Always advance phase to avoid clicks when harmonics re-enter
        double hPhase = harmPhases_[i] + harmFreq * inverseSampleRate_;
        hPhase -= std::floor (hPhase);
        harmPhases_[i] = hPhase;

        if (harmFreq < nyquist)
        {
            double amp = fader * scanEnv_[i] * tiltFactors_[i];
            harmonicMix += std::sin (hPhase * juce::MathConstants<double>::twoPi) * amp;
        }
    }
    harmonicMix *= 0.25;  // Normalization factor from gen~

    // --- Section 7: AR envelope (coefficients updated at control rate above) ---
    double envTarget = gateOpen_ ? currentVelocity_ : 0.0;
    double envCoeff = gateOpen_ ? attackCoeff_ : releaseCoeff_;
    envLevel_ = envTarget + envCoeff * (envLevel_ - envTarget);

    // --- Section 9: Output with corrected DC blocker order ---
    masterLevelSmoothed_.setTargetValue (*params_.masterLevel);
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

void HarmonicEngine::handlePitchBend (int pitchWheelValue)
{
    // Convert 14-bit MIDI pitch bend (0-16383, center 8192) to +/- 2 semitones
    pitchBendSemitones_ = ((pitchWheelValue - 8192) / 8192.0) * 2.0;
}
