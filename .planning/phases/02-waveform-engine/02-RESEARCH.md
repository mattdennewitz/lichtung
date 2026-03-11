# Phase 2: Waveform Engine - Research

**Researched:** 2026-03-10
**Domain:** DSP engine -- additive synthesis, PolyBLEP waveforms, MIDI, gen~ to C++ translation
**Confidence:** HIGH

## Summary

Phase 2 translates the gen~ codebox DSP algorithms into C++ classes that integrate with the existing Phase 1 PluginProcessor/APVTS infrastructure. The gen~ source code has been fully extracted and analyzed. It reveals several critical implementation details that differ from the earlier architecture research assumptions -- most importantly, the harmonic bank uses **independent per-harmonic phase accumulators** (not shared-phase multiplication), the triangle wave uses a simple leaky integrator with a fixed leak coefficient of 0.999, and the DC blocker runs at ~5Hz (not ~40Hz as previously speculated).

The gen~ codebox is a monolithic sample-processing block with clear sections: frequency/FM calculation, master phase accumulator, PolyBLEP waveforms (saw/square/triangle), scan Gaussian envelope, spectral tilt, 8-harmonic sine bank, AR envelope, DC blocker, and output selection. The C++ translation maps these sections directly to class methods within a HarmonicEngine coordinator. All internal state (phase accumulators, integrators, DC blocker state) maps to `double` member variables. The output selector is a hard switch (`selector~ 4 1` in the patch) with no crossfade.

For Phase 2 specifically, scan and tilt are included because the gen~ codebox computes them per-sample as part of the harmonic bank -- they are inseparable from the harmonic mix output. FM and AR envelope are also in the gen~ code but per the roadmap, FM belongs to Phase 3. However, a minimal envelope (gate on/off) and basic MIDI handling are needed in Phase 2 to make the synth playable. The approach is: implement the full harmonic engine with scan/tilt, PolyBLEP waveforms, DC blocker, output selector, master level, and basic MIDI note-on/note-off with a simple gate (no AR shaping yet -- that comes in Phase 3).

**Primary recommendation:** Translate the gen~ codebox line-by-line into C++ within a HarmonicEngine class, using `double` for all internal DSP state. Implement per-harmonic phase accumulators exactly as the gen~ code does. Wire to PluginProcessor via APVTS parameter pointers cached in prepareToPlay.

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| DSP-01 | 8-harmonic sine bank with individual level controls (0-1 per harmonic) | gen~ codebox section 6: per-harmonic phase accumulators, `peek(harm_levels, 0, i)` for fader levels, `sin(h_phase * twopi) * amp` |
| DSP-02 | Scan envelope (Gaussian window) with center (0-1) and width (0-1) | gen~ codebox section 4: `center_pos = scan_center * 7.0`, `sigma = 0.3 + scan_width * 9.7`, Gaussian `exp(-0.5 * dist * dist)` |
| DSP-03 | Spectral tilt (linear-in-dB slope, +/-24dB range) | gen~ codebox section 5: `tilt_db = spectral_tilt * (norm_pos - 0.5) * 24.0`, converted via `pow(10, tilt_db / 20.0)` |
| DSP-04 | DC blocker on harmonic mix output (~5Hz highpass) | gen~ codebox section 8: `dc_R = 1.0 - (twopi * 5.0 * inv_sr)`, first-order highpass |
| DSP-05 | Anti-aliasing -- skip harmonics above Nyquist | gen~ codebox section 6: `if (harm_freq < nyquist)` guard, zeros phase for skipped harmonics |
| DSP-06 | Double-precision phase accumulator | gen~ uses 64-bit internally; C++ translation uses `double` for all phase/frequency state |
| WAVE-01 | PolyBLEP sawtooth waveform | gen~ codebox section 3: raw saw `2*p-1` with PolyBLEP correction at 0/1 boundary |
| WAVE-02 | PolyBLEP square waveform | gen~ codebox section 3: raw square `p<0.5?1:-1` with PolyBLEP at both 0 and 0.5 transitions |
| WAVE-03 | Triangle waveform via leaky integrated square | gen~ codebox section 3: `tri_raw = tri_integrator * 0.999 + sqr_out * phase_inc * 4.0` |
| WAVE-04 | Output selector -- hard switch between Harmonic Mix, Triangle, Saw, Square | gen~ out1-out4 to `selector~ 4 1` in patcher; C++ uses simple switch on `output_select` parameter |
| PERF-06 | MIDI note input with velocity sensitivity | gen~ inputs in1 (freq from mtof~), in2 (gate), in3 (velocity); C++ handles noteOn/noteOff in processBlock |
| PERF-07 | Master level control (0-1) | gen~ codebox section 9: `raw_mix = harm_mix * env * master_level` applied to all outputs |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| JUCE | 8.0.6+ (already in project) | Plugin framework, APVTS, audio I/O | Already set up in Phase 1 |
| C++ standard library | C++20 | `<cmath>`, `<array>`, `<algorithm>` | `std::sin`, `std::exp`, `std::pow`, `std::floor`, `std::clamp` |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `juce::SmoothedValue` | (part of JUCE) | Parameter interpolation | All continuous parameters read in audio thread |
| `juce::MidiBuffer` | (part of JUCE) | MIDI event iteration | Sample-accurate MIDI processing in processBlock |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Custom DSP classes | `juce::dsp::Oscillator` | JUCE oscillator does not support PolyBLEP or additive scan/tilt; must be custom |
| Custom DC blocker | `juce::dsp::IIR::Filter` | IIR filter works but the gen~ formula is a 5-line first-order highpass; simpler to translate directly |

