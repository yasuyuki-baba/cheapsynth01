#include <JuceHeader.h>
#include "CS01AudioProcessor.h"
#include "CS01Synth/EGProcessor.h"
#include "CS01Synth/VCOProcessor.h"
#include "CS01Synth/OriginalVCFProcessor.h"
#include "CS01Synth/VCAProcessor.h"
#include <gtest/gtest.h>
#include <cstdlib>
#include <fstream>

TEST(NoteOnsetTest, NoteLifecycleKeepsUpstreamWaveformContinuous) {
    for (double rate : {44100.0, 48000.0, 96000.0}) {
        for (int wave : {0, 1, 2, 3, 4}) {
            CS01AudioProcessor owner;
            auto& state = owner.getValueTreeState();
            const auto set = [&](const juce::String& id, float value) {
                auto* p = state.getParameter(id);
                p->setValueNotifyingHost(p->convertTo0to1(value));
            };
            set(ParameterIds::waveType, static_cast<float>(wave));
            set(ParameterIds::feet, 2);
            set(ParameterIds::release, 0.005f);
            set(ParameterIds::modDepth, 0);
            VCOProcessor played(state), held(state);
            for (auto* vco : {&played, &held}) {
                vco->setExternalOversampling(true);
                vco->setFreeRunning(true);
                vco->prepareToPlay(rate * 4, 64);
                vco->getSoundGenerator()->startNote(45, 1, 8192);
            }
            juce::AudioBuffer<float> a(1, 64), b(1, 64);
            juce::MidiBuffer midi;
            double idleEnergy = 0;
            for (int block = 0; block < 200; ++block) {
                if (block == 10)
                    played.getSoundGenerator()->stopNote(true);
                if (block == 100) {
                    ASSERT_FALSE(played.getSoundGenerator()->isActive());
                    played.getSoundGenerator()->startNote(45, 1, 8192);
                }
                a.clear();
                b.clear();
                played.processBlock(a, midi);
                held.processBlock(b, midi);
                for (int i = 0; i < 64; ++i) {
                    ASSERT_FLOAT_EQ(a.getSample(0, i), b.getSample(0, i))
                        << "rate=" << rate << ", wave=" << wave << ", block=" << block;
                    if (block >= 50 && block < 100)
                        idleEnergy += a.getSample(0, i) * a.getSample(0, i);
                }
            }
            EXPECT_GT(idleEnergy, 1);
        }
    }
}

TEST(NoteOnsetTest, NoiseContinuesPastReleaseDeadlineInsideBlock) {
    for (double rate : {44100.0, 48000.0, 96000.0}) {
        CS01AudioProcessor owner;
        auto& state = owner.getValueTreeState();
        for (const auto& setting : std::array<std::pair<juce::String, float>, 2>{
                 {{ParameterIds::feet, 4.0f}, {ParameterIds::release, 0.001f}}}) {
            auto* p = state.getParameter(setting.first);
            p->setValueNotifyingHost(p->convertTo0to1(setting.second));
        }
        VCOProcessor vco(state);
        vco.setFreeRunning(true);
        vco.prepareToPlay(rate * 4, 512);
        vco.getSoundGenerator()->startNote(45, 1, 8192);
        vco.getSoundGenerator()->stopNote(true);
        juce::AudioBuffer<float> audio(1, 512);
        juce::MidiBuffer midi;
        audio.clear();
        vco.processBlock(audio, midi);
        EXPECT_FALSE(vco.getSoundGenerator()->isActive());
        EXPECT_GT(audio.getRMSLevel(0, 384, 128), 1.0e-4f);
        audio.clear();
        vco.processBlock(audio, midi);
        EXPECT_GT(audio.getRMSLevel(0, 0, 512), 1.0e-4f);
    }
}

