# Phase 3: Modulation and Performance - Research

**Researched:** 2026-03-10
**Domain:** FM synthesis, AR envelope, legato/glide, tuning, parameter smoothing -- all translating gen~ and Max patcher logic to C++
**Confidence:** HIGH

## Summary

Phase 3 adds everything that gives the Verbos its musical character beyond raw oscillation. The work divides into three clear domains: (1) FM synthesis with an internal sine LFO applying both linear and exponential modulation, (2) the AR envelope with monophonic legato behavior and portamento/glide, and (3) tuning controls plus comprehensive parameter smoothing. All three domains have definitive reference implementations -- the FM and AR envelope are directly in the gen~ codebox (sections 1 and 7), while glide and gate counting are Max patcher-level objects (`slide~` and `accum`) that must be re-implemented in C++.

The current Phase 2 codebase already has the correct APVTS parameter IDs registered for every Phase 3 parameter (`coarse_tune`, `fine_tune`, `glide`, `lin_fm_depth`, `exp_fm_depth`, `fm_rate`, `attack`, `release`). The HarmonicEngine class has cached pointers for Phase 2 params but not yet for Phase 3 params. The existing 5ms anti-click gate ramp must be replaced with the full AR envelope. The existing `handleNoteOn`/`handleNoteOff` must be replaced with proper gate counting (legato) and the frequency must be routed through a glide processor before reaching the phase accumulators.

**Primary recommendation:** Implement Phase 3 in two logical chunks: (1) FM + tuning + AR envelope (modifies renderSample and frequency calculation), then (2) legato gate counting + portamento + parameter smoothing (modifies note handling and all parameter reads). Both chunks modify HarmonicEngine; no new files are needed.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| FM-01 | Linear FM with internal sine LFO | gen~ section 1: `lin_fm_amount = lin_fm * freq * fm_lfo`, additive to freq. LFO is independent sine osc with `fm_phase` History |
| FM-02 | Exponential FM with internal sine LFO | gen~ section 1: `exp_fm_amount = pow(2, exp_fm * fm_lfo)`, multiplicative on freq. Same LFO drives both |
| FM-03 | FM LFO rate control (0.1Hz to audio rate ~20kHz) | gen~ param `fm_rate_internal`, APVTS param `fm_rate` already registered with skewed range 0.1-20000Hz |
| PERF-01 | Coarse tuning (+/-24 semitones) | gen~ section 1: `tune_ratio = pow(2, (coarse_tune * 100 + fine_tune) / 1200)`, applied multiplicatively to base freq |
| PERF-02 | Fine tuning (+/-100 cents) | Same formula as PERF-01, combined with coarse into single ratio |
| PERF-03 | Portamento/glide (0-2000ms) | Max patcher: `slide~` with sample count = glide_ms / 1000 * sr. Exponential follower on MIDI note number (before mtof~) |
| PERF-04 | AR envelope (attack 1-2000ms, release 1-5000ms, exponential curves) | gen~ section 7: `attack_coeff = exp(-1.0 / (attack_ms * 0.001 * sr))`, exponential follower targeting velocity on gate-on, 0 on gate-off |
| PERF-05 | Monophonic with legato (gate counting, no retrigger) | Max patcher: velocity -> `if > 0 then 1 else -1` -> `accum` -> `> 0`. Gate is positive when any note held. Overlapping notes keep gate high. |
| PERF-08 | Parameter smoothing on all continuously-variable parameters | Use `juce::SmoothedValue` for all continuous params (scan, tilt, FM depth, FM rate, attack, release, glide). Phase 2 already smooths master_level and harm faders. |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| JUCE | 8.0.12 (already in project) | Plugin framework, APVTS, SmoothedValue | Already set up, no changes needed |
| C++ standard library | C++20 | `std::exp`, `std::pow`, `std::sin`, `std::clamp`, `std::max` | All math functions for gen~ translation |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `juce::SmoothedValue<float>` | (part of JUCE) | Parameter interpolation to prevent zipper noise | Every continuous parameter read in audio thread |
| `juce::MidiMessage::getMidiNoteInHertz()` | (part of JUCE) | MIDI note to frequency conversion | After glide processing, for the glided note |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Custom exponential envelope | `juce::ADSR` | JUCE ADSR uses linear segments by default, gen~ uses exponential coefficients -- must match gen~ exactly |
| Custom slide~ implementation | `juce::SmoothedValue` for glide | SmoothedValue uses linear interpolation by default, not the exponential behavior of slide~ -- must implement slide~ formula |

