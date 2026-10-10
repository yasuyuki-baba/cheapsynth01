#include "ParameterFormatting.h"
#include "UI/SliderValuePopup.h"
#include "CS01AudioProcessor.h"
#include "CS01AudioProcessorEditor.h"

#include <gtest/gtest.h>

using ParameterFormatting::Style;

TEST(ParameterFormattingTest, PopupTracksThumbWithoutStandardPopup) {
    juce::Component host;
    host.setBounds(0, 0, 600, 400);
    juce::Slider slider;
    host.addAndMakeVisible(slider);
    slider.setBounds(300, 50, 40, 280);
    slider.setSliderStyle(juce::Slider::LinearVertical);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setRange(0, 1);
    SliderValuePopup popup(host);
    popup.attach(slider);
    popup.showForSlider(slider);
    ASSERT_TRUE(popup.isVisible());
    const int lowY = popup.getBounds().getCentreY();
    slider.setValue(1.0, juce::sendNotificationSync);
    const int highY = popup.getBounds().getCentreY();
    EXPECT_LT(highY, lowY - 200);
    EXPECT_NEAR(highY, slider.getY() + slider.getPositionOfValue(1.0), 2);
    const auto bounds = popup.getBounds();
    popup.refresh();
    EXPECT_EQ(popup.getBounds(), bounds);
    EXPECT_EQ(slider.getCurrentPopupDisplay(), nullptr);
}

TEST(ParameterFormattingTest, UnitsPrecisionAndBoundaries) {
    EXPECT_EQ(ParameterFormatting::format(999, Style::Frequency), "999 Hz");
    EXPECT_EQ(ParameterFormatting::format(1000, Style::Frequency), "1.000 kHz");
    EXPECT_EQ(ParameterFormatting::format(20000, Style::Frequency), "20.000 kHz");
    EXPECT_EQ(ParameterFormatting::format(5, Style::Rate), "5.00 Hz");
    EXPECT_EQ(ParameterFormatting::format(0.999, Style::Time), "999 ms");
    EXPECT_EQ(ParameterFormatting::format(1, Style::Time), "1.000 s");
    EXPECT_EQ(ParameterFormatting::format(0.208, Style::StepTime), "208 ms/st");
    EXPECT_EQ(ParameterFormatting::format(0, Style::StepTime), "0 ms/st");
    EXPECT_EQ(ParameterFormatting::format(-0.25, Style::Cents), "-25.0 cent");
    EXPECT_EQ(ParameterFormatting::format(0.25, Style::Cents), "+25.0 cent");
    EXPECT_EQ(ParameterFormatting::format(-0.00001, Style::SignedPercent), "0.0 %");
    EXPECT_EQ(ParameterFormatting::format(0.8, Style::Percent), "80.0 %");
    EXPECT_EQ(ParameterFormatting::format(12, Style::Semitones), "12 st");
}

TEST(ParameterFormattingTest, ParsingAndInvalidInput) {
    EXPECT_DOUBLE_EQ(ParameterFormatting::parse("2.5 kHz", Style::Frequency), 2500);
    EXPECT_DOUBLE_EQ(ParameterFormatting::parse("100", Style::Time), 0.1);
    EXPECT_DOUBLE_EQ(ParameterFormatting::parse("1.2 s", Style::Time), 1.2);
    EXPECT_DOUBLE_EQ(ParameterFormatting::parse("120 ms/st", Style::StepTime), 0.12);
    EXPECT_DOUBLE_EQ(ParameterFormatting::parse("-25 cent", Style::Cents), -0.25);
    EXPECT_DOUBLE_EQ(ParameterFormatting::parse("80 %", Style::Percent), 0.8);
    for (const auto* input : {"", "garbage", "nan", "inf", "1e999", "80 bananas", "80 % extra"})
        EXPECT_DOUBLE_EQ(ParameterFormatting::parse(input, Style::Percent, 0.7), 0.7);
}

TEST(ParameterFormattingTest, ParameterRoundTripsAndClamping) {
    CS01AudioProcessor processor;
    for (auto* parameter : processor.getParameters()) {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        ASSERT_NE(ranged, nullptr);
        if (dynamic_cast<juce::AudioParameterChoice*>(ranged))
            continue;
        for (const float normalised : {0.0f, ranged->getDefaultValue(), 0.37f, 1.0f}) {
            const auto value = ranged->convertFrom0to1(normalised);
            const auto text = ranged->getText(normalised, 0);
            const auto restored = ranged->convertFrom0to1(ranged->getValueForText(text));
            const auto& range = ranged->getNormalisableRange();
            EXPECT_NEAR(restored, value, std::max(0.00051f, range.interval)) << text;
        }
    }
    auto* pitch = processor.getValueTreeState().getParameter(ParameterIds::pitch);
    EXPECT_FLOAT_EQ(pitch->convertFrom0to1(pitch->getValueForText("9999 cent")), 1.0f);
    auto* volume = processor.getValueTreeState().getParameter(ParameterIds::volume);
    EXPECT_NEAR(volume->convertFrom0to1(volume->getValueForText("invalid")), 0.7f, 0.00001f);
}

TEST(ParameterFormattingTest, AttachmentsAndEditorUseFormattedValues) {
    CS01AudioProcessor processor;
    auto* volume = processor.getValueTreeState().getParameter(ParameterIds::volume);
    juce::Slider slider;
    juce::SliderParameterAttachment attachment(*volume, slider);
    EXPECT_EQ(slider.getTextFromValue(0.7), "70.0 %");
    EXPECT_NEAR(slider.getValueFromText("80 %"), 0.8, 0.00001);
    CS01AudioProcessorEditor editor(processor);
    int count = 0;
    for (auto* panel : editor.getChildren()) {
        for (auto* child : panel->getChildren()) {
            if (auto* control = dynamic_cast<juce::Slider*>(child)) {
                EXPECT_TRUE(control->getTextFromValue(control->getValue()).containsChar(' '));
                if (control->getSliderStyle() != juce::Slider::IncDecButtons)
                    EXPECT_TRUE(control->getTooltip().isEmpty());
                ++count;
            }
        }
    }
    EXPECT_EQ(count, 19);
}

TEST(ParameterFormattingTest, StateRestorePreservesInternalValues) {
    CS01AudioProcessor processor;
    auto* volume = processor.getValueTreeState().getParameter(ParameterIds::volume);
    volume->setValueNotifyingHost(volume->convertTo0to1(0.321f));
    juce::MemoryBlock state;
    processor.getStateInformation(state);
    volume->setValueNotifyingHost(1.0f);
    processor.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    EXPECT_NEAR(processor.getValueTreeState().getRawParameterValue(ParameterIds::volume)->load(),
                0.321f, 0.00001f);
}