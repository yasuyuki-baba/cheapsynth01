#include "CS01LookAndFeel.h"

// Color Palette Definitions (CS-01 Black Model Style)
const juce::Colour CS01LookAndFeel::Palette::background =
    juce::Colour::fromString("FF2b2b2b");  // Uniform Dark Grey (Charcoal)
const juce::Colour CS01LookAndFeel::Palette::panelBackground =
    juce::Colour::fromString("FF2b2b2b");  // Same as background
const juce::Colour CS01LookAndFeel::Palette::accentCyan =
    juce::Colour::fromString("FF00ffff");  // Cyan
const juce::Colour CS01LookAndFeel::Palette::accentMagenta =
    juce::Colour::fromString("FF00ffff");  // Cyan (Unified)
const juce::Colour CS01LookAndFeel::Palette::text =
    juce::Colour::fromString("FFffffff");  // White Text
const juce::Colour CS01LookAndFeel::Palette::textDim = juce::Colour::fromString("FF888888");
const juce::Colour CS01LookAndFeel::Palette::knobBase =
    juce::Colour::fromString("FF111111");  // Black Plastic

CS01LookAndFeel::CS01LookAndFeel() {
    setColour(juce::Label::textColourId, Palette::text);
    setColour(juce::Slider::textBoxTextColourId, Palette::text);
    setColour(juce::ToggleButton::textColourId, Palette::text);
    setColour(juce::ToggleButton::tickColourId, Palette::accentCyan);
    setColour(juce::ToggleButton::tickDisabledColourId, Palette::textDim);
}

CS01LookAndFeel::~CS01LookAndFeel() {}

void CS01LookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                       float sliderPos, float minSliderPos, float maxSliderPos,
                                       const juce::Slider::SliderStyle, juce::Slider& slider) {
    if (slider.getProperties().getWithDefault("performanceWheel", false)) {
        auto wheel = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                            static_cast<float>(width), static_cast<float>(height))
                         .withSizeKeepingCentre(juce::jmin(30.0f, static_cast<float>(width)),
                                                static_cast<float>(height));
        g.setColour(juce::Colours::black);
        g.fillRoundedRectangle(wheel.expanded(3.0f), 5.0f);
        juce::ColourGradient shading(juce::Colour(0xff171717), wheel.getX(), wheel.getY(),
                                     juce::Colour(0xff171717), wheel.getRight(), wheel.getY(),
                                     false);
        shading.addColour(0.5, juce::Colour(0xff666666));
        g.setGradientFill(shading);
        g.fillRoundedRectangle(wheel, 4.0f);
        g.setColour(juce::Colours::black.withAlpha(0.65f));
        for (float rib = wheel.getY() + 5.0f; rib < wheel.getBottom() - 3.0f; rib += 7.0f)
            g.drawHorizontalLine(juce::roundToInt(rib), wheel.getX() + 3.0f,
                                 wheel.getRight() - 3.0f);
        g.setColour(Palette::accentCyan);
        const auto indicatorY =
            juce::jlimit(wheel.getY() + 2.0f, wheel.getBottom() - 2.0f, sliderPos);
        g.fillRect(wheel.getX() + 2.0f, indicatorY - 1.0f, wheel.getWidth() - 4.0f, 2.0f);
        return;
    }
    // Flat Plastic Fader
    auto trackWidth = 4.0f;
    // JUCE already reserves space for the thumb in the supplied slider bounds.
    // Use those same endpoints so the cap stays on the slot at minimum/maximum.
    juce::Rectangle<float> track(x + (float)width * 0.5f - trackWidth * 0.5f, (float)y, trackWidth,
                                 (float)height);

    // Track (Black Slot)
    g.setColour(juce::Colours::black);
    g.fillRect(track);

    // Fader Cap (Rectangular, Light Grey Plastic)
    auto thumbWidth = 16.0f;
    auto thumbHeight = 24.0f;
    juce::Rectangle<float> thumb(x + (float)width * 0.5f - thumbWidth * 0.5f,
                                 sliderPos - thumbHeight / 2, thumbWidth, thumbHeight);

    g.setColour(juce::Colours::lightgrey);
    g.fillRect(thumb);

    // Cap Detail (Recessed Grip)
    g.setColour(juce::Colours::grey);
    g.fillRect(thumb.reduced(2.0f, 10.0f));

    // Indicator Line (Cyan Paint)
    g.setColour(Palette::accentCyan);
    g.fillRect(thumb.withHeight(2.0f).withCentre(thumb.getCentre()));
}