**No additional dependencies needed.** All Phase 3 features are custom DSP translated from gen~ and Max patcher objects.

## Architecture Patterns

### Recommended Changes to Existing Structure
```
src/
  PluginProcessor.h/.cpp    # No changes needed
  dsp/
    HarmonicEngine.h/.cpp   # ALL Phase 3 changes go here:
                             #   - Add FM LFO + tuning to renderSample()
                             #   - Replace gate ramp with AR envelope
                             #   - Add glide (slide~) processor
                             #   - Replace handleNoteOn/Off with legato gate counting
                             #   - Add SmoothedValue for remaining params
                             #   - Cache Phase 3 APVTS param pointers
```

### Pattern 1: FM Synthesis (gen~ Section 1 Translation)

**What:** The gen~ code computes FM before the phase accumulator. An internal sine LFO (independent phase accumulator) drives both linear and exponential FM. Linear FM adds a frequency offset proportional to carrier frequency. Exponential FM multiplies frequency by a power of 2. Both are applied, then frequency is clamped.

**Critical detail -- the gen~ order of operations:**
```cpp
// 1. Apply tuning to base frequency
double tuneRatio = std::pow(2.0, (coarseTune * 100.0 + fineTune) / 1200.0);
double freq = baseFreq * tuneRatio;

// 2. Compute FM LFO sample
double fmInc = fmRate * inverseSampleRate_;
double fmP = fmPhase_ + fmInc;
fmP -= std::floor(fmP);
fmPhase_ = fmP;
double fmLfo = std::sin(fmP * twoPi);

// 3. Apply linear FM (additive: Hz offset proportional to carrier)
double linFmAmount = linFmDepth * freq * fmLfo;

// 4. Apply exponential FM (multiplicative: pitch scaling)
double expFmAmount = std::pow(2.0, expFmDepth * fmLfo);

// 5. Combine
freq = freq * expFmAmount + linFmAmount;

// 6. Clamp to valid range
freq = std::clamp(freq, 0.01, nyquist * 0.98);
```

**Key insight:** Linear FM amount scales with carrier frequency (`lin_fm * freq * fm_lfo`). This means the modulation index stays perceptually consistent across the pitch range -- a standard FM synthesis convention. Exponential FM is pitch-symmetric because it operates in octave space (`pow(2, depth * lfo)`).

### Pattern 2: AR Envelope (gen~ Section 7 Translation)

**What:** Single-pole exponential envelope follower. Attack coefficient makes the envelope rise toward the velocity target; release coefficient makes it fall toward zero. The exponential shape gives musically natural curves -- fast initial response that slows asymptotically.

**Formula from gen~:**
```cpp
// In prepare() or when params change:
// attack_coeff = exp(-1.0 / (attack_ms * 0.001 * sampleRate))
// release_coeff = exp(-1.0 / (release_ms * 0.001 * sampleRate))

// Per-sample:
double target = gateOpen_ ? currentVelocity_ : 0.0;
double coeff = gateOpen_ ? attackCoeff_ : releaseCoeff_;
envLevel_ = target + coeff * (envLevel_ - target);
```

**This replaces the Phase 2 linear gate ramp.** The 5ms anti-click ramp (`gateLevel_`, `gateAttackCoeff_`, `gateReleaseCoeff_`) is removed entirely. The AR envelope handles both the musical shaping and click prevention (minimum 1ms attack/release).