TEST(NoteOnsetTest, VcaMutesFreeRunningSourcesAtEveryEnvelopeDepth) {
    for (int filter : {0, 1}) {
        for (int feet : {2, 4}) {
            for (float depth : {0.0f, 0.4f, 1.0f}) {
                CS01AudioProcessor processor;
                const auto set = [&](const juce::String& id, float value) {
                    auto* p = processor.getValueTreeState().getParameter(id);
                    p->setValueNotifyingHost(p->convertTo0to1(value));
                };
                set(ParameterIds::filterType, static_cast<float>(filter));
                set(ParameterIds::feet, static_cast<float>(feet));
                set(ParameterIds::waveType, 3);
                set(ParameterIds::attack, 0.001f);
                set(ParameterIds::decay, 0.001f);
                set(ParameterIds::sustain, 0.6f);
                set(ParameterIds::release, 0.005f);
                set(ParameterIds::vcaEgDepth, depth);
                set(ParameterIds::volume, 0.7f);
                set(ParameterIds::breathVca, 0);
                processor.prepareToPlay(48000, 64);
                processor.flushPendingGraphChangesForTesting();
                juce::AudioBuffer<float> audio(2, 64);
                juce::MidiBuffer midi;
                for (int block = 0; block < 20; ++block) {
                    processor.processBlock(audio, midi);
                    ASSERT_EQ(audio.getMagnitude(0, 64), 0);
                }
                for (int repetition = 0; repetition < 2; ++repetition) {
                    midi.addEvent(juce::MidiMessage::noteOn(1, 45, 1.0f), 17);
                    double energy = 0;
                    for (int block = 0; block < 30; ++block) {
                        processor.processBlock(audio, midi);
                        for (int i = 0; i < 64; ++i) {
                            ASSERT_TRUE(std::isfinite(audio.getSample(0, i)));
                            energy += std::pow(audio.getSample(0, i), 2);
                        }
                    }
                    ASSERT_GT(energy, 1.0e-5);
                    midi.addEvent(juce::MidiMessage::noteOff(1, 45), 13);
                    for (int block = 0; block < 400; ++block)
                        processor.processBlock(audio, midi);
                    EXPECT_LT(audio.getMagnitude(0, 64), 1.0e-6);
                    midi.addEvent(juce::MidiMessage::allSoundOff(1), 0);
                    processor.processBlock(audio, midi);
                    EXPECT_EQ(audio.getMagnitude(0, 64), 0);
                }
                processor.releaseResources();
            }
        }
    }
}

TEST(NoteOnsetTest, NoteGateIsSampleAccurateAcrossRetriggerAndReleaseEdits) {
    for (double rate : {44100.0, 48000.0, 96000.0}) {
        const auto render = [&](int blockSize) {
            CS01AudioProcessor owner;
            auto& state = owner.getValueTreeState();
            const auto setRelease = [&](float seconds) {
                auto* p = state.getParameter(ParameterIds::release);
                p->setValueNotifyingHost(p->convertTo0to1(seconds));
            };
            setRelease(0.01f);
            EGProcessor eg(state);
            eg.prepareToPlay(rate, blockSize);
            std::vector<float> values;
            const std::array events{0, 100, 200, 300, 400, 1500};
            for (size_t event = 0; event + 1 < events.size(); ++event) {
                if (event == 0 || event == 3)
                    eg.startEnvelope();
                if (event == 1 || event == 4)
                    eg.releaseEnvelope();
                if (event == 2)
                    setRelease(0.005f);
                for (int position = events[event]; position < events[event + 1];) {
                    const int count = std::min(blockSize, events[event + 1] - position);
                    juce::AudioBuffer<float> audio(1, count);
                    juce::MidiBuffer midi;
                    eg.processBlock(audio, midi);
                    for (int i = 0; i < count; ++i)
                        values.push_back(eg.getNoteGateForSample(i));
                    position += count;
                }
            }
            return values;
        };
        const auto expected = render(1);
        for (int block : {7, 64})
            EXPECT_EQ(render(block), expected);
        EXPECT_GT(expected[99], 0.99f);
        EXPECT_GT(expected[399], expected[300]);
        EXPECT_EQ(expected.back(), 0);
        for (float value : expected)
            ASSERT_GE(value, 0);
        for (float value : expected)
            ASSERT_LE(value, 1);
    }
}
TEST(NoteOnsetTest, Observation_RenderRepeatedNotes) {
    const char* destination = std::getenv("CHEAPSYNTH_ONSET_OUTPUT");
    if (destination == nullptr)
        GTEST_SKIP() << "Set CHEAPSYNTH_ONSET_OUTPUT to save comparison WAVs";
    const juce::File directory(destination);
    ASSERT_TRUE(directory.createDirectory().wasOk());
    for (int wave : {0, 1, 2, 3, 4}) {
        CS01AudioProcessor processor;
        auto& state = processor.getValueTreeState();
        const auto set = [&](const juce::String& id, float value) {
            auto* p = state.getParameter(id);
            p->setValueNotifyingHost(p->convertTo0to1(value));
        };
        set(ParameterIds::waveType, static_cast<float>(wave));
        set(ParameterIds::feet, 2);
        set(ParameterIds::cutoff, 962);
        set(ParameterIds::resonance, 1);
        set(ParameterIds::attack, 0.001f);
        set(ParameterIds::decay, 0.037f);
        set(ParameterIds::sustain, 0);
        set(ParameterIds::release, 0.199f);
        set(ParameterIds::vcaEgDepth, 1);
        set(ParameterIds::vcfEgDepth, 0.25f);
        set(ParameterIds::volume, 0.7f);
        set(ParameterIds::breathVca, 0);
        set(ParameterIds::breathVcf, 0);
        set(ParameterIds::modDepth, 0);
        processor.prepareToPlay(48000, 64);
        processor.flushPendingGraphChangesForTesting();
        juce::AudioBuffer<float> audio(2, 64), recording(1, 96000);
        for (int position = 0; position < 96000; position += 64) {
            juce::MidiBuffer midi;
            for (int note = 0; note < 4; ++note) {
                for (bool on : {true, false}) {
                    const int time = 9600 + note * 21600 + (on ? 0 : 8640);
                    if (time >= position && time < position + 64)
                        midi.addEvent(on ? juce::MidiMessage::noteOn(1, 45, 1.0f)
                                         : juce::MidiMessage::noteOff(1, 45),
                                      time - position);
                }
            }
            processor.processBlock(audio, midi);
            recording.copyFrom(0, position, audio, 0, 0, 64);
        }
        const auto file = directory.getChildFile("wave-" + juce::String(wave) + ".wav");
        std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
        ASSERT_NE(stream, nullptr);
        juce::WavAudioFormat format;
        auto writer = format.createWriterFor(stream, juce::AudioFormatWriterOptions{}
                                                         .withSampleRate(48000)
                                                         .withNumChannels(1)
                                                         .withBitsPerSample(24));
        ASSERT_NE(writer, nullptr);
        EXPECT_TRUE(writer->writeFromAudioSampleBuffer(recording, 0, recording.getNumSamples()));
    }
}

