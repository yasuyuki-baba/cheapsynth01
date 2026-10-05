#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>

// One audio-thread producer and one message-thread consumer. Storage never resizes.
class AudioDisplayFifo {
   public:
    static constexpr int capacity = 8192;
    static constexpr int displaySamples = 512;

    void setEnabled(bool enabled) {
        collecting.store(enabled);
    }

    void push(const juce::AudioBuffer<float>& audio) {
        if (!collecting.load() || audio.getNumChannels() == 0)
            return;
        int start1, size1, start2, size2;
        fifo.prepareToWrite(juce::jmin(audio.getNumSamples(), capacity - 1), start1, size1, start2,
                            size2);
        const auto copy = [&](int start, int size, int offset) {
            for (int channel = 0; channel < 2; ++channel) {
                const auto* source =
                    audio.getReadPointer(juce::jmin(channel, audio.getNumChannels() - 1));
                juce::FloatVectorOperations::copy(samples[channel].data() + start, source + offset,
                                                  size);
            }
        };
        copy(start1, size1, 0);
        copy(start2, size2, size1);
        fifo.finishedWrite(size1 + size2);
    }

    // destination is preallocated by the editor; consume all queued data, keep its latest tail.
    int readLatest(juce::AudioBuffer<float>& destination) {
        const int available = fifo.getNumReady();
        const int count = juce::jmin(available, destination.getNumSamples());
        int start1, size1, start2, size2;
        fifo.prepareToRead(available, start1, size1, start2, size2);
        const int skip = size1 + size2 - count;
        for (int channel = 0; channel < juce::jmin(2, destination.getNumChannels()); ++channel) {
            auto* output = destination.getWritePointer(channel);
            for (int i = 0; i < count; ++i) {
                const int position = skip + i;
                const int index = position < size1 ? start1 + position : start2 + position - size1;
                output[i] = samples[channel][index];
            }
        }
        fifo.finishedRead(size1 + size2);
        return count;
    }

   private:
    std::array<std::array<float, capacity>, 2> samples{};
    juce::AbstractFifo fifo{capacity};
    std::atomic<bool> collecting{false};
};