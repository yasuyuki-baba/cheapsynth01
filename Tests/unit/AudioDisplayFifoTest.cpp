#include <gtest/gtest.h>
#include <JuceHeader.h>
#include <thread>
#include "../../Source/UI/AudioDisplayFifo.h"
#include "../../Source/CS01AudioProcessor.h"
#include "../../Source/CS01AudioProcessorEditor.h"

TEST(AudioDisplayFifoTest, DisabledAndFullQueuesNeverGrowAndRecover) {
    AudioDisplayFifo fifo;
    juce::AudioBuffer<float> input(1, AudioDisplayFifo::capacity * 2);
    juce::AudioBuffer<float> output(2, AudioDisplayFifo::displaySamples);
    for (int i = 0; i < input.getNumSamples(); ++i)
        input.setSample(0, i, static_cast<float>(i));
    fifo.push(input);
    EXPECT_EQ(fifo.readLatest(output), 0);
    fifo.setEnabled(true);
    for (int i = 0; i < 100; ++i)
        fifo.push(input);
    ASSERT_EQ(fifo.readLatest(output), AudioDisplayFifo::displaySamples);
    for (int i = 0; i < output.getNumSamples(); ++i) {
        const float expected =
            static_cast<float>(AudioDisplayFifo::capacity - 1 - output.getNumSamples() + i);
        EXPECT_FLOAT_EQ(output.getSample(0, i), expected);
        EXPECT_FLOAT_EQ(output.getSample(1, i), expected);
    }
    EXPECT_EQ(fifo.readLatest(output), 0);
    fifo.push(input);
    EXPECT_EQ(fifo.readLatest(output), AudioDisplayFifo::displaySamples);
}

TEST(AudioDisplayFifoTest, WraparoundKeepsLatestStereoSamples) {
    AudioDisplayFifo fifo;
    fifo.setEnabled(true);
    juce::AudioBuffer<float> input(2, 700);
    juce::AudioBuffer<float> output(2, 512);
    for (int block = 0; block < 30; ++block) {
        for (int i = 0; i < 700; ++i) {
            input.setSample(0, i, static_cast<float>(block * 700 + i));
            input.setSample(1, i, -input.getSample(0, i));
        }
        fifo.push(input);
        ASSERT_EQ(fifo.readLatest(output), 512);
        for (int i = 0; i < 512; ++i) {
            EXPECT_FLOAT_EQ(output.getSample(0, i), static_cast<float>(block * 700 + 188 + i));
            EXPECT_FLOAT_EQ(output.getSample(1, i), -output.getSample(0, i));
        }
    }
}

TEST(AudioDisplayFifoTest, ConcurrentProducerConsumerPreserveSampleOrder) {
    AudioDisplayFifo fifo;
    fifo.setEnabled(true);
    std::atomic<bool> done{false};
    std::thread producer([&] {
        juce::AudioBuffer<float> input(2, 64);
        for (int block = 0; block < 2000; ++block) {
            for (int i = 0; i < 64; ++i) {
                input.setSample(0, i, static_cast<float>(block * 64 + i));
                input.setSample(1, i, -input.getSample(0, i));
            }
            fifo.push(input);
        }
        done.store(true);
    });
    juce::AudioBuffer<float> output(2, 512);
    float previous = -1;
    do {
        const int count = fifo.readLatest(output);
        for (int i = 0; i < count; ++i) {
            EXPECT_GT(output.getSample(0, i), previous);
            EXPECT_FLOAT_EQ(output.getSample(1, i), -output.getSample(0, i));
            previous = output.getSample(0, i);
        }
    } while (!done.load());
    producer.join();
}

TEST(AudioDisplayFifoTest, EditorVisibilityAndDestructionControlCollection) {
    CS01AudioProcessor processor;
    auto editor = std::make_unique<CS01AudioProcessorEditor>(processor);
    auto& fifo = processor.getAudioDisplayFifo();
    juce::AudioBuffer<float> input(2, 64), output(2, 512);
    input.clear();
    fifo.push(input);
    EXPECT_EQ(fifo.readLatest(output), 0);
    juce::TextButton* toggle = nullptr;
    for (auto* child : editor->getChildren())
        if (auto* button = dynamic_cast<juce::TextButton*>(child))
            if (button->getButtonText() == "KEYBOARD + MONITOR")
                toggle = button;
    ASSERT_NE(toggle, nullptr);
    toggle->setToggleState(true, juce::dontSendNotification);
    toggle->onClick();
    fifo.push(input);
    EXPECT_EQ(fifo.readLatest(output), 64);
    toggle->setToggleState(false, juce::dontSendNotification);
    toggle->onClick();
    fifo.push(input);
    EXPECT_EQ(fifo.readLatest(output), 0);
    toggle->setToggleState(true, juce::dontSendNotification);
    toggle->onClick();
    editor.reset();
    fifo.push(input);
    EXPECT_EQ(fifo.readLatest(output), 0);
}