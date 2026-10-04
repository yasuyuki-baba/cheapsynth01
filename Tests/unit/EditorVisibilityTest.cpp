#include <gtest/gtest.h>
#include <JuceHeader.h>
#include "../../Source/CS01AudioProcessor.h"
#include "../../Source/CS01AudioProcessorEditor.h"

TEST(EditorVisibilityTest, FilterModesUseHeaderRow) {
    CS01AudioProcessor processor;
    CS01AudioProcessorEditor editor(processor);
    VCFComponent* filter = nullptr;
    for (auto* child : editor.getChildren())
        if (auto* component = dynamic_cast<VCFComponent*>(child))
            filter = component;
    ASSERT_NE(filter, nullptr);
    EXPECT_GE(filter->getWidth(), 120);
    juce::ToggleButton* original = nullptr;
    juce::ToggleButton* modern = nullptr;
    for (auto* child : filter->getChildren()) {
        EXPECT_TRUE(filter->getLocalBounds().contains(child->getBounds()));
        if (auto* button = dynamic_cast<juce::ToggleButton*>(child)) {
            if (button->getButtonText() == "I")
                original = button;
            if (button->getButtonText() == "II")
                modern = button;
        }
        if (auto* slider = dynamic_cast<juce::Slider*>(child)) {
            EXPECT_GE(slider->getY(), 30);
        }
    }
    ASSERT_NE(original, nullptr);
    ASSERT_NE(modern, nullptr);
    EXPECT_EQ(original->getY(), 0);
    EXPECT_EQ(modern->getY(), 0);
    EXPECT_EQ(original->getWidth(), 36);
    EXPECT_EQ(modern->getWidth(), 36);
    EXPECT_LE(original->getRight(), modern->getX());
    EXPECT_LE(modern->getBottom(), 22);
    modern->onClick();
    EXPECT_TRUE(modern->getToggleState());
    EXPECT_FALSE(original->getToggleState());
    original->onClick();
    EXPECT_TRUE(original->getToggleState());
    EXPECT_FALSE(modern->getToggleState());
}

TEST(EditorVisibilityTest, CombinedDisplayPreservesUpperPanelLayout) {
    CS01AudioProcessor processor;
    CS01AudioProcessorEditor editor(processor);
    juce::MidiKeyboardComponent* keyboard = nullptr;
    juce::TextButton* keyboardButton = nullptr;
    std::vector<std::pair<juce::Component*, juce::Rectangle<int>>> upperBounds;
    for (auto* child : editor.getChildren()) {
        if (auto* keys = dynamic_cast<juce::MidiKeyboardComponent*>(child))
            keyboard = keys;
        if (auto* button = dynamic_cast<juce::TextButton*>(child)) {
            if (button->getButtonText() == "KEYBOARD + MONITOR")
                keyboardButton = button;
        }
    }
    ASSERT_NE(keyboard, nullptr);
    ASSERT_NE(keyboardButton, nullptr);
    for (auto* child : editor.getChildren())
        if (child != keyboard && child != &editor.getOscilloscope() &&
            dynamic_cast<juce::ResizableCornerComponent*>(child) == nullptr &&
            dynamic_cast<juce::ResizableBorderComponent*>(child) == nullptr)
            upperBounds.emplace_back(child, child->getBounds());
    EXPECT_FALSE(keyboard->isVisible());
    EXPECT_FALSE(keyboardButton->getToggleState());
    EXPECT_FALSE(editor.getOscilloscope().isVisible());
    EXPECT_EQ(editor.getWidth(), 1240);
    EXPECT_EQ(editor.getHeight(), 400);

    auto toggle = [](juce::TextButton& button) {
        button.setToggleState(!button.getToggleState(), juce::dontSendNotification);
        button.onClick();
    };
    toggle(*keyboardButton);
    EXPECT_TRUE(keyboard->isVisible());
    EXPECT_EQ(editor.getHeight(), 640);
    EXPECT_TRUE(editor.getOscilloscope().isVisible());
    EXPECT_GT(keyboard->getWidth(), 0);
    EXPECT_LT(keyboard->getRight(), editor.getOscilloscope().getX());
    EXPECT_EQ(keyboard->getY(), editor.getOscilloscope().getY());
    EXPECT_EQ(keyboard->getHeight(), editor.getOscilloscope().getHeight());
    EXPECT_GT(keyboard->getWidth(), editor.getOscilloscope().getWidth());
    for (const auto& [component, bounds] : upperBounds)
        EXPECT_EQ(component->getBounds(), bounds);
    toggle(*keyboardButton);
    EXPECT_FALSE(keyboard->isVisible());
    EXPECT_FALSE(editor.getOscilloscope().isVisible());
    EXPECT_EQ(editor.getHeight(), 400);
    for (const auto& [component, bounds] : upperBounds)
        EXPECT_EQ(component->getBounds(), bounds);
}

