#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
#include <atomic>

// Decoupled parameter interface — no APVTS dependency
struct EngineParams
{
    std::atomic<float>* harmLevels[8] = {};
    std::atomic<float>* scanCenter = nullptr;
    std::atomic<float>* scanWidth = nullptr;
    std::atomic<float>* spectralTilt = nullptr;
    std::atomic<float>* masterLevel = nullptr;
    std::atomic<float>* outputSelect = nullptr;
    std::atomic<float>* coarseTune = nullptr;
    std::atomic<float>* fineTune = nullptr;
    std::atomic<float>* linFmDepth = nullptr;
    std::atomic<float>* expFmDepth = nullptr;
    std::atomic<float>* fmRate = nullptr;
    std::atomic<float>* attack = nullptr;
    std::atomic<float>* release = nullptr;
    std::atomic<float>* glide = nullptr;
};

class HarmonicEngine
{
public:
    HarmonicEngine() = default;

    void prepare (double sampleRate, int maxBlockSize, const EngineParams& params);

    double renderSample();

    void handleNoteOn (int midiNote, float velocity);
    void handleNoteOff (int midiNote);
    void handlePitchBend (int pitchWheelValue);

private:
    // Control-rate downsampling (expensive math every N samples)
    static constexpr int kControlRate = 32;
    int controlCounter_ = 0;

    // Sample rate
    double sampleRate_ = 44100.0;
    double inverseSampleRate_ = 1.0 / 44100.0;

    // Master phase accumulator (PolyBLEP waveforms)
    double phase_ = 0.0;

    // Per-harmonic phase accumulators (CRITICAL: independent, not shared)
    std::array<double, 8> harmPhases_ = {};

    // Triangle wave leaky integrator state
    double triIntegrator_ = 0.0;
    double triLeak_ = 0.999;

    // DC blocker state
    double dcX1_ = 0.0;
    double dcY1_ = 0.0;
    double dcR_ = 0.0;

    // Scan Gaussian envelope per harmonic
    std::array<double, 8> scanEnv_ = {};

    // Tilt multiplier per harmonic
    std::array<double, 8> tiltFactors_ = {};

    // Cached envelope coefficients (recomputed at control rate)
    double attackCoeff_ = 0.0;
    double releaseCoeff_ = 0.0;

    // Note state
    double currentVelocity_ = 0.0;
    bool gateOpen_ = false;

    // Legato gate counting and portamento glide
    int gateCount_ = 0;
    int targetNote_ = 69;
    double glidedNote_ = 69.0;
    bool hasPlayedNote_ = false;

    // Pitch bend state (standard +/- 2 semitones)
    double pitchBendSemitones_ = 0.0;

    // FM LFO state
    double fmPhase_ = 0.0;

    // AR envelope level
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

    // Decoupled parameter pointers
    EngineParams params_;
};
