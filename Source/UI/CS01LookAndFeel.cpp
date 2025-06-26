#include "CS01LookAndFeel.h"

CS01LookAndFeel::CS01LookAndFeel()
{
}

CS01LookAndFeel::~CS01LookAndFeel()
{
}

void CS01LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                       float sliderPos,
                                       float minSliderPos,
                                       float maxSliderPos,
                                       const juce::Slider::SliderStyle,
                                       juce::Slider& slider)
{
    // Slider track
    auto trackWidth = juce::jmin (6.0f, (float) width * 0.25f);
    juce::Rectangle<float> track (x + (float) width * 0.5f - trackWidth * 0.5f, (float) y, trackWidth, (float) height);
    g.setColour (juce::Colours::darkgrey);
    g.fillRect (track);

    // Slider thumb
    auto thumbHeight = trackWidth * 2.0f;
    juce::Rectangle<float> thumb (track.getX(), sliderPos - thumbHeight / 2, trackWidth, thumbHeight);
    g.setColour (juce::Colours::lightgrey);
    g.fillRect (thumb);
}

void CS01LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
{
    auto dirtyBounds = juce::Rectangle<int>(x, y, width, height).toFloat();
    auto size = juce::jmin(dirtyBounds.getWidth(), dirtyBounds.getHeight());
    auto bounds = dirtyBounds.withSizeKeepingCentre(size, size).reduced(10);

    auto radius = size / 2.0f - 10.0f;
    auto toAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    auto lineW = juce::jmin(4.0f, radius * 0.5f);
    auto arcRadius = radius - lineW * 0.5f;

    // Background
    g.setColour(juce::Colours::darkgrey);
    g.fillEllipse(bounds);

    // Knob line
    juce::Path p;
    p.addCentredArc(bounds.getCentreX(), bounds.getCentreY(), arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(juce::Colours::grey);
    g.strokePath(p, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Current value line
    juce::Path valueArc;
    valueArc.addCentredArc(bounds.getCentreX(), bounds.getCentreY(), arcRadius, arcRadius, 0.0f, rotaryStartAngle, toAngle, true);
    g.setColour(juce::Colours::lightblue);
    g.strokePath(valueArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Pointer
    auto thumbWidth = lineW * 2.0f;
    juce::Point<float> thumbPoint(bounds.getCentreX() + (radius - thumbWidth * 0.5f) * std::cos(toAngle - juce::MathConstants<float>::halfPi),
                                  bounds.getCentreY() + (radius - thumbWidth * 0.5f) * std::sin(toAngle - juce::MathConstants<float>::halfPi));
    g.setColour(juce::Colours::white);
    g.fillEllipse(juce::Rectangle<float>(thumbWidth, thumbWidth).withCentre(thumbPoint));
}

juce::Font CS01LookAndFeel::getLabelFont(juce::Label& label)
{
    return juce::FontOptions(juce::jmin(14.0f, (float)label.getHeight() * 0.8f));
}

void CS01LookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    LookAndFeel_V4::drawToggleButton(g, button, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
}
