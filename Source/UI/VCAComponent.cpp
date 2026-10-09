#include "UI/VCAComponent.h"

#include "Parameters.h"
#include "UI/CS01LookAndFeel.h"

VCAComponent::VCAComponent(juce::AudioProcessorValueTreeState& apvts) : valueTreeState(apvts) {
    vcaEgDepthSlider.setSliderStyle(juce::Slider::LinearVertical);
    vcaEgDepthSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(vcaEgDepthSlider);
    vcaEgDepthLabel.setText("EG DEPTH", juce::dontSendNotification);
    vcaEgDepthSlider.setTooltip("Envelope control depth; percentage is not output gain.");
    addAndMakeVisible(vcaEgDepthLabel);

    vcaEgDepthAttachment = std::make_unique<CS01SliderParameterAttachment>(
        *valueTreeState.getParameter(ParameterIds::vcaEgDepth), vcaEgDepthSlider);
    // Match the VCO faders without changing parameter ranges or values.
    vcaEgDepthSlider.setPopupDisplayEnabled(true, true, this);
    vcaEgDepthSlider.setSliderSnapsToMousePosition(false);
    vcaEgDepthSlider.setDoubleClickReturnValue(
        true, valueTreeState.getParameter(ParameterIds::vcaEgDepth)
                  ->convertFrom0to1(
                      valueTreeState.getParameter(ParameterIds::vcaEgDepth)->getDefaultValue()));
}

VCAComponent::~VCAComponent() {}

void VCAComponent::paint(juce::Graphics& g) {
    CS01LookAndFeel::drawSectionBackground(g, getLocalBounds(), "VCA");
}

void VCAComponent::resized() {
    juce::Grid grid;
    using Track = juce::Grid::TrackInfo;
    using Fr = juce::Grid::Fr;

    grid.templateRows = {Track(Fr(1)), Track(juce::Grid::Px(32))};
    grid.templateColumns = {Track(Fr(1))};

    grid.items = {juce::GridItem(vcaEgDepthSlider), juce::GridItem(vcaEgDepthLabel)};

    grid.performLayout(getLocalBounds().withTrimmedTop(30).withTrimmedBottom(10));
}
