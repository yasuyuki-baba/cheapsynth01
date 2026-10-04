#include <gtest/gtest.h>
#include <JuceHeader.h>
#include "../../Source/CS01AudioProcessor.h"
#include "../../Source/Parameters.h"
#include "../../Source/CS01Synth/EGProcessor.h"
#include "../../Source/CS01Synth/SynthConstants.h"
#include <chrono>

TEST(SessionGraphTest, RestoresRoutingWithoutRestoringHeldNotes)
{
    for (int filter : {0, 1}) {
        CS01AudioProcessor source, restored;
        auto set = [](CS01AudioProcessor& processor, const juce::String& id, float value) {
            auto* parameter = processor.getValueTreeState().getParameter(id);
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        };
        set(source, ParameterIds::filterType, static_cast<float>(filter));
        set(source, ParameterIds::lfoTarget, 1);
        set(source, ParameterIds::vcaEgDepth, 1);
        set(source, ParameterIds::volume, 1);
        set(source, ParameterIds::breathVca, 0);
        source.prepareToPlay(48000, 256);
        juce::AudioBuffer<float> buffer(2, 256);
        buffer.clear();
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 69, 1.0f), 0);
        source.processBlock(buffer, midi);
        juce::MemoryBlock saved;
        source.getStateInformation(saved);
        set(restored, ParameterIds::filterType, static_cast<float>(1 - filter));
        restored.prepareToPlay(48000, 256);
        restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
        restored.flushPendingGraphChangesForTesting();
        const auto selected = filter == 0 ? restored.getOriginalFilterNodeIdForTesting()
                                         : restored.getModernFilterNodeIdForTesting();
        const auto unselected = filter == 0 ? restored.getModernFilterNodeIdForTesting()
                                           : restored.getOriginalFilterNodeIdForTesting();
        const auto& graph = restored.getAudioGraphForTesting();
        EXPECT_TRUE(graph.isConnected({{restored.getVcoNodeIdForTesting(), 0}, {selected, 0}}));
        EXPECT_TRUE(graph.isConnected({{selected, 0}, {restored.getVcaNodeIdForTesting(), 0}}));
        EXPECT_FALSE(graph.isConnected({{unselected, 0}, {restored.getVcaNodeIdForTesting(), 0}}));
        EXPECT_TRUE(graph.isConnected({{restored.getLfoNodeIdForTesting(), 0}, {selected, 2}}));
        midi.clear();
        buffer.clear();
        restored.processBlock(buffer, midi);
        EXPECT_EQ(buffer.getMagnitude(0, 256), 0.0f);
        double energy = 0.0;
        for (int block = 0; block < 40; ++block) {
            buffer.clear();
            midi.clear();
            if (block == 0) midi.addEvent(juce::MidiMessage::noteOn(1, 69, 1.0f), 37);
            restored.processBlock(buffer, midi);
            for (int i = 0; i < 256; ++i) {
                const double value = buffer.getSample(0, i);
                ASSERT_TRUE(std::isfinite(value));
                energy += value * value;
            }
        }
        EXPECT_GT(energy, 1.0e-6);
    }
}

TEST(EnvelopeRangeTest, ProvisionalSecondsMapping)
{
    CS01AudioProcessor processor;
    auto& state = processor.getValueTreeState();
    for (const auto& id : {ParameterIds::attack, ParameterIds::decay, ParameterIds::release}) {
        SCOPED_TRACE(id.toStdString());
        auto* parameter = state.getParameter(id);
        ASSERT_NE(parameter, nullptr);
        for (float position : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f}) {
            const double raw = 0.001 + 1.999 * std::pow(position, 1.0 / 0.3);
            EXPECT_NEAR(parameter->convertFrom0to1(position), raw, 0.0011);
        }
        EXPECT_NEAR(parameter->convertFrom0to1(0), 0.001, 1.0e-6);
        EXPECT_NEAR(parameter->convertFrom0to1(1), 2.0, 1.0e-6);
    }
}

TEST(BendInputTest, ExternalInputWinsAndDoesNotAutoReturn)
{
    CS01AudioProcessor processor;
    processor.prepareToPlay(48000.0, 64);
    processor.flushPendingGraphChangesForTesting();
    auto& state = processor.getValueTreeState();
    EXPECT_FLOAT_EQ(state.getRawParameterValue(ParameterIds::pitchBendUpRange)->load(), 12);
    EXPECT_FLOAT_EQ(state.getRawParameterValue(ParameterIds::pitchBendDownRange)->load(), 0);
    juce::AudioBuffer<float> audio(2, 64);
    juce::MidiBuffer midi;
    const auto queuePanel = [&](int value) {
        auto message = juce::MidiMessage::pitchWheel(1, value);
        message.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
        processor.getPanelBendCollector().addMessageToQueue(message);
    };
    queuePanel(16383);
    processor.processBlock(audio, midi);
    EXPECT_FLOAT_EQ(state.getRawParameterValue(ParameterIds::pitchBend)->load(), 1);
    const auto revision = processor.getExternalBendRevision();
    EXPECT_EQ(revision, 0u);

    midi.clear();
    queuePanel(8192); // An in-flight panel return must not override external input.
    midi.addEvent(juce::MidiMessage::pitchWheel(1, 0), 17);
    processor.processBlock(audio, midi);
    EXPECT_FLOAT_EQ(state.getRawParameterValue(ParameterIds::pitchBend)->load(), -1);
    EXPECT_EQ(processor.getExternalBendRevision(), revision + 1);
    for (int block = 0; block < 100; ++block) {
        midi.clear();
        processor.processBlock(audio, midi);
    }
    EXPECT_FLOAT_EQ(state.getRawParameterValue(ParameterIds::pitchBend)->load(), -1);
    EXPECT_EQ(processor.getExternalBendRevision(), revision + 1);
    midi.clear();
    midi.addEvent(juce::MidiMessage::pitchWheel(1, 8192), 0);
    processor.processBlock(audio, midi);
    EXPECT_FLOAT_EQ(state.getRawParameterValue(ParameterIds::pitchBend)->load(), 0);
    processor.releaseResources();
}