void CS01LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                       float sliderPosProportional, float rotaryStartAngle,
                                       float rotaryEndAngle, juce::Slider& slider) {
    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
    auto size = juce::jmin(bounds.getWidth(), bounds.getHeight());
    auto knobRect = bounds.withSizeKeepingCentre(size, size);

    auto radius = size / 2.0f;
    auto toAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // Tick Marks (Cyan/White)
    g.setColour(juce::Colours::white.withAlpha(0.8f));
    auto centre = knobRect.getCentre();
    auto numTicks = 10;
    for (int i = 0; i <= numTicks; ++i) {
        float angle =
            rotaryStartAngle + (rotaryEndAngle - rotaryStartAngle) * (float)i / (float)numTicks;
        float tickLen = 5.0f;
        juce::Line<float> tick(centre.getPointOnCircumference(radius + 4.0f, angle),
                               centre.getPointOnCircumference(radius + 4.0f + tickLen, angle));
        g.drawLine(tick, 1.0f);
    }

    // Knob Body (Black Plastic)
    g.setColour(Palette::knobBase);
    g.fillEllipse(knobRect);

    // Knob Top (Slightly lighter for definition)
    g.setColour(Palette::knobBase.brighter(0.1f));
    g.fillEllipse(knobRect.reduced(3.0f));

    // Indicator Line (Cyan Paint)
    juce::Path p;
    p.startNewSubPath(centre);
    p.lineTo(centre.getPointOnCircumference(radius * 0.9f, toAngle));
    g.setColour(Palette::accentCyan);
    g.strokePath(p, juce::PathStrokeType(3.0f));
}

juce::Font CS01LookAndFeel::getLabelFont(juce::Label& label) {
    return juce::Font("Helvetica", 12.0f, juce::Font::bold);
}

void CS01LookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                       bool shouldDrawButtonAsHighlighted,
                                       bool shouldDrawButtonAsDown) {
    // Compact switch: the full component remains clickable.
    auto area = button.getLocalBounds().toFloat().reduced(2.0f);
    auto indicator = area.removeFromLeft(14.0f).withSizeKeepingCentre(12.0f, 10.0f);
    g.setColour(juce::Colours::black);
    g.fillRect(indicator.expanded(1.0f));
    g.setColour(button.getToggleState() ? Palette::accentCyan : Palette::textDim);
    g.fillRect(indicator);
    if (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown) {
        g.setColour(Palette::text.withAlpha(0.1f));
        g.fillRect(button.getLocalBounds());
    }
    g.setColour(button.isEnabled() ? Palette::text : Palette::textDim);
    g.setFont(11.0f);
    g.drawFittedText(button.getButtonText(), area.reduced(3.0f, 0.0f).toNearestInt(),
                     juce::Justification::centredLeft, 1);
}

void CS01LookAndFeel::drawSectionBackground(juce::Graphics& g, juce::Rectangle<int> bounds,
                                            const juce::String& title) {
    // Minimalist Section Header
    // Just a white line and title.

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font("Helvetica", 15.0f, juce::Font::bold));

    auto titleWidth = juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), title);

    // Draw Title
    g.drawText(title, bounds.getX(), bounds.getY(), titleWidth + 10, 20,
               juce::Justification::centredLeft, false);

    // Draw Line after title
    g.drawLine(bounds.getX() + titleWidth + 10.0f, bounds.getY() + 10.0f, bounds.getRight() - 10.0f,
               bounds.getY() + 10.0f, 2.0f);

    // Vertical Divider (Right side)
    g.setColour(juce::Colours::white.withAlpha(0.3f));
    g.drawLine(bounds.getRight(), bounds.getY(), bounds.getRight(), bounds.getBottom(), 1.0f);
}
