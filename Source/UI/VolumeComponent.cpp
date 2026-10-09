#include "UI/VolumeComponent.h"

#include "Parameters.h"
#include "UI/CS01LookAndFeel.h"

VolumeComponent::VolumeComponent(juce::AudioProcessorValueTreeState& apvts)
    : valueTreeState(apvts) {
    volumeSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    volumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(volumeSlider);
    volumeLabel.setText("MASTER", juce::dontSendNotification);
    addAndMakeVisible(volumeLabel);

    volumeAttachment = std::make_unique<CS01SliderParameterAttachment>(
        *valueTreeState.getParameter(ParameterIds::volume), volumeSlider);
    volumeSlider.setPopupDisplayEnabled(true, true, this);
    volumeSlider.setTooltip("Master knob position; percentage is not linear output gain.");
}

VolumeComponent::~VolumeComponent() {}

void VolumeComponent::paint(juce::Graphics& g) {
    CS01LookAndFeel::drawSectionBackground(g, getLocalBounds(), "VOLUME");
}

void VolumeComponent::resized() {
    auto bounds = getLocalBounds().withTrimmedTop(20);
    auto labelHeight = 15;
    auto knobArea = bounds.withTrimmedBottom(labelHeight);

    float knobSize =
        juce::jmin(80.0f, juce::jmin(knobArea.getWidth(), knobArea.getHeight()) * 0.9f);
    volumeSlider.setBounds(knobArea.withSizeKeepingCentre(knobSize, knobSize));
    volumeLabel.setBounds(knobArea.getX(), volumeSlider.getBottom(), knobArea.getWidth(),
                          labelHeight);
}