**No additional dependencies needed.** All DSP is custom, translated from gen~ codebox.

## Architecture Patterns

### Recommended Project Structure
```
src/
  PluginProcessor.h/.cpp    # Existing -- add HarmonicEngine member, wire processBlock
  dsp/
    HarmonicEngine.h/.cpp   # Top-level DSP coordinator
    HarmonicBank.h/.cpp     # 8 sine oscillators + scan + tilt
    PolyBLEPOscillator.h/.cpp  # Saw, square, triangle
    DCBlocker.h/.cpp        # First-order DC blocking filter
```

### Pattern 1: gen~ to C++ Translation (Line-by-Line)

**What:** Each gen~ codebox section maps to a C++ method. gen~ `History` becomes `double` member variables. gen~ `Data` becomes `std::array<double, 8>`. gen~ `Param` reads become APVTS `getRawParameterValue()` pointer dereferences. gen~ `peek`/`poke` on Data become direct array reads/writes.

**When to use:** Always, for this project. The gen~ code IS the spec.

**gen~ to C++ mapping (verified against actual codebox):**

| gen~ Construct | C++ Equivalent | Notes |
|----------------|---------------|-------|
| `History x(0)` | `double x_ = 0.0;` member variable | Reset in `prepare()` |
| `Data arr(8)` | `std::array<double, 8> arr_ = {};` | Fixed-size, stack-allocated |
| `Buffer harm_levels("---harm_levels")` | `std::array<std::atomic<float>*, 8>` APVTS param pointers | Cached in `prepare()` |
| `Param foo(default)` | `std::atomic<float>* fooParam;` from APVTS | Cached pointer, read per-sample |
| `peek(data, idx)` | `data_[idx]` | Direct array access |
| `poke(data, val, idx)` | `data_[idx] = val` | Direct array write |
| `peek(buffer, 0, channel)` | `*harmLevelParams_[channel]` | APVTS atomic float read |
| `samplerate` | `sampleRate_` stored in `prepare()` | `double`, never hardcode |
| `twopi` | `juce::MathConstants<double>::twoPi` | Or `2.0 * M_PI` |
| `clamp(x, lo, hi)` | `std::clamp(x, lo, hi)` | C++17 |
| `in1, in2, in3` | Method parameters or member state | freq, gate, velocity |

**Example: Phase accumulator (from gen~ section 2):**
```cpp
// gen~ original:
//   phase_inc = freq * inv_sr;
//   p = phase + phase_inc;
//   p = p - floor(p);
//   phase = p;

// C++ translation:
double phaseInc = freq * inverseSampleRate_;
double p = phase_ + phaseInc;
p -= std::floor(p);
phase_ = p;
```

### Pattern 2: Per-Harmonic Phase Accumulators (CRITICAL DIFFERENCE)

**What:** The gen~ code maintains **independent phase accumulators per harmonic**, NOT `masterPhase * harmonicNumber`. Each harmonic accumulates `harmFreq * invSr` independently.

**Why this matters:** The earlier architecture research assumed `sin(2 * PI * harmonic * masterPhase)`. The gen~ code does NOT do this. It uses `Data harm_phases(8)` with independent phase tracking. While mathematically equivalent for a static fundamental, they diverge during FM modulation and frequency changes -- the per-harmonic approach produces smoother harmonic transitions during pitch changes.

**From gen~ codebox section 6:**
```cpp
// gen~ original:
//   h_phase = peek(harm_phases, i);
//   h_phase = h_phase + harm_freq * inv_sr;
//   h_phase = h_phase - floor(h_phase);
//   poke(harm_phases, h_phase, i);
//   harm_mix += sin(h_phase * twopi) * amp;

// C++ translation:
for (int i = 0; i < 8; ++i)
{
    int harmNum = i + 1;
    double harmFreq = freq * harmNum;

    if (harmFreq < nyquist)
    {
        double hPhase = harmPhases_[i];
        hPhase += harmFreq * inverseSampleRate_;
        hPhase -= std::floor(hPhase);
        harmPhases_[i] = hPhase;

        double fader = static_cast<double>(*harmLevelParams_[i]);
        double scanVal = scanEnv_[i];
        double tiltVal = tiltFactors_[i];
        double amp = fader * scanVal * tiltVal;

        harmonicMix += std::sin(hPhase * twoPi) * amp;
    }
    else
    {
        harmPhases_[i] = 0.0;  // Zero phase for skipped harmonics
    }
}
harmonicMix *= 0.25;  // Normalization factor from gen~
```

### Pattern 3: PolyBLEP Implementation (Exact gen~ Algorithm)

**What:** The gen~ codebox implements standard PolyBLEP with a specific correction polynomial. The triangle is a leaky integrator of the PolyBLEP square with fixed leak coefficient 0.999 (NOT frequency-dependent).

**Critical detail -- triangle integrator from gen~:**
```cpp
// gen~ original:
//   leak = 0.999;
//   tri_raw = tri_integrator * leak + sqr_out * phase_inc * 4.0;
//   tri_integrator = tri_raw;
//   tri_out = clamp(tri_raw, -1.0, 1.0);

// C++ translation:
double triRaw = triIntegrator_ * 0.999 + sqrOut * phaseInc * 4.0;
triIntegrator_ = triRaw;
double triOut = std::clamp(triRaw, -1.0, 1.0);
```

Note: The leak coefficient is a fixed 0.999, not frequency-dependent. The `clamp` at the end handles any DC drift or overshoot. This is simpler than what Pitfall 3 in PITFALLS.md warned about -- the gen~ code chose simplicity over theoretical correctness.

### Pattern 4: DC Blocker (5Hz, NOT 40Hz)

**What:** The gen~ code uses a 5Hz cutoff DC blocker, not the ~40Hz that `dcblock~` defaults to. This is explicitly coded as `dc_R = 1.0 - (twopi * 5.0 * inv_sr)`.

```cpp
// C++ translation:
// In prepare():
dcR_ = 1.0 - (juce::MathConstants<double>::twoPi * 5.0 * inverseSampleRate_);

// Per-sample:
double mixDC = rawMix - dcX1_ + dcR_ * dcY1_;
dcX1_ = rawMix;
dcY1_ = mixDC;
```

### Pattern 5: Scan Gaussian Envelope (Exact Formula)

**What:** Scan center maps 0..1 to harmonic positions 0..7. Width maps 0..1 to sigma 0.3..10. Standard Gaussian applied per harmonic.

```cpp
// gen~ original:
//   center_pos = scan_center * 7.0;
//   sigma = 0.3 + scan_width * 9.7;
//   dist = (i - center_pos) / sigma;
//   scan_val = exp(-0.5 * dist * dist);

// C++ translation (compute per-sample or when params change):
double centerPos = scanCenter * 7.0;
double sigma = 0.3 + scanWidth * 9.7;

for (int i = 0; i < 8; ++i)
{
    double dist = (static_cast<double>(i) - centerPos) / sigma;
    scanEnv_[i] = std::exp(-0.5 * dist * dist);
}
```

