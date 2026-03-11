# Architecture Patterns

**Domain:** JUCE monophonic additive synthesizer plugin (VST3/AU)
**Researched:** 2026-03-10
**Confidence:** MEDIUM (based on training data -- web search unavailable for verification)

## Recommended Architecture

For a monophonic synth this simple, skip JUCE's `Synthesiser`/`SynthesiserVoice` class hierarchy entirely. That system is designed for polyphonic voice management and adds unnecessary indirection for a single-voice instrument. Instead, use a flat architecture: `AudioProcessor` owns a single DSP engine object that processes audio directly in `processBlock`.

### High-Level Structure

```
Host (DAW)
  |
  v
PluginProcessor (AudioProcessor subclass)
  |-- AudioProcessorValueTreeState (all parameters)
  |-- HarmonicEngine (single DSP engine instance)
  |     |-- PhaseAccumulator (fundamental frequency tracking)
  |     |-- HarmonicBank (8 sine oscillators, scan, tilt)
  |     |-- PolyBLEPOscillator (saw, square, triangle)
  |     |-- FMProcessor (linear FM, exponential FM, internal LFO)
  |     |-- AREnvelope (attack-release with gate counting)
  |     |-- DCBlocker (first-order high-pass)
  |     |-- GlideProcessor (portamento)
  |     `-- OutputSelector (hard switch between waveform outputs)
  |
  v
PluginEditor (AudioProcessorEditor subclass)
  |-- PitchSection (coarse, fine, glide controls)
  |-- HarmonicsSection (8 vertical sliders)
  |-- ScanTiltSection (scan center, width, tilt amount)
  |-- FMSection (FM amount, LFO rate, lin/exp mode)
  |-- OutputSection (waveform selector, master level)
  `-- EnvelopeSection (attack, release controls)
```

### Component Boundaries

| Component | Responsibility | Communicates With |
|-----------|---------------|-------------------|
| **PluginProcessor** | Hosts parameters (APVTS), routes MIDI, calls DSP engine per block, serializes state | Host (DAW), PluginEditor, HarmonicEngine |
| **AudioProcessorValueTreeState** | Stores all automatable parameters, provides atomic access from audio thread | PluginProcessor, PluginEditor (attachments) |
| **HarmonicEngine** | Top-level DSP coordinator: receives note events + parameter snapshots, renders audio samples | PluginProcessor (called by), all DSP sub-components (owns) |
| **PhaseAccumulator** | Maintains master phase (0-1 ramp), calculates frequency from MIDI note + tuning + FM | HarmonicEngine, FMProcessor (receives FM offset) |
| **HarmonicBank** | 8 sine oscillators at integer multiples of fundamental, applies scan envelope + tilt | PhaseAccumulator (reads master phase), HarmonicEngine (output) |
| **PolyBLEPOscillator** | Generates anti-aliased saw/square/triangle from master phase | PhaseAccumulator (reads master phase), HarmonicEngine (output) |
| **FMProcessor** | Internal LFO generating FM signal, applies linear or exponential FM offset | PhaseAccumulator (modifies frequency) |
| **AREnvelope** | Attack-release envelope with exponential curves, gate counting for legato | HarmonicEngine (scales output amplitude) |
| **GlideProcessor** | Smooths frequency transitions between notes (portamento) | PhaseAccumulator (smooths target frequency) |
| **DCBlocker** | First-order DC blocking filter on harmonic mix output | HarmonicBank (post-processing) |
| **OutputSelector** | Hard switches between harmonic mix, saw, square, triangle | HarmonicBank, PolyBLEPOscillator, HarmonicEngine (selects which output) |
| **PluginEditor** | GUI with sections matching the Max patch layout | APVTS (parameter attachments for all controls) |

## Data Flow

### Audio Thread (processBlock)

