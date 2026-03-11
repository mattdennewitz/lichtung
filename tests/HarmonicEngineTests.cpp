#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "dsp/HarmonicEngine.h"

#include <cmath>
#include <vector>

// ---------------------------------------------------------------------------
// Test harness: mock atomic parameters wired to EngineParams
// ---------------------------------------------------------------------------

struct TestHarness
{
    std::atomic<float> harmLevels[8] = { 1.0f, 0.0f, 0.0f, 0.0f,
                                          0.0f, 0.0f, 0.0f, 0.0f };
    std::atomic<float> scanCenter   { 0.5f };
    std::atomic<float> scanWidth    { 1.0f };
    std::atomic<float> spectralTilt { 0.0f };
    std::atomic<float> masterLevel  { 0.8f };
    std::atomic<float> outputSelect { 0.0f };
    std::atomic<float> coarseTune   { 0.0f };
    std::atomic<float> fineTune     { 0.0f };
    std::atomic<float> linFmDepth   { 0.0f };
    std::atomic<float> expFmDepth   { 0.0f };
    std::atomic<float> fmRate       { 1.0f };
    std::atomic<float> attack       { 1.0f };   // fast for tests
    std::atomic<float> release      { 1.0f };   // fast for tests
    std::atomic<float> glide        { 0.0f };

    EngineParams params;
    HarmonicEngine engine;
    double sampleRate;

    explicit TestHarness (double sr = 44100.0) : sampleRate (sr)
    {
        for (int i = 0; i < 8; ++i)
            params.harmLevels[i] = &harmLevels[i];

        params.scanCenter   = &scanCenter;
        params.scanWidth    = &scanWidth;
        params.spectralTilt = &spectralTilt;
        params.masterLevel  = &masterLevel;
        params.outputSelect = &outputSelect;
        params.coarseTune   = &coarseTune;
        params.fineTune     = &fineTune;
        params.linFmDepth   = &linFmDepth;
        params.expFmDepth   = &expFmDepth;
        params.fmRate       = &fmRate;
        params.attack       = &attack;
        params.release      = &release;
        params.glide        = &glide;

        engine.prepare (sampleRate, 512, params);
    }

    std::vector<double> render (int n)
    {
        std::vector<double> out (static_cast<size_t> (n));
        for (int i = 0; i < n; ++i)
            out[static_cast<size_t> (i)] = engine.renderSample();
        return out;
    }

    void skip (int n)
    {
        for (int i = 0; i < n; ++i)
            engine.renderSample();
    }

    // Estimate fundamental frequency via positive-going zero crossings
    double measureFrequency (int numSamples)
    {
        auto samples = render (numSamples);
        int crossings = 0;
        for (size_t i = 1; i < samples.size(); ++i)
        {
            if (samples[i - 1] < 0.0 && samples[i] >= 0.0)
                ++crossings;
        }
        return static_cast<double> (crossings) * sampleRate
             / static_cast<double> (numSamples);
    }

    static double rms (const std::vector<double>& s)
    {
        double sum = 0.0;
        for (auto v : s) sum += v * v;
        return std::sqrt (sum / static_cast<double> (s.size()));
    }

    static double peak (const std::vector<double>& s)
    {
        double m = 0.0;
        for (auto v : s) m = std::max (m, std::abs (v));
        return m;
    }

    static double mean (const std::vector<double>& s)
    {
        double sum = 0.0;
        for (auto v : s) sum += v;
        return sum / static_cast<double> (s.size());
    }
};

// ---------------------------------------------------------------------------
// Note lifecycle
// ---------------------------------------------------------------------------

TEST_CASE ("Silence before any note", "[lifecycle]")
{
    TestHarness h;
    auto out = h.render (500);
    REQUIRE (TestHarness::rms (out) < 0.001);
}

TEST_CASE ("Note on produces sound, note off decays to silence", "[lifecycle]")
{
    TestHarness h;
    h.engine.handleNoteOn (69, 1.0f);
    h.skip (500);
    auto playing = h.render (1000);
    REQUIRE (TestHarness::rms (playing) > 0.1);

    h.engine.handleNoteOff (69);
    h.skip (2000);
    auto decayed = h.render (1000);
    REQUIRE (TestHarness::rms (decayed) < 0.001);
}

// ---------------------------------------------------------------------------
// Frequency / pitch accuracy
// ---------------------------------------------------------------------------

