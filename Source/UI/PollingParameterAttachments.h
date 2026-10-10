#pragma once

#include <JuceHeader.h>

#include <functional>
#include <limits>
#include <utility>

// Message-thread-owned bindings. They never register an audio parameter listener;
// automation is sampled from the authoritative parameter value at 60 Hz.
class CS01PollingParameterAttachment : private juce::Timer {
   public:
    CS01PollingParameterAttachment(juce::RangedAudioParameter& p,
                                   std::function<void(float)> callback,
                                   juce::UndoManager* undo = nullptr)
        : parameter(p), update(std::move(callback)), undoManager(undo) {}
    ~CS01PollingParameterAttachment() override {
        stop();
        if (gestureActive)
            endGesture();
    }

    void sendInitialUpdate() {
        jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
        lastObserved = parameter.getValue();
        update(parameter.convertFrom0to1(lastObserved));
    }
    void start() {
        startTimerHz(60);
    }
    void stop() {
        stopTimer();
    }
    void beginGesture() {
        if (!gestureActive) {
            if (undoManager != nullptr)
                undoManager->beginNewTransaction();
            gestureActive = true;
            parameter.beginChangeGesture();
        }
    }
    void endGesture() {
        if (gestureActive) {
            gestureActive = false;
            parameter.endChangeGesture();
        }
    }
    void setValueAsPartOfGesture(float value) {
        // A host may return to the last polled value before the next tick.
        // Force a refresh after GUI edits so that value still wins over the control.
        lastObserved = std::numeric_limits<float>::quiet_NaN();
        const auto normalised = parameter.convertTo0to1(value);
        if (!juce::approximatelyEqual(parameter.getValue(), normalised))
            parameter.setValueNotifyingHost(normalised);
    }
    void setValueAsCompleteGesture(float value) {
        if (!juce::approximatelyEqual(parameter.getValue(), parameter.convertTo0to1(value))) {
            beginGesture();
            setValueAsPartOfGesture(value);
            endGesture();
        }
    }

   private:
    void timerCallback() override {
        const auto value = parameter.getValue();
        if (value != lastObserved) {
            lastObserved = value;
            update(parameter.convertFrom0to1(value));
        }
    }
    juce::RangedAudioParameter& parameter;
    std::function<void(float)> update;
    juce::UndoManager* undoManager;
    float lastObserved = std::numeric_limits<float>::quiet_NaN();
    bool gestureActive = false;

    JUCE_DECLARE_NON_COPYABLE(CS01PollingParameterAttachment)
};

class CS01SliderParameterAttachment : private juce::Slider::Listener {
   public:
    CS01SliderParameterAttachment(juce::RangedAudioParameter& p, juce::Slider& control,
                                  juce::UndoManager* undo = nullptr)
        : slider(control),
          attachment(
              p,
              [this](float value) {
                  const juce::ScopedValueSetter<bool> guard(ignoreCallbacks, true);
                  slider.setValue(value, juce::sendNotificationSync);
              },
              undo) {
        slider.valueFromTextFunction = [&p](const juce::String& text) {
            return static_cast<double>(p.convertFrom0to1(p.getValueForText(text)));
        };
        slider.textFromValueFunction = [&p](double value) {
            return p.getText(p.convertTo0to1(static_cast<float>(value)), 0);
        };
        slider.setDoubleClickReturnValue(true, p.convertFrom0to1(p.getDefaultValue()));
        auto range = p.getNormalisableRange();
        // Preserve custom conversions and snapping, not just the skew exponent.
        juce::NormalisableRange<double> mapped(
            range.start, range.end,
            [range](double minimum, double maximum, double value) mutable {
                range.start = static_cast<float>(minimum);
                range.end = static_cast<float>(maximum);
                return static_cast<double>(range.convertFrom0to1(static_cast<float>(value)));
            },
            [range](double minimum, double maximum, double value) mutable {
                range.start = static_cast<float>(minimum);
                range.end = static_cast<float>(maximum);
                return static_cast<double>(range.convertTo0to1(static_cast<float>(value)));
            },
            [range](double minimum, double maximum, double value) mutable {
                range.start = static_cast<float>(minimum);
                range.end = static_cast<float>(maximum);
                return static_cast<double>(range.snapToLegalValue(static_cast<float>(value)));
            });
        mapped.interval = range.interval;
        mapped.skew = range.skew;
        mapped.symmetricSkew = range.symmetricSkew;
        slider.setNormalisableRange(mapped);
        sendInitialUpdate();
        slider.valueChanged();
        slider.addListener(this);
        attachment.start();
    }
    CS01SliderParameterAttachment(juce::AudioProcessorValueTreeState& state, const juce::String& id,
                                  juce::Slider& control)
        : CS01SliderParameterAttachment(*state.getParameter(id), control, state.undoManager) {}
    ~CS01SliderParameterAttachment() override {
        attachment.stop();
        slider.removeListener(this);
    }
    void sendInitialUpdate() {
        attachment.sendInitialUpdate();
    }

   private:
    void sliderValueChanged(juce::Slider*) override {
        if (!ignoreCallbacks)
            attachment.setValueAsPartOfGesture(static_cast<float>(slider.getValue()));
    }
    void sliderDragStarted(juce::Slider*) override {
        attachment.beginGesture();
    }
    void sliderDragEnded(juce::Slider*) override {
        attachment.endGesture();
    }
    juce::Slider& slider;
    CS01PollingParameterAttachment attachment;
    bool ignoreCallbacks = false;

