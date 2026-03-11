# Phase 4: Custom GUI - Context

**Gathered:** 2026-03-11
**Status:** Ready for planning

<domain>
## Phase Boundary

Replace the generic JUCE parameter editor with a custom 5-section plugin editor window. All parameters already exist in APVTS — this phase is purely visual/interaction, no DSP changes.

</domain>

<decisions>
## Implementation Decisions

### Visual style
- Modern flat aesthetic — solid dark background, flat controls, no textures or gradients
- Dark navy background (#1a1a2e)
- Cool cyan/teal accent color (#00d4aa) for active controls, fader fills, knob indicators
- Light gray (#cccccc) for labels and text
- Subtle panel cards (#222244) for section boundaries — slightly lighter rounded rectangles on the dark background
- Section headers centered at top of each section panel
- Clean sans-serif labels throughout

### Section arrangement
- Two-row layout:
  - **Top row:** Pitch (left, narrow) + Harmonics (right, wide — 8 faders)
  - **Bottom row:** Scan/Tilt (left) + FM (center) + Output (right)
- Envelope controls (Attack, Release) live inside the Output section
- Knob sizes varied by importance — key knobs (scan center, master level) slightly larger
- Parameter values always visible as text below each knob/fader

### Harmonic faders
- 8 vertical faders with solid teal fill
- Labeled with both harmonic number (1-8) on top and frequency ratio (1x-8x) below
- Numeric value readout (0.00-1.00) below each fader
- Medium height (~60-70% of section) with breathing room for labels and spacing

### Output selector
- Dropdown menu (ComboBox) for the 4 output modes
- Full names: "Harmonic Mix", "Triangle", "Sawtooth", "Square"
- No waveform icons — text is sufficient
- Master level knob same size and color as other knobs (no special distinction)

### Knob interaction
- Vertical drag style (JUCE RotaryVerticalDrag) — drag up to increase, down to decrease
- Double-click resets knob/fader to default value
- No modifier-key fine-tuning — single drag sensitivity only

### Branding
- No plugin name shown in the editor
- No manufacturer name shown
- Controls-only editor — DAW provides plugin identification in its own chrome

### Window dimensions
- Base size: 700 x 450 pixels
- Fixed aspect ratio
- Resizable: 75%-150% scale (525x338 to 1050x675)

### Claude's Discretion
- Exact padding and spacing between controls
- Font choice and sizes (within sans-serif constraint)
- Exact knob arc range and indicator style
- Section width proportions within the two-row layout
- Fader thumb/track visual details

</decisions>

<specifics>
## Specific Ideas

- Modern flat look inspired by Vital and Surge XT — clean, not skeuomorphic
- Two-row grouping follows signal flow: generation (top) then modification (bottom)
- Values always visible — no hover-only display

</specifics>

<code_context>
## Existing Code Insights

### Reusable Assets
- `PluginProcessor::apvts` — All 21 parameters already registered with proper groups, ranges, and defaults
- `juce::AudioProcessorValueTreeState` parameter groups match the 5 GUI sections (pitch, harmonics, scan, fm, envelope+output)
- Parameter IDs well-defined: harm_1-8, scan_center, scan_width, spectral_tilt, etc.

### Established Patterns
- JUCE 8.0.12 via FetchContent — use JUCE LookAndFeel for custom drawing
- `createEditor()` currently returns `GenericAudioProcessorEditor` — replace with custom `PluginEditor`
- `juce_audio_utils` already linked (provides `AudioProcessorEditor` base)

### Integration Points
- `PluginProcessor::createEditor()` — swap GenericAudioProcessorEditor for custom PluginEditor
- `PluginProcessor::apvts` (public) — attach SliderAttachment/ComboBoxAttachment to controls
- `hasEditor()` already returns true

</code_context>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 04-custom-gui*
*Context gathered: 2026-03-11*