### Pattern 3: Gate Counting for Legato (Max Patcher Translation)

**What:** The Max patch implements monophonic legato via gate counting at the patcher level (before gen~). MIDI velocity > 0 adds +1 to accumulator, velocity == 0 adds -1. Gate is 1 when accumulator > 0. This means overlapping notes keep the gate high -- no retrigger.

**C++ translation:**
```cpp
void HarmonicEngine::handleNoteOn(int midiNote, float velocity)
{
    gateCount_++;
    targetNote_ = midiNote;  // Always update target note for glide
    currentVelocity_ = static_cast<double>(velocity);
    gateOpen_ = (gateCount_ > 0);
    // Note: do NOT reset envelope -- legato means envelope continues
}

void HarmonicEngine::handleNoteOff(int midiNote)
{
    gateCount_ = std::max(0, gateCount_ - 1);  // Clamp to prevent negative
    gateOpen_ = (gateCount_ > 0);
    // Note: do NOT change targetNote_ -- sustaining notes keep their pitch
}
```

**Important subtlety:** In the Max patch, `stripnote` filters out note-offs before the note number reaches the glide processor. This means only note-on messages update the target pitch. Note-offs only affect the gate count. The C++ must replicate this: `handleNoteOff` should NOT change `targetNote_`.

### Pattern 4: Portamento / Glide (Max `slide~` Translation)

**What:** The Max patch applies `slide~` (exponential follower) to the MIDI note number in the signal domain, BEFORE `mtof~` converts to frequency. The slide operates on note numbers, not frequencies. This is important because gliding in note-number space produces perceptually linear pitch change (equal intervals per unit time), while gliding in frequency space would be logarithmic.

**`slide~` behavior:** `slide~ up down` where up/down are the slide coefficients in samples. The formula:
```
y[n] = y[n-1] + (x[n] - y[n-1]) / coefficient
```
When coefficient = 1, output = input (no slide). When coefficient > 1, output follows input with exponential smoothing. Coefficient is computed from glide time: `samples = glide_ms / 1000.0 * sampleRate`.

**C++ translation:**
```cpp
// In prepare():
// No fixed coefficient -- computed from parameter each sample (or smoothed)

// Per-sample in renderSample():
double glideSamples = glideTimeMs * 0.001 * sampleRate_;
double slideCoeff = std::max(1.0, glideSamples);  // Minimum 1 = no slide

// Exponential slide on note number
glidedNote_ += (targetNote_ - glidedNote_) / slideCoeff;

// Convert glided note to frequency, then apply tuning
double baseFreq = 440.0 * std::pow(2.0, (glidedNote_ - 69.0) / 12.0);
```

**Key insight:** The glide happens on MIDI note number (before mtof), not on frequency. This must be replicated exactly. Using `juce::MidiMessage::getMidiNoteInHertz()` after the glide is equivalent to `mtof~` in the patch.

### Pattern 5: Parameter Smoothing Strategy

**What:** PERF-08 requires smoothing on ALL continuously-variable parameters. Phase 2 already smooths master_level and the 8 harmonic faders. Phase 3 must add smoothing for the remaining parameters.

**Parameters needing SmoothedValue addition in Phase 3:**
| Parameter | ID | Smoothing Time | Reason |
|-----------|----|---------------|--------|
| Scan Center | `scan_center` | 64 samples | Modulates harmonic amplitudes |
| Scan Width | `scan_width` | 64 samples | Modulates harmonic amplitudes |
| Spectral Tilt | `spectral_tilt` | 64 samples | Modulates harmonic amplitudes |
| Lin FM Depth | `lin_fm_depth` | 64 samples | Modulates frequency |
| Exp FM Depth | `exp_fm_depth` | 64 samples | Modulates frequency |
| FM Rate | `fm_rate` | 64 samples | Modulates LFO speed |
| Attack | `attack` | 64 samples | Envelope coefficient |
| Release | `release` | 64 samples | Envelope coefficient |