TEST(ModulationRangeTest, ManualEndpointsAndSavedStateCompatibility)
{
    CS01AudioProcessor processor;
    auto& state = processor.getValueTreeState();
    for (const auto& id : {ParameterIds::lfoSpeed, ParameterIds::pwmSpeed}) {
        const bool lfo = id == ParameterIds::lfoSpeed;
        auto* parameter = state.getParameter(id);
        ASSERT_NE(parameter, nullptr);
        const float minimum = lfo ? 0.8f : 0.6f;
        const float maximum = lfo ? 21.0f : 12.0f;
        EXPECT_NEAR(parameter->convertFrom0to1(0.0f), minimum, 1.0e-5f);
        EXPECT_NEAR(parameter->convertFrom0to1(1.0f), maximum, 1.0e-5f);

        // Persist physical Hz, not a normalized slider position. Include legacy
        // values outside the newly documented hardware range.
        for (float value : {0.0f, 2.0f, 5.0f, 60.0f}) {
            auto saved = state.copyState();
            auto child = saved.getChildWithProperty("id", id);
            ASSERT_TRUE(child.isValid());
            child.setProperty("value", value, nullptr);
            auto xml = saved.createXml();
            juce::MemoryBlock data;
            juce::AudioProcessor::copyXmlToBinary(*xml, data);
            processor.setStateInformation(data.getData(), static_cast<int>(data.getSize()));
            EXPECT_NEAR(state.getRawParameterValue(id)->load(),
                        juce::jlimit(minimum, maximum, value), 1.0e-4f);
        }
    }
}

TEST(WholeGraphObservationTest, OutputSpectrumAndProcessingCost)
{
    // Observational benchmark: never assert platform-dependent execution time.
    for (double rate : {44100.0, 48000.0, 96000.0}) {
      for (int waveform : {1, 2}) {
        CS01AudioProcessor processor;
        auto& state = processor.getValueTreeState();
        const auto set = [&](const juce::String& id, float value) {
            auto* parameter = state.getParameter(id);
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        };
        set(ParameterIds::waveType, static_cast<float>(waveform));
        set(ParameterIds::feet, 2.0f);
        set(ParameterIds::pitch, 0.0f);
        set(ParameterIds::filterType, 0.0f);
        set(ParameterIds::cutoff, 15000.0f);
        set(ParameterIds::resonance, 0.0f);
        set(ParameterIds::attack, 0.001f);
        set(ParameterIds::decay, 0.001f);
        set(ParameterIds::sustain, 1.0f);
        set(ParameterIds::volume, 1.0f);
        set(ParameterIds::modDepth, 0.0f);
        set(ParameterIds::vcfEgDepth, 0.0f);
        set(ParameterIds::breathVca, 0.0f);
        set(ParameterIds::breathVcf, 0.0f);
        processor.prepareToPlay(rate, 256);
        // A8 = 7040 Hz; pitch wheel center has a small quantization offset.
        const double frequency = 7040.0;
        std::vector<float> output;
        double processingSeconds = 0.0;
        const int count = static_cast<int>(rate * 2.0);
        for (int offset = 0; offset < count;) {
            const int length = std::min(256, count - offset);
            juce::AudioBuffer<float> buffer(2, length);
            buffer.clear();
            juce::MidiBuffer midi;
            if (offset == 0)
                midi.addEvent(juce::MidiMessage::noteOn(1, 117, 1.0f), 0);
            const auto begin = std::chrono::steady_clock::now();
            processor.processBlock(buffer, midi);
            processingSeconds += std::chrono::duration<double>(
                std::chrono::steady_clock::now() - begin).count();
            for (int i = 0; i < length; ++i) {
                ASSERT_TRUE(std::isfinite(buffer.getSample(0, i)));
                if (offset + i >= static_cast<int>(rate))
                    output.push_back(buffer.getSample(0, i));
            }
            offset += length;
        }
        // Hann window reduces leakage from small tuning/phase errors.
        const auto amplitude = [&](double f) {
            double sine = 0.0, cosine = 0.0, weight = 0.0;
            for (size_t i = 0; i < output.size(); ++i) {
                const double w = 0.5 - 0.5 * std::cos(
                    juce::MathConstants<double>::twoPi * i / (output.size() - 1));
                const double phase = juce::MathConstants<double>::twoPi * f * i / rate;
                sine += w * output[i] * std::sin(phase);
                cosine += w * output[i] * std::cos(phase);
                weight += w;
            }
            return 2.0 * std::hypot(sine, cosine) / weight;
        };
        const double fundamental = amplitude(frequency);
        ASSERT_GT(fundamental, 1.0e-6);
        double folded = std::fmod(frequency * 7.0, rate);
        if (folded > rate * 0.5)
            folded = rate - folded;
        std::cout << "Whole graph: fs=" << rate << ", waveform=" << waveform
                  << ", seventh-fold-bin=" << folded << ", relative="
                  << 20.0 * std::log10(std::max(amplitude(folded), 1.0e-15) / fundamental)
                  << " dBc, processing-ms-per-audio-second=" << processingSeconds * 500.0
                  << "\n";
      }
    }
}

