#include "CS01VCAComponent.h"
#include "../Parameters.h"

CS01VCAComponent::CS01VCAComponent(juce::AudioProcessorValueTreeState& apvts) : valueTreeState(apvts)
{
    vcaEgDepthSlider.setSliderStyle(juce::Slider::LinearVertical);
    vcaEgDepthSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(vcaEgDepthSlider);
    vcaEgDepthLabel.setText("EG DEPTH", juce::dontSendNotification);
    addAndMakeVisible(vcaEgDepthLabel);

    vcaEgDepthAttachment = std::make_unique<juce::SliderParameterAttachment>(
        *valueTreeState.getParameter(ParameterIds::vcaEgDepth), vcaEgDepthSlider);
}

CS01VCAComponent::~CS01VCAComponent() {}

void CS01VCAComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white);
    g.setFont(15.0f);
    g.drawFittedText("VCA", getLocalBounds(), juce::Justification::centredTop, 1);
}

void CS01VCAComponent::resized()
{
    juce::Grid grid;
    using Track = juce::Grid::TrackInfo;
    using Fr = juce::Grid::Fr;

    grid.templateRows    = { Track(Fr(5)), Track(Fr(1)) }; // Make slider area taller
    grid.templateColumns = { Track(Fr(1)) };

    grid.items = {
        juce::GridItem(vcaEgDepthSlider),
        juce::GridItem(vcaEgDepthLabel)
    };
    
    grid.performLayout(getLocalBounds().reduced(10).withTrimmedTop(20));
}
