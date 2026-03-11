# Domain Pitfalls

**Domain:** JUCE audio plugin (additive synthesizer, gen~ to C++ translation)
**Researched:** 2026-03-10
**Confidence:** MEDIUM (based on training data -- web search unavailable for verification)

---

## Critical Pitfalls

Mistakes that cause rewrites, broken audio, or host rejection.

### Pitfall 1: Allocating Memory or Locking on the Audio Thread

**What goes wrong:** Using `new`, `delete`, `malloc`, `std::vector::push_back`, `std::mutex::lock`, `String` operations, or any system call that can block inside `processBlock()`. The audio thread has a hard real-time deadline (e.g., 1.3ms at 48kHz/64 samples). Any allocation or lock can cause audio glitches, dropouts, or full hangs.

**Why it happens:** C++ makes it easy to accidentally allocate -- creating a `std::string`, resizing a vector, logging with `DBG()`, even `juce::String` concatenation allocates. Developers coming from Max/MSP never deal with this because the runtime handles memory.

**Consequences:** Intermittent audio glitches under load. May only manifest on some hosts or buffer sizes. Extremely hard to reproduce and debug.

**Prevention:**
- Pre-allocate all buffers in `prepareToPlay()`, never in `processBlock()`
- Use `std::atomic` for parameter communication, never `std::mutex`
- Zero `DBG()` or logging calls in the audio path
- Use `juce::AudioBuffer` sized in `prepareToPlay()`
- For the harmonic levels array: use `std::array<std::atomic<float>, 8>` or read from `AudioProcessorValueTreeState` parameters directly (those are already atomic)
- Run with AddressSanitizer and ThreadSanitizer during development to catch violations

**Detection:** Audio glitches that appear randomly, especially when CPU is loaded. DAW audio dropout indicators firing. Use tools like `RADSan` (RealtimeSanitizer) if available, or Xcode's Thread Sanitizer.

**Phase relevance:** Must be established from Phase 1 (DSP core). This is an architectural constraint, not something you bolt on later.

---

### Pitfall 2: gen~ Phase Accumulator Precision Loss at High Frequencies

**What goes wrong:** gen~ uses `accum` with a `phasewrap` (0-1 range) for phase accumulation. A naive C++ translation using `float` loses precision at higher frequencies because the phase increment becomes a larger fraction of 1.0, and `float` has only ~7 digits of precision. Over time, the phase accumulator drifts, causing pitch inaccuracy and timbral changes.

**Why it happens:** gen~ internally operates at 64-bit (`double`) precision for accumulators. Translating to `float` because "JUCE uses float buffers" loses the accumulator precision that gen~ took for granted.

**Consequences:** Subtle pitch drift and timbral instability, especially noticeable on harmonics 4-8 at higher fundamental frequencies. The additive mix will sound different from the Max/MSP reference.

**Prevention:**
- Use `double` for all phase accumulators and frequency calculations internally
- Only convert to `float` at the final output stage when writing to `AudioBuffer<float>`
- The phase wrap operation (`phase -= std::floor(phase)` or `phase = std::fmod(phase, 1.0)`) should use `double`
- For 8 harmonics, you have 8 phase accumulators -- all must be `double`

**Detection:** Compare output against Max/MSP patch at high notes (C6+). Listen for beating or pitch wobble on sustained notes. Measure with a tuner plugin on the output.

**Phase relevance:** Phase 1 (DSP core). The oscillator core must be double-precision from the start. Retrofitting precision is painful because it touches every DSP calculation.

---

### Pitfall 3: PolyBLEP Implementation Getting Edge Cases Wrong

**What goes wrong:** PolyBLEP anti-aliasing requires detecting when the phase wraps (crosses 0/1 boundary) and applying a polynomial correction. Common mistakes: (a) not handling the case where phase increment is negative (FM!), (b) applying the correction at the wrong sample, (c) forgetting that square wave needs PolyBLEP at *both* transitions (0 and 0.5), (d) the leaky integrator for triangle having DC offset or wrong gain.

**Why it happens:** PolyBLEP looks simple in a tutorial but has subtle edge cases. The gen~ patch likely uses a different anti-aliasing approach or none (gen~ operates differently with its `phasor~` and built-in waveform operators). Translating "the behavior" rather than "the algorithm" leads to mismatches.

**Consequences:** Aliasing artifacts at high frequencies, clicks at note transitions, DC offset in triangle wave, audible differences from the reference patch.