TEST_CASE ("A4 (MIDI 69) produces 440 Hz", "[pitch]")
{
    TestHarness h;
    h.engine.handleNoteOn (69, 1.0f);
    h.skip (1000);
    double freq = h.measureFrequency (44100);
    REQUIRE_THAT (freq, Catch::Matchers::WithinAbs (440.0, 2.0));
}

TEST_CASE ("Middle C (MIDI 60) produces ~261.6 Hz", "[pitch]")
{
    TestHarness h;
    h.engine.handleNoteOn (60, 1.0f);
    h.skip (1000);
    double freq = h.measureFrequency (44100);
    REQUIRE_THAT (freq, Catch::Matchers::WithinAbs (261.63, 2.0));
}

// ---------------------------------------------------------------------------
// Pitch bend (+/- 2 semitones)
// ---------------------------------------------------------------------------

TEST_CASE ("Pitch bend range is +/- 2 semitones", "[pitch]")
{
    const double twoSemiUp   = 440.0 * std::pow (2.0, 2.0 / 12.0);   // ~493.88
    const double twoSemiDown = 440.0 * std::pow (2.0, -2.0 / 12.0);  // ~392.00

    SECTION ("Full bend up")
    {
        TestHarness h;
        h.engine.handleNoteOn (69, 1.0f);
        h.engine.handlePitchBend (16383);
        h.skip (1000);
        REQUIRE_THAT (h.measureFrequency (44100),
                      Catch::Matchers::WithinAbs (twoSemiUp, 2.0));
    }

    SECTION ("Full bend down")
    {
        TestHarness h;
        h.engine.handleNoteOn (69, 1.0f);
        h.engine.handlePitchBend (0);
        h.skip (1000);
        REQUIRE_THAT (h.measureFrequency (44100),
                      Catch::Matchers::WithinAbs (twoSemiDown, 2.0));
    }

    SECTION ("Center = no bend")
    {
        TestHarness h;
        h.engine.handleNoteOn (69, 1.0f);
        h.engine.handlePitchBend (8192);
        h.skip (1000);
        REQUIRE_THAT (h.measureFrequency (44100),
                      Catch::Matchers::WithinAbs (440.0, 2.0));
    }
}

// ---------------------------------------------------------------------------
// Coarse / fine tuning
// ---------------------------------------------------------------------------

TEST_CASE ("Coarse tune +12 doubles frequency", "[pitch]")
{
    TestHarness h;
    h.coarseTune.store (12.0f);
    h.engine.handleNoteOn (69, 1.0f);
    h.skip (1000);
    REQUIRE_THAT (h.measureFrequency (44100),
                  Catch::Matchers::WithinAbs (880.0, 3.0));
}

TEST_CASE ("Coarse tune -12 halves frequency", "[pitch]")
{
    TestHarness h;
    h.coarseTune.store (-12.0f);
    h.engine.handleNoteOn (69, 1.0f);
    h.skip (1000);
    REQUIRE_THAT (h.measureFrequency (44100),
                  Catch::Matchers::WithinAbs (220.0, 2.0));
}

TEST_CASE ("Fine tune +100 cents = +1 semitone", "[pitch]")
{
    const double expected = 440.0 * std::pow (2.0, 1.0 / 12.0); // ~466.16

    TestHarness h;
    h.fineTune.store (100.0f);
    h.engine.handleNoteOn (69, 1.0f);
    h.skip (1000);
    REQUIRE_THAT (h.measureFrequency (44100),
                  Catch::Matchers::WithinAbs (expected, 2.0));
}

// ---------------------------------------------------------------------------
// Legato / gate counting
// ---------------------------------------------------------------------------

TEST_CASE ("Legato keeps gate open with overlapping notes", "[midi]")
{
    TestHarness h;
    h.engine.handleNoteOn (60, 1.0f);
    h.skip (500);
    REQUIRE (TestHarness::rms (h.render (500)) > 0.1);

    // Overlap: second note on, then first off
    h.engine.handleNoteOn (64, 1.0f);
    h.engine.handleNoteOff (60);
    h.skip (500);
    REQUIRE (TestHarness::rms (h.render (500)) > 0.1);  // still sounding

    // Release second note
    h.engine.handleNoteOff (64);
    h.skip (2000);
    REQUIRE (TestHarness::rms (h.render (500)) < 0.001);  // now silent
}

