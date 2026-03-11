#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <cmath>
#include <atomic>

class HarmonicEngine
{
public:
    HarmonicEngine() = default;

    void prepare (double sampleRate, int maxBlockSize,
                  juce::AudioProcessorValueTreeState& apvts);

    double renderSample();

    void handleNoteOn (int midiNote, float velocity);
    void handleNoteOff (int midiNote);

private:
    // Sample rate
    double sampleRate_ = 44100.0;
    double inverseSampleRate_ = 1.0 / 44100.0;

    // Master phase accumulator (PolyBLEP waveforms)
    double phase_ = 0.0;

    // Per-harmonic phase accumulators (CRITICAL: independent, not shared)
    std::array<double, 8> harmPhases_ = {};

    // Triangle wave leaky integrator state
    double triIntegrator_ = 0.0;

    // DC blocker state
    double dcX1_ = 0.0;
    double dcY1_ = 0.0;
    double dcR_ = 0.0;

    // Scan Gaussian envelope per harmonic
    std::array<double, 8> scanEnv_ = {};

    // Tilt multiplier per harmonic
    std::array<double, 8> tiltFactors_ = {};

    // Note state
    double currentFreq_ = 440.0;
    double currentVelocity_ = 0.0;
    bool gateOpen_ = false;

    // 5ms anti-click gate ramp
    double gateLevel_ = 0.0;
    double gateAttackCoeff_ = 0.0;
    double gateReleaseCoeff_ = 0.0;

    // Smoothed parameter values
    juce::SmoothedValue<float> masterLevelSmoothed_;
    std::array<juce::SmoothedValue<float>, 8> harmLevelSmoothed_;

    // Cached APVTS parameter pointers
    std::array<std::atomic<float>*, 8> harmLevelParams_ = {};
    std::atomic<float>* scanCenterParam_ = nullptr;
    std::atomic<float>* scanWidthParam_ = nullptr;
    std::atomic<float>* spectralTiltParam_ = nullptr;
    std::atomic<float>* masterLevelParam_ = nullptr;
    std::atomic<float>* outputSelectParam_ = nullptr;
};
