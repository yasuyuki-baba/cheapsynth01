#include <JuceHeader.h>

#include "CS01AudioProcessor.h"
#include "Parameters.h"
#include "RealtimeAudit.h"
#include "UI/PollingParameterAttachments.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

namespace {
template <typename Predicate>
bool awaitUi(Predicate ready) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!ready() && std::chrono::steady_clock::now() < end)
        juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    return ready();
}
struct NotificationSpy : juce::AudioProcessorParameter::Listener {
    void parameterValueChanged(int, float) override {
        ++values;
    }
    void parameterGestureChanged(int, bool start) override {
        start ? ++starts : ++ends;
    }
    int values = 0, starts = 0, ends = 0;
};
void update(juce::RangedAudioParameter& p, float value) {
    p.setValueNotifyingHost(p.convertTo0to1(value));
}
}  // namespace

TEST(PollingAttachmentTest, WorkerAutomationCoalescesLatestValuesWithoutTouchingControls) {
    juce::AudioParameterFloat scalar({"scalar", 1}, "Scalar",
                                     juce::NormalisableRange<float>(1, 100, 0.1f, 0.3f), 10);
    juce::AudioParameterBool flag({"flag", 1}, "Flag", false);
    juce::AudioParameterChoice choice({"choice", 1}, "Choice", {"A", "B", "C"}, 0);
    juce::Slider slider;
    juce::ToggleButton button;
    juce::ComboBox combo;
    combo.addItemList({"A", "B", "C"}, 1);
    CS01SliderParameterAttachment a(scalar, slider);
    CS01ButtonParameterAttachment b(flag, button);
    CS01ComboBoxParameterAttachment c(choice, combo);
    int callbacks = 0;
    bool wrongThread = false;
    const auto observe = [&] {
        ++callbacks;
        wrongThread |= !juce::MessageManager::getInstance()->isThisTheMessageThread();
    };
    slider.onValueChange = observe;
    button.onStateChange = observe;
    combo.onChange = observe;
    const auto initial = slider.getValue();
    std::thread worker([&] {
        for (int i = 0; i < 1000; ++i) {
            update(scalar, i % 2 ? 21 : 61);
            update(flag, i % 2);
            update(choice, i % 3);
        }
        update(scalar, 73);
        update(flag, 1);
        update(choice, 2);
    });
    worker.join();
    EXPECT_EQ(callbacks, 0);
    EXPECT_EQ(slider.getValue(), initial);
    EXPECT_FALSE(button.getToggleState());
    EXPECT_EQ(combo.getSelectedItemIndex(), 0);
    ASSERT_TRUE(awaitUi([&] {
        return juce::approximatelyEqual(slider.getValue(), 73.0) && button.getToggleState() &&
               combo.getSelectedItemIndex() == 2;
    }));
    EXPECT_FALSE(wrongThread);
    EXPECT_EQ(callbacks, 3);
}