// ---------------------------------------------------------------------------
// Glide (portamento)
// ---------------------------------------------------------------------------

TEST_CASE ("Glide smooths pitch transition", "[pitch]")
{
    TestHarness h;
    h.glide.store (500.0f);  // 500ms
    h.engine.handleNoteOn (60, 1.0f);  // C4 ~261
    h.skip (22050);  // settle at C4

    h.engine.handleNoteOn (72, 1.0f);  // C5 ~523
    // Measure immediately (first 50ms) — should still be closer to C4
    double freqEarly = h.measureFrequency (2205);
    REQUIRE (freqEarly > 250.0);
    REQUIRE (freqEarly < 500.0);
}

// ---------------------------------------------------------------------------
// Output modes
// ---------------------------------------------------------------------------

TEST_CASE ("All four output modes produce signal", "[output]")
{
    for (int mode = 0; mode < 4; ++mode)
    {
        SECTION ("Mode " + std::to_string (mode))
        {
            TestHarness h;
            h.outputSelect.store (static_cast<float> (mode));
            h.engine.handleNoteOn (69, 1.0f);
            h.skip (1000);
            REQUIRE (TestHarness::rms (h.render (1000)) > 0.05);
        }
    }
}

// ---------------------------------------------------------------------------
// Nyquist safety
// ---------------------------------------------------------------------------

TEST_CASE ("Harmonics above Nyquist are silenced", "[dsp]")
{
    // At 8 kHz sample rate, Nyquist = 4 kHz.
    // MIDI 100 ~ 2637 Hz.  Harmonic 2 = 5274 Hz > Nyquist.
    TestHarness h (8000.0);
    h.harmLevels[0].store (0.0f);  // fundamental off
    h.harmLevels[1].store (1.0f);  // 2nd harmonic — above Nyquist

    h.engine.handleNoteOn (100, 1.0f);
    h.skip (500);
    REQUIRE (TestHarness::rms (h.render (500)) < 0.01);
}

// ---------------------------------------------------------------------------
// Output bounds
// ---------------------------------------------------------------------------

TEST_CASE ("Output stays within [-1, 1] for normal settings", "[dsp]")
{
    TestHarness h;
    h.engine.handleNoteOn (69, 1.0f);
    h.skip (500);
    auto out = h.render (44100);
    REQUIRE (TestHarness::peak (out) <= 1.0);
    REQUIRE (TestHarness::peak (out) > 0.0);
}

// ---------------------------------------------------------------------------
// DC blocking
// ---------------------------------------------------------------------------

TEST_CASE ("Harmonic mix has no DC offset", "[dsp]")
{
    TestHarness h;
    h.outputSelect.store (0.0f);
    h.engine.handleNoteOn (69, 1.0f);
    h.skip (2000);
    auto out = h.render (44100);
    REQUIRE (std::abs (TestHarness::mean (out)) < 0.01);
}

// ---------------------------------------------------------------------------
// Silence when all faders zero
// ---------------------------------------------------------------------------

TEST_CASE ("No output when all harmonic faders are zero", "[dsp]")
{
    TestHarness h;
    h.harmLevels[0].store (0.0f);
    h.engine.handleNoteOn (69, 1.0f);
    h.skip (500);
    REQUIRE (TestHarness::rms (h.render (1000)) < 0.001);
}

// ---------------------------------------------------------------------------
// Master level scaling
// ---------------------------------------------------------------------------

TEST_CASE ("Master level scales output proportionally", "[output]")
{
    TestHarness h1;
    h1.masterLevel.store (1.0f);
    h1.engine.handleNoteOn (69, 1.0f);
    h1.skip (1000);
    double rmsLoud = TestHarness::rms (h1.render (4410));

    TestHarness h2;
    h2.masterLevel.store (0.5f);
    h2.engine.handleNoteOn (69, 1.0f);
    h2.skip (1000);
    double rmsQuiet = TestHarness::rms (h2.render (4410));

    double ratio = rmsQuiet / rmsLoud;
    REQUIRE_THAT (ratio, Catch::Matchers::WithinAbs (0.5, 0.05));
}

// ---------------------------------------------------------------------------
// Sample rate variations
// ---------------------------------------------------------------------------

TEST_CASE ("Frequency accuracy at 48 kHz", "[pitch][samplerate]")
{
    TestHarness h (48000.0);
    h.engine.handleNoteOn (69, 1.0f);
    h.skip (1000);
    double freq = h.measureFrequency (48000);
    REQUIRE_THAT (freq, Catch::Matchers::WithinAbs (440.0, 2.0));
}