**Parameters that do NOT need SmoothedValue:**
| Parameter | ID | Reason |
|-----------|----|--------|
| Coarse Tune | `coarse_tune` | Integer (stepped), no interpolation needed |
| Output Select | `output_select` | Discrete choice, hard switch by design |
| Glide Time | `glide` | The glide processor itself provides smoothing on the pitch; the time parameter changes infrequently |

**Smoothing ramp length:** Phase 2 established 64 samples as the ramp length. Continue this convention for consistency.

### Pattern 6: DC Blocker Placement Change

**Critical difference from Phase 2:** In the gen~ code, the DC blocker is applied AFTER the envelope:
```
raw_mix = harm_mix * env * master_level;
mix_dc = raw_mix - dc_x1 + dc_R * dc_y1;
```

The current Phase 2 code applies the DC blocker before the envelope. Phase 3 must reorder to match gen~: apply envelope and master level first, then DC block, for the harmonic mix output only. Other outputs (tri, saw, sqr) get envelope and master level but no DC blocker -- matching the gen~ code.

### Anti-Patterns to Avoid

- **Gliding in frequency space instead of note-number space:** The Max patch glides on MIDI note numbers (before mtof~). Gliding in Hz space produces non-uniform pitch intervals. Must glide on note numbers.
- **Using `juce::ADSR` for the envelope:** JUCE's ADSR uses linear segments. The gen~ envelope is a single-pole exponential follower -- different math, different sound. Translate the gen~ formula directly.
- **Resetting envelope on legato note transitions:** The whole point of gate counting is that overlapping notes do NOT retrigger the envelope. The gate stays high, the envelope continues at its current level, and only the target pitch changes.
- **Recomputing envelope coefficients per-sample from raw params:** The `std::exp(-1.0 / ...)` computation is relatively expensive. Compute coefficients only when the smoothed parameter value changes, or cache and update per-block.
- **Forgetting to clamp gate count:** The `accum` in Max can receive unbalanced +1/-1 messages (e.g., note-on without matching note-off due to MIDI quirks). Clamp `gateCount_` to a minimum of 0 to prevent it going negative.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Parameter smoothing | Custom interpolation | `juce::SmoothedValue<float>` | Thread-safe, tested, already used in Phase 2 |
| MIDI note to Hz | Lookup table or formula | `juce::MidiMessage::getMidiNoteInHertz()` | Standard tuning, handles fractional notes |
| Denormal prevention | Per-sample checks | `juce::ScopedNoDenormals` | Already in processBlock from Phase 2 |
| Exponential envelope | ADSR class or linear ramp | Direct gen~ formula translation | gen~ uses single-pole IIR, must match |

**Key insight:** The only "don't hand-roll" that's unusual here is the envelope. Most synth tutorials use `juce::ADSR` or linear ramps, but the gen~ code uses a fundamentally different topology (exponential follower), so it MUST be hand-rolled to match the reference. Everything else uses JUCE utilities.

## Common Pitfalls

### Pitfall 1: PolyBLEP Breaks with Negative Phase Increment from FM
**What goes wrong:** At high linear FM depths, `freq` can go negative momentarily (through-zero FM), making `phase_inc` negative. The PolyBLEP correction assumes positive `dt`.
**Why it happens:** Linear FM adds `lin_fm * freq * lfo` to frequency. When lfo is -1 and depth is high, result can be negative.
**How to avoid:** The gen~ code clamps: `freq = clamp(freq, 0.01, nyquist * 0.98)`. Apply this clamp AFTER combining FM but BEFORE the phase accumulator. This prevents negative frequencies entirely.
**Warning signs:** Harsh distortion or silence at high linear FM depths.

