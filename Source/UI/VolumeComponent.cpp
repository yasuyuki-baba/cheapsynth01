#include "CS01LookAndFeel.h"
#include "VolumeComponent.h"
#include "../Parameters.h"

VolumeComponent::VolumeComponent(juce::AudioProcessorValueTreeState& apvts)
    : valueTreeState(apvts) {
    volumeSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    volumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(volumeSlider);
    volumeLabel.setText("MASTER", juce::dontSendNotification);
    addAndMakeVisible(volumeLabel);

    volumeAttachment = std::make_unique<juce::SliderParameterAttachment>(
        *valueTreeState.getParameter(ParameterIds::volume), volumeSlider);
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