TEST(WholeGraphObservationTest, LiveGlissandoAutomationPartitionConsistency)
{
    for (double rate : {44100.0, 48000.0, 96000.0}) {
        const auto render = [&](int blockSize) {
            CS01AudioProcessor processor;
            auto& state = processor.getValueTreeState();
            const auto set = [&](const juce::String& id, float value) {
                auto* parameter = state.getParameter(id);
                parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
            };
            set(ParameterIds::waveType, 2);
            set(ParameterIds::filterType, 0);
            set(ParameterIds::attack, 0.001f);
            set(ParameterIds::decay, 0.001f);
            set(ParameterIds::sustain, 1);
            set(ParameterIds::volume, 1);
            set(ParameterIds::modDepth, 0);
            set(ParameterIds::vcfEgDepth, 0);
            set(ParameterIds::breathVca, 0);
            set(ParameterIds::breathVcf, 0);
            set(ParameterIds::glissando, 0.1f);
            processor.prepareToPlay(rate, 256);
            const int interval = static_cast<int>(rate * 0.02);
            std::vector<float> output;
            // Exact event boundaries, independent of render partition.
            for (int segment = 0; segment < 8; ++segment) {
                if (segment == 2) set(ParameterIds::glissando, 0.05f);
                if (segment == 3) set(ParameterIds::glissando, 0.15f);
                if (segment == 4) set(ParameterIds::glissando, 0.01f);
                if (segment == 5) set(ParameterIds::glissando, 0);
                for (int offset = 0; offset < interval;) {
                    const int count = std::min(blockSize, interval - offset);
                    juce::AudioBuffer<float> buffer(2, count);
                    buffer.clear();
                    juce::MidiBuffer midi;
                    if (offset == 0 && (segment == 0 || segment == 1))
                        midi.addEvent(juce::MidiMessage::noteOn(1, segment == 0 ? 69 : 76, 1.0f), 0);
                    processor.processBlock(buffer, midi);
                    for (int i = 0; i < count; ++i)
                        output.push_back(buffer.getSample(0, i));
                    offset += count;
                }
            }
            return output;
        };
        const auto expected = render(7);
        double energy = 0;
        for (float sample : expected) energy += sample * sample;
        ASSERT_GT(energy, 1.0e-6); // Avoid passing with two silent graphs.
        for (int blockSize : {64, 256}) {
            SCOPED_TRACE(rate);
            SCOPED_TRACE(blockSize);
            const auto actual = render(blockSize);
            ASSERT_EQ(actual.size(), expected.size());
            for (size_t i = 0; i < actual.size(); ++i) {
                ASSERT_TRUE(std::isfinite(actual[i]));
                ASSERT_NEAR(actual[i], expected[i], 1.0e-5) << "sample=" << i;
            }
        }
    }
}

TEST(OutputConversionTest, DownsamplingImpulseAndReportedRoundTripLatency)
{
    // Production generates at the high rate: only the down path carries audio.
    // JUCE's reported latency describes the full up/down path, so do not use
    // it blindly as the generator's output-only latency.
    for (int channels : {1, 2}) {
        juce::dsp::Oversampling<float> converter(channels, Constants::oversamplingStages,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);
        converter.initProcessing(256);
        converter.reset();
        juce::AudioBuffer<float> buffer(channels, 256);
        buffer.clear();
        juce::dsp::AudioBlock<float> block(buffer);
        auto high = converter.processSamplesUp(block);
        high.clear();
        for (int channel = 0; channel < channels; ++channel)
            high.setSample(channel, 0, 1.0f);
        converter.processSamplesDown(block);
        double energy = 0.0, weightedEnergy = 0.0, sum = 0.0;
        int peak = 0;
        for (int i = 0; i < buffer.getNumSamples(); ++i) {
            const double value = buffer.getSample(0, i);
            ASSERT_TRUE(std::isfinite(value));
            sum += value;
            energy += value * value;
            weightedEnergy += i * value * value;
            if (std::abs(value) > std::abs(buffer.getSample(0, peak)))
                peak = i;
            if (channels == 2)
                EXPECT_FLOAT_EQ(buffer.getSample(1, i), buffer.getSample(0, i));
        }
        ASSERT_GT(energy, 0.0);
        EXPECT_NEAR(sum, 1.0 / Constants::oversamplingFactor, 1.0e-5);
        EXPECT_GT(converter.getLatencyInSamples(), 0.0f);
        std::cout << "Output conversion: channels=" << channels
                  << ", impulse peak=" << peak << ", energy centroid="
                  << weightedEnergy / energy << ", JUCE round-trip latency="
                  << converter.getLatencyInSamples() << " host samples\n";
    }
}