### Pitfall 2: Envelope Coefficient Blows Up at Very Short Times
**What goes wrong:** `exp(-1.0 / (attack_ms * 0.001 * sr))` approaches 1.0 as time approaches 0, and the formula breaks down at extremely small values.
**Why it happens:** Division by near-zero.
**How to avoid:** The gen~ code uses `max(attack_ms, 1)` to clamp minimum time to 1ms. The APVTS parameter range already starts at 1ms, but apply `std::max(1.0, ...)` as a safety net.
**Warning signs:** Clicks at minimum attack time, or envelope never reaching target.

### Pitfall 3: Glide Coefficient of Zero Causes Division by Zero
**What goes wrong:** When glide time is 0ms, `slideCoeff` becomes 0, causing division by zero in the slide formula.
**Why it happens:** `glide_ms / 1000 * sr = 0` when glide_ms = 0.
**How to avoid:** Clamp slide coefficient to minimum 1.0: `slideCoeff = std::max(1.0, glideSamples)`. When coefficient is 1, `y += (x - y) / 1 = x - y`, so output instantly equals input -- which is correct behavior for zero glide time.
**Warning signs:** NaN or inf in output, silence, or noise burst when glide is set to 0.

### Pitfall 4: Glide Initializes to Wrong Note
**What goes wrong:** On first note-on, glide slides from some default (e.g., A4/69) to the target note, producing an audible sweep.
**Why it happens:** `glidedNote_` initialized to a default value, not the first played note.
**How to avoid:** On the very first note-on (when no previous note exists), set `glidedNote_` directly to the target note instead of gliding. Track whether a note has been played with a boolean flag, or initialize `glidedNote_` to -1 and check in renderSample.
**Warning signs:** Audible pitch sweep on the first note after plugin load.

### Pitfall 5: Tuning Applied in Wrong Place Causes FM Modulation of Tuning
**What goes wrong:** If tuning ratio is applied after FM, the FM modulation depth is unaffected by tuning. If applied before FM, linear FM depth scales with the tuned frequency (correct behavior per gen~).
**Why it happens:** Order of operations matters.
**How to avoid:** Follow gen~ exactly: `freq = base_freq * tune_ratio` FIRST, then apply FM. Linear FM uses the tuned freq as its reference: `lin_fm * freq * lfo`.
**Warning signs:** FM depth changes when tuning is adjusted (if order is wrong).

### Pitfall 6: SmoothedValue Not Reset on prepareToPlay
**What goes wrong:** After sample rate change, SmoothedValue ramp lengths are wrong, causing either too-slow or too-fast smoothing.
**Why it happens:** SmoothedValue stores ramp length in seconds internally but recalculation depends on `reset()` being called with new sample rate.
**How to avoid:** Call `reset(sampleRate, rampLengthInSeconds)` for every SmoothedValue in `prepare()`. Then call `setCurrentAndTargetValue()` to prevent initial ramp from old value.
**Warning signs:** Audible zipper noise after sample rate changes, or parameters "stuck" at old values.

## Code Examples

### Complete FM + Tuning + Envelope Integration (renderSample modification)

