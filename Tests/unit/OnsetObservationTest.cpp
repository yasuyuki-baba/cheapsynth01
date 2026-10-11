#include "SynthEngine.h"
#include <gtest/gtest.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <vector>

namespace {
using cs01::Param;
void configure(cs01::ParameterState& state, int wave) {
    state.set(Param::WaveType, wave);
    state.set(Param::Feet, 2);
    state.set(Param::Cutoff, 962);
    state.set(Param::Resonance, 1);
    state.set(Param::Attack, .001f);
    state.set(Param::Decay, .037f);
    state.set(Param::Sustain, 0);
    state.set(Param::Release, .199f);
    state.set(Param::VcaEgDepth, 1);
    state.set(Param::VcfEgDepth, .25f);
    state.set(Param::Volume, .7f);
    state.set(Param::BreathVca, 0);
    state.set(Param::BreathVcf, 0);
    state.set(Param::ModDepth, 0);
}
void integer(std::ostream& out, uint32_t value, int bytes) {
    for (int i = 0; i < bytes; ++i)
        out.put(static_cast<char>(value >> (8 * i)));
}
// Framework-independent mono PCM24 export, preserving the original probe format.
void wave(std::ostream& out, const std::vector<float>& values) {
    uint32_t bytes = static_cast<uint32_t>(values.size() * 3);
    out.write("RIFF", 4);
    integer(out, bytes + 36, 4);
    out.write("WAVEfmt ", 8);
    integer(out, 16, 4);
    integer(out, 1, 2);
    integer(out, 1, 2);
    integer(out, 48000, 4);
    integer(out, 48000 * 3, 4);
    integer(out, 3, 2);
    integer(out, 24, 2);
    out.write("data", 4);
    integer(out, bytes, 4);
    for (float sample : values) {
        int32_t pcm = static_cast<int32_t>(
            std::lround(std::clamp(static_cast<double>(sample), -1., 1.) * 8388607));
        integer(out, static_cast<uint32_t>(pcm), 3);
    }
}
std::filesystem::path destination() {
    const char* path = std::getenv("CHEAPSYNTH_ONSET_OUTPUT");
    if (!path || !*path)
        return {};
    std::filesystem::path output(path);
    std::filesystem::create_directories(output);
    return output;
}
}  // namespace
TEST(OnsetObservationTest, Observation_RepeatedNotesAndOptionalPcm24Export) {
    auto directory = destination();
    for (int waveform = 0; waveform < 5; ++waveform) {
        cs01::ParameterState state;
        configure(state, waveform);
        SynthEngine engine(state);
        engine.prepare(48000);
        std::vector<float> recording(96000);
        double energy = 0;
        for (int i = 0; i < 96000; ++i) {
            for (int note = 0; note < 4; ++note) {
                if (i == 9600 + note * 21600)
                    engine.handleMidi({0x90, 45, 127});
                if (i == 9600 + note * 21600 + 8640)
                    engine.handleMidi({0x80, 45, 0});
            }
            recording[i] = engine.renderSample();
            ASSERT_TRUE(std::isfinite(recording[i]));
            energy += recording[i] * recording[i];
        }
        ASSERT_GT(energy, 0);
        if (!directory.empty()) {
            auto path = directory / ("iplug2-wave-" + std::to_string(waveform) + ".wav");
            std::ofstream output(path, std::ios::binary);
            ASSERT_TRUE(output.is_open());
            wave(output, recording);
            output.close();
            ASSERT_TRUE(output.good());
            EXPECT_EQ(std::filesystem::file_size(path), 44 + recording.size() * 3);
        }
    }
}
TEST(OnsetObservationTest, Observation_StageTransientsAndOptionalCsvExport) {
    auto directory = destination();
    for (int waveform = 0; waveform < 5; ++waveform) {
        cs01::ParameterState state;
        configure(state, waveform);
        VCOProcessor vco(state);
        EGProcessor eg(state);
        OriginalVCFProcessor filter(state);
        VCAProcessor amp(state);
        vco.prepareToPlay(192000, 1);
        eg.prepareToPlay(192000, 1);
        filter.prepareToPlay(192000, 1);
        amp.prepareToPlay(192000, 1);
        std::ofstream csv;
        if (!directory.empty()) {
            csv.open(directory / ("iplug2-stage-wave-" + std::to_string(waveform) + ".csv"));
            ASSERT_TRUE(csv.is_open());
            csv << "time,vco,eg,vcf,vca\n" << std::setprecision(9);
        }
        for (int i = 0; i < 192000; ++i) {
            if (i == 38400 || i == 134400) {
                vco.getSoundGenerator()->startNote(45, 1, 8192);
                eg.startEnvelope();
            }
            if (i == 76800 || i == 172800) {
                vco.getSoundGenerator()->stopNote(true);
                eg.releaseEnvelope();
            }
            vco.getSoundGenerator()->setReleaseSamplesRemaining(
                static_cast<int>(eg.getReleaseSamplesRemaining()));
            float oscillator = vco.processSample(0), envelope = eg.processSample();
            float filtered = filter.processSample(oscillator, envelope, 0),
                  amplified = amp.processSample(filtered, envelope, eg.getNoteGate());
            ASSERT_TRUE(std::isfinite(amplified));
            if (csv.is_open() && i % 4 == 0)
                csv << i / 192000. << ',' << oscillator << ',' << envelope << ',' << filtered << ','
                    << amplified << '\n';
        }
        if (csv.is_open()) {
            csv.close();
            EXPECT_TRUE(csv.good());
        }
    }
}