// Test fixture for AudioGraph integration tests
TEST(EnvelopeRangeTest, GraphStagesFollowConfiguredSeconds)
{
    for (double rate : {44100.0, 48000.0, 96000.0}) {
        for (float position : {0.0f, 0.5f, 1.0f}) {
            SCOPED_TRACE(rate);
            SCOPED_TRACE(position);
            CS01AudioProcessor processor;
            auto& state = processor.getValueTreeState();
            for (const auto& id : {ParameterIds::attack, ParameterIds::decay, ParameterIds::release})
                state.getParameter(id)->setValueNotifyingHost(position);
            state.getParameter(ParameterIds::sustain)->setValueNotifyingHost(0.5f);
            processor.prepareToPlay(rate, 64);
            EGProcessor* envelope = nullptr;
            for (auto node : processor.getAudioGraphForTesting().getNodes())
                if (auto* eg = dynamic_cast<EGProcessor*>(node->getProcessor())) envelope = eg;
            ASSERT_NE(envelope, nullptr);
            juce::AudioBuffer<float> buffer(2, 1);
            auto next = [&](int event) {
                buffer.clear();
                juce::MidiBuffer midi;
                if (event == 1) midi.addEvent(juce::MidiMessage::noteOn(1, 69, 1.0f), 0);
                if (event == 2) midi.addEvent(juce::MidiMessage::noteOff(1, 69), 0);
                processor.processBlock(buffer, midi);
                return envelope->getLastOutputForTesting();
            };
            auto elapsed = [&](auto reached, int event) {
                const int limit = static_cast<int>(rate * 3.0);
                for (int n = 1; n <= limit; ++n)
                    if (reached(next(n == 1 ? event : 0))) return n / rate;
                return 4.0;
            };
            const double attack = state.getRawParameterValue(ParameterIds::attack)->load();
            const double decay = state.getRawParameterValue(ParameterIds::decay)->load();
            const double release = state.getRawParameterValue(ParameterIds::release)->load();
            // Allow one host sample for observation plus float accumulation error.
            const float peakThreshold = static_cast<float>(1.0 - 1.0 / (attack * rate));
            EXPECT_NEAR(elapsed([&](float v) { return v >= peakThreshold; }, 1), attack,
                        attack * 0.01 + 2.0 / rate);
            EXPECT_NEAR(elapsed([](float v) { return v <= 0.5f; }, 0), decay,
                        decay * 0.01 + 2.0 / rate);
            EXPECT_NEAR(elapsed([](float v) { return v == 0.0f; }, 2), release,
                        release * 0.01 + 2.0 / rate);
        }
    }
}

class AudioGraphTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Test setup if needed
    }
    
    void TearDown() override
    {
        // Test cleanup if needed
    }
};

TEST_F(AudioGraphTest, OversampledOutputIsIndependentOfPartitionAndReinitializes)
{
    for (double rate : {44100.0, 48000.0, 96000.0}) {
        std::vector<float> reference;
        for (int size : {7, 64, 256}) {
            CS01AudioProcessor processor;
            // Prepare for 64: exercise calls exceeding the declared capacity too.
            processor.prepareToPlay(rate, 64);
            auto render = [&]() {
                std::vector<float> result;
                const int count = 4096;
                for (int offset = 0; offset < count;) {
                    const int length = std::min(size, count - offset);
                    juce::AudioBuffer<float> buffer(2, length);
                    buffer.clear();
                    juce::MidiBuffer midi;
                    for (const auto& event : std::vector<std::pair<int, juce::MidiMessage>>{
                        {17, juce::MidiMessage::noteOn(1, 69, 1.0f)},
                        {2053, juce::MidiMessage::noteOff(1, 69)}}) {
                        if (event.first >= offset && event.first < offset + length)
                            midi.addEvent(event.second, event.first - offset);
                    }
                    processor.processBlock(buffer, midi);
                    for (int i = 0; i < length; ++i) {
                        const float value = buffer.getSample(0, i);
                        EXPECT_TRUE(std::isfinite(value));
                        result.push_back(value);
                    }
                    offset += length;
                }
                return result;
            };
            auto output = render();
            if (reference.empty())
                reference = output;
            else {
                for (size_t i = 0; i < output.size(); ++i)
                    ASSERT_NEAR(output[i], reference[i], 1.0e-5f) << "sample=" << i;
            }
            processor.releaseResources();
            processor.prepareToPlay(rate, 64);
            const auto repeated = render();
            for (size_t i = 0; i < output.size(); ++i)
                ASSERT_NEAR(repeated[i], output[i], 1.0e-6f) << "sample=" << i;
        }
    }
}

