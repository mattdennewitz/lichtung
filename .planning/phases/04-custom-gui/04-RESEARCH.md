# Phase 4: Custom GUI - Research

**Researched:** 2026-03-11
**Domain:** JUCE custom plugin editor, LookAndFeel, parameter attachments
**Confidence:** HIGH

## Summary

This phase replaces the `GenericAudioProcessorEditor` with a custom `PluginEditor` featuring five sections (Pitch, Harmonics, Scan/Tilt, FM, Output) in a two-row layout. The work is entirely UI — no DSP changes. All 21 parameters already exist in APVTS with proper groups, ranges, and defaults. The `juce_audio_utils` module (already linked) transitively provides all required GUI classes through its dependency chain (`juce_audio_processors` -> `juce_gui_extra` -> `juce_gui_basics`).

The implementation requires three new files: `PluginEditor.h`, `PluginEditor.cpp` (the custom editor), and a custom `LookAndFeel` subclass for the flat dark aesthetic. JUCE's `SliderAttachment` and `ComboBoxAttachment` handle bidirectional parameter binding. Resizable editors with fixed aspect ratio are well-supported via `setResizeLimits()` + `getConstrainer()->setFixedAspectRatio()`.

**Primary recommendation:** Subclass `LookAndFeel_V4` for custom drawing, use `AudioProcessorValueTreeState::SliderAttachment`/`ComboBoxAttachment` for all parameter binding, and implement resizing via `setResizeLimits` with a fixed aspect ratio constrainer.

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **Visual style:** Modern flat aesthetic — dark navy background (#1a1a2e), cyan/teal accent (#00d4aa), light gray labels (#cccccc), subtle panel cards (#222244). Section headers centered. Clean sans-serif labels.
- **Section arrangement:** Two-row layout. Top row: Pitch (left, narrow) + Harmonics (right, wide — 8 faders). Bottom row: Scan/Tilt (left) + FM (center) + Output (right). Envelope controls (Attack, Release) live inside the Output section. Knob sizes varied by importance. Parameter values always visible as text below each knob/fader.
- **Harmonic faders:** 8 vertical faders with solid teal fill. Labeled with harmonic number (1-8) on top and frequency ratio (1x-8x) below. Numeric value readout (0.00-1.00) below each fader. Medium height (~60-70% of section).
- **Output selector:** Dropdown menu (ComboBox) for 4 output modes. Full names: "Harmonic Mix", "Triangle", "Sawtooth", "Square". No waveform icons.
- **Knob interaction:** Vertical drag style (JUCE RotaryVerticalDrag). Double-click resets to default. No modifier-key fine-tuning.
- **Branding:** No plugin name or manufacturer name in editor. Controls-only.
- **Window dimensions:** Base size 700x450 pixels. Fixed aspect ratio. Resizable 75%-150% scale (525x338 to 1050x675).

### Claude's Discretion
- Exact padding and spacing between controls
- Font choice and sizes (within sans-serif constraint)
- Exact knob arc range and indicator style
- Section width proportions within the two-row layout
- Fader thumb/track visual details

### Deferred Ideas (OUT OF SCOPE)
None — discussion stayed within phase scope.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| GUI-01 | 5-section layout: Pitch, Harmonics, Scan/Tilt, FM, Output | Two-row FlexBox or manual `setBounds()` layout in `resized()`. Sections drawn as rounded rectangles via LookAndFeel or paint(). |
| GUI-02 | 8 vertical faders for harmonic levels | `juce::Slider` with `LinearVertical` style, custom `drawLinearSlider()` override for teal fill. `SliderAttachment` to `harm_1`..`harm_8`. |
| GUI-03 | Rotary knobs for scan center, scan width, tilt, FM controls, envelope, tuning | `juce::Slider` with `RotaryVerticalDrag` style, custom `drawRotarySlider()` override. `SliderAttachment` to each parameter ID. |
| GUI-04 | Output mode selector (menu or segmented control) | `juce::ComboBox` with `ComboBoxAttachment` to `output_select` parameter. Items already defined in `AudioParameterChoice`. |
| GUI-05 | Sensibly sized fixed window (~700px wide) | `setSize(700, 450)`, `setResizable(true, true)`, `setResizeLimits(525, 338, 1050, 675)`, `getConstrainer()->setFixedAspectRatio(700.0/450.0)`. |
</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| juce::AudioProcessorEditor | JUCE 8.0.12 | Base class for plugin editor | Standard JUCE plugin GUI base class |
| juce::LookAndFeel_V4 | JUCE 8.0.12 | Custom drawing for sliders, knobs, combo boxes | Provides all virtual draw methods; subclass and override only what's needed |
| juce::Slider | JUCE 8.0.12 | Faders (LinearVertical) and knobs (RotaryVerticalDrag) | Standard JUCE control for continuous parameters |
| juce::ComboBox | JUCE 8.0.12 | Output mode dropdown | Standard JUCE dropdown for choice parameters |
| APVTS::SliderAttachment | JUCE 8.0.12 | Bidirectional slider-to-parameter binding | Handles value sync, undo, automation in one line |
| APVTS::ComboBoxAttachment | JUCE 8.0.12 | Bidirectional combobox-to-parameter binding | Maps combo items linearly across parameter range |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| juce::Label | JUCE 8.0.12 | Section headers and parameter labels | Text display for all labels |
| juce::Graphics | JUCE 8.0.12 | Custom painting (section backgrounds, fader fills) | In `paint()` and LookAndFeel overrides |
| juce::ComponentBoundsConstrainer | JUCE 8.0.12 | Fixed aspect ratio resizing | Already built into AudioProcessorEditor via `getConstrainer()` |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| LookAndFeel_V4 subclass | Custom Component subclasses with paint() | LookAndFeel is cleaner — one class controls all drawing, components stay standard |
| Manual setBounds() layout | juce::FlexBox / juce::Grid | FlexBox adds complexity; with fixed aspect ratio and known layout, manual bounds are simpler and more predictable |
| juce::TextButton for output select | juce::ComboBox | User decision locked ComboBox; TextButton bank would need custom attachment logic |

**Installation:** No additional dependencies. `juce_audio_utils` (already linked in CMakeLists.txt) transitively provides `juce_gui_basics` and `juce_gui_extra` through its dependency chain.

## Architecture Patterns

### Recommended Project Structure
```
src/
├── PluginProcessor.h/.cpp     # Existing — only change: createEditor() returns new PluginEditor
├── PluginEditor.h/.cpp        # NEW — custom editor with 5-section layout
├── ui/
│   └── CustomLookAndFeel.h/.cpp  # NEW — flat dark LookAndFeel_V4 subclass
└── dsp/
    └── HarmonicEngine.h/.cpp  # Existing — no changes
```

### Pattern 1: Custom AudioProcessorEditor
**What:** Subclass `AudioProcessorEditor`, own all child components, attach to APVTS in constructor.
**When to use:** Always for custom plugin GUIs.
**Example:**
```cpp
// PluginEditor.h
class PluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PluginProcessor& processorRef;
    CustomLookAndFeel customLookAndFeel;

    // Sliders for knobs and faders
    juce::Slider coarseTuneSlider, fineTuneSlider, glideSlider;
    juce::Slider harmonicSliders[8];
    juce::Slider scanCenterSlider, scanWidthSlider, spectralTiltSlider;
    juce::Slider linFmDepthSlider, expFmDepthSlider, fmRateSlider;
    juce::Slider attackSlider, releaseSlider, masterLevelSlider;
    juce::ComboBox outputSelectBox;

    // Attachments (must be declared AFTER the components they attach to)
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> coarseTuneAttach;
    // ... one per slider
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> outputSelectAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
```

### Pattern 2: LookAndFeel_V4 Subclass for Flat Dark Theme
**What:** Override `drawRotarySlider()`, `drawLinearSlider()`, `drawComboBox()` to match the dark flat aesthetic.
**When to use:** When you need consistent custom drawing across all controls.
**Example:**
```cpp
// CustomLookAndFeel.h
class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CustomLookAndFeel();

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider& slider) override;

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle style, juce::Slider& slider) override;

    void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox& box) override;
};
```

### Pattern 3: Resizable Editor with Fixed Aspect Ratio
**What:** Enable host and user resizing with constrained aspect ratio and min/max bounds.
**When to use:** When the user wants scalable UI with consistent proportions.
**Example:**
```cpp
// In PluginEditor constructor:
setSize (700, 450);
setResizable (true, true);
setResizeLimits (525, 338, 1050, 675);  // 75%-150% of base
getConstrainer()->setFixedAspectRatio (700.0 / 450.0);
```

### Pattern 4: Attachment Lifecycle
**What:** APVTS attachments must be destroyed BEFORE the components they attach to.
**When to use:** Always — this is a hard requirement.
**Key rule:** Declare attachments as members AFTER the slider/combobox members. C++ destroys members in reverse declaration order, so attachments die first.

### Anti-Patterns to Avoid
- **Attaching to wrong parameter ID:** Parameter IDs are string-based. A typo silently creates a non-functional attachment. Use constants or verify against APVTS parameter list.
- **Calling setSize() before setResizeLimits():** The constrainer must be configured before the initial size is set; otherwise the first resize check may misbehave. Call `setResizeLimits` and `setFixedAspectRatio` before `setSize`.
- **Scaling children manually in resized():** With fixed aspect ratio, use `getLocalBounds()` and proportional math. Do not hard-code pixel positions.
- **Creating attachments before addAndMakeVisible:** The component should be fully configured (style, range, etc.) before creating the attachment.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Parameter sync | Manual value listeners | `SliderAttachment` / `ComboBoxAttachment` | Handles undo, automation, thread safety, value normalization |
| Resize constraining | Manual bounds checking | `setResizeLimits()` + `setFixedAspectRatio()` | Built-in, host-compatible, handles all edge cases |
| Control drawing | Custom Component subclasses per control type | `LookAndFeel_V4` overrides | One class handles all drawing; controls remain standard JUCE widgets |
| Double-click reset | Manual mouse listener | `Slider::setDoubleClickReturnValue(true, defaultVal)` | Built-in JUCE Slider feature |
| Value text display | Separate labels synced via listeners | `Slider::setTextBoxStyle()` with `TextBoxBelow` | Built-in; auto-syncs with slider value |

**Key insight:** JUCE's component system already handles 90% of plugin GUI needs. The custom work is purely visual (LookAndFeel overrides) and layout (resized()). Don't rebuild what JUCE provides.

## Common Pitfalls

### Pitfall 1: Attachment Destruction Order
**What goes wrong:** Crash on plugin close — attachment tries to detach from destroyed slider.
**Why it happens:** Attachments declared before sliders in header; C++ destroys in reverse order.
**How to avoid:** Declare all `juce::Slider` and `juce::ComboBox` members BEFORE their `std::unique_ptr<Attachment>` members.
**Warning signs:** Crash in destructor, JUCE assertion failure about dangling pointers.

### Pitfall 2: ComboBox Item Indexing
**What goes wrong:** ComboBox shows wrong selection or off-by-one errors.
**Why it happens:** `ComboBoxAttachment` maps items linearly across the parameter range. `AudioParameterChoice` with N items uses range [0, N-1]. ComboBox item IDs are 1-based by default, but attachment handles this. Adding items manually after attachment can cause mismatch.
**How to avoid:** Let the `AudioParameterChoice` define the items. Use `ComboBoxAttachment` which populates the ComboBox automatically — OR — add items with `addItemList()` before creating the attachment.
**Warning signs:** First item never selectable, wrong mode plays.

### Pitfall 3: Resized() Not Scaling Proportionally
**What goes wrong:** Controls overlap or have gaps at non-default sizes.
**Why it happens:** Hard-coded pixel positions instead of proportional layout.
**How to avoid:** In `resized()`, derive all positions from `getLocalBounds()` using proportional fractions (e.g., `area.removeFromTop (area.getHeight() * 0.55f)` for top row).
**Warning signs:** Layout breaks at min or max scale.

### Pitfall 4: Forgetting setLookAndFeel(nullptr) in Destructor
**What goes wrong:** Dangling LookAndFeel pointer after editor is destroyed if LookAndFeel was set on child components.
**Why it happens:** Components store a raw pointer to LookAndFeel. If the LookAndFeel is a member of the editor, it may be destroyed before child components try to un-reference it.
**How to avoid:** In the editor destructor, call `setLookAndFeel(nullptr)` on the editor itself (which propagates to children). Alternatively, since the LookAndFeel is a member of the editor and children are also members, ensure LookAndFeel is declared before child components so it is destroyed last.
**Warning signs:** Crash on plugin window close.

### Pitfall 5: LinearVertical Fader Text Overlap
**What goes wrong:** Fader value text overlaps the fader track or labels.
**Why it happens:** Default text box position doesn't account for custom layout with labels above and below.
**How to avoid:** Use `Slider::setTextBoxStyle(Slider::TextBoxBelow, false, width, height)` with explicit dimensions. Reserve space in layout for the text box.
**Warning signs:** Text clipped or overlapping track.

## Code Examples

### Creating and Configuring a Rotary Knob
```cpp
// Source: JUCE Slider API documentation
auto& slider = scanCenterSlider;
slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
slider.setDoubleClickReturnValue (true, 0.5f);  // default from APVTS
addAndMakeVisible (slider);
scanCenterAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
    processorRef.apvts, "scan_center", slider);
```

### Creating a Vertical Fader
```cpp
// Source: JUCE Slider API documentation
auto& fader = harmonicSliders[i];
fader.setSliderStyle (juce::Slider::LinearVertical);
fader.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 40, 16);
fader.setDoubleClickReturnValue (true, (i == 0) ? 1.0f : 0.0f);
addAndMakeVisible (fader);
harmonicAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
    processorRef.apvts, "harm_" + juce::String (i + 1), fader);
```

### Creating the ComboBox
```cpp
// Source: JUCE ComboBox/APVTS documentation
outputSelectBox.addItemList (
    juce::StringArray { "Harmonic Mix", "Triangle", "Sawtooth", "Square" }, 1);
addAndMakeVisible (outputSelectBox);
outputSelectAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
    processorRef.apvts, "output_select", outputSelectBox);
```

### Proportional Layout in resized()
```cpp
void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced (8);

    // Top row: ~55% height
    auto topRow = area.removeFromTop (static_cast<int> (area.getHeight() * 0.55f));
    auto pitchArea = topRow.removeFromLeft (static_cast<int> (topRow.getWidth() * 0.22f));
    auto harmonicsArea = topRow;  // remaining width

    // Bottom row: remaining ~45%
    auto bottomRow = area;
    int thirdWidth = bottomRow.getWidth() / 3;
    auto scanArea = bottomRow.removeFromLeft (thirdWidth);
    auto fmArea = bottomRow.removeFromLeft (thirdWidth);
    auto outputArea = bottomRow;

    // Then position controls within each section area...
}
```

### Custom drawRotarySlider (flat dark theme)
```cpp
// Source: JUCE LookAndFeel_V4 source / tutorial
void CustomLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float rotaryStartAngle,
                                           float rotaryEndAngle, juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float> (x, y, width, height).reduced (2.0f);
    auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    auto centreX = bounds.getCentreX();
    auto centreY = bounds.getCentreY();
    auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Background arc
    juce::Path bgArc;
    bgArc.addCentredArc (centreX, centreY, radius, radius, 0.0f,
                          rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour (0xff222244));  // panel color
    g.strokePath (bgArc, juce::PathStrokeType (3.0f));

    // Value arc
    juce::Path valueArc;
    valueArc.addCentredArc (centreX, centreY, radius, radius, 0.0f,
                             rotaryStartAngle, angle, true);
    g.setColour (juce::Colour (0xff00d4aa));  // teal accent
    g.strokePath (valueArc, juce::PathStrokeType (3.0f));

    // Indicator dot
    auto thumbRadius = 4.0f;
    auto thumbX = centreX + (radius - 6.0f) * std::cos (angle - juce::MathConstants<float>::halfPi);
    auto thumbY = centreY + (radius - 6.0f) * std::sin (angle - juce::MathConstants<float>::halfPi);
    g.setColour (juce::Colours::white);
    g.fillEllipse (thumbX - thumbRadius, thumbY - thumbRadius, thumbRadius * 2.0f, thumbRadius * 2.0f);
}
```

### Custom drawLinearSlider (vertical fader with teal fill)
```cpp
void CustomLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                                           juce::Slider::SliderStyle style, juce::Slider& /*slider*/)
{
    if (style == juce::Slider::LinearVertical)
    {
        auto trackWidth = 6.0f;
        auto trackX = x + width * 0.5f - trackWidth * 0.5f;

        // Track background
        g.setColour (juce::Colour (0xff222244));
        g.fillRoundedRectangle (trackX, (float) y, trackWidth, (float) height, 3.0f);

        // Filled portion (from bottom up)
        auto filledHeight = (float) (y + height) - sliderPos;
        g.setColour (juce::Colour (0xff00d4aa));
        g.fillRoundedRectangle (trackX, sliderPos, trackWidth, filledHeight, 3.0f);
    }
}
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| LookAndFeel_V2/V3 | LookAndFeel_V4 | JUCE 5+ | V4 is the modern default with flat styling as baseline |
| ParameterAttachment (raw) | SliderAttachment / ComboBoxAttachment | JUCE 5+ | Higher-level, less boilerplate |
| Non-resizable editors | setResizable + setResizeLimits + setFixedAspectRatio | Improved in JUCE 7-8 | Better host compatibility, especially VST3/AU |

**Deprecated/outdated:**
- `LookAndFeel_V1`/`V2`: Still functional but look dated. V4 is standard.
- `Slider::Listener` for parameter sync: Replaced by `SliderAttachment` which handles thread safety and undo.

## Open Questions

1. **CoarseTune as Rotary Knob**
   - What we know: `coarse_tune` is an `AudioParameterInt` (-24 to 24). Sliders work fine with int params via SliderAttachment.
   - What's unclear: Whether to display as rotary knob or a different control (it's discrete semitones).
   - Recommendation: Use rotary knob same as others. The attachment handles int snapping. Set `Slider::setNumDecimalPlacesToDisplay(0)` for clean display.

2. **ComboBox Population Method**
   - What we know: `AudioParameterChoice` already stores the string array. `ComboBoxAttachment` can work with manually added items or auto-populated items.
   - What's unclear: Whether `ComboBoxAttachment` auto-populates the ComboBox in JUCE 8, or if items must be added first.
   - Recommendation: Manually add items with `addItemList()` before creating the attachment. This is the safe, well-documented path.

## Sources

### Primary (HIGH confidence)
- JUCE AudioProcessorEditor API: https://docs.juce.com/master/classjuce_1_1AudioProcessorEditor.html — constructor, setResizable, setResizeLimits, getConstrainer
- JUCE LookAndFeel_V4 source: https://github.com/juce-framework/JUCE/blob/master/modules/juce_gui_basics/lookandfeel/juce_LookAndFeel_V4.cpp — drawRotarySlider, drawLinearSlider, drawComboBox signatures
- JUCE Slider API: https://docs.juce.com/master/classSlider.html — SliderStyle, TextBoxStyle, setDoubleClickReturnValue
- JUCE ComboBoxAttachment: https://docs.juce.com/master/classjuce_1_1AudioProcessorValueTreeState_1_1ComboBoxAttachment.html — linear mapping behavior

### Secondary (MEDIUM confidence)
- JUCE LookAndFeel tutorial: https://juce.com/tutorials/tutorial_look_and_feel_customisation/ — custom draw override patterns
- JUCE Forum resizable plugin: https://forum.juce.com/t/resizable-plugin-how-to/45043 — setResizable + setFixedAspectRatio pattern
- JUCE Plugin tutorial: https://docs.juce.com/master/tutorial_code_basic_plugin.html — basic editor setup

### Tertiary (LOW confidence)
- None — all findings verified with official sources.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — JUCE built-in classes, verified against official API docs and source code
- Architecture: HIGH — well-established JUCE plugin editor patterns, multiple official examples
- Pitfalls: HIGH — attachment lifecycle and LookAndFeel ownership are extensively documented in JUCE forums and docs

**Research date:** 2026-03-11
**Valid until:** 2026-04-11 (stable domain, JUCE 8.0.12 is pinned)
