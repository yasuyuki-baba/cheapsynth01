#include "VCFComponent.h"
#include "../Parameters.h"

VCFComponent::VCFComponent(juce::AudioProcessorValueTreeState& apvts) : valueTreeState(apvts)
{
    cutoffSlider.setSliderStyle(juce::Slider::LinearVertical);
    cutoffSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(cutoffSlider);
    cutoffLabel.setText("CUTOFF", juce::dontSendNotification);
    addAndMakeVisible(cutoffLabel);
    
    resonanceButton.setButtonText("RES");
    addAndMakeVisible(resonanceButton);
    resonanceLabel.setText("ON/OFF", juce::dontSendNotification);
    addAndMakeVisible(resonanceLabel);
    
    vcfEgDepthSlider.setSliderStyle(juce::Slider::LinearVertical);
    vcfEgDepthSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(vcfEgDepthSlider);
    vcfEgDepthLabel.setText("EG DEPTH", juce::dontSendNotification);
    addAndMakeVisible(vcfEgDepthLabel);

    cutoffAttachment = std::make_unique<juce::SliderParameterAttachment>(
        *valueTreeState.getParameter(ParameterIds::cutoff), cutoffSlider);
    resonanceAttachment = std::make_unique<juce::ButtonParameterAttachment>(
        *valueTreeState.getParameter(ParameterIds::resonance), resonanceButton);
    vcfEgDepthAttachment = std::make_unique<juce::SliderParameterAttachment>(
        *valueTreeState.getParameter(ParameterIds::vcfEgDepth), vcfEgDepthSlider);
}

VCFComponent::~VCFComponent() {}

void VCFComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white);
    g.setFont(15.0f);
    g.drawFittedText("VCF", getLocalBounds(), juce::Justification::centredTop, 1);
}

void VCFComponent::resized()
{
    juce::Grid grid;
    using Track = juce::Grid::TrackInfo;
    using Fr = juce::Grid::Fr;

    grid.templateRows    = { Track(Fr(5)), Track(Fr(1)) }; // Make slider area taller
    grid.templateColumns = { Track(Fr(1)), Track(Fr(1)), Track(Fr(1)) };

    grid.items = {
        juce::GridItem(cutoffSlider),
        juce::GridItem(resonanceButton),
        juce::GridItem(vcfEgDepthSlider),
        juce::GridItem(cutoffLabel),
        juce::GridItem(resonanceLabel),
        juce::GridItem(vcfEgDepthLabel)
    };
    
    grid.performLayout(getLocalBounds().reduced(10).withTrimmedTop(20));
}