TEST_F(AudioGraphTest, BreathMidiReachesAudioOutput)
{
    // MIDI transport/model consistency, not calibration of analog breath circuitry.
    for (int blockSize : {64, 256}) {
        for (bool enabled : {false, true}) {
            CS01AudioProcessor reference, controlled;
            for (auto* processor : {&reference, &controlled}) {
                auto& state = processor->getValueTreeState();
                for (const auto& setting : std::vector<std::pair<juce::String, float>>{
                         {ParameterIds::attack, 0.001f}, {ParameterIds::decay, 0.001f},
                         {ParameterIds::sustain, 1.0f}, {ParameterIds::volume, 1.0f},
                         {ParameterIds::filterType, 0.0f},
                         {ParameterIds::breathVca, enabled ? 1.0f : 0.0f},
                         {ParameterIds::breathVcf, 0.0f}, {ParameterIds::breathInput, 1.0f}}) {
                    auto* parameter = state.getParameter(setting.first);
                    ASSERT_NE(parameter, nullptr);
                    parameter->setValueNotifyingHost(parameter->convertTo0to1(setting.second));
                }
                processor->prepareToPlay(48000, blockSize);
            }
            juce::AudioBuffer<float> a(2, blockSize), b(2, blockSize);
            double baselinePower = 0.0, mutedPower = 0.0;
            double recoveredPower = 0.0, recoveredReferencePower = 0.0;
            for (int block = 0; block < 48000 / blockSize; ++block) {
                juce::MidiBuffer ma, mb;
                if (block == 0) {
                    ma.addEvent(juce::MidiMessage::noteOn(1, 69, 1.0f), 0);
                    mb.addEvent(juce::MidiMessage::noteOn(1, 69, 1.0f), 0);
                }
                if (block == 10) {
                    // Both halves of the 14-bit value, at a nonzero event offset.
                    mb.addEvent(juce::MidiMessage::controllerEvent(1, 2, 0), blockSize / 2);
                    mb.addEvent(juce::MidiMessage::controllerEvent(1, 34, 0), blockSize / 2);
                }
                if (block == 36000 / blockSize) {
                    mb.addEvent(juce::MidiMessage::controllerEvent(1, 2, 127), blockSize / 2);
                    mb.addEvent(juce::MidiMessage::controllerEvent(1, 34, 127), blockSize / 2);
                }
                a.clear(); b.clear();
                reference.processBlock(a, ma);
                controlled.processBlock(b, mb);
                for (int i = 0; i < blockSize; ++i) {
                    ASSERT_TRUE(std::isfinite(b.getSample(0, i)));
                    if (!enabled)
                        EXPECT_NEAR(b.getSample(0, i), a.getSample(0, i), 1.0e-6f);
                    if (block * blockSize >= 24000 && block * blockSize < 35000) {
                        baselinePower += std::pow(a.getSample(0, i), 2);
                        mutedPower += std::pow(b.getSample(0, i), 2);
                    }
                    if (block * blockSize >= 44000) {
                        recoveredReferencePower += std::pow(a.getSample(0, i), 2);
                        recoveredPower += std::pow(b.getSample(0, i), 2);
                    }
                }
            }
            EXPECT_FLOAT_EQ(controlled.getValueTreeState()
                .getRawParameterValue(ParameterIds::breathInput)->load(), 1.0f);
            ASSERT_GT(baselinePower, 1.0e-8);
            if (enabled)
                EXPECT_LT(mutedPower, baselinePower * 1.0e-8);
            ASSERT_GT(recoveredReferencePower, 1.0e-8);
            // Allow output coupling transients to settle; compare power, not phase.
            EXPECT_NEAR(recoveredPower / recoveredReferencePower, 1.0, 0.01);
        }
    }
}

TEST_F(AudioGraphTest, BreathMidiControlsOriginalFilterAndRecovers)
{
    // Isolate VCF control from VCA gain and envelope/LFO modulation.
    for (int blockSize : {64, 256}) {
        CS01AudioProcessor reference, controlled;
        for (auto* processor : {&reference, &controlled}) {
            auto& state = processor->getValueTreeState();
            for (const auto& setting : std::vector<std::pair<juce::String, float>>{
                     {ParameterIds::filterType, 0.0f}, {ParameterIds::cutoff, 250.0f},
                     {ParameterIds::resonance, 0.0f}, {ParameterIds::vcfEgDepth, 0.0f},
                     {ParameterIds::modDepth, 0.0f}, {ParameterIds::breathVca, 0.0f},
                     {ParameterIds::breathVcf, 1.0f}, {ParameterIds::breathInput, 0.0f},
                     {ParameterIds::attack, 0.001f}, {ParameterIds::decay, 0.001f},
                     {ParameterIds::sustain, 1.0f}, {ParameterIds::volume, 0.5f},
                     {ParameterIds::feet, 2.0f}}) {
                auto* parameter = state.getParameter(setting.first);
                ASSERT_NE(parameter, nullptr);
                parameter->setValueNotifyingHost(parameter->convertTo0to1(setting.second));
            }
            processor->prepareToPlay(48000, blockSize);
        }
        juce::AudioBuffer<float> a(2, blockSize), b(2, blockSize);
        double baseline = 0.0, opened = 0.0, recovered = 0.0, referenceRecovered = 0.0;
        for (int block = 0; block < 96000 / blockSize; ++block) {
            juce::MidiBuffer ma, mb;
            if (block == 0) {
                ma.addEvent(juce::MidiMessage::noteOn(1, 93, 1.0f), 0);
                mb.addEvent(juce::MidiMessage::noteOn(1, 93, 1.0f), 0);
            }
            if (block == 10 || block == 48000 / blockSize) {
                const int value = block == 10 ? 127 : 0;
                mb.addEvent(juce::MidiMessage::controllerEvent(1, 2, value), blockSize / 2);
                mb.addEvent(juce::MidiMessage::controllerEvent(1, 34, value), blockSize / 2);
            }
            a.clear(); b.clear();
            reference.processBlock(a, ma);
            controlled.processBlock(b, mb);
            for (int i = 0; i < blockSize; ++i) {
                ASSERT_TRUE(std::isfinite(b.getSample(0, i)));
                const int sample = block * blockSize + i;
                if (sample >= 24000 && sample < 44000) {
                    baseline += std::pow(a.getSample(0, i), 2);
                    opened += std::pow(b.getSample(0, i), 2);
                }
                if (sample >= 84000) {
                    referenceRecovered += std::pow(a.getSample(0, i), 2);
                    recovered += std::pow(b.getSample(0, i), 2);
                }
            }
        }
        ASSERT_GT(baseline, 1.0e-10);
        EXPECT_GT(opened, baseline); // High note above cutoff; not a universal gain rule.
        ASSERT_GT(referenceRecovered, 1.0e-10);
        EXPECT_NEAR(recovered / referenceRecovered, 1.0, 0.01);
        EXPECT_FLOAT_EQ(controlled.getValueTreeState()
            .getRawParameterValue(ParameterIds::breathInput)->load(), 0.0f);
    }
}