**Prevention:**
- Implement PolyBLEP as a standalone utility function, unit-test it in isolation
- For sawtooth: apply PolyBLEP at phase wrap (near 0/1)
- For square: apply PolyBLEP at both 0 and 0.5 phase crossings
- For triangle (leaky integrated square): the leaky integrator coefficient must account for frequency (`leak = 1.0 - (frequency / sampleRate)`), and the output must be gain-compensated. Use the formula: `triangle += leak * (square - triangle)` is wrong -- use a proper leaky integrator: `output = alpha * square + (1 - alpha) * prevOutput` with DC blocking after
- Handle negative phase increments from FM by checking `abs(phaseIncrement)` for PolyBLEP threshold
- Test at A7 (3520Hz) and above where aliasing becomes audible

**Detection:** Spectrum analyzer showing aliasing mirrors. A/B comparison with the Max/MSP patch at high frequencies. Listen for metallic or harsh overtones that the patch does not produce.

**Phase relevance:** Phase 1 (DSP core). Get the waveform generators right before building anything on top of them.

---

### Pitfall 4: Parameter Smoothing Omitted -- Zipper Noise and Clicks

**What goes wrong:** Moving any parameter (harmonic levels, scan center, tilt, master level) without smoothing causes discontinuities in the audio output -- audible as zipper noise (rapid staircase artifacts) or clicks.

**Why it happens:** In Max/MSP, `slide~` and signal-rate connections inherently smooth everything. In JUCE, `processBlock()` receives parameter values that can change between blocks (or even mid-block via automation). Without explicit smoothing, each block boundary is a discontinuity.

**Consequences:** Every knob movement produces audible artifacts. Automation sounds terrible. The plugin will feel broken compared to the smooth Max/MSP experience.

**Prevention:**
- Use `juce::SmoothedValue<float>` for every continuously-variable parameter read in the audio thread
- Set smoothing ramp time in `prepareToPlay()` (typically 10-50ms, matching `slide~` behavior from the patch)
- For the 8 harmonic faders: each needs its own `SmoothedValue`
- For scan center, scan width, tilt: all need smoothing
- Process smoothing per-sample within `processBlock()` (call `.getNextValue()` per sample, not per block)
- For the output selector (hard switch): this is the one parameter that should NOT be smoothed (matches `selector~` behavior), but consider a fast crossfade (1-2ms) to avoid clicks on switching

**Detection:** Move any parameter rapidly during playback -- if you hear zipper noise or stepping, smoothing is missing. Automate parameters and listen.

**Phase relevance:** Phase 1 (DSP core). Bake `SmoothedValue` into the parameter reading pattern from day one. Adding it later means touching every parameter access site.

---

### Pitfall 5: processBlock Not Handling Variable Buffer Sizes

**What goes wrong:** Assuming `processBlock()` always receives the same buffer size. Hosts can (and do) call `processBlock()` with different buffer sizes between calls, or with sizes different from what was passed to `prepareToPlay()`. Some hosts split buffers for sample-accurate automation.

**Why it happens:** In Max/MSP, signal vector size is fixed during runtime. Developers assume the same in JUCE.

**Consequences:** Buffer overruns (crash), or DSP that only works at one buffer size. Logic Pro is known for calling with smaller buffers than configured.

