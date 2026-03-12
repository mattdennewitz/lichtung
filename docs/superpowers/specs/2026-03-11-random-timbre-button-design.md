# Random Timbre Button

## Summary

A button in the Harmonics panel header that randomizes 11 timbre parameters with musically weighted distributions.

## Parameters Affected

**Harmonic faders (8):** Amplitude = `random^(n * 0.5)` where n is harmonic number (1-8). Lower harmonics get more uniform spread; upper harmonics skew toward lower values. Any harmonic can hit zero.

**Scan center:** Uniform 0-1.

**Scan width:** `0.3 + random * 0.7` (range 0.3-1.0). Prevents narrow widths that mute harmonics.

**Spectral tilt:** `random * 1.0 - 0.5` (range -0.5 to +0.5). Avoids extremes that kill harmonic regions.

## UI

Small teal-outlined button labeled "?" in the Harmonics panel header row, right-aligned opposite the "HARMONICS" title. Styled consistently with CustomLookAndFeel.

## Implementation

- Button click handler calls `param->setValueNotifyingHost(normalizedValue)` for each parameter
- Uses APVTS normalized values so automation and DAW undo work correctly
- Random source: `juce::Random::getSystemRandom()`
- No new DSP parameters, no new APVTS entries
- No processBlock changes

## Files Changed

- `src/PluginEditor.h` — add button member, click handler declaration
- `src/PluginEditor.cpp` — button setup, layout in harmonics header, randomization logic