### Pattern 6: Spectral Tilt (Exact Formula)

**What:** Linear-in-dB slope centered at harmonic index midpoint. Range: +/-24dB per `spectral_tilt` parameter (-1..+1).

```cpp
// gen~ original:
//   norm_pos = i / 7.0;
//   tilt_db = spectral_tilt * (norm_pos - 0.5) * 24.0;
//   tilt_val = pow(10, tilt_db / 20.0);

// C++ translation:
for (int i = 0; i < 8; ++i)
{
    double normPos = static_cast<double>(i) / 7.0;
    double tiltDb = spectralTilt * (normPos - 0.5) * 24.0;
    tiltFactors_[i] = std::pow(10.0, tiltDb / 20.0);
}
```

### Pattern 7: MIDI Handling for Phase 2 (Minimal)

**What:** Phase 2 needs basic MIDI to make the synth playable. Full legato/envelope/glide comes in Phase 3. For Phase 2: note-on sets frequency and opens gate, note-off closes gate. Simple on/off amplitude (no AR shaping).

```cpp
void HarmonicEngine::handleNoteOn(int midiNote, float velocity)
{
    currentFreq_ = juce::MidiMessage::getMidiNoteInHertz(midiNote);
    currentVelocity_ = velocity;
    gateOpen_ = true;
}

void HarmonicEngine::handleNoteOff(int midiNote)
{
    // Simple: any note-off closes gate (proper gate counting in Phase 3)
    gateOpen_ = false;
}
```

### Pattern 8: Output Selector (Hard Switch)

**What:** `selector~ 4 1` in the patch is a hard switch with no crossfade. The APVTS parameter `output_select` is an `AudioParameterChoice` with values 0-3.

```cpp
// Map output_select: 0=Harmonic Mix, 1=Triangle, 2=Sawtooth, 3=Square
int outputMode = static_cast<int>(*outputSelectParam_);
double selectedOutput;
switch (outputMode)
{
    case 0: selectedOutput = mixDC; break;   // DC-blocked harmonic mix
    case 1: selectedOutput = triOut; break;
    case 2: selectedOutput = sawOut; break;
    case 3: selectedOutput = sqrOut; break;
    default: selectedOutput = mixDC; break;
}
```

### Anti-Patterns to Avoid

- **Using shared phase * harmonic number:** The gen~ code uses independent per-harmonic phase accumulators. Do NOT simplify to `sin(phase * h * twoPi)` -- it would deviate from the reference implementation.
- **Frequency-dependent leak for triangle:** The gen~ uses fixed `0.999`. Do not "improve" it with a frequency-dependent coefficient -- match the reference first.
- **Crossfading the output selector:** The gen~ patch uses a hard `selector~`. No crossfade. If clicks occur on switching, that matches the original behavior.
- **Computing scan/tilt only when parameters change:** The gen~ computes them every sample. For Phase 2, match this behavior. Optimization (caching when SmoothedValue stops ramping) can come later if needed.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| MIDI note to frequency | Custom lookup table | `juce::MidiMessage::getMidiNoteInHertz()` | Handles fractional notes, standard 440Hz tuning |
| Parameter thread safety | Atomic wrapper | APVTS `getRawParameterValue()` | Already returns `std::atomic<float>*` |
| MIDI iteration in processBlock | Custom MIDI parsing | `juce::MidiBuffer` iterator | Handles all MIDI message types, sorted by timestamp |
| Sine function | Wavetable lookup | `std::sin()` | 8 harmonics at audio rate is trivially cheap; wavetable adds complexity without measurable benefit for 8 calls per sample |
| Two-pi constant | Literal `6.28318...` | `juce::MathConstants<double>::twoPi` | Correct precision, readable |

## Common Pitfalls

### Pitfall 1: Using Float for Phase Accumulators
**What goes wrong:** Phase drift causes pitch inaccuracy on sustained notes, especially on higher harmonics
**Why it happens:** The JUCE audio buffer is `float` so developers use `float` everywhere
**How to avoid:** All internal DSP state (`phase_`, `harmPhases_[]`, `triIntegrator_`, `dcX1_`, `dcY1_`) must be `double`. Only cast to `float` when writing to the output buffer.
**Warning signs:** Pitch wobble on notes held for >10 seconds, especially at higher octaves

