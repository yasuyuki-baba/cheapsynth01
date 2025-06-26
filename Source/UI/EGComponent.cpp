#include "EGComponent.h"
#include "../Parameters.h"

EGComponent::EGComponent(juce::AudioProcessorValueTreeState& apvts) : valueTreeState(apvts)
{
    attackSlider.setSliderStyle(juce::Slider::LinearVertical);
    attackSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(attackSlider);
    attackLabel.setText("A", juce::dontSendNotification);
    addAndMakeVisible(attackLabel);
    attackAttachment = std::make_unique<juce::SliderParameterAttachment>(
        *valueTreeState.getParameter(ParameterIds::attack), attackSlider);

    decaySlider.setSliderStyle(juce::Slider::LinearVertical);
    decaySlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(decaySlider);
    decayLabel.setText("D", juce::dontSendNotification);
    addAndMakeVisible(decayLabel);
    decayAttachment = std::make_unique<juce::SliderParameterAttachment>(
        *valueTreeState.getParameter(ParameterIds::decay), decaySlider);

    sustainSlider.setSliderStyle(juce::Slider::LinearVertical);
    sustainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(sustainSlider);
    sustainLabel.setText("S", juce::dontSendNotification);
    addAndMakeVisible(sustainLabel);
    sustainAttachment = std::make_unique<juce::SliderParameterAttachment>(
        *valueTreeState.getParameter(ParameterIds::sustain), sustainSlider);

    releaseSlider.setSliderStyle(juce::Slider::LinearVertical);
    releaseSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(releaseSlider);
    releaseLabel.setText("R", juce::dontSendNotification);
    addAndMakeVisible(releaseLabel);
    releaseAttachment = std::make_unique<juce::SliderParameterAttachment>(
        *valueTreeState.getParameter(ParameterIds::release), releaseSlider);
}

EGComponent::~EGComponent() {}

void EGComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white);
    g.setFont(15.0f);
    g.drawFittedText("EG", getLocalBounds(), juce::Justification::centredTop, 1);
}

void EGComponent::resized()
{
    juce::Grid grid;
    using Track = juce::Grid::TrackInfo;
    using Fr = juce::Grid::Fr;

    grid.templateRows    = { Track(Fr(5)), Track(Fr(1)) }; // Make slider area taller
    grid.templateColumns = { Track(Fr(1)), Track(Fr(1)), Track(Fr(1)), Track(Fr(1)) };

    grid.items = {
        juce::GridItem(attackSlider),
        juce::GridItem(decaySlider),
        juce::GridItem(sustainSlider),
        juce::GridItem(releaseSlider),
        juce::GridItem(attackLabel),
        juce::GridItem(decayLabel),
        juce::GridItem(sustainLabel),
        juce::GridItem(releaseLabel)
    };

    grid.performLayout(getLocalBounds().reduced(10).withTrimmedTop(20));
}
