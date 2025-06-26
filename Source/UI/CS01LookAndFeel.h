#pragma once
#include "JuceHeader.h"

class CS01LookAndFeel : public juce::LookAndFeel_V4
{
public:
    CS01LookAndFeel();
    ~CS01LookAndFeel() override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos,
                           float minSliderPos,
                           float maxSliderPos,
                           const juce::Slider::SliderStyle,
                           juce::Slider&) override;

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional,
                           float rotaryStartAngle,
                           float rotaryEndAngle,
                           juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    juce::Font getLabelFont (juce::Label& label) override;
private:
};