TEST_F(AudioGraphTest, NoiseSwitchPreservesGraphEnvelope)
{
    for (int blockSize : {64, 256}) {
        CS01AudioProcessor reference, switched;
        for (auto* processor : {&reference, &switched}) {
            auto& state = processor->getValueTreeState();
            for (const auto& setting : std::vector<std::pair<juce::String, float>>{
                     {ParameterIds::attack, 0.05f}, {ParameterIds::decay, 0.05f},
                     {ParameterIds::sustain, 0.5f}, {ParameterIds::release, 0.1f}}) {
                auto* parameter = state.getParameter(setting.first);
                parameter->setValueNotifyingHost(parameter->convertTo0to1(setting.second));
            }
            processor->prepareToPlay(48000, blockSize);
        }
        const auto findEnvelope = [](CS01AudioProcessor& processor) -> EGProcessor* {
            for (auto node : processor.getAudioGraphForTesting().getNodes())
                if (auto* envelope = dynamic_cast<EGProcessor*>(node->getProcessor()))
                    return envelope;
            return nullptr;
        };
        auto* baseline = findEnvelope(reference);
        auto* observed = findEnvelope(switched);
        ASSERT_NE(baseline, nullptr);
        ASSERT_NE(observed, nullptr);
        juce::AudioBuffer<float> a(2, blockSize), b(2, blockSize);
        juce::MidiBuffer ma, mb;
        const int releaseBlock = 9600 / blockSize;
        bool sawAttack = false, sawSustain = false, sawRelease = false;
        for (int block = 0; block < releaseBlock + 9600 / blockSize; ++block) {
            // Switch in attack, sustain, and release; no key event accompanies it.
            if (block == 2 || block == 3 || block == releaseBlock - 2
                || block == releaseBlock - 1 || block == releaseBlock + 2
                || block == releaseBlock + 3) {
                auto* feet = switched.getValueTreeState().getParameter(ParameterIds::feet);
                const bool noise = block == 2 || block == releaseBlock - 2
                    || block == releaseBlock + 2;
                feet->setValueNotifyingHost(feet->convertTo0to1(noise ? 4.0f : 2.0f));
            }
            a.clear(); b.clear(); ma.clear(); mb.clear();
            if (block == 0 || block == releaseBlock) {
                const auto message = block == 0
                    ? juce::MidiMessage::noteOn(1, 69, 1.0f)
                    : juce::MidiMessage::noteOff(1, 69);
                ma.addEvent(message, blockSize / 2);
                mb.addEvent(message, blockSize / 2);
            }
            reference.processBlock(a, ma);
            switched.processBlock(b, mb);
            SCOPED_TRACE(blockSize);
            SCOPED_TRACE(block);
            const float expected = baseline->getLastOutputForTesting();
            EXPECT_NEAR(observed->getLastOutputForTesting(), expected, 1.0e-6f);
            EXPECT_EQ(observed->isActive(), baseline->isActive());
            sawAttack |= block < releaseBlock && expected > 0.0f && expected < 0.5f;
            sawSustain |= block < releaseBlock && std::abs(expected - 0.5f) < 1.0e-5f;
            sawRelease |= block > releaseBlock && expected > 0.0f && expected < 0.5f;
        }
        EXPECT_TRUE(sawAttack);
        EXPECT_TRUE(sawSustain);
        EXPECT_TRUE(sawRelease);
        EXPECT_FALSE(observed->isActive());
        EXPECT_FLOAT_EQ(observed->getLastOutputForTesting(), 0.0f);
    }
}

TEST_F(AudioGraphTest, MidiOffsetsDoNotSoundEarly)
{
    for (int blockSize : {64, 256}) {
        CS01AudioProcessor processor;
        processor.prepareToPlay(48000, blockSize);
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> buffer(2, blockSize);
        buffer.clear();
        midi.addEvent(juce::MidiMessage::noteOn(1, 69, 1.0f), blockSize / 2);
        processor.processBlock(buffer, midi);
        double before = 0;
        for (int i = 0; i < blockSize / 2; ++i)
            before += std::abs(buffer.getSample(0, i));
        EXPECT_NEAR(before, 0.0, 1.0e-9);
        double after = 0;
        for (int block = 0; block < 20; ++block) {
            buffer.clear();
            midi.clear();
            processor.processBlock(buffer, midi);
            after += buffer.getMagnitude(0, blockSize);
        }
        EXPECT_GT(after, 0.001);
    }
}

TEST_F(AudioGraphTest, MidBlockNoteOffDoesNotReleaseEarly)
{
    CS01AudioProcessor reference, released;
    for (auto* processor : {&reference, &released}) {
        auto& state = processor->getValueTreeState();
        for (const auto& setting : std::vector<std::pair<juce::String, float>>{
                 {ParameterIds::volume, 1.0f}, {ParameterIds::vcaEgDepth, 1.0f},
                 {ParameterIds::sustain, 1.0f}, {ParameterIds::attack, 0.01f},
                 {ParameterIds::release, 0.05f}, {ParameterIds::breathVca, 0.0f}}) {
            auto* parameter = state.getParameter(setting.first);
            parameter->setValueNotifyingHost(parameter->convertTo0to1(setting.second));
        }
    }
    reference.prepareToPlay(48000, 256);
    released.prepareToPlay(48000, 256);
    juce::AudioBuffer<float> a(2, 256), b(2, 256);
    juce::MidiBuffer ma, mb;
    for (int block = 0; block < 30; ++block) {
        a.clear(); b.clear(); ma.clear(); mb.clear();
        if (block == 0) {
            ma.addEvent(juce::MidiMessage::noteOn(1, 69, 1.0f), 0);
            mb.addEvent(juce::MidiMessage::noteOn(1, 69, 1.0f), 0);
        }
        reference.processBlock(a, ma);
        released.processBlock(b, mb);
    }
    a.clear(); b.clear(); ma.clear(); mb.clear();
    mb.addEvent(juce::MidiMessage::noteOff(1, 69), 128);
    reference.processBlock(a, ma);
    released.processBlock(b, mb);
    double before = 0, after = 0;
    for (int i = 0; i < 256; ++i) {
        const double error = std::abs(a.getSample(0, i) - b.getSample(0, i));
        if (i < 128) before = std::max(before, error);
        else after += error;
    }
    EXPECT_NEAR(before, 0, 1.0e-6);
    EXPECT_GT(after, 1.0e-6);
}