```
1. processBlock(buffer, midiMessages) called by host
   |
2. Iterate MIDI events in buffer, extract:
   |-- noteOn: update target frequency, trigger envelope (if not legato)
   |-- noteOff: release envelope (if gate count reaches 0)
   |-- pitchBend: adjust frequency offset
   |
3. Read parameter values from APVTS (atomic float pointers, lock-free)
   |
4. For each sample in buffer:
   |
   a. GlideProcessor: smooth target frequency -> current frequency
   |
   b. FMProcessor: generate LFO sample, calculate FM offset
   |
   c. PhaseAccumulator: advance phase by (currentFreq + fmOffset) / sampleRate
   |
   d. HarmonicBank: for each harmonic 1-8:
   |   - Check if harmonic * freq > Nyquist -> skip
   |   - Calculate sine at (harmonic * masterPhase)
   |   - Apply individual level (from parameter)
   |   - Apply scan envelope (Gaussian centered on scanCenter, width scanWidth)
   |   - Apply tilt (linear-in-dB slope: +/-24dB range across 8 harmonics)
   |   - Sum all harmonics
   |   - Apply DCBlocker to sum
   |
   e. PolyBLEPOscillator: generate saw/square/triangle from masterPhase
   |
   f. OutputSelector: pick one output based on selector parameter
   |
   g. AREnvelope: calculate envelope value, apply to selected output
   |
   h. Apply master level
   |
   i. Write to output buffer (mono -> both channels)
```

### Parameter Flow (Thread-Safe)

```
GUI Thread                          Audio Thread
    |                                    |
PluginEditor                        processBlock
    |                                    |
SliderAttachment ---> APVTS -----> getRawParameterValue()
    |                (atomic)            |
    |                                    v
    |                            Parameter snapshot
    |                            used per-sample or
    |                            per-block (smoothed)
```

Key rule: **Never allocate, lock, or block on the audio thread.** APVTS handles this with atomic floats. Parameter smoothing (for click-free changes) should use `juce::SmoothedValue` on the audio thread side.

### State Serialization

```
getStateInformation():  APVTS.copyState() -> XML -> MemoryBlock
setStateInformation():  MemoryBlock -> XML -> APVTS.replaceState()
```

Host handles preset save/load. No custom preset management needed.

## Patterns to Follow

### Pattern 1: Direct DSP Engine (No Synthesiser Class)

**What:** Skip `juce::Synthesiser` / `juce::SynthesiserVoice`. Own a single `HarmonicEngine` instance directly in `PluginProcessor`.

**When:** Monophonic synths where voice allocation adds no value.

**Why:** `Synthesiser` adds voice-stealing, round-robin, and allocation logic that is pure overhead for a mono synth. A direct engine is simpler, faster, and easier to debug. The gen~ codebox is a single monolithic DSP block -- a direct engine maps naturally to that.

**Example:**
```cpp
class PluginProcessor : public juce::AudioProcessor
{
    juce::AudioProcessorValueTreeState apvts;
    HarmonicEngine engine;

    void processBlock (juce::AudioBuffer<float>& buffer,
                       juce::MidiBuffer& midiMessages) override
    {
        engine.processBlock (buffer, midiMessages, apvts);
    }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override
    {
        engine.prepare (sampleRate, samplesPerBlock);
    }
};
```

### Pattern 2: MIDI-Splitting Within processBlock

**What:** Process MIDI events sample-accurately by splitting the audio buffer at each MIDI event timestamp.

**When:** Always, for sample-accurate note timing.

**Why:** gen~ processes MIDI sample-accurately. Batching MIDI events per block introduces timing jitter up to one buffer's worth of latency. For a monophonic synth, this is especially audible on fast passages.

**Example:**
```cpp
void HarmonicEngine::processBlock (juce::AudioBuffer<float>& buffer,
                                    juce::MidiBuffer& midi,
                                    const juce::AudioProcessorValueTreeState& apvts)
{
    int currentSample = 0;

    for (const auto metadata : midi)
    {
        // Render audio up to this MIDI event
        renderSamples (buffer, currentSample, metadata.samplePosition);
        // Handle the MIDI event
        handleMidiEvent (metadata.getMessage());
        currentSample = metadata.samplePosition;
    }
    // Render remaining samples
    renderSamples (buffer, currentSample, buffer.getNumSamples());
}
```

### Pattern 3: Gate Counting for Legato

**What:** Track overlapping note-on/note-off with a gate counter. Only trigger envelope on transition from 0->1. Only release on transition from 1->0.

**When:** Monophonic legato mode.

**Why:** Matches the gen~ `accum` gate counting logic exactly. When a new note arrives while one is held, glide to new pitch without retriggering the envelope.

**Example:**
```cpp
void handleNoteOn (int note, float velocity)
{
    gateCount++;
    targetNote = note;
    if (gateCount == 1) // First note -- trigger
    {
        envelope.trigger (velocity);
        currentNote = note; // Jump, no glide on first note
    }
    // else: legato -- glide will smooth currentNote -> targetNote
}

void handleNoteOff (int note)
{
    gateCount = std::max (0, gateCount - 1);
    if (gateCount == 0)
        envelope.release();
}
```