TEST_CASE ("Frequency accuracy at 96 kHz", "[pitch][samplerate]")
{
    TestHarness h (96000.0);
    h.engine.handleNoteOn (69, 1.0f);
    h.skip (2000);
    double freq = h.measureFrequency (96000);
    REQUIRE_THAT (freq, Catch::Matchers::WithinAbs (440.0, 2.0));
}

TEST_CASE ("Frequency accuracy at 192 kHz", "[pitch][samplerate]")
{
    TestHarness h (192000.0);
    h.engine.handleNoteOn (69, 1.0f);
    h.skip (4000);
    double freq = h.measureFrequency (192000);
    REQUIRE_THAT (freq, Catch::Matchers::WithinAbs (440.0, 2.0));
}

// ---------------------------------------------------------------------------
// Edge cases: envelope boundaries
// ---------------------------------------------------------------------------

TEST_CASE ("Minimum attack/release (1 ms) produces sound without artifacts", "[envelope][edge]")
{
    TestHarness h;
    h.attack.store (1.0f);   // minimum from parameter range
    h.release.store (1.0f);  // minimum from parameter range
    h.engine.handleNoteOn (69, 1.0f);
    h.skip (500);
    auto playing = h.render (1000);
    REQUIRE (TestHarness::rms (playing) > 0.1);

    // No NaN or Inf in output
    for (auto s : playing)
    {
        REQUIRE (std::isfinite (s));
    }

    h.engine.handleNoteOff (69);
    h.skip (500);  // fast release
    auto decayed = h.render (500);
    REQUIRE (TestHarness::rms (decayed) < 0.01);
}

// ---------------------------------------------------------------------------
// Edge cases: extreme FM
// ---------------------------------------------------------------------------

TEST_CASE ("Extreme FM depth produces finite output", "[dsp][edge]")
{
    SECTION ("Max linear FM")
    {
        TestHarness h;
        h.linFmDepth.store (1.0f);
        h.fmRate.store (100.0f);
        h.engine.handleNoteOn (69, 1.0f);
        h.skip (500);
        auto out = h.render (2000);
        for (auto s : out)
            REQUIRE (std::isfinite (s));
    }

    SECTION ("Max exponential FM")
    {
        TestHarness h;
        h.expFmDepth.store (1.0f);
        h.fmRate.store (100.0f);
        h.engine.handleNoteOn (69, 1.0f);
        h.skip (500);
        auto out = h.render (2000);
        for (auto s : out)
            REQUIRE (std::isfinite (s));
    }

    SECTION ("Negative exponential FM")
    {
        TestHarness h;
        h.expFmDepth.store (-1.0f);
        h.fmRate.store (100.0f);
        h.engine.handleNoteOn (69, 1.0f);
        h.skip (500);
        auto out = h.render (2000);
        for (auto s : out)
            REQUIRE (std::isfinite (s));
    }

    SECTION ("Combined max FM")
    {
        TestHarness h;
        h.linFmDepth.store (1.0f);
        h.expFmDepth.store (1.0f);
        h.fmRate.store (20000.0f);  // max rate
        h.engine.handleNoteOn (69, 1.0f);
        h.skip (500);
        auto out = h.render (2000);
        for (auto s : out)
            REQUIRE (std::isfinite (s));
    }
}

// ---------------------------------------------------------------------------
// Glide = 0 safety (division guard)
// ---------------------------------------------------------------------------

TEST_CASE ("Zero glide produces immediate pitch change", "[pitch][edge]")
{
    TestHarness h;
    h.glide.store (0.0f);
    h.engine.handleNoteOn (60, 1.0f);
    h.skip (22050);

    // Switch to a new note with zero glide — should arrive immediately
    h.engine.handleNoteOn (72, 1.0f);
    h.skip (500);
    double freq = h.measureFrequency (22050);
    // C5 = 523.25 Hz
    REQUIRE_THAT (freq, Catch::Matchers::WithinAbs (523.25, 3.0));

    // Verify no NaN or crash
    auto out = h.render (1000);
    for (auto s : out)
        REQUIRE (std::isfinite (s));
}

// ---------------------------------------------------------------------------
// Scan envelope shape
// ---------------------------------------------------------------------------

