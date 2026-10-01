#pragma once
#include "JuceHeader.h"

class CS01LookAndFeel : public juce::LookAndFeel_V4 {
   public:
    CS01LookAndFeel();
    ~CS01LookAndFeel() override;

    // Color Palette
    struct Palette {
        static const juce::Colour background;
        static const juce::Colour panelBackground;
        static const juce::Colour accentCyan;
        static const juce::Colour accentMagenta;
        static const juce::Colour text;
        static const juce::Colour textDim;
        static const juce::Colour knobBase;
    };

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override;

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                          float minSliderPos, float maxSliderPos, const juce::Slider::SliderStyle,
                          juce::Slider& slider) override;

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    // Custom helper for drawing section backgrounds
    static void drawSectionBackground(juce::Graphics& g, juce::Rectangle<int> bounds,
                                      const juce::String& title);

    juce::Font getLabelFont(juce::Label& label) override;

   private:
};