**Prevention:**
- Never hardcode buffer sizes. Always use `buffer.getNumSamples()` for the current block size
- Pre-allocate internal buffers to the maximum expected size (use `prepareToPlay()`'s `maximumExpectedSamplesPerBlock` parameter, but still check actual size)
- Loop constructs: `for (int i = 0; i < buffer.getNumSamples(); ++i)`
- Test with multiple buffer sizes (64, 128, 256, 512, 1024, 2048) and verify identical output

**Detection:** Crashes or glitches when switching buffer sizes in the DAW. Test in multiple hosts (Logic, Ableton, Reaper) which use different buffering strategies.

**Phase relevance:** Phase 1. Structural from the beginning.

---

### Pitfall 6: FM Implementation Producing Wrong Results at Audio Rate

**What goes wrong:** The project extends FM LFO range to audio rate (0.1Hz to audio rate). Linear FM and exponential FM behave very differently at audio rate vs sub-audio rate. Common mistakes: (a) exponential FM applied to frequency directly causes asymmetric pitch deviation, (b) linear FM not properly scaled causes the modulator to dominate, (c) FM index not sample-rate-compensated.

**Why it happens:** The gen~ patch caps at 100Hz, so its FM behavior at audio rate is untested territory. Extending the range means entering FM synthesis domain where subtle implementation details drastically change the sound.

**Consequences:** FM sounds nothing like expected. Harsh aliasing from FM sidebands exceeding Nyquist. Unstable pitch at high modulation depths.

**Prevention:**
- **Linear FM:** Add modulator output directly to frequency in Hz, not to phase increment. Scale modulator by FM depth in Hz. This is true through-zero linear FM.
- **Exponential FM:** Modulator scales frequency logarithmically: `freq * pow(2, modulator * depth_in_octaves)`. This always stays positive (no through-zero).
- At audio-rate FM, the sidebands will exceed Nyquist -- this is expected and matches analog behavior. Do not try to bandlimit FM (it is not possible in real-time for arbitrary modulation).
- Sample-rate compensate the FM depth so it sounds the same at 44.1k and 96k
- Test FM at audio rate extensively -- this is new territory beyond the original patch

**Detection:** Compare sub-audio FM behavior against Max/MSP patch first (should match). Then evaluate audio-rate FM on its own merits since there is no reference.

**Phase relevance:** Phase 1 (DSP core) for sub-audio FM matching the patch. Audio-rate FM testing in Phase 2 or wherever FM extension is implemented.

---

## Moderate Pitfalls

### Pitfall 7: Scan Gaussian Window Computed Per-Sample Wastefully

**What goes wrong:** Computing `exp(-0.5 * ((harmonic - center) / width)^2)` for 8 harmonics on every sample when scan parameters have not changed. This is wasteful and, while not a correctness issue for a mono synth, it sets a bad pattern.

**Prevention:**
- Recompute the 8 Gaussian weights only when scan center or scan width `SmoothedValue` indicates it is still ramping (`.isSmoothing()`)
- Cache the weights in a `std::array<double, 8>`
- When smoothing is complete, use cached values until parameters move again
- This keeps CPU near zero for static settings

**Detection:** Profile `processBlock()` with Instruments. If Gaussian exp/pow calls dominate, caching is needed.

**Phase relevance:** Phase 1 optimization pass. Not blocking but good practice.

---

### Pitfall 8: Plugin State Save/Restore Losing Harmonic Fader Positions

**What goes wrong:** The 8 harmonic faders are non-standard parameters. If they are not properly registered with `AudioProcessorValueTreeState` (APVTS), the DAW cannot save or restore them. Users save a project, reopen, and the harmonic levels are wrong.

**Why it happens:** Developers create custom UI for the faders but forget to back them with proper JUCE parameters. Or they use raw member variables instead of APVTS parameters.

**Consequences:** Lost settings on project reload. This makes the plugin unusable in production.

**Prevention:**
- Register all 8 harmonic levels as `AudioParameterFloat` in the APVTS layout
- Use parameter IDs like `harmonic_1` through `harmonic_8`
- Every audible control must be a registered parameter -- no exceptions
- Test: save DAW project, close, reopen, verify all parameters restored
- Also test with host parameter randomization to verify full range works

**Detection:** Save/load test in your primary DAW. Check `getStateInformation` / `setStateInformation` round-trip.

**Phase relevance:** Phase 1 (parameter setup). Must be correct from initial parameter architecture.

---

### Pitfall 9: DC Offset Accumulation in the Harmonic Mix

**What goes wrong:** The additive mix of 8 harmonics can produce a DC offset depending on harmonic levels and phases. The project spec calls for a DC blocker, but implementing it wrong (wrong cutoff frequency, wrong topology) either removes wanted low-frequency content or fails to block DC.

**Prevention:**
- Use a standard first-order highpass DC blocker: `y[n] = x[n] - x[n-1] + R * y[n-1]` where `R = 1 - (2 * pi * cutoff / sampleRate)`
- Cutoff frequency: 5-20Hz is typical. The gen~ patch likely uses `dcblock~` which defaults to ~40Hz cutoff (R=0.9954 at 44.1k) -- match this
- Place the DC blocker after the harmonic mix but before the master level
- Make sure `R` is recalculated in `prepareToPlay()` if sample rate changes
- Do NOT put DC blocker on the PolyBLEP outputs (they should not have DC issues if implemented correctly; the leaky-integrated triangle may, but handle that separately)

**Detection:** Route output to a spectrum analyzer and check for energy at 0Hz. Play a note and observe if the waveform drifts off-center.

**Phase relevance:** Phase 1 (DSP core), when building the harmonic mix output path.

---

### Pitfall 10: GUI Blocking the Message Thread / Audio Thread Contention

**What goes wrong:** JUCE's GUI runs on the message thread. If GUI updates read audio-thread state without proper atomic access, you get data races. If the GUI does expensive operations (repainting all 8 faders every frame), it can starve the message thread and cause the host UI to lag.

**Prevention:**
- Use `AudioProcessorValueTreeState::Attachment` classes for all GUI controls -- this handles thread-safe parameter communication automatically
- Repaint faders only when their values actually change (use `parameterChanged` callbacks, do not poll)
- For any custom visualization (waveform display, spectrum), use a lock-free FIFO (`juce::AbstractFifo`) to send data from audio thread to GUI
- Never call `getParameter()` from the audio thread and `setParameter()` from the GUI thread without APVTS mediating

**Detection:** UI feels sluggish when audio is playing. ThreadSanitizer reports data races. Instruments shows excessive time in `paint()` calls.

**Phase relevance:** Phase 2 (GUI). But the data flow pattern (APVTS attachments, lock-free FIFOs) must be planned in Phase 1.

---

### Pitfall 11: Legato / Gate Counting Logic Off-by-One Errors

**What goes wrong:** The Max/MSP patch uses `accum` for gate counting to implement legato (overlapping notes glide without retriggering the AR envelope). Translating this to MIDI `noteOn`/`noteOff` handling in JUCE has subtle edge cases: (a) some hosts send `noteOn` with velocity 0 instead of `noteOff`, (b) MIDI messages may arrive out of order within a buffer, (c) the gate count can go negative if a `noteOff` arrives without a matching `noteOn` (e.g., after loading a project mid-note).

**Prevention:**
- Handle both `noteOff` and `noteOn` with velocity 0 as note-off events
- Clamp gate count to minimum 0 (`gateCount = std::max(0, gateCount - 1)`)
- Process MIDI events in timestamp order within each buffer (JUCE's `MidiBuffer` iterator provides this)
- Trigger envelope on transition from `gateCount == 0` to `gateCount > 0`
- Release envelope on transition from `gateCount > 0` to `gateCount == 0`
- Portamento applies when `gateCount` was already > 0 on a new `noteOn`
- Test: rapid note overlaps, sustain pedal (if honoring it), and project-load-mid-note scenarios

**Detection:** Stuck notes, envelope not retriggering when it should, or retriggering during legato passages. Test with a MIDI monitor to verify gate count behavior.

**Phase relevance:** Phase 1 (MIDI handling / voice management).

---

### Pitfall 12: Sample Rate Change Not Handled in prepareToPlay

**What goes wrong:** The host can change sample rate (e.g., switching from 44.1k to 96k). If `prepareToPlay()` does not recalculate all sample-rate-dependent values, the plugin sounds wrong -- wrong pitch, wrong filter cutoffs, wrong envelope times.

**Prevention:**
- Recalculate in `prepareToPlay()`: all SmoothedValue ramp times, DC blocker coefficient, PolyBLEP thresholds, AR envelope coefficients, portamento rate, FM scaling
- Store `currentSampleRate` as a member and use it everywhere (never hardcode 44100.0)
- Test at 44.1k, 48k, 88.2k, 96k, and 192k

**Detection:** Plugin sounds different at different sample rates. Pitch is wrong at 96k. Envelope times are doubled at 88.2k.

**Phase relevance:** Phase 1. Must be part of `prepareToPlay()` from the start.

---

## Minor Pitfalls

### Pitfall 13: AU Validation Failure

**What goes wrong:** The Audio Unit must pass Apple's `auval` validation tool to be loaded by Logic Pro. Common failures: not reporting latency correctly, not handling zero-length buffers, not supporting the correct channel configurations.

**Prevention:**
- Run `auval -v aumu [manufacturer] [subtype]` during CI or manual testing
- Report 0 latency (this plugin has no latency)
- Support mono-out and stereo-out configurations (even if output is mono, hosts may expect stereo)
- Handle `processBlock()` called with 0 samples gracefully (just return)

**Detection:** `auval` failures. Plugin not appearing in Logic Pro.

**Phase relevance:** Phase 3 (packaging/distribution).

---

### Pitfall 14: JUCE Version / CMake Build Complexity

**What goes wrong:** JUCE build system configuration (Projucer vs CMake) causes friction. Projucer is being phased out in favor of CMake. Using Projucer locks you into its project management. Using CMake requires understanding JUCE's CMake API (`juce_add_plugin`, target properties, etc.).

**Prevention:**
- Use CMake directly (not Projucer). JUCE's CMake API is mature and well-documented.
- Pin a specific JUCE version (7.x latest stable) via git submodule or CPM/FetchContent
- Use `juce_add_plugin()` with explicit `PLUGIN_MANUFACTURER_CODE`, `PLUGIN_CODE`, `FORMATS VST3 AU`
- Keep `CMakeLists.txt` minimal and well-commented

**Detection:** Build fails on clean checkout. Cannot generate Xcode project. Plugin format not appearing.

**Phase relevance:** Phase 0 (project setup). Must be solid before any DSP work begins.

---

### Pitfall 15: Forgetting to Handle Edge Cases in AR Envelope

**What goes wrong:** The AR envelope has subtle edge cases: (a) retrigger during attack phase (should restart from current level, not jump to 0), (b) retrigger during release phase (should restart from current level), (c) extremely short attack/release times causing clicks, (d) envelope coefficient calculation using wrong formula.

**Prevention:**
- Use exponential envelope segments: `level = target + (level - target) * coefficient` where `coefficient = exp(-1.0 / (time_seconds * sampleRate))`
- On trigger: set target to 1.0, start from current level (not 0)
- On release: set target to 0.0, start from current level (not 1.0)
- Minimum attack/release time of ~0.1ms to prevent clicks even when set to minimum
- The gen~ `slide~` equivalent for envelope may use a different smoothing formula -- verify against the patch

**Detection:** Clicks on note transitions. Envelope shape looks wrong on oscilloscope. Behavior differs from Max/MSP patch on rapid retriggering.

**Phase relevance:** Phase 1 (DSP core).

---

## Phase-Specific Warnings

| Phase Topic | Likely Pitfall | Mitigation |
|-------------|---------------|------------|
| Project setup (CMake/JUCE) | Build system complexity, wrong JUCE version | Use CMake, pin JUCE version, verify AU/VST3 build before writing DSP |
| DSP core (oscillators) | Float precision loss in phase accumulators | Use double for all internal DSP, float only at output |
| DSP core (PolyBLEP) | Aliasing, clicks, wrong triangle implementation | Unit test each waveform in isolation, compare spectra |
| DSP core (harmonic mix) | DC offset, no parameter smoothing | DC blocker from day one, SmoothedValue on all parameters |
| DSP core (FM) | Audio-rate FM is new territory beyond patch | Match sub-audio behavior first, then test audio-rate independently |
| DSP core (AR envelope) | Retrigger glitches, wrong exponential curves | Test rapid note sequences, compare with gen~ slide behavior |
| MIDI / voice management | Legato gate counting edge cases | Handle noteOn vel=0, clamp gate count, test overlapping notes |
| GUI | Thread safety, performance | APVTS attachments only, lock-free FIFOs for visualization |
| Plugin packaging | AU validation failure | Run auval early and often, handle edge cases in processBlock |
| Sample rate handling | DSP sounds wrong at non-44.1k rates | Recalculate everything in prepareToPlay, test at 48k and 96k |

---

## gen~ to C++ Translation Cheat Sheet

These are the most common translation errors when converting gen~ codebox to JUCE C++:

| gen~ Construct | JUCE/C++ Equivalent | Common Mistake |
|----------------|---------------------|----------------|
| `phasewrap` / `accum` | `phase = fmod(phase + increment, 1.0)` (double) | Using float, not handling negative wrap |
| `slide~` (up, down) | `SmoothedValue` or manual: `out += (in - out) * coeff` | Forgetting different up/down coefficients |
| `selector~` | Direct assignment / mux | Adding crossfade (patch does not have one) |
| `dcblock~` | First-order highpass | Wrong cutoff frequency (gen~ default is ~40Hz) |
| `poke~` / `peek~` (buffer) | `std::array` or APVTS parameters | Using non-atomic shared state |
| `accum` (gate count) | Integer counter with noteOn/noteOff | Not clamping to 0, not handling vel=0 noteOn |
| `param` | `AudioParameterFloat` via APVTS | Not smoothing parameter reads in audio thread |
| History (z-1) | Member variable storing previous sample | Not initializing to 0, not resetting on prepareToPlay |
| `samplerate` | `getSampleRate()` stored in prepareToPlay | Hardcoding 44100 |
| `FixDenorm` | `juce::dsp::util::snapToZero()` or manual check | Ignoring denormals entirely (causes CPU spikes) |

---

## Sources

- JUCE framework documentation and audio plugin hosting behavior (training data, MEDIUM confidence)
- PolyBLEP algorithm: Valimaki & Franck, "Alias-Suppressed Oscillators Based on Differentiated Polynomial Waveforms" (training data, HIGH confidence on algorithm, MEDIUM on implementation details)
- gen~ codebox behavior: Cycling '74 documentation (training data, MEDIUM confidence)
- Audio thread real-time constraints: widely established in audio programming literature (HIGH confidence)
- Note: Web search was unavailable for verification. All findings based on training data. Recommend verifying JUCE 7.x API specifics against current Context7 or official docs during implementation.