### Pitfall 2: Forgetting to Reset State in prepareToPlay
**What goes wrong:** Phase accumulators, integrators, and DC blocker state carry over from previous sessions or sample rate changes, causing clicks or wrong behavior on first playback
**Why it happens:** Member variables initialized in constructor but not reset when host calls `prepareToPlay`
**How to avoid:** Reset ALL `History`-equivalent state variables in `prepare()`: `phase_ = 0`, all 8 `harmPhases_` to 0, `triIntegrator_ = 0`, `dcX1_ = 0`, `dcY1_ = 0`, `fmPhase_ = 0`, `envLevel_ = 0`
**Warning signs:** Clicks or pops at playback start, different sound after sample rate change

### Pitfall 3: PolyBLEP Threshold with Negative Phase Increment
**What goes wrong:** When FM produces negative frequency (through-zero FM), `phase_inc` goes negative and PolyBLEP corrections break
**Why it happens:** PolyBLEP `t = p / dt` assumes positive `dt`
**How to avoid:** For Phase 2 (no FM), `freq` is clamped positive so this is not an issue. For Phase 3 when FM is added, use `abs(phase_inc)` for PolyBLEP threshold. The gen~ code clamps freq: `freq = clamp(freq, 0.01, nyquist * 0.98)`
**Warning signs:** Harsh distortion at high FM depths (Phase 3 concern)

### Pitfall 4: Harmonic Mix Normalization Factor
**What goes wrong:** Output clips when multiple harmonics are active
**Why it happens:** Missing the `harm_mix *= 0.25` normalization from the gen~ code
**How to avoid:** Apply `harmonicMix *= 0.25` after the harmonic summation loop, exactly as gen~ does. This scales the sum of 8 harmonics to a reasonable range.
**Warning signs:** Clipping/distortion when enabling multiple harmonics

### Pitfall 5: Output Selector Offset
**What goes wrong:** Wrong waveform plays for each selector position
**Why it happens:** The Max patch uses `selector~ 4 1` which is 1-indexed (inlet 0 = selector input, inlets 1-4 = signal inputs). The menu adds 1 (`obj-out-plus1`). APVTS `AudioParameterChoice` is 0-indexed.
**How to avoid:** Map APVTS values directly: 0=Harmonic Mix (gen~ out1), 1=Triangle (out2), 2=Sawtooth (out3), 3=Square (out4). The order in createParameterLayout already matches: `{"Harmonic Mix", "Triangle", "Sawtooth", "Square"}`
**Warning signs:** Menu labels don't match the sound

### Pitfall 6: Scan Width Sigma Mapping
**What goes wrong:** Scan envelope behaves differently from Max/MSP
**Why it happens:** Wrong mapping for sigma. The gen~ code maps width 0..1 to sigma 0.3..10 via `sigma = 0.3 + scan_width * 9.7`
**How to avoid:** Use the exact formula. At width=0, sigma=0.3 (very narrow, nearly single harmonic). At width=1, sigma=10 (very wide, all harmonics pass through).
**Warning signs:** Width=0 doesn't isolate a single harmonic; width=1 still filters

## Code Examples

### Complete Per-Sample DSP Render (gen~ sections 2-9 combined)