```cpp
// Source: gen~ codebox sections 1, 7, and Max patcher glide
double HarmonicEngine::renderSample()
{
    // --- Glide (slide~): exponential follower on note number ---
    double glideSamples = std::max(1.0, glideSmoothed_.getNextValue() * 0.001 * sampleRate_);
    glidedNote_ += (static_cast<double>(targetNote_) - glidedNote_) / glideSamples;

    // --- Base frequency from glided note ---
    double baseFreq = juce::MidiMessage::getMidiNoteInHertz(static_cast<int>(std::round(glidedNote_)));
    // For sub-semitone accuracy with glide:
    baseFreq = 440.0 * std::pow(2.0, (glidedNote_ - 69.0) / 12.0);

    // --- Tuning (gen~ section 1) ---
    double coarseTune = static_cast<double>(coarseTuneParam_->load());
    double fineTune = static_cast<double>(fineTuneSmoothed_.getNextValue());
    double tuneRatio = std::pow(2.0, (coarseTune * 100.0 + fineTune) / 1200.0);
    double freq = baseFreq * tuneRatio;

    // --- FM LFO (gen~ section 1) ---
    double fmRate = static_cast<double>(fmRateSmoothed_.getNextValue());
    double fmInc = fmRate * inverseSampleRate_;
    double fmP = fmPhase_ + fmInc;
    fmP -= std::floor(fmP);
    fmPhase_ = fmP;
    double fmLfo = std::sin(fmP * juce::MathConstants<double>::twoPi);

    // --- Linear FM (additive) ---
    double linFmDepth = static_cast<double>(linFmSmoothed_.getNextValue());
    double linFmAmount = linFmDepth * freq * fmLfo;

    // --- Exponential FM (multiplicative) ---
    double expFmDepth = static_cast<double>(expFmSmoothed_.getNextValue());
    double expFmAmount = std::pow(2.0, expFmDepth * fmLfo);

    // --- Combine FM and clamp ---
    freq = freq * expFmAmount + linFmAmount;
    double nyquist = sampleRate_ * 0.5;
    freq = std::clamp(freq, 0.01, nyquist * 0.98);

    // --- Store for use by phase accumulator and harmonic bank ---
    currentFreq_ = freq;

    // ... (sections 2-6 unchanged from Phase 2) ...

    // --- AR Envelope (gen~ section 7, replaces gate ramp) ---
    double attackMs = static_cast<double>(attackSmoothed_.getNextValue());
    double releaseMs = static_cast<double>(releaseSmoothed_.getNextValue());
    double attackCoeff = std::exp(-1.0 / (std::max(1.0, attackMs) * 0.001 * sampleRate_));
    double releaseCoeff = std::exp(-1.0 / (std::max(1.0, releaseMs) * 0.001 * sampleRate_));

    double envTarget = gateOpen_ ? currentVelocity_ : 0.0;
    double envCoeff = gateOpen_ ? attackCoeff : releaseCoeff;
    envLevel_ = envTarget + envCoeff * (envLevel_ - envTarget);

    // --- Output (gen~ section 9, corrected order) ---
    double masterLvl = static_cast<double>(masterLevelSmoothed_.getNextValue());

    // Apply envelope and master level to all outputs
    double rawMix = harmonicMix * envLevel_ * masterLvl;
    double rawTri = triOut * envLevel_ * masterLvl;
    double rawSaw = sawOut * envLevel_ * masterLvl;
    double rawSqr = sqrOut * envLevel_ * masterLvl;

    // DC blocker on mix only (after envelope, matching gen~)
    double mixDC = rawMix - dcX1_ + dcR_ * dcY1_;
    dcX1_ = rawMix;
    dcY1_ = mixDC;

    int outputMode = static_cast<int>(*outputSelectParam_);
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
```

### Gate Counting for Legato

```cpp
// Source: Max patcher signal flow: notein -> stripnote -> gate counting
void HarmonicEngine::handleNoteOn(int midiNote, float velocity)
{
    gateCount_++;
    gateOpen_ = true;
    targetNote_ = midiNote;
    currentVelocity_ = static_cast<double>(velocity);

    // First note: set glide position immediately (no slide from default)
    if (!hasPlayedNote_)
    {
        glidedNote_ = static_cast<double>(midiNote);
        hasPlayedNote_ = true;
    }
    // Subsequent notes: glidedNote_ will slide toward targetNote_ in renderSample
}

void HarmonicEngine::handleNoteOff(int /*midiNote*/)
{
    gateCount_ = std::max(0, gateCount_ - 1);
    gateOpen_ = (gateCount_ > 0);
    // Do NOT update targetNote_ -- matches stripnote behavior
}
```

### New Member Variables Needed