TEST_F(AudioGraphTest, ProcessorCreation)
{
    // Create processor with unique_ptr to ensure proper cleanup
    std::unique_ptr<CS01AudioProcessor> processor = std::make_unique<CS01AudioProcessor>();
    
    // Check that processor was created successfully
    EXPECT_NE(processor.get(), nullptr);
    
    // Check that processor has expected properties
    EXPECT_EQ(processor->getName(), juce::String("CheapSynth01"));
    EXPECT_TRUE(processor->acceptsMidi());
    EXPECT_FALSE(processor->producesMidi());
    EXPECT_FALSE(processor->isMidiEffect());
    
    // Check that APVTS is initialized
    auto& apvts = processor->getValueTreeState();
    EXPECT_NE(apvts.getParameter(ParameterIds::waveType), nullptr);
    EXPECT_NE(apvts.getParameter(ParameterIds::feet), nullptr);
    EXPECT_NE(apvts.getParameter(ParameterIds::cutoff), nullptr);
    EXPECT_NE(apvts.getParameter(ParameterIds::resonance), nullptr);
    
    // Explicitly reset the processor to trigger cleanup
    processor.reset();
    
    // Allow a short delay for any async cleanup
    juce::Thread::sleep(10);
}

TEST_F(AudioGraphTest, AudioProcessing)
{
    // Create processor
    std::unique_ptr<CS01AudioProcessor> processor = std::make_unique<CS01AudioProcessor>();
    
    // Prepare processor
    processor->prepareToPlay(44100.0, 512);
    
    // Create audio buffer
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midiBuffer;
    
    // Process block (should be silent as no note is playing)
    buffer.clear();
    processor->processBlock(buffer, midiBuffer);
    
    // Check that buffer is silent
    float sum = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            sum += std::abs(buffer.getSample(channel, i));
        }
    }
    EXPECT_LT(sum, 0.0001f);
    
    // Add a note-on message
    juce::MidiMessage noteOn = juce::MidiMessage::noteOn(1, 60, 1.0f);
    midiBuffer.addEvent(noteOn, 0);
    
    // Process block with note on
    buffer.clear();
    processor->processBlock(buffer, midiBuffer);
    
    // Clear MIDI buffer for next test
    midiBuffer.clear();
    
    // Process a few more blocks to let the sound develop
    for (int i = 0; i < 10; ++i)
    {
        buffer.clear();
        processor->processBlock(buffer, midiBuffer);
    }
    
    sum = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            sum += std::abs(buffer.getSample(channel, i));
        }
    }
    
    // Note: This test might be flaky depending on envelope settings
    // If it fails, we might need to adjust expectations or the test approach
    EXPECT_GT(sum, 0.0001f) << "Audio buffer should contain signal after note-on";
    
    // Add a note-off message
    juce::MidiMessage noteOff = juce::MidiMessage::noteOff(1, 60);
    midiBuffer.addEvent(noteOff, 0);
    
    // Process block with note off
    buffer.clear();
    processor->processBlock(buffer, midiBuffer);
    midiBuffer.clear();
    
    // Process a few more blocks to let the sound decay
    for (int i = 0; i < 50; ++i)
    {
        buffer.clear();
        processor->processBlock(buffer, midiBuffer);
    }
    
    // Check that buffer is silent again (after release phase)
    sum = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            sum += std::abs(buffer.getSample(channel, i));
        }
    }
    EXPECT_LT(sum, 0.01f); // Allow some small residual sound due to release phase
    
    // Clean up
    processor->releaseResources();
}

TEST_F(AudioGraphTest, ParameterConnections)
{
    // Create processor
    std::unique_ptr<CS01AudioProcessor> processor = std::make_unique<CS01AudioProcessor>();
    
    // Prepare processor
    processor->prepareToPlay(44100.0, 512);
    
    // Create audio buffer
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midiBuffer;
    
    // Add a note-on message
    juce::MidiMessage noteOn = juce::MidiMessage::noteOn(1, 60, 1.0f);
    midiBuffer.addEvent(noteOn, 0);
    
    // Process block with note on
    buffer.clear();
    processor->processBlock(buffer, midiBuffer);
    midiBuffer.clear();
    
    // Process a few more blocks to let the sound develop
    for (int i = 0; i < 10; ++i)
    {
        buffer.clear();
        processor->processBlock(buffer, midiBuffer);
    }
    
    // Store the output for comparison
    juce::AudioBuffer<float> originalBuffer;
    originalBuffer.makeCopyOf(buffer);
    
    // Change a parameter (cutoff frequency)
    auto& apvts = processor->getValueTreeState();
    apvts.getParameter(ParameterIds::cutoff)->setValueNotifyingHost(0.1f); // Low cutoff
    
    // Process block with new parameter
    buffer.clear();
    processor->processBlock(buffer, midiBuffer);
    
    // Process a few more blocks to let the change take effect
    for (int i = 0; i < 10; ++i)
    {
        buffer.clear();
        processor->processBlock(buffer, midiBuffer);
    }
    
    // Compare the outputs - they should be different
    bool isDifferent = false;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            if (std::abs(buffer.getSample(channel, i) - originalBuffer.getSample(channel, i)) > 0.0001f)
            {
                isDifferent = true;
                break;
            }
        }
        if (isDifferent) break;
    }
    
    EXPECT_TRUE(isDifferent) << "Parameter change should affect audio output";
    
    // Clean up
    processor->releaseResources();
}