TEST(PollingAttachmentTest, SliderRangeFormattingDefaultsAndHostGesturesArePreserved) {
    CS01AudioProcessor processor;
    for (auto* parameter : processor.getParameters()) {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        ASSERT_NE(ranged, nullptr);
        juce::Slider slider, reference;
        juce::SliderParameterAttachment original(*ranged, reference);
        CS01SliderParameterAttachment binding(*ranged, slider);
        for (float n : {0.0f, 0.1f, 0.5f, 0.9f, 1.0f}) {
            EXPECT_FLOAT_EQ(static_cast<float>(slider.proportionOfLengthToValue(n)),
                            static_cast<float>(reference.proportionOfLengthToValue(n)));
            const auto actual = ranged->convertFrom0to1(n);
            EXPECT_DOUBLE_EQ(slider.valueToProportionOfLength(actual),
                             reference.valueToProportionOfLength(actual));
            EXPECT_EQ(slider.getTextFromValue(actual), ranged->getText(n, 0));
        }
        const auto expected = ranged->convertFrom0to1(ranged->getDefaultValue());
        EXPECT_DOUBLE_EQ(slider.getDoubleClickReturnValue(), expected);
    }
    auto* owned = new juce::AudioParameterFloat({"scalar", 1}, "Scalar", 0, 1, 0.25f);
    processor.addParameter(owned);
    auto& scalar = *owned;
    juce::Slider slider;
    NotificationSpy spy;
    scalar.addListener(&spy);
    std::unique_ptr<juce::Slider::ScopedDragNotification> drag;
    {
        CS01SliderParameterAttachment binding(scalar, slider);
        drag = std::make_unique<juce::Slider::ScopedDragNotification>(slider);
        slider.setValue(0.8, juce::sendNotificationSync);
        EXPECT_FLOAT_EQ(scalar.get(), 0.8f);
        EXPECT_EQ(spy.starts, 1);
        EXPECT_EQ(spy.ends, 0);
        drag.reset();
        EXPECT_EQ(spy.ends, 1);
        scalar.setValueNotifyingHost(0.4f);
        const int notifications = spy.values;
        ASSERT_TRUE(
            awaitUi([&] { return slider.getValue() == static_cast<double>(scalar.get()); }));
        EXPECT_EQ(spy.values, notifications);
        drag = std::make_unique<juce::Slider::ScopedDragNotification>(slider);
    }
    EXPECT_EQ(spy.starts, 2);
    EXPECT_EQ(spy.ends, 2);  // Closing a control during a drag balances its host gesture.
    drag.reset();
    scalar.removeListener(&spy);
}

TEST(PollingAttachmentTest, ButtonAndComboEditsNotifyHostOnceWithBalancedGestures) {
    CS01AudioProcessor processor;
    auto& flag = *static_cast<juce::AudioParameterFloat*>(
        processor.apvts.getParameter(ParameterIds::resonance));
    auto& choice = *static_cast<juce::AudioParameterChoice*>(
        processor.apvts.getParameter(ParameterIds::filterType));
    flag.setValueNotifyingHost(0);
    choice.setValueNotifyingHost(0);
    juce::ToggleButton button;
    button.setClickingTogglesState(true);
    juce::ComboBox combo;
    combo.addItemList({"Original", "Modern"}, 1);
    NotificationSpy a, b;
    flag.addListener(&a);
    choice.addListener(&b);
    {
        CS01ButtonParameterAttachment buttonBinding(flag, button);
        CS01ComboBoxParameterAttachment comboBinding(choice, combo);
        button.triggerClick();
        ASSERT_TRUE(awaitUi([&] { return flag.get(); }));
        combo.setSelectedItemIndex(1, juce::sendNotificationSync);
        EXPECT_EQ(choice.getIndex(), 1);
        EXPECT_EQ(a.values, 1);
        EXPECT_EQ(a.starts, 1);
        EXPECT_EQ(a.ends, 1);
        EXPECT_EQ(b.values, 1);
        EXPECT_EQ(b.starts, 1);
        EXPECT_EQ(b.ends, 1);
    }
    flag.removeListener(&a);
    choice.removeListener(&b);
}

TEST(PollingAttachmentTest, HostReturnToLastPolledValueWinsOverInterveningGuiEdit) {
    juce::AudioParameterFloat scalar({"scalar", 1}, "Scalar", 0, 1, 0.25f);
    juce::Slider slider;
    CS01SliderParameterAttachment binding(scalar, slider);
    slider.setValue(0.8, juce::sendNotificationSync);
    std::thread worker([&] { scalar.setValueNotifyingHost(0.25f); });
    worker.join();
    ASSERT_TRUE(awaitUi([&] { return slider.getValue() == 0.25; }));
    // MIDI changes the authoritative parameter without broadcasting on the audio thread.
    std::thread midi([&] { static_cast<juce::AudioProcessorParameter&>(scalar).setValue(0.5f); });
    midi.join();
    ASSERT_TRUE(awaitUi([&] { return slider.getValue() == 0.5; }));
}