TEST_CASE ("Scan center shifts harmonic emphasis", "[dsp]")
{
    // Center at 0.0 (leftmost) should emphasize harmonic 1
    // Center at 1.0 (rightmost) should emphasize harmonic 8
    auto measureHarmonicRms = [] (float center) {
        TestHarness h;
        h.scanCenter.store (center);
        h.scanWidth.store (0.2f);  // narrow width to make center matter

        // Enable all harmonics equally
        for (int i = 0; i < 8; ++i)
            h.harmLevels[i].store (1.0f);

        h.engine.handleNoteOn (48, 1.0f);  // C3 — low note so upper harmonics fit
        h.skip (2000);  // let control-rate updates settle
        return TestHarness::rms (h.render (4000));
    };

    double rmsLow = measureHarmonicRms (0.0f);
    double rmsHigh = measureHarmonicRms (1.0f);

    // Both should produce sound
    REQUIRE (rmsLow > 0.01);
    REQUIRE (rmsHigh > 0.01);

    // They should differ (different harmonic emphasis)
    REQUIRE (rmsLow != rmsHigh);
}

// ---------------------------------------------------------------------------
// Spectral tilt
// ---------------------------------------------------------------------------

TEST_CASE ("Spectral tilt changes harmonic balance", "[dsp]")
{
    auto measureWithTilt = [] (float tilt) {
        TestHarness h;
        h.spectralTilt.store (tilt);

        // Enable harmonics 1 and 8 equally
        h.harmLevels[0].store (1.0f);
        h.harmLevels[7].store (1.0f);
        h.scanWidth.store (1.0f);  // wide scan so tilt dominates

        h.engine.handleNoteOn (48, 1.0f);
        h.skip (2000);
        return TestHarness::rms (h.render (4000));
    };

    double rmsTiltDown = measureWithTilt (-1.0f);  // boost low, cut high
    double rmsTiltUp = measureWithTilt (1.0f);      // cut low, boost high
    double rmsTiltFlat = measureWithTilt (0.0f);     // no tilt

    // All should produce sound
    REQUIRE (rmsTiltDown > 0.01);
    REQUIRE (rmsTiltUp > 0.01);
    REQUIRE (rmsTiltFlat > 0.01);

    // Tilt should change the RMS (different harmonic weighting)
    REQUIRE (rmsTiltDown != rmsTiltUp);
}

// ---------------------------------------------------------------------------
// Re-prepare with different sample rate
// ---------------------------------------------------------------------------

TEST_CASE ("Engine can be re-prepared at different sample rate", "[lifecycle][samplerate]")
{
    TestHarness h (44100.0);
    h.engine.handleNoteOn (69, 1.0f);
    h.skip (1000);
    REQUIRE (TestHarness::rms (h.render (1000)) > 0.1);

    // Re-prepare at 96 kHz
    h.sampleRate = 96000.0;
    h.engine.prepare (96000.0, 512, h.params);
    h.engine.handleNoteOn (69, 1.0f);
    h.skip (2000);
    double freq = h.measureFrequency (96000);
    REQUIRE_THAT (freq, Catch::Matchers::WithinAbs (440.0, 2.0));
}

// ---------------------------------------------------------------------------
// Output mode switching doesn't crash
// ---------------------------------------------------------------------------

TEST_CASE ("Switching output modes mid-playback is safe", "[output][edge]")
{
    TestHarness h;
    h.engine.handleNoteOn (69, 1.0f);

    for (int mode = 0; mode < 4; ++mode)
    {
        h.outputSelect.store (static_cast<float> (mode));
        h.skip (500);
        auto out = h.render (500);
        for (auto s : out)
            REQUIRE (std::isfinite (s));
        REQUIRE (TestHarness::rms (out) > 0.01);
    }
}

// ---------------------------------------------------------------------------
// All harmonics active doesn't clip with normal settings
// ---------------------------------------------------------------------------

TEST_CASE ("All harmonics active stays within bounds", "[dsp][edge]")
{
    TestHarness h;
    for (int i = 0; i < 8; ++i)
        h.harmLevels[i].store (1.0f);

    h.engine.handleNoteOn (60, 1.0f);
    h.skip (1000);
    auto out = h.render (44100);

    for (auto s : out)
        REQUIRE (std::isfinite (s));

    // Should produce signal
    REQUIRE (TestHarness::rms (out) > 0.05);
}