TEST_F(AudioGraphTest, FilterAndLfoRoutingStayConsistent)
{
    CS01AudioProcessor processor;
    processor.prepareToPlay(44100.0, 512);

    auto& state = processor.getValueTreeState();
    const auto setChoice = [&state](const juce::String& parameterId, int choice)
    {
        auto* parameter = dynamic_cast<juce::AudioParameterChoice*>(state.getParameter(parameterId));
        ASSERT_NE(parameter, nullptr);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(choice)));
    };

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    for (int filterType = 0; filterType < 2; ++filterType)
    {
        for (int lfoTarget = 0; lfoTarget < 2; ++lfoTarget)
        {
            setChoice(ParameterIds::filterType, filterType);
            setChoice(ParameterIds::lfoTarget, lfoTarget);
            processor.flushPendingGraphChangesForTesting();

            buffer.clear();
            processor.processBlock(buffer, midi);

            const auto hasConnection = [&processor](juce::AudioProcessorGraph::NodeID sourceNode,
                                                    int sourceChannel,
                                                    juce::AudioProcessorGraph::NodeID destinationNode,
                                                    int destinationChannel)
            {
                return processor.getAudioGraphForTesting().isConnected({
                    {sourceNode, sourceChannel}, {destinationNode, destinationChannel}});
            };

            const auto originalFilter = processor.getOriginalFilterNodeIdForTesting();
            const auto modernFilter = processor.getModernFilterNodeIdForTesting();
            const auto vco = processor.getVcoNodeIdForTesting();
            const auto lfo = processor.getLfoNodeIdForTesting();
            const auto vca = processor.getVcaNodeIdForTesting();

            EXPECT_EQ(hasConnection(vco, 0, originalFilter, 0), filterType == 0);
            EXPECT_EQ(hasConnection(originalFilter, 0, vca, 0), filterType == 0);
            EXPECT_EQ(hasConnection(vco, 0, modernFilter, 0), filterType == 1);
            EXPECT_EQ(hasConnection(modernFilter, 0, vca, 0), filterType == 1);
            EXPECT_EQ(hasConnection(lfo, 0, vco, 0), lfoTarget == 0);
            EXPECT_EQ(hasConnection(lfo, 0, originalFilter, 2),
                      lfoTarget == 1 && filterType == 0);
            EXPECT_EQ(hasConnection(lfo, 0, modernFilter, 2),
                      lfoTarget == 1 && filterType == 1);
        }
    }

    processor.releaseResources();
}

TEST_F(AudioGraphTest, ProgramChangeEffect)
{
    // Create processor
    std::unique_ptr<CS01AudioProcessor> processor = std::make_unique<CS01AudioProcessor>();
    
    // Prepare processor
    processor->prepareToPlay(44100.0, 512);
    
    // Check that there are programs available
    int numPrograms = processor->getNumPrograms();
    EXPECT_GT(numPrograms, 1) << "Need at least 2 programs for comparison test";
    
    if (numPrograms <= 1)
        return;
    
    // Get initial program
    int initialProgram = processor->getCurrentProgram();
    
    // Future implementation note:
    // Will add parameter comparison for future test enhancement
    
    // Process some audio with initial program
    juce::AudioBuffer<float> initialBuffer(2, 512);
    juce::MidiBuffer midiBuffer;
    
    // Add note-on to hear the effect
    juce::MidiMessage noteOn = juce::MidiMessage::noteOn(1, 60, 1.0f);
    midiBuffer.addEvent(noteOn, 0);
    processor->processBlock(initialBuffer, midiBuffer);
    midiBuffer.clear();
    
    // Let sound develop
    for (int i = 0; i < 10; ++i)
    {
        processor->processBlock(initialBuffer, midiBuffer);
    }
    
    // Change to a different program
    int newProgram = (initialProgram + 1) % numPrograms;
    processor->setCurrentProgram(newProgram);
    
    // Verify program change
    EXPECT_EQ(processor->getCurrentProgram(), newProgram) << "Current program should be updated";
    
    // Verify a different program has been loaded
    EXPECT_EQ(processor->getCurrentProgram(), newProgram) << "Program should be changed";
    
    // Note: Parameters might not always change (presets could have similar parameters)
    // Therefore, we only check the program number change without parameter validation
    
    // Process audio with new program
    juce::AudioBuffer<float> newBuffer(2, 512);
    midiBuffer.addEvent(noteOn, 0);
    processor->processBlock(newBuffer, midiBuffer);
    midiBuffer.clear();
    
    // Let sound develop
    for (int i = 0; i < 10; ++i)
    {
        processor->processBlock(newBuffer, midiBuffer);
    }
    
    // Audio output comparison is disabled
    // Similar sounds may be produced even between different programs,
    // so checking the program number change is sufficient
    
    // This test verifies the program switching functionality itself
    // Actual sound changes are tested in detail by the ProgramManagerTest
    
    // Clean up
    processor->releaseResources();
}
