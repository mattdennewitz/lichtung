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

    // Legato gate counting and portamento glide
    int gateCount_ = 0;
    int targetNote_ = 69;
    double glidedNote_ = 69.0;
    bool hasPlayedNote_ = false;

    // FM LFO state
    double fmPhase_ = 0.0;

    // AR envelope level (replaces 5ms gate ramp)
    double envLevel_ = 0.0;

    // Smoothed parameter values
    juce::SmoothedValue<float> masterLevelSmoothed_;
    std::array<juce::SmoothedValue<float>, 8> harmLevelSmoothed_;
    juce::SmoothedValue<float> scanCenterSmoothed_;
    juce::SmoothedValue<float> scanWidthSmoothed_;
    juce::SmoothedValue<float> spectralTiltSmoothed_;
    juce::SmoothedValue<float> linFmSmoothed_;
    juce::SmoothedValue<float> expFmSmoothed_;
    juce::SmoothedValue<float> fmRateSmoothed_;
    juce::SmoothedValue<float> attackSmoothed_;
    juce::SmoothedValue<float> releaseSmoothed_;
    juce::SmoothedValue<float> glideSmoothed_;
    juce::SmoothedValue<float> fineTuneSmoothed_;

    // Cached APVTS parameter pointers
    std::array<std::atomic<float>*, 8> harmLevelParams_ = {};
    std::atomic<float>* scanCenterParam_ = nullptr;
    std::atomic<float>* scanWidthParam_ = nullptr;
    std::atomic<float>* spectralTiltParam_ = nullptr;
    std::atomic<float>* masterLevelParam_ = nullptr;
    std::atomic<float>* outputSelectParam_ = nullptr;

    // Phase 3: FM, tuning, AR envelope parameter pointers
    std::atomic<float>* coarseTuneParam_ = nullptr;
    std::atomic<float>* fineTuneParam_ = nullptr;
    std::atomic<float>* linFmDepthParam_ = nullptr;
    std::atomic<float>* expFmDepthParam_ = nullptr;
    std::atomic<float>* fmRateParam_ = nullptr;
    std::atomic<float>* attackParam_ = nullptr;
    std::atomic<float>* releaseParam_ = nullptr;
    std::atomic<float>* glideParam_ = nullptr;
};