```cpp
// Source: gen~ codebox from HarmonicOscillator.maxpat
double HarmonicEngine::renderSample()
{
    // --- Phase accumulator (section 2) ---
    double phaseInc = currentFreq_ * inverseSampleRate_;
    double p = phase_ + phaseInc;
    p -= std::floor(p);
    phase_ = p;

    // --- PolyBLEP Sawtooth (section 3) ---
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

    // --- PolyBLEP Square (section 3) ---
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

    // --- Triangle: leaky integrated square (section 3) ---
    double triRaw = triIntegrator_ * 0.999 + sqrOut * phaseInc * 4.0;
    triIntegrator_ = triRaw;
    double triOut = std::clamp(triRaw, -1.0, 1.0);

    // --- Scan envelope (section 4) ---
    double centerPos = static_cast<double>(*scanCenterParam_) * 7.0;
    double sigma = 0.3 + static_cast<double>(*scanWidthParam_) * 9.7;
    for (int i = 0; i < 8; ++i)
    {
        double dist = (static_cast<double>(i) - centerPos) / sigma;
        scanEnv_[i] = std::exp(-0.5 * dist * dist);
    }

    // --- Spectral tilt (section 5) ---
    double tiltParam = static_cast<double>(*spectralTiltParam_);
    for (int i = 0; i < 8; ++i)
    {
        double normPos = static_cast<double>(i) / 7.0;
        double tiltDb = tiltParam * (normPos - 0.5) * 24.0;
        tiltFactors_[i] = std::pow(10.0, tiltDb / 20.0);
    }

    // --- 8-harmonic sine bank (section 6) ---
    double harmonicMix = 0.0;
    double nyquist = sampleRate_ * 0.5;
    for (int i = 0; i < 8; ++i)
    {
        int harmNum = i + 1;
        double harmFreq = currentFreq_ * harmNum;
        if (harmFreq < nyquist)
        {
            double hPhase = harmPhases_[i] + harmFreq * inverseSampleRate_;
            hPhase -= std::floor(hPhase);
            harmPhases_[i] = hPhase;

            double fader = static_cast<double>(*harmLevelParams_[i]);
            double amp = fader * scanEnv_[i] * tiltFactors_[i];
            harmonicMix += std::sin(hPhase * twoPi_) * amp;
        }
        else
        {
            harmPhases_[i] = 0.0;
        }
    }
    harmonicMix *= 0.25;

    // --- DC blocker on harmonic mix (section 8) ---
    double mixDC = harmonicMix - dcX1_ + dcR_ * dcY1_;
    dcX1_ = harmonicMix;
    dcY1_ = mixDC;

    // --- Output selector (section 9) ---
    // Phase 2: simple gate instead of AR envelope
    double env = gateOpen_ ? currentVelocity_ : 0.0;
    double masterLvl = static_cast<double>(*masterLevelParam_);

    int outputMode = static_cast<int>(*outputSelectParam_);
    double selected;
    switch (outputMode)
    {
        case 0: selected = mixDC; break;
        case 1: selected = triOut; break;
        case 2: selected = sawOut; break;
        case 3: selected = sqrOut; break;
        default: selected = mixDC; break;
    }

    return selected * env * masterLvl;
}
```

### processBlock Integration with MIDI Splitting

```cpp
// Source: Architecture pattern from ARCHITECTURE.md, adapted for Phase 2
void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                    juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    auto* leftChannel = buffer.getWritePointer(0);
    auto* rightChannel = buffer.getWritePointer(1);

    int currentSample = 0;

    for (const auto metadata : midiMessages)
    {
        // Render samples up to this MIDI event
        for (int s = currentSample; s < metadata.samplePosition; ++s)
        {
            float sample = static_cast<float>(engine_.renderSample());
            leftChannel[s] = sample;
            rightChannel[s] = sample;
        }
        currentSample = metadata.samplePosition;

        // Handle MIDI event
        auto msg = metadata.getMessage();
        if (msg.isNoteOn())
            engine_.handleNoteOn(msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isNoteOff())
            engine_.handleNoteOff(msg.getNoteNumber());
    }

    // Render remaining samples
    for (int s = currentSample; s < buffer.getNumSamples(); ++s)
    {
        float sample = static_cast<float>(engine_.renderSample());
        leftChannel[s] = sample;
        rightChannel[s] = sample;
    }
}
```

### prepareToPlay Setup

