#include "UI/VCOComponent.h"

#include "Parameters.h"
#include "UI/CS01LookAndFeel.h"

#include <functional>

VCOComponent::VCOComponent(juce::AudioProcessorValueTreeState& apvts) : valueTreeState(apvts) {
    // Glissando, Pitch, PWM Speed Sliders (same as before)
    glissandoSlider.setSliderStyle(juce::Slider::LinearVertical);
    glissandoSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(glissandoSlider);
    glissandoLabel.setText("GLISS.", juce::dontSendNotification);
    glissandoSlider.setTooltip("Time per semitone (ms/st). Zero disables glissando.");
    addAndMakeVisible(glissandoLabel);
    glissandoAttachment = std::make_unique<CS01SliderParameterAttachment>(
        *valueTreeState.getParameter(ParameterIds::glissando), glissandoSlider);

    pitchSlider.setSliderStyle(juce::Slider::LinearVertical);
    pitchSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(pitchSlider);
    pitchLabel.setText("PITCH", juce::dontSendNotification);
    pitchSlider.setTooltip("Fine tuning in cents; 100 cents = one semitone.");
    addAndMakeVisible(pitchLabel);
    pitchAttachment = std::make_unique<CS01SliderParameterAttachment>(
        *valueTreeState.getParameter(ParameterIds::pitch), pitchSlider);

    pwmSpeedSlider.setSliderStyle(juce::Slider::LinearVertical);
    pwmSpeedSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(pwmSpeedSlider);
    pwmSpeedLabel.setText("PWM SPEED", juce::dontSendNotification);
    addAndMakeVisible(pwmSpeedLabel);
    pwmSpeedAttachment = std::make_unique<CS01SliderParameterAttachment>(
        *valueTreeState.getParameter(ParameterIds::pwmSpeed), pwmSpeedSlider);

    juce::Slider* sliders[] = {&glissandoSlider, &pitchSlider, &pwmSpeedSlider};
    const juce::String ids[] = {ParameterIds::glissando, ParameterIds::pitch,
                                ParameterIds::pwmSpeed};
    for (int i = 0; i < 3; ++i) {
        auto* parameter = valueTreeState.getParameter(ids[i]);
        sliders[i]->setPopupDisplayEnabled(true, true, this);
        sliders[i]->setSliderSnapsToMousePosition(false);
        sliders[i]->setDoubleClickReturnValue(
            true, parameter->convertFrom0to1(parameter->getDefaultValue()));
    }

    // --- Waveform Buttons ---
    auto* waveTypeParam = valueTreeState.getParameter(ParameterIds::waveType);
    jassert(waveTypeParam != nullptr);
    addAndMakeVisible(waveTypeLabel);
    waveTypeLabel.setText("WAVEFORM", juce::dontSendNotification);

    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(waveTypeParam)) {
        auto choices = choiceParam->choices;
        for (int i = 0; i < choices.size(); ++i) {
            auto* button = waveTypeButtons.add(new juce::ToggleButton(choices[i]));
            addAndMakeVisible(button);
            button->setRadioGroupId(1);  // Group ID for waveform
            button->setClickingTogglesState(true);
        }
    }

    // --- Feet Buttons ---
    auto* feetParam = valueTreeState.getParameter(ParameterIds::feet);
    jassert(feetParam != nullptr);
    addAndMakeVisible(feetLabel);
    feetLabel.setText("FEET", juce::dontSendNotification);

    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(feetParam)) {
        auto choices = choiceParam->choices;
        for (int i = 0; i < choices.size(); ++i) {
            auto* button = feetButtons.add(new juce::ToggleButton(choices[i]));
            addAndMakeVisible(button);
            button->setRadioGroupId(2);  // Group ID for feet
            button->setClickingTogglesState(true);
        }
    }

    waveTypeAttachment = std::make_unique<CS01ChoiceButtonParameterAttachment>(
        *static_cast<juce::AudioParameterChoice*>(waveTypeParam), waveTypeButtons,
        std::function<void(int)>{}, valueTreeState.undoManager);
    feetAttachment = std::make_unique<CS01ChoiceButtonParameterAttachment>(
        *static_cast<juce::AudioParameterChoice*>(feetParam), feetButtons,
        std::function<void(int)>{}, valueTreeState.undoManager);
}

VCOComponent::~VCOComponent() = default;

void VCOComponent::paint(juce::Graphics& g) {
    CS01LookAndFeel::drawSectionBackground(g, getLocalBounds(), "VCO");
}

void VCOComponent::resized() {
    auto bounds = getLocalBounds().withTrimmedTop(30).withTrimmedBottom(10);
    auto labels = bounds.removeFromBottom(32);
    const int unit = bounds.getWidth() / 5;
    juce::Slider* sliders[] = {&glissandoSlider, &pitchSlider, &pwmSpeedSlider};
    juce::Label* sliderLabels[] = {&glissandoLabel, &pitchLabel, &pwmSpeedLabel};
    for (int i = 0; i < 3; ++i) {
        sliders[i]->setBounds(bounds.removeFromLeft(unit));
        sliderLabels[i]->setBounds(labels.removeFromLeft(unit));
        sliderLabels[i]->setJustificationType(juce::Justification::centred);
    }
    const int waveWidth = unit;
    auto waves = bounds.removeFromLeft(waveWidth);
    waveTypeLabel.setBounds(labels.removeFromLeft(waveWidth));
    feetLabel.setBounds(labels);
    waveTypeLabel.setJustificationType(juce::Justification::centred);
    feetLabel.setJustificationType(juce::Justification::centred);
    const int waveHeight = juce::jmin(32, waves.getHeight() / waveTypeButtons.size());
    for (auto* button : waveTypeButtons)
        button->setBounds(waves.removeFromTop(waveHeight).reduced(2));
    const int feetHeight = juce::jmin(32, bounds.getHeight() / feetButtons.size());
    for (auto* button : feetButtons)
        button->setBounds(bounds.removeFromTop(feetHeight).reduced(2));
}