TEST(NoteOnsetTest, Observation_StageTransients) {
    const char* destination = std::getenv("CHEAPSYNTH_ONSET_OUTPUT");
    if (destination == nullptr)
        GTEST_SKIP() << "Set CHEAPSYNTH_ONSET_OUTPUT to save stage CSVs";
    const juce::File directory(destination);
    ASSERT_TRUE(directory.createDirectory().wasOk());
    for (int wave : {0, 1, 2, 3, 4}) {
        CS01AudioProcessor owner;
        auto& state = owner.getValueTreeState();
        const auto set = [&](const juce::String& id, float value) {
            auto* p = state.getParameter(id);
            p->setValueNotifyingHost(p->convertTo0to1(value));
        };
        set(ParameterIds::waveType, static_cast<float>(wave));
        set(ParameterIds::feet, 2);
        set(ParameterIds::cutoff, 962);
        set(ParameterIds::resonance, 1);
        set(ParameterIds::attack, 0.001f);
        set(ParameterIds::decay, 0.037f);
        set(ParameterIds::sustain, 0);
        set(ParameterIds::release, 0.199f);
        set(ParameterIds::vcaEgDepth, 1);
        set(ParameterIds::vcfEgDepth, 0.25f);
        set(ParameterIds::volume, 0.7f);
        set(ParameterIds::breathVca, 0);
        set(ParameterIds::breathVcf, 0);
        set(ParameterIds::modDepth, 0);
        VCOProcessor vco(state);
        vco.setExternalOversampling(true);
        vco.setFreeRunning(true);
        EGProcessor eg(state);
        OriginalVCFProcessor vcf(state);
        VCAProcessor vca(state);
        vca.setNoteGateSource(&eg);
        for (auto* p : std::array<juce::AudioProcessor*, 4>{&vco, &eg, &vcf, &vca})
            p->prepareToPlay(192000, 1);
        juce::AudioBuffer<float> oscillator(1, 1), envelope(1, 1), filter(3, 1), amp(2, 1);
        juce::MidiBuffer midi;
        std::ofstream csv(directory.getChildFile("stage-wave-" + juce::String(wave) + ".csv")
                              .getFullPathName()
                              .toStdString());
        ASSERT_TRUE(csv.is_open());
        csv << "time,vco,eg,vcf,vca\n";
        for (int i = 0; i < 192000; ++i) {
            if (i == 38400 || i == 134400) {
                vco.getSoundGenerator()->startNote(45, 1, 8192);
                eg.startEnvelope();
            }
            if (i == 76800 || i == 172800) {
                vco.getSoundGenerator()->stopNote(true);
                eg.releaseEnvelope();
            }
            oscillator.clear();
            vco.processBlock(oscillator, midi);
            eg.processBlock(envelope, midi);
            filter.setSample(0, 0, oscillator.getSample(0, 0));
            filter.setSample(1, 0, envelope.getSample(0, 0));
            filter.setSample(2, 0, 0);
            vcf.processBlock(filter, midi);
            amp.setSample(0, 0, filter.getSample(0, 0));
            amp.setSample(1, 0, envelope.getSample(0, 0));
            vca.processBlock(amp, midi);
            ASSERT_TRUE(std::isfinite(amp.getSample(0, 0)));
            if (i % 4 == 0)
                csv << i / 192000.0 << ',' << oscillator.getSample(0, 0) << ','
                    << envelope.getSample(0, 0) << ',' << filter.getSample(0, 0) << ','
                    << amp.getSample(0, 0) << '\n';
        }
    }
}