### Pattern 4: Parameter Smoothing with SmoothedValue

**What:** Use `juce::SmoothedValue` for parameters that cause clicks/zipper noise when changed abruptly (level, scan center, tilt amount).

**When:** Any continuous parameter that directly modulates audio output.

**Why:** Atomic parameter reads give you the new value instantly. Without smoothing, a knob jump from 0.0 to 1.0 causes a discontinuity in the audio signal. SmoothedValue interpolates over a configurable number of samples.

```cpp
juce::SmoothedValue<float> masterLevel;

void prepare (double sampleRate, int /*blockSize*/)
{
    masterLevel.reset (sampleRate, 0.02); // 20ms smoothing
}

void renderSample()
{
    masterLevel.setTargetValue (*apvts.getRawParameterValue ("masterLevel"));
    float sample = engine.getNextSample();
    sample *= masterLevel.getNextValue();
}
```

### Pattern 5: Shared Phase Accumulator

**What:** Use a single master phase accumulator (0.0 to 1.0 ramp) for all waveform generators. Harmonics multiply the phase: `sin(2 * PI * harmonic * phase)`.

**When:** Additive synthesis with related harmonic frequencies.

**Why:** This is exactly how the gen~ codebox works. One `phasor` drives everything. It ensures harmonics stay phase-locked to the fundamental, and PolyBLEP oscillators use the same phase. Keeps frequency/FM/glide logic in one place.

```cpp
// Single phase accumulator
phase += frequency / sampleRate;
phase -= std::floor(phase); // Wrap to [0, 1)

// Harmonics use multiples
for (int h = 1; h <= 8; ++h)
{
    float harmonicPhase = std::fmod(phase * h, 1.0f);
    float sample = std::sin(2.0f * juce::MathConstants<float>::pi * harmonicPhase);
    // apply levels, scan, tilt...
}

// PolyBLEP uses same phase
float saw = polyBlepSaw(phase, phaseIncrement);
```

## Anti-Patterns to Avoid

### Anti-Pattern 1: Using juce::Synthesiser for a Mono Synth

**What:** Creating a `SynthesiserVoice` subclass, adding it to a `Synthesiser`, setting max voices to 1.

