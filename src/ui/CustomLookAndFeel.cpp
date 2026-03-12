#include "CustomLookAndFeel.h"

CustomLookAndFeel::CustomLookAndFeel()
{
    // ComboBox colours
    setColour (juce::ComboBox::textColourId, juce::Colours::white);
    setColour (juce::ComboBox::backgroundColourId, panelColour);
    setColour (juce::ComboBox::outlineColourId, accentTeal);

    // PopupMenu colours
    setColour (juce::PopupMenu::backgroundColourId, backgroundColour);
    setColour (juce::PopupMenu::textColourId, labelGray);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accentTeal);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);

    // Slider text box
    setColour (juce::Slider::textBoxTextColourId, labelGray);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

    // Label
    setColour (juce::Label::textColourId, labelGray);
}

void CustomLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float rotaryStartAngle,
                                           float rotaryEndAngle, juce::Slider& /*slider*/)
{
    auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (2.0f);
    auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    auto centreX = bounds.getCentreX();
    auto centreY = bounds.getCentreY();
    auto arcRadius = radius - 4.0f;
    auto lineWidth = 3.0f;

    // Background arc
    juce::Path backgroundArc;
    backgroundArc.addCentredArc (centreX, centreY, arcRadius, arcRadius, 0.0f,
                                  rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (panelColour);
    g.strokePath (backgroundArc, juce::PathStrokeType (lineWidth, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));

    // Value arc
    auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    juce::Path valueArc;
    valueArc.addCentredArc (centreX, centreY, arcRadius, arcRadius, 0.0f,
                             rotaryStartAngle, toAngle, true);
    g.setColour (accentTeal);
    g.strokePath (valueArc, juce::PathStrokeType (lineWidth, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

    // Indicator dot
    auto dotRadius = 3.0f;
    auto dotX = centreX + arcRadius * std::cos (toAngle - juce::MathConstants<float>::halfPi);
    auto dotY = centreY + arcRadius * std::sin (toAngle - juce::MathConstants<float>::halfPi);
    g.setColour (juce::Colours::white);
    g.fillEllipse (dotX - dotRadius, dotY - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);
}

void CustomLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                                           juce::Slider::SliderStyle style, juce::Slider& /*slider*/)
{
    if (style != juce::Slider::LinearVertical)
        return;

    auto trackWidth = 6.0f;
    auto trackX = (float) x + ((float) width - trackWidth) * 0.5f;
    auto trackBounds = juce::Rectangle<float> (trackX, (float) y, trackWidth, (float) height);

    // Track background
    g.setColour (panelColour);
    g.fillRoundedRectangle (trackBounds, 3.0f);

    // Teal fill from bottom up to slider position
    auto fillHeight = (float) (y + height) - sliderPos;
    if (fillHeight > 0.0f)
    {
        auto fillBounds = juce::Rectangle<float> (trackX, sliderPos, trackWidth, fillHeight);
        g.setColour (accentTeal);
        g.fillRoundedRectangle (fillBounds, 3.0f);
    }
}

void CustomLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                                       int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                                       juce::ComboBox& /*box*/)
{
    auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat();

    // Background
    g.setColour (panelColour);
    g.fillRoundedRectangle (bounds, 4.0f);

    // Teal outline
    g.setColour (accentTeal);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);

    // Dropdown arrow
    auto arrowZone = juce::Rectangle<float> ((float) width - 20.0f, 0.0f, 16.0f, (float) height);
    juce::Path arrow;
    arrow.addTriangle (arrowZone.getCentreX() - 4.0f, arrowZone.getCentreY() - 2.0f,
                       arrowZone.getCentreX() + 4.0f, arrowZone.getCentreY() - 2.0f,
                       arrowZone.getCentreX(), arrowZone.getCentreY() + 3.0f);
    g.setColour (juce::Colours::white);
    g.fillPath (arrow);
}

void CustomLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                                            bool /*isSeparator*/, bool isActive, bool isHighlighted,
                                            bool /*isTicked*/, bool /*hasSubMenu*/,
                                            const juce::String& text, const juce::String& /*shortcutKeyText*/,
                                            const juce::Drawable* /*icon*/, const juce::Colour* /*textColour*/)
{
    if (isHighlighted && isActive)
    {
        g.setColour (accentTeal);
        g.fillRect (area);
        g.setColour (juce::Colours::white);
    }
    else
    {
        g.setColour (backgroundColour);
        g.fillRect (area);
        g.setColour (labelGray);
    }

    g.drawText (text, area.reduced (8, 0), juce::Justification::centredLeft, true);
}

void CustomLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                const juce::Colour& bgColour,
                                                bool isMouseOverButton, bool isButtonDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    auto cornerSize = 4.0f;

    // Background: slightly brighten on hover/press
    auto bg = isButtonDown ? bgColour.brighter (0.2f)
            : isMouseOverButton ? bgColour.brighter (0.1f)
            : bgColour;
    g.setColour (bg);
    g.fillRoundedRectangle (bounds, cornerSize);

    // Teal outline
    g.setColour (accentTeal);
    g.drawRoundedRectangle (bounds, cornerSize, 1.0f);
}

void CustomLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    g.setColour (labelGray);
    g.setFont (label.getFont());
    g.drawText (label.getText(), label.getLocalBounds(), label.getJustificationType(), true);
}
