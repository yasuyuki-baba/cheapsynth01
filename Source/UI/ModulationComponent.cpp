#include "UI/ModulationComponent.h"

#include "CS01AudioProcessor.h"
#include "Parameters.h"
#include "ParameterFormatting.h"
#include "UI/CS01LookAndFeel.h"

#include <functional>

ModulationComponent::ModulationComponent(CS01AudioProcessor& p) : processor(p) {
    // Pitch Bend Slider
    pitchBendSlider.setSliderStyle(juce::Slider::LinearVertical);
    pitchBendSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    pitchBendSlider.setRange(-1.0, 1.0, 0.001);
    pitchBendSlider.textFromValueFunction = [](double value) {
        return ParameterFormatting::format(value, ParameterFormatting::Style::SignedPercent);
    };
    pitchBendSlider.valueFromTextFunction = [](const juce::String& text) {
        return ParameterFormatting::parse(text, ParameterFormatting::Style::SignedPercent);
    };
    pitchBendSlider.setTooltip("Bend wheel position; UP/DOWN set the range in semitones (st).");
    pitchBendSlider.setValue(0.0);
    pitchBendSlider.getProperties().set("performanceWheel", true);
    pitchBendSlider.setSliderSnapsToMousePosition(false);
    pitchBendSlider.setPopupDisplayEnabled(true, true, this);
    pitchBendSlider.setDoubleClickReturnValue(true, 0.0);
    pitchBendSlider.addListener(this);
    addAndMakeVisible(pitchBendSlider);
    pitchBendLabel.setText("BEND", juce::dontSendNotification);
    pitchBendLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(pitchBendLabel);
    pitchBendSlider.onDragStart = [this] {
        draggingBend = true;
        returningBend = false;
        bendRevision = processor.getExternalBendRevision();
    };
    pitchBendSlider.onDragEnd = [this] {
        draggingBend = false;
        if (bendRevision == processor.getExternalBendRevision()) {
            returningBend = true;
            returnStarted = juce::Time::getMillisecondCounterHiRes();
            returnPosition = pitchBendSlider.getValue();
        }
    };
    for (auto* slider : {&bendUpSlider, &bendDownSlider}) {
        slider->setSliderStyle(juce::Slider::IncDecButtons);
        slider->setTextBoxStyle(juce::Slider::TextBoxLeft, false, 48, 20);
        slider->setRange(0, 12, 1);
        slider->setTooltip("Pitch bend range in semitones");
        addAndMakeVisible(slider);
    }
    bendUpLabel.setText("UP", juce::dontSendNotification);
    bendDownLabel.setText("DOWN", juce::dontSendNotification);
    addAndMakeVisible(bendUpLabel);
    addAndMakeVisible(bendDownLabel);
    bendUpAttachment = std::make_unique<CS01SliderParameterAttachment>(
        processor.getValueTreeState(), ParameterIds::pitchBendUpRange, bendUpSlider);
    bendDownAttachment = std::make_unique<CS01SliderParameterAttachment>(
        processor.getValueTreeState(), ParameterIds::pitchBendDownRange, bendDownSlider);

    // Mod Depth Slider
    modDepthSlider.setSliderStyle(juce::Slider::LinearVertical);
    modDepthSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    modDepthSlider.setRange(0.0, 1.0, 0.001);
    modDepthSlider.textFromValueFunction = [](double value) {
        return ParameterFormatting::format(value, ParameterFormatting::Style::Percent);
    };
    modDepthSlider.valueFromTextFunction = [](const juce::String& text) {
        return ParameterFormatting::parse(text, ParameterFormatting::Style::Percent);
    };
    modDepthSlider.setTooltip("Modulation wheel amount, not a frequency or gain percentage.");
    modDepthSlider.setValue(
        processor.getValueTreeState().getRawParameterValue(ParameterIds::modDepth)->load(),
        juce::dontSendNotification);
    modDepthSlider.getProperties().set("performanceWheel", true);
    modDepthSlider.setSliderSnapsToMousePosition(false);
    modDepthSlider.setPopupDisplayEnabled(true, true, this);
    modDepthSlider.setDoubleClickReturnValue(true, 0.0);
    modDepthSlider.addListener(this);
    addAndMakeVisible(modDepthSlider);
    modDepthSlider.onDragStart = [this] { draggingMod = true; };
    modDepthSlider.onDragEnd = [this] { draggingMod = false; };
    modDepthLabel.setText("MOD", juce::dontSendNotification);
    modDepthLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(modDepthLabel);

    // LFO Target Buttons
    auto* lfoTargetParam = processor.getValueTreeState().getParameter(ParameterIds::lfoTarget);
    jassert(lfoTargetParam != nullptr);
    addAndMakeVisible(lfoTargetLabel);
    lfoTargetLabel.setText("TARGET", juce::dontSendNotification);

    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(lfoTargetParam)) {
        auto choices = choiceParam->choices;
        for (int i = 0; i < choices.size(); ++i) {
            auto* button = lfoTargetButtons.add(new juce::ToggleButton(choices[i]));
            addAndMakeVisible(button);
            button->setRadioGroupId(3);  // Group ID for LFO Target
            button->setClickingTogglesState(true);
        }
    }

    lfoTargetAttachment = std::make_unique<CS01ChoiceButtonParameterAttachment>(
        *static_cast<juce::AudioParameterChoice*>(lfoTargetParam), lfoTargetButtons,
        std::function<void(int)>{}, processor.getValueTreeState().undoManager);
    bendRevision = processor.getExternalBendRevision();
    startTimerHz(120);
}