```cpp
void HarmonicEngine::prepare(double sampleRate, int /*maxBlockSize*/,
                              juce::AudioProcessorValueTreeState& apvts)
{
    sampleRate_ = sampleRate;
    inverseSampleRate_ = 1.0 / sampleRate;

    // DC blocker coefficient (5Hz cutoff)
    dcR_ = 1.0 - (juce::MathConstants<double>::twoPi * 5.0 * inverseSampleRate_);

    // Reset all state
    phase_ = 0.0;
    harmPhases_.fill(0.0);
    triIntegrator_ = 0.0;
    dcX1_ = 0.0;
    dcY1_ = 0.0;
    scanEnv_.fill(1.0);
    tiltFactors_.fill(1.0);
    gateOpen_ = false;
    currentFreq_ = 440.0;
    currentVelocity_ = 0.0;

    // Cache APVTS parameter pointers (these are stable for plugin lifetime)
    for (int i = 0; i < 8; ++i)
        harmLevelParams_[i] = apvts.getRawParameterValue("harm_" + juce::String(i + 1));

    scanCenterParam_ = apvts.getRawParameterValue("scan_center");
    scanWidthParam_ = apvts.getRawParameterValue("scan_width");
    spectralTiltParam_ = apvts.getRawParameterValue("spectral_tilt");
    masterLevelParam_ = apvts.getRawParameterValue("master_level");
    outputSelectParam_ = apvts.getRawParameterValue("output_select");
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Wavetable oscillators | Direct computation (`std::sin`) | Always valid for small harmonic counts | 8 sin calls per sample is trivially fast on modern CPUs (~0.01% CPU) |
| `dcblock~` default 40Hz | Custom 5Hz cutoff (verified from gen~ code) | N/A -- this was a misconception in earlier research | Use 5Hz, not 40Hz |
| Shared phase * harmonic number | Per-harmonic phase accumulators | gen~ design choice | Must match reference for A/B validation |
| Frequency-dependent triangle leak | Fixed 0.999 coefficient | gen~ design choice | Simpler, matches reference |

## Open Questions

1. **Parameter smoothing scope for Phase 2**
   - What we know: SmoothedValue is needed for all continuous parameters to avoid zipper noise
   - What's unclear: Should Phase 2 implement full smoothing, or defer to Phase 3 (PERF-08)?
   - Recommendation: Implement SmoothedValue for `master_level` and the 8 harmonic faders in Phase 2 (they directly modulate audio amplitude). Defer scan/tilt/FM smoothing to Phase 3 where PERF-08 is formally addressed. This keeps Phase 2 focused while avoiding the worst zipper noise.

2. **Gate vs AR envelope for Phase 2**
   - What we know: The gen~ code has a full AR envelope (section 7). Phase 3 owns PERF-04 (AR envelope).
   - What's unclear: Should Phase 2 implement a hard gate (instant on/off) or a minimal anti-click ramp?
   - Recommendation: Implement a 5ms attack/release ramp (not the full AR envelope) to prevent clicks on note on/off. This is a different concern from the musical AR envelope in Phase 3.

3. **Denormal handling**
   - What we know: The gen~ code has `FixDenorm` references. Denormals cause CPU spikes.
   - Recommendation: Use `juce::ScopedNoDenormals` at the top of `processBlock()`. This sets the CPU flag to flush denormals to zero for the entire block. Simple, correct, no per-sample overhead.

## Sources

### Primary (HIGH confidence)
- gen~ codebox source extracted from `/Users/matt/src/harmonic-oscillator-plugin/HarmonicOscillator.maxpat` -- complete DSP algorithm reference
- Phase 1 source code (`src/PluginProcessor.h`, `src/PluginProcessor.cpp`) -- existing APVTS parameter layout with exact parameter IDs
- JUCE `AudioProcessorValueTreeState::getRawParameterValue()` -- returns `std::atomic<float>*`, stable API

### Secondary (MEDIUM confidence)
- PolyBLEP algorithm (Valimaki & Franck) -- well-documented technique, gen~ implementation matches standard approach
- DC blocker first-order highpass topology -- standard DSP, verified against gen~ code
- `juce::SmoothedValue` API -- training data, stable since JUCE 5

### Tertiary (LOW confidence)
- Triangle wave leaky integrator behavior at extreme frequencies -- the fixed 0.999 coefficient may cause DC drift at very low frequencies; gen~ clamps as mitigation but untested in C++
- Exact CPU cost of 8x `std::sin()` per sample -- expected trivial but not benchmarked

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- no new dependencies, everything is custom DSP + existing JUCE
- Architecture: HIGH -- gen~ codebox is the definitive spec, fully extracted and analyzed
- DSP algorithms: HIGH -- line-by-line translation from verified gen~ source
- Pitfalls: HIGH -- verified against actual gen~ code (corrected earlier assumptions about DC blocker cutoff, triangle leak, phase accumulator strategy)

**Research date:** 2026-03-10
**Valid until:** Indefinite (gen~ codebox is the fixed reference; JUCE API is stable)