TEST(EditorVisibilityTest, CompactPerformanceKnobsStayWithinTheirPanels) {
    CS01AudioProcessor processor;
    CS01AudioProcessorEditor editor(processor);
    int knobCount = 0;
    for (auto* panel : editor.getChildren()) {
        const bool breath = dynamic_cast<BreathControlComponent*>(panel) != nullptr;
        const bool volume = dynamic_cast<VolumeComponent*>(panel) != nullptr;
        if (!breath && !volume)
            continue;
        for (auto* child : panel->getChildren()) {
            EXPECT_TRUE(panel->getLocalBounds().contains(child->getBounds()));
            if (auto* slider = dynamic_cast<juce::Slider*>(child)) {
                ++knobCount;
                EXPECT_GT(slider->getHeight(), 0);
                EXPECT_LE(slider->getWidth(), breath ? 60 : 80);
                EXPECT_LE(slider->getHeight(), breath ? 60 : 80);
            }
        }
    }
    EXPECT_EQ(knobCount, 3);
}

TEST(EditorVisibilityTest, WheelSettingsAreBelowTheirCorrespondingWheels) {
    CS01AudioProcessor processor;
    ModulationComponent controls(processor);
    controls.setBounds(0, 0, 204, 298);
    juce::Label* bend = nullptr;
    juce::Label* mod = nullptr;
    juce::Label* up = nullptr;
    juce::Label* down = nullptr;
    juce::Label* target = nullptr;
    for (auto* child : controls.getChildren()) {
        if (auto* label = dynamic_cast<juce::Label*>(child)) {
            if (label->getText() == "BEND")
                bend = label;
            if (label->getText() == "MOD")
                mod = label;
            if (label->getText() == "UP")
                up = label;
            if (label->getText() == "DOWN")
                down = label;
            if (label->getText() == "TARGET")
                target = label;
        }
        EXPECT_TRUE(controls.getLocalBounds().contains(child->getBounds()));
    }
    ASSERT_NE(bend, nullptr);
    ASSERT_NE(mod, nullptr);
    ASSERT_NE(up, nullptr);
    ASSERT_NE(down, nullptr);
    ASSERT_NE(target, nullptr);
    EXPECT_GT(up->getY(), bend->getBottom());
    EXPECT_GT(down->getY(), up->getBottom());
    EXPECT_GT(target->getY(), mod->getBottom());
    EXPECT_LT(up->getX(), target->getX());
}

TEST(EditorVisibilityTest, SoundSlidersHaveUniformSpacing) {
    CS01AudioProcessor processor;
    CS01AudioProcessorEditor editor(processor);
    for (const int width : {1240, 1600, 2400}) {
        editor.setSize(width, 400);
        std::vector<int> centres;
        for (auto* panel : editor.getChildren()) {
            if (dynamic_cast<LFOComponent*>(panel) == nullptr &&
                dynamic_cast<VCOComponent*>(panel) == nullptr &&
                dynamic_cast<VCFComponent*>(panel) == nullptr &&
                dynamic_cast<VCAComponent*>(panel) == nullptr &&
                dynamic_cast<EGComponent*>(panel) == nullptr)
                continue;
            for (auto* child : panel->getChildren()) {
                if (dynamic_cast<juce::Slider*>(child) != nullptr) {
                    EXPECT_TRUE(panel->getLocalBounds().contains(child->getBounds()));
                    centres.push_back(panel->getX() + child->getBounds().getCentreX());
                }
            }
        }
        std::sort(centres.begin(), centres.end());
        ASSERT_EQ(centres.size(), 12u);
        const double pitch = (width - 392) / 14.0;
        for (size_t i = 1; i < centres.size(); ++i) {
            // Waveform and octave selectors occupy the two intervening VCO columns.
            const double expected = pitch * (i == 4 ? 3 : 1);
            EXPECT_NEAR(centres[i] - centres[i - 1], expected, 2.0);
        }
    }
}
