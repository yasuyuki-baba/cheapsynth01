#include "LFOComponent.h"
#include "../Parameters.h"

LFOComponent::LFOComponent(juce::AudioProcessorValueTreeState& apvts) : valueTreeState(apvts)
{
    lfoSpeedSlider.setSliderStyle(juce::Slider::LinearVertical);
    lfoSpeedSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(lfoSpeedSlider);
    lfoSpeedLabel.setText("SPEED", juce::dontSendNotification);
    addAndMakeVisible(lfoSpeedLabel);

    lfoSpeedAttachment = std::make_unique<juce::SliderParameterAttachment>(
        *valueTreeState.getParameter(ParameterIds::lfoSpeed), lfoSpeedSlider);
}

LFOComponent::~LFOComponent() {}

void LFOComponent::paint(juce::Graphics& g)
{
    // The LookAndFeel will now handle the drawing of the components.
    // We can keep this for background and titles if needed.
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white);
    g.setFont(15.0f);
    g.drawFittedText("LFO", getLocalBounds(), juce::Justification::centredTop, 1);
}

void LFOComponent::resized()
{
    juce::Grid grid;
    using Track = juce::Grid::TrackInfo;
    using Fr = juce::Grid::Fr;

    grid.templateRows    = { Track(Fr(5)), Track(Fr(1)) }; // Make slider area taller
    grid.templateColumns = { Track(Fr(1)) };

    grid.items = {
        juce::GridItem(lfoSpeedSlider),
        juce::GridItem(lfoSpeedLabel)
    };

    grid.performLayout(getLocalBounds().reduced(10).withTrimmedTop(20));
}
