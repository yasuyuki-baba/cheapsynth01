#include "CS01LookAndFeel.h"
#include "LFOComponent.h"
#include "../Parameters.h"

LFOComponent::LFOComponent(juce::AudioProcessorValueTreeState& apvts) : valueTreeState(apvts) {
    lfoSpeedSlider.setSliderStyle(juce::Slider::LinearVertical);
    lfoSpeedSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(lfoSpeedSlider);
    lfoSpeedLabel.setText("SPEED", juce::dontSendNotification);
    addAndMakeVisible(lfoSpeedLabel);

    lfoSpeedAttachment = std::make_unique<juce::SliderParameterAttachment>(
        *valueTreeState.getParameter(ParameterIds::lfoSpeed), lfoSpeedSlider);
    // Match the VCO faders without changing parameter ranges or values.
    lfoSpeedSlider.setPopupDisplayEnabled(true, true, this);
    lfoSpeedSlider.setSliderSnapsToMousePosition(false);
    lfoSpeedSlider.setDoubleClickReturnValue(true,
        valueTreeState.getParameter(ParameterIds::lfoSpeed)->convertFrom0to1(
            valueTreeState.getParameter(ParameterIds::lfoSpeed)->getDefaultValue()));

}

LFOComponent::~LFOComponent() {}

void LFOComponent::paint(juce::Graphics& g) {
    CS01LookAndFeel::drawSectionBackground(g, getLocalBounds(), "LFO");
}

void LFOComponent::resized() {
    juce::Grid grid;
    using Track = juce::Grid::TrackInfo;
    using Fr = juce::Grid::Fr;

    grid.templateRows = {Track(Fr(1)), Track(juce::Grid::Px(32))};
    grid.templateColumns = {Track(Fr(1))};

    grid.items = {juce::GridItem(lfoSpeedSlider), juce::GridItem(lfoSpeedLabel)};

    grid.performLayout(getLocalBounds().reduced(10).withTrimmedTop(20));
}