```cpp
// Phase 3 additions to HarmonicEngine.h:

// FM LFO state
double fmPhase_ = 0.0;

// AR envelope state (replaces gateLevel_, gateAttackCoeff_, gateReleaseCoeff_)
double envLevel_ = 0.0;

// Legato gate counting (replaces simple gateOpen_)
int gateCount_ = 0;
// gateOpen_ stays, but is now derived from gateCount_

// Glide state
int targetNote_ = 69;       // MIDI note number (A4)
double glidedNote_ = 69.0;  // Smoothed note for portamento
bool hasPlayedNote_ = false; // Prevents glide on first note

// SmoothedValue for Phase 3 params
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

// Cached APVTS parameter pointers (Phase 3)
std::atomic<float>* coarseTuneParam_ = nullptr;
std::atomic<float>* fineTuneParam_ = nullptr;
std::atomic<float>* glideParam_ = nullptr;
std::atomic<float>* linFmDepthParam_ = nullptr;
std::atomic<float>* expFmDepthParam_ = nullptr;
std::atomic<float>* fmRateParam_ = nullptr;
std::atomic<float>* attackParam_ = nullptr;
std::atomic<float>* releaseParam_ = nullptr;
```

### prepare() Additions

```cpp
// In HarmonicEngine::prepare(), add after existing code:

// Cache Phase 3 APVTS parameter pointers
coarseTuneParam_ = apvts.getRawParameterValue("coarse_tune");
fineTuneParam_ = apvts.getRawParameterValue("fine_tune");
glideParam_ = apvts.getRawParameterValue("glide");
linFmDepthParam_ = apvts.getRawParameterValue("lin_fm_depth");
expFmDepthParam_ = apvts.getRawParameterValue("exp_fm_depth");
fmRateParam_ = apvts.getRawParameterValue("fm_rate");
attackParam_ = apvts.getRawParameterValue("attack");
releaseParam_ = apvts.getRawParameterValue("release");

// Initialize SmoothedValues (64 samples = ~1.5ms at 44.1kHz)
double smoothTime = 64.0 / sampleRate;
scanCenterSmoothed_.reset(sampleRate, smoothTime);
scanWidthSmoothed_.reset(sampleRate, smoothTime);
spectralTiltSmoothed_.reset(sampleRate, smoothTime);
linFmSmoothed_.reset(sampleRate, smoothTime);
expFmSmoothed_.reset(sampleRate, smoothTime);
fmRateSmoothed_.reset(sampleRate, smoothTime);
attackSmoothed_.reset(sampleRate, smoothTime);
releaseSmoothed_.reset(sampleRate, smoothTime);
glideSmoothed_.reset(sampleRate, smoothTime);
fineTuneSmoothed_.reset(sampleRate, smoothTime);

// Set current values to prevent initial ramp
scanCenterSmoothed_.setCurrentAndTargetValue(*scanCenterParam_);
scanWidthSmoothed_.setCurrentAndTargetValue(*scanWidthParam_);
spectralTiltSmoothed_.setCurrentAndTargetValue(*spectralTiltParam_);
linFmSmoothed_.setCurrentAndTargetValue(*linFmDepthParam_);
expFmSmoothed_.setCurrentAndTargetValue(*expFmDepthParam_);
fmRateSmoothed_.setCurrentAndTargetValue(*fmRateParam_);
attackSmoothed_.setCurrentAndTargetValue(*attackParam_);
releaseSmoothed_.setCurrentAndTargetValue(*releaseParam_);
glideSmoothed_.setCurrentAndTargetValue(*glideParam_);
fineTuneSmoothed_.setCurrentAndTargetValue(*fineTuneParam_);

// Reset Phase 3 state
fmPhase_ = 0.0;
envLevel_ = 0.0;
gateCount_ = 0;
targetNote_ = 69;
glidedNote_ = 69.0;
hasPlayedNote_ = false;
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `juce::ADSR` (linear segments) | Exponential single-pole follower | N/A -- gen~ design | Must use gen~ formula, not JUCE ADSR |
| SmoothedValue for glide | Custom slide~ translation | N/A -- Max design | slide~ is exponential, SmoothedValue is linear |
| currentFreq_ set directly in handleNoteOn | Glide processor in renderSample | Phase 3 change | Frequency no longer set in handleNoteOn |

**Deprecated from Phase 2 (to be removed):**
- `gateLevel_`, `gateAttackCoeff_`, `gateReleaseCoeff_` -- replaced by AR envelope
- Direct `currentFreq_` assignment in `handleNoteOn` -- replaced by glide processor

## Open Questions

1. **Envelope coefficient computation cost per-sample**
   - What we know: `std::exp(-1.0 / x)` is called twice per sample (attack and release coefficients). Only one is used per sample, but both are computed.
   - What's unclear: Whether this is a measurable CPU cost.
   - Recommendation: Compute both per-sample initially (matches gen~ behavior exactly). If profiling shows it matters, cache coefficients and only recompute when the smoothed parameter value changes. The SmoothedValue `isSmoothing()` method can gate recomputation.

2. **DC blocker order change from Phase 2**
   - What we know: Phase 2 applies DC blocker before envelope. gen~ applies it after.
   - What's unclear: Whether this produces audible differences for the harmonic mix.
   - Recommendation: Change to match gen~ (after envelope). This is the correct translation. The DC blocker state variables (`dcX1_`, `dcY1_`) should be reset in prepare() as before.

3. **`slide~` exact coefficient interpretation**
   - What we know: Max's `slide~` formula is `y = y + (x - y) / coeff` where coeff is in samples. When coeff = 0, the `slide~ 0 0` object initializes to 0 and the coefficient comes from inlets 2 and 3.
   - Recommendation: Use the standard formula. The glide dial already converts ms to samples via `ms / 1000 * sr`. Clamp coefficient minimum to 1.0 (instant follow).

## Sources

### Primary (HIGH confidence)
- gen~ codebox source: `/Users/matt/src/harmonic-oscillator-plugin/code/harmonic_osc_engine.genexpr` -- sections 1 (FM + tuning) and 7 (AR envelope) are the definitive reference
- Max patcher: `/Users/matt/src/harmonic-oscillator-plugin/HarmonicOscillator.maxpat` -- patchline analysis reveals gate counting (`accum`), glide (`slide~`), and signal flow
- Phase 2 source code: `src/dsp/HarmonicEngine.h/.cpp` -- existing architecture to extend
- Phase 2 APVTS layout: `src/PluginProcessor.cpp` -- all Phase 3 parameter IDs already registered

### Secondary (MEDIUM confidence)
- Max/MSP `slide~` documentation -- exponential follower formula `y = y + (x - y) / slide_samples`
- Max/MSP `accum` documentation -- accumulator with clamp behavior
- `juce::SmoothedValue` API -- stable since JUCE 5, ramp-based linear interpolation

### Tertiary (LOW confidence)
- CPU cost of per-sample `std::exp` and `std::pow` calls -- expected manageable for mono synth but not benchmarked
- Interaction of exponential FM at audio rate with PolyBLEP corrections -- the frequency clamp should prevent issues but extreme settings are untested

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- no new dependencies, extending existing HarmonicEngine
- Architecture: HIGH -- gen~ codebox and Max patcher provide exact specifications for all algorithms
- FM synthesis: HIGH -- gen~ section 1 is unambiguous, line-by-line translation
- AR envelope: HIGH -- gen~ section 7 is a standard exponential follower
- Legato/glide: HIGH -- Max patcher signal flow fully traced (accum for gate counting, slide~ for portamento)
- Parameter smoothing: HIGH -- SmoothedValue already used in Phase 2, extending to remaining params
- Pitfalls: HIGH -- known edge cases identified from gen~ analysis (frequency clamping, coefficient clamping, glide initialization)

**Research date:** 2026-03-10
**Valid until:** Indefinite (gen~ codebox and Max patcher are fixed references; JUCE API is stable)