void ModulationComponent::timerCallback() {
    if (!draggingMod) {
        modDepthSlider.setValue(
            processor.getValueTreeState().getRawParameterValue(ParameterIds::modDepth)->load(),
            juce::dontSendNotification);
    }

    const auto revision = processor.getExternalBendRevision();
    if (revision != bendRevision) {
        bendRevision = revision;
        returningBend = false;
    }
    if (draggingBend)
        return;
    if (returningBend) {
        const double progress = juce::jlimit(
            0.0, 1.0, (juce::Time::getMillisecondCounterHiRes() - returnStarted) / 60.0);
        pitchBendSlider.setValue(returnPosition * (1.0 - progress), juce::dontSendNotification);
        sliderValueChanged(&pitchBendSlider);
        returningBend = progress < 1.0;
        return;
    }
    pitchBendSlider.setValue(
        processor.getValueTreeState().getRawParameterValue(ParameterIds::pitchBend)->load(),
        juce::dontSendNotification);
}

ModulationComponent::~ModulationComponent() {
    stopTimer();
    pitchBendSlider.removeListener(this);
    modDepthSlider.removeListener(this);
}

void ModulationComponent::paint(juce::Graphics& g) {
    CS01LookAndFeel::drawSectionBackground(g, getLocalBounds(), "CONTROL");
    g.setColour(CS01LookAndFeel::Palette::textDim.withAlpha(0.3f));
    g.drawVerticalLine(getWidth() / 2, 30.0f, static_cast<float>(getHeight() - 8));
}

void ModulationComponent::sliderValueChanged(juce::Slider* slider) {
    if (slider == &pitchBendSlider) {
        const double position = slider->getValue();
        int pitchWheelValue = juce::roundToInt(8192 + position * (position >= 0 ? 8191 : 8192));
        auto message = juce::MidiMessage::pitchWheel(1, pitchWheelValue);
        message.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
        processor.getPanelBendCollector().addMessageToQueue(message);
    } else if (slider == &modDepthSlider) {
        // Mod depth is 0.0 to 1.0. MIDI CC is 0-127.
        int controllerValue = static_cast<int>(slider->getValue() * 127);
        auto message = juce::MidiMessage::controllerEvent(1, 1, controllerValue);  // CC #1
        message.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
        processor.getMidiMessageCollector().addMessageToQueue(message);
    }
}

void ModulationComponent::resized() {
    auto bounds = getLocalBounds().reduced(5).withTrimmedTop(24);
    auto bend = bounds.removeFromLeft(bounds.getWidth() / 2).reduced(6, 0);
    auto mod = bounds.reduced(6, 0);
    pitchBendLabel.setBounds(bend.removeFromTop(24));
    modDepthLabel.setBounds(mod.removeFromTop(24));
    auto bendSettings = bend.removeFromBottom(88);
    auto targets = mod.removeFromBottom(88);
    bend.removeFromBottom(10);
    mod.removeFromBottom(10);
    pitchBendSlider.setBounds(bend);
    modDepthSlider.setBounds(mod);
    bendUpLabel.setBounds(bendSettings.removeFromTop(18));
    bendUpSlider.setBounds(bendSettings.removeFromTop(22));
    bendDownLabel.setBounds(bendSettings.removeFromTop(18));
    bendDownSlider.setBounds(bendSettings.removeFromTop(22));

    lfoTargetLabel.setBounds(targets.removeFromTop(22));
    for (auto* button : lfoTargetButtons)
        button->setBounds(targets.removeFromTop(28));
}