TEST(PollingAttachmentTest, DestructionDuringAutomationAndBeforeTickLeavesNoCallback) {
    juce::AudioParameterFloat scalar({"scalar", 1}, "Scalar", 0, 1, 0.25f);
    juce::Slider slider;
    std::atomic<bool> running{true};
    std::thread worker([&] {
        while (running.load()) {
            scalar.setValueNotifyingHost(0.8f);
            scalar.setValueNotifyingHost(0.2f);
        }
    });
    for (int i = 0; i < 32; ++i) {
        auto binding = std::make_unique<CS01SliderParameterAttachment>(scalar, slider);
        binding.reset();
    }
    running.store(false);
    worker.join();
    int callbacks = 0;
    slider.onValueChange = [&] { ++callbacks; };
    const auto old = slider.getValue();
    scalar.setValueNotifyingHost(0.9f);
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
    EXPECT_EQ(callbacks, 0);
    EXPECT_EQ(slider.getValue(), old);
}

#if defined(CHEAPSYNTH_RT_AUDIT)
TEST(PollingAttachmentRealtimeTest, WorkerNotificationHasNoAttachmentHeapOperations) {
    juce::AudioParameterFloat scalar({"scalar", 1}, "Scalar", 0, 1, 0.25f);
    juce::AudioParameterBool flag({"flag", 1}, "Flag", false);
    juce::AudioParameterChoice choice({"choice", 1}, "Choice", {"A", "B", "C"}, 0);
    juce::Slider slider;
    juce::ToggleButton button;
    juce::ComboBox combo;
    combo.addItemList({"A", "B", "C"}, 1);
    CS01SliderParameterAttachment a(scalar, slider);
    CS01ButtonParameterAttachment b(flag, button);
    CS01ComboBoxParameterAttachment c(choice, combo);
    std::size_t allocations = 0, deallocations = 0, locks = 0;
    std::thread worker([&] {
        realtimeAudit::begin();
        for (int i = 0; i < 1000; ++i) {
            scalar.setValueNotifyingHost(i % 2 ? 0.8f : 0.2f);
            flag.setValueNotifyingHost(i % 2);
            choice.setValueNotifyingHost((i % 3) * 0.5f);
        }
        realtimeAudit::end();
        allocations = realtimeAudit::allocations;
        deallocations = realtimeAudit::deallocations;
        locks = realtimeAudit::locks;
    });
    worker.join();
    std::cout << "Attachment worker probe: allocations=" << allocations
              << " frees=" << deallocations << " locks=" << locks << "\n";
    EXPECT_EQ(allocations, 0u);
    EXPECT_EQ(deallocations, 0u);
    // JUCE's parameter listener dispatch itself still locks; report rather than
    // claim the complete host notification path is lock-free.
}

TEST(PollingAttachmentRealtimeTest, OpenEditorAddsNoHeapOrMutexOperationsToWorkerAutomation) {
    CS01AudioProcessor processor;
    const auto& parameters = processor.getParameters();
    struct Counts {
        std::size_t allocations = 0, frees = 0, locks = 0;
    };
    const auto measure = [&] {
        Counts result;
        std::thread worker([&] {
            realtimeAudit::begin();
            for (int repeat = 0; repeat < 100; ++repeat)
                for (auto* parameter : parameters)
                    parameter->setValueNotifyingHost((repeat % 10) * 0.1f);
            realtimeAudit::end();
            result = {realtimeAudit::allocations, realtimeAudit::deallocations,
                      realtimeAudit::locks};
        });
        worker.join();
        return result;
    };
    const auto closed = measure();
    auto editor = std::unique_ptr<juce::AudioProcessorEditor>(processor.createEditor());
    const auto opened = measure();
    std::cout << "Editor automation probe: closed allocations=" << closed.allocations
              << " frees=" << closed.frees << " locks=" << closed.locks
              << "; opened allocations=" << opened.allocations << " frees=" << opened.frees
              << " locks=" << opened.locks << "\n";
    EXPECT_EQ(closed.allocations, 0u);
    EXPECT_EQ(closed.frees, 0u);
    EXPECT_EQ(opened.allocations, closed.allocations);
    EXPECT_EQ(opened.frees, closed.frees);
    EXPECT_EQ(opened.locks, closed.locks);
    editor.reset();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
}

#endif