    JUCE_DECLARE_NON_COPYABLE(CS01SliderParameterAttachment)
};

class CS01ButtonParameterAttachment : private juce::Button::Listener {
   public:
    CS01ButtonParameterAttachment(juce::RangedAudioParameter& p, juce::Button& control,
                                  juce::UndoManager* undo = nullptr)
        : button(control),
          attachment(
              p,
              [this](float value) {
                  const juce::ScopedValueSetter<bool> guard(ignoreCallbacks, true);
                  button.setToggleState(value >= 0.5f, juce::sendNotificationSync);
              },
              undo) {
        sendInitialUpdate();
        button.addListener(this);
        attachment.start();
    }
    CS01ButtonParameterAttachment(juce::AudioProcessorValueTreeState& state, const juce::String& id,
                                  juce::Button& control)
        : CS01ButtonParameterAttachment(*state.getParameter(id), control, state.undoManager) {}
    ~CS01ButtonParameterAttachment() override {
        attachment.stop();
        button.removeListener(this);
    }
    void sendInitialUpdate() {
        attachment.sendInitialUpdate();
    }

   private:
    void buttonClicked(juce::Button*) override {
        if (!ignoreCallbacks)
            attachment.setValueAsCompleteGesture(button.getToggleState() ? 1.0f : 0.0f);
    }
    juce::Button& button;
    CS01PollingParameterAttachment attachment;
    bool ignoreCallbacks = false;

    JUCE_DECLARE_NON_COPYABLE(CS01ButtonParameterAttachment)
};

class CS01ComboBoxParameterAttachment : private juce::ComboBox::Listener {
   public:
    CS01ComboBoxParameterAttachment(juce::RangedAudioParameter& p, juce::ComboBox& control,
                                    juce::UndoManager* undo = nullptr)
        : parameter(p),
          comboBox(control),
          attachment(
              p,
              [this](float value) {
                  const auto index = juce::roundToInt(parameter.convertTo0to1(value) *
                                                      (comboBox.getNumItems() - 1));
                  if (index != comboBox.getSelectedItemIndex()) {
                      const juce::ScopedValueSetter<bool> guard(ignoreCallbacks, true);
                      comboBox.setSelectedItemIndex(index, juce::sendNotificationSync);
                  }
              },
              undo) {
        sendInitialUpdate();
        comboBox.addListener(this);
        attachment.start();
    }
    CS01ComboBoxParameterAttachment(juce::AudioProcessorValueTreeState& state,
                                    const juce::String& id, juce::ComboBox& control)
        : CS01ComboBoxParameterAttachment(*state.getParameter(id), control, state.undoManager) {}
    ~CS01ComboBoxParameterAttachment() override {
        attachment.stop();
        comboBox.removeListener(this);
    }
    void sendInitialUpdate() {
        attachment.sendInitialUpdate();
    }

   private:
    void comboBoxChanged(juce::ComboBox*) override {
        if (!ignoreCallbacks) {
            const float value = comboBox.getNumItems() > 1
                                    ? static_cast<float>(comboBox.getSelectedItemIndex()) /
                                          (comboBox.getNumItems() - 1)
                                    : 0.0f;
            attachment.setValueAsCompleteGesture(parameter.convertFrom0to1(value));
        }
    }
    juce::RangedAudioParameter& parameter;
    juce::ComboBox& comboBox;
    CS01PollingParameterAttachment attachment;
    bool ignoreCallbacks = false;

    JUCE_DECLARE_NON_COPYABLE(CS01ComboBoxParameterAttachment)
};

// One binding owns an entire exclusive choice group. JUCE sends click callbacks
// to the deselected radio button too; only the selected button may write a value.
class CS01ChoiceButtonParameterAttachment {
   public:
    CS01ChoiceButtonParameterAttachment(juce::AudioParameterChoice& parameter,
                                        juce::OwnedArray<juce::ToggleButton>& controls,
                                        std::function<void(int)> onChoiceChanged = {},
                                        juce::UndoManager* undo = nullptr)
        : buttons(controls),
          attachment(
              parameter,
              [this, onChoiceChanged](float value) {
                  const int index = juce::roundToInt(value);
                  for (int i = 0; i < buttons.size(); ++i)
                      buttons[i]->setToggleState(i == index, juce::dontSendNotification);
                  if (onChoiceChanged)
                      onChoiceChanged(index);
              },
              undo) {
        jassert(buttons.size() == parameter.choices.size());
        for (int i = 0; i < buttons.size(); ++i) {
            auto* button = buttons[i];
            button->onClick = [this, button, i] {
                if (button->getToggleState()) {
                    attachment.setValueAsCompleteGesture(static_cast<float>(i));
                    attachment.sendInitialUpdate();
                }
            };
        }
        attachment.sendInitialUpdate();
        attachment.start();
    }
    ~CS01ChoiceButtonParameterAttachment() {
        attachment.stop();
        for (auto* button : buttons)
            button->onClick = nullptr;
    }

   private:
    juce::OwnedArray<juce::ToggleButton>& buttons;
    CS01PollingParameterAttachment attachment;

    JUCE_DECLARE_NON_COPYABLE(CS01ChoiceButtonParameterAttachment)
};