**Why bad:** Voice allocation overhead, awkward legato handling (voice stealing isn't the same as legato), extra indirection layer. The Synthesiser class wasn't designed for this use case and fights you on gate counting.

**Instead:** Own a `HarmonicEngine` directly. Handle noteOn/noteOff yourself with gate counting.

### Anti-Pattern 2: Separate Phase Accumulators per Waveform

**What:** Having independent phase accumulators for the harmonic bank and PolyBLEP oscillators.

**Why bad:** Phase drift between outputs. Output selector switch produces a discontinuity. Duplicated frequency/FM/glide logic.

**Instead:** Single master phase shared by all generators.

### Anti-Pattern 3: Processing Parameters Once Per Block

**What:** Reading parameter values once at the start of processBlock and using them for the entire buffer.

**Why bad:** Step-changes in parameters cause clicks. FM LFO rate changes produce audible artifacts. With buffers up to 2048 samples (~46ms at 44.1kHz), this is very audible.

**Instead:** Read parameters per-sample (cheap -- atomic float read) or use SmoothedValue to interpolate.

### Anti-Pattern 4: Using float for Phase Accumulation

**What:** Accumulating phase in a 32-bit float over the lifetime of a note.

**Why bad:** At audio frequencies, a float loses precision after several seconds. At 1000Hz with 44100 sample rate, after ~16 seconds you exceed float mantissa precision and get pitch drift / harmonic distortion.

**Instead:** Use `double` for the phase accumulator and frequency calculations. The gen~ `phasor` operates at 64-bit internally.

### Anti-Pattern 5: Allocating Memory on the Audio Thread

**What:** Creating vectors, strings, or any heap-allocated objects in processBlock or renderSample.

**Why bad:** Heap allocation can block (mutex in allocator), causing audio glitches.

**Instead:** Pre-allocate everything in `prepareToPlay`. Use fixed-size arrays. The DSP engine for this synth needs no dynamic allocation at all.

## Suggested Build Order (Dependencies)

Build order follows the signal flow, with each layer testable independently:

```
Phase 1: Foundation
  PluginProcessor + APVTS (parameter tree)
  |-- Can verify: plugin loads in DAW, parameters visible to host

Phase 2: Core DSP Engine
  PhaseAccumulator + basic sine output
  |-- Depends on: Phase 1 (needs somewhere to live)
  |-- Can verify: plays a sine wave at correct pitch from MIDI note

Phase 3: Waveforms
  HarmonicBank (8 harmonics, individual levels) + Nyquist check
  PolyBLEPOscillator (saw, square, triangle)
  OutputSelector
  DCBlocker
  |-- Depends on: Phase 2 (needs phase accumulator)
  |-- Can verify: all 4 output modes produce correct waveforms

Phase 4: Modulation & Control
  Scan envelope (Gaussian window)
  Spectral tilt
  FMProcessor (internal LFO, lin/exp modes)
  AREnvelope + gate counting (legato)
  GlideProcessor (portamento)
  Coarse/fine tuning
  |-- Depends on: Phase 3 (modulates the waveform generators)
  |-- Can verify: compare against gen~ output for same parameter settings

Phase 5: GUI
  PluginEditor with all sections
  Parameter attachments
  |-- Depends on: Phase 1 (APVTS), Phase 4 (all parameters exist)
  |-- Can verify: visual match to Max patch layout, controls work

Phase 6: Polish & Validation
  A/B testing against gen~ output
  Performance profiling
  Universal binary build (Apple Silicon + Intel)
  |-- Depends on: everything
```

### Build Order Rationale

1. **Foundation first** because everything depends on PluginProcessor + APVTS existing.
2. **Phase accumulator before waveforms** because all waveform generators consume the master phase.
3. **Waveforms before modulation** because scan/tilt/FM modify waveform parameters -- you need the base waveforms working to hear modulation effects.
4. **Modulation before GUI** because the GUI is just knobs attached to parameters. The DSP must exist first to define what parameters exist.
5. **GUI after DSP** because you can test all DSP with hardcoded parameter values or the DAW's generic parameter editor. GUI is cosmetic until DSP is correct.
6. **Validation last** because A/B testing requires both the complete DSP chain and a way to set identical parameters.

## Scalability Considerations

Not applicable in the traditional sense (this is a single-voice desktop plugin), but relevant dimensions:

| Concern | Current Design | If Extended |
|---------|---------------|-------------|
| Polyphony | Single engine instance | Would need voice pool + allocation -- redesign HarmonicEngine as voice, add manager layer |
| More harmonics | Fixed 8 (matching Verbos) | Array-based, just increase count. Nyquist check prevents runaway |
| Additional waveforms | 4 outputs (harmonic, saw, sq, tri) | Add to OutputSelector enum, add generator to PolyBLEP |
| Oversampling | Not needed (<1% CPU) | Wrap engine in `juce::dsp::Oversampling` if FM aliasing becomes an issue at audio-rate FM |

## Key Architecture Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Skip juce::Synthesiser | Direct engine | Mono synth, gate counting for legato, simpler code |
| Single phase accumulator | Shared by all generators | Matches gen~ design, phase-coherent harmonics |
| double precision phase | 64-bit accumulator | Prevents pitch drift on sustained notes |
| Sample-accurate MIDI | Buffer splitting at MIDI timestamps | Matches gen~ timing accuracy |
| APVTS for all parameters | AudioProcessorValueTreeState | Thread-safe, host automation, state serialization |
| Render loop: per-sample | Not per-block for DSP | Required for FM at audio rate, smooth parameter changes |

## Sources

- JUCE AudioProcessor documentation (juce.com/doc) -- MEDIUM confidence (training data, not live-verified)
- JUCE Synthesiser tutorial (docs.juce.com) -- MEDIUM confidence (training data)
- JUCE AudioProcessorValueTreeState patterns -- HIGH confidence (well-established, stable API)
- gen~ codebox architecture inferred from PROJECT.md description -- HIGH confidence (direct project context)
- PolyBLEP algorithm -- HIGH confidence (well-documented DSP technique, stable for years)
- Monophonic synth gate counting pattern -- HIGH confidence (standard technique across frameworks)

**Note:** Web search and documentation fetch were unavailable during this research session. All architectural recommendations are based on training data knowledge of JUCE (which has been stable in this area for years -- the AudioProcessor/APVTS/Synthesiser patterns have not changed significantly since JUCE 5). Confidence is MEDIUM overall rather than HIGH because live verification was not possible.
