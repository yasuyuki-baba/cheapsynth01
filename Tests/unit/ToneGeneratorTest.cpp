#include <gtest/gtest.h>
#include <JuceHeader.h>
#include "../../Source/CS01Synth/ToneGenerator.h"
#include "../mocks/MockToneGenerator.h"
#include "../../Source/CS01Synth/WaveformStrategies.h"

namespace {
struct SpectrumObservation {
    double mean = 0.0, rms = 0.0, fundamental = 0.0, third = 0.0;
};
template <typename Generate>
SpectrumObservation observeSpectrum(double rate, double frequency, Generate generate)
{
    double sum = 0.0, power = 0.0, sine = 0.0, cosine = 0.0;
    double sine3 = 0.0, cosine3 = 0.0;
    const int count = static_cast<int>(rate);
    for (int i = 0; i < count * 3; ++i) {
        const double phase = juce::MathConstants<double>::twoPi * frequency * i / rate;
        const double value = generate(i, phase);
        if (i < count * 2)
            continue;
        sum += value;
        power += value * value;
        sine += value * std::sin(phase);
        cosine += value * std::cos(phase);
        sine3 += value * std::sin(phase * 3.0);
        cosine3 += value * std::cos(phase * 3.0);
    }
    return {sum / count, std::sqrt(power / count),
        2.0 * std::hypot(sine, cosine) / count,
        2.0 * std::hypot(sine3, cosine3) / count};
}
}

TEST(VcoSpectrumTest, MeasurementWithKnownSine)
{
    for (double rate : {44100.0, 48000.0, 96000.0}) {
        const auto result = observeSpectrum(rate, 110.0,
            [](int, double phase) { return 0.3 + 0.5 * std::sin(phase); });
        EXPECT_NEAR(result.mean, 0.3, 1.0e-10);
        EXPECT_NEAR(result.rms, std::sqrt(0.09 + 0.125), 1.0e-10);
        EXPECT_NEAR(result.fundamental, 0.5, 1.0e-10);
        EXPECT_NEAR(result.third, 0.0, 1.0e-10);
    }
}

TEST(VcoSpectrumTest, TimeConstantsPreserveDecayPerSecond)
{
    WaveformTimeConstants constants;
    for (float rate : {44100.0f, 48000.0f, 96000.0f}) {
        constants.update(rate);
        const auto check = [&](double actual, double reference) {
            // Compare log decay per second, independently of waveform output.
            EXPECT_NEAR(std::log(actual) * rate, std::log(reference) * 44100.0, 0.006);
        };
        check(constants.triangleLeak, static_cast<double>(0.9999f));
        check(1.0 - constants.triangleDcAmount, 1.0 - static_cast<double>(0.005f));
        check(constants.sawLeak, static_cast<double>(0.998f));
        check(constants.pwmPole, static_cast<double>(0.98f));
    }
    constants.update(44100.0f);
    EXPECT_FLOAT_EQ(constants.triangleLeak, 0.9999f);
    EXPECT_FLOAT_EQ(constants.triangleDcAmount, 0.005f);
    EXPECT_FLOAT_EQ(constants.sawLeak, 0.998f);
    EXPECT_FLOAT_EQ(constants.pwmPole, 0.98f);
}

TEST(VcoSpectrumTest, WaveformStrategySampleRateCharacterization)
{
    // Isolate strategies using a controlled master; this is not a full-VCO test.
    for (double frequency : {110.0, 440.0}) {
        for (double rate : {44100.0, 48000.0, 96000.0}) {
            for (int waveform = 0; waveform < 5; ++waveform) {
                std::unique_ptr<IWaveformStrategy> strategy;
                switch (waveform) {
                    case 0: strategy = std::make_unique<TriangleWaveformStrategy>(); break;
                    case 1: strategy = std::make_unique<SawtoothWaveformStrategy>(); break;
                    case 2: strategy = std::make_unique<SquareWaveformStrategy>(); break;
                    case 3: strategy = std::make_unique<PulseWaveformStrategy>(); break;
                    default: strategy = std::make_unique<PWMWaveformStrategy>(); break;
                }
                juce::dsp::Oscillator<float> pwm;
                pwm.initialise([](float x) { return std::asin(std::sin(x))
                    * 2.0f / juce::MathConstants<float>::pi; });
                pwm.prepare({rate, 256, 1});
                pwm.setFrequency(0.0f, true);
                float previous = 0.0f;
                const float increment = static_cast<float>(frequency / rate);
                const auto result = observeSpectrum(rate, frequency, [&](int i, double) {
                    const float phase = static_cast<float>(std::fmod(i * frequency / rate, 1.0));
                    float master = phase < 0.5f ? 1.0f : -1.0f;
                    master += poly_blep(phase, increment);
                    master -= poly_blep(std::fmod(phase + 0.5f, 1.0f), increment);
                    master = std::tanh(master * 1.2f);
                    return std::tanh(strategy->generate(master, phase, increment,
                        static_cast<float>(rate), previous, pwm) * 1.2f);
                });
                ASSERT_TRUE(std::isfinite(result.rms));
                ASSERT_GT(result.fundamental, 0.0);
                EXPECT_LE(result.rms, 1.0);
                std::cout << "VCO spectrum: f=" << frequency << ", fs=" << rate
                          << ", waveform=" << waveform << ", mean=" << result.mean
                          << ", RMS=" << result.rms << ", fundamental=" << result.fundamental
                          << ", H3=" << result.third << '\n';
            }
        }
    }
}

TEST(ToneGeneratorRealTest, PitchAndFeetFromMeasuredPeriods)
{
    // Exercise the production generator, not MockToneGenerator.
    for (double sampleRate : {44100.0, 48000.0, 96000.0}) {
        for (int feet = 0; feet < 4; ++feet) {
            juce::AudioProcessorGraph host;
            juce::AudioProcessorValueTreeState::ParameterLayout layout;
            const auto add = [&](const juce::String& id, float low, float high, float value) {
                layout.add(std::make_unique<juce::AudioParameterFloat>(id, id, low, high, value));
            };
            add(ParameterIds::feet, 0.0f, 4.0f, static_cast<float>(feet));
            add(ParameterIds::waveType, 0.0f, 4.0f, 2.0f);
            add(ParameterIds::pwmSpeed, 0.0f, 60.0f, 0.0f);
            add(ParameterIds::modDepth, 0.0f, 1.0f, 0.0f);
            add(ParameterIds::pitchBend, -12.0f, 12.0f, 0.0f);
            add(ParameterIds::pitch, -12.0f, 12.0f, 0.0f);
            add(ParameterIds::pitchBendUpRange, 0.0f, 24.0f, 2.0f);
            add(ParameterIds::pitchBendDownRange, 0.0f, 24.0f, 2.0f);
            juce::AudioProcessorValueTreeState state(host, nullptr, "PARAMETERS", std::move(layout));
            ToneGenerator generator(state);
            generator.prepare({sampleRate, 256, 1});
            generator.startNote(69, 1.0f, 8192);
            generator.setPitchBend(0.0f); // Isolate tuning from MIDI wheel quantization.
            generator.updateBlockRateParameters();
            double first = 0.0, last = 0.0;
            int crossings = 0;
            float previous = generator.getNextSample();
            for (int i = 1; i < static_cast<int>(sampleRate * 2.0); ++i) {
                const float value = generator.getNextSample();
                ASSERT_TRUE(std::isfinite(value));
                if (i >= sampleRate && previous <= 0.0f && value > 0.0f) {
                    const double position = i - 1 + (-previous / static_cast<double>(value - previous));
                    if (crossings == 0)
                        first = position;
                    last = position;
                    ++crossings;
                }
                previous = value;
            }
            ASSERT_GT(crossings, 2);
            const double measured = (crossings - 1) * sampleRate / (last - first);
            const double expected = 440.0 * std::pow(2.0, feet - 2);
            SCOPED_TRACE(sampleRate);
            SCOPED_TRACE(feet);
            EXPECT_NEAR(measured, expected, expected * 0.001);
            std::cout << "VCO pitch: fs=" << sampleRate << ", feetIndex=" << feet
                      << ", measured=" << measured << ", expected=" << expected << " Hz\n";
        }
    }
}

// Helper class to manage APVTS and processor lifecycle
class APVTSHolder
{
public:
    APVTSHolder()
    {
        // Create a dummy processor
        dummyProcessor = std::make_unique<juce::AudioProcessorGraph>();
        
        // Create parameter layout
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        
        layout.add(std::make_unique<juce::AudioParameterFloat>("vco_waveform", "Waveform", 0.0f, 4.0f, 0.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>("vco_octave", "Octave", -2.0f, 2.0f, 0.0f));
        
        // Create APVTS
        apvts = std::make_unique<juce::AudioProcessorValueTreeState>(*dummyProcessor, nullptr, "Parameters", std::move(layout));
    }
    
    ~APVTSHolder() = default;
    
    juce::AudioProcessorValueTreeState& getAPVTS() { return *apvts; }
    
private:
    std::unique_ptr<juce::AudioProcessorGraph> dummyProcessor;
    std::unique_ptr<juce::AudioProcessorValueTreeState> apvts;
};

// Test fixture class for ToneGenerator tests
class ToneGeneratorTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Set up test environment
        apvtsHolder = std::make_unique<APVTSHolder>();
        toneGenerator = std::make_unique<testing::MockToneGenerator>(apvtsHolder->getAPVTS());
        
        // Prepare the tone generator
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = 44100.0;
        spec.maximumBlockSize = 512;
        spec.numChannels = 1;
        toneGenerator->prepare(spec);
    }
    
    void TearDown() override
    {
        toneGenerator.reset();
        apvtsHolder.reset();
    }
    
    std::unique_ptr<APVTSHolder> apvtsHolder;
    std::unique_ptr<testing::MockToneGenerator> toneGenerator;
};

TEST_F(ToneGeneratorTest, Initialization)
{
    // Check initial state
    EXPECT_FALSE(toneGenerator->isActive());
    EXPECT_EQ(toneGenerator->getCurrentlyPlayingNote(), 0);
    
    // Check that prepare doesn't crash
    EXPECT_TRUE(true);
}

TEST_F(ToneGeneratorTest, NoteOnOff)
{
    // Check initial state
    EXPECT_FALSE(toneGenerator->isActive());
    EXPECT_EQ(toneGenerator->getCurrentlyPlayingNote(), 0);
    
    // Start a note
    toneGenerator->startNote(60, 1.0f, 8192);
    
    // Check that note is active
    EXPECT_TRUE(toneGenerator->isActive());
    EXPECT_EQ(toneGenerator->getCurrentlyPlayingNote(), 60);
    
    // Get a sample and check it's non-zero
    float sample = toneGenerator->getNextSample();
    EXPECT_GT(std::abs(sample), 0.0f);
    
    // Stop the note without tail off
    toneGenerator->stopNote(false);
    
    // Check that note is no longer active
    EXPECT_FALSE(toneGenerator->isActive());
    EXPECT_EQ(toneGenerator->getCurrentlyPlayingNote(), 0);
    
    // Get a sample and check it's zero
    sample = toneGenerator->getNextSample();
    EXPECT_NEAR(sample, 0.0f, 0.0001f);
    
    // Start a note again
    toneGenerator->startNote(60, 1.0f, 8192);
    
    // Check that note is active
    EXPECT_TRUE(toneGenerator->isActive());
    
    // Stop the note with tail off
    toneGenerator->stopNote(true);
    
    // Check that note is still active during tail off
    EXPECT_TRUE(toneGenerator->isActive());
    
    // Get a sample and check it's non-zero (tail off)
    sample = toneGenerator->getNextSample();
    EXPECT_GT(std::abs(sample), 0.0f);
}

TEST_F(ToneGeneratorTest, PitchBend)
{
    // Start a note with center pitch bend
    toneGenerator->startNote(60, 1.0f, 8192);
    
    // Get samples with center pitch bend
    float sample1 = toneGenerator->getNextSample();
    float sample2 = toneGenerator->getNextSample();
    
    // Apply pitch bend up
    toneGenerator->pitchWheelMoved(16383); // Maximum pitch bend up
    
    // Get samples with pitch bend up
    float sample3 = toneGenerator->getNextSample();
    float sample4 = toneGenerator->getNextSample();
    
    // Check that the samples are different (frequency has changed)
    EXPECT_TRUE(std::abs(sample1 - sample3) > 0.0001f || std::abs(sample2 - sample4) > 0.0001f);
    
    // Apply center pitch bend
    toneGenerator->pitchWheelMoved(8192);
    
    // Get samples with center pitch bend
    float sample5 = toneGenerator->getNextSample();
    float sample6 = toneGenerator->getNextSample();
    
    // Apply pitch bend down
    toneGenerator->pitchWheelMoved(0); // Maximum pitch bend down
    
    // Get samples with pitch bend down
    float sample7 = toneGenerator->getNextSample();
    float sample8 = toneGenerator->getNextSample();
    
    // Check that the samples are different (frequency has changed)
    EXPECT_TRUE(std::abs(sample5 - sample7) > 0.0001f || std::abs(sample6 - sample8) > 0.0001f);
}

TEST_F(ToneGeneratorTest, WaveformGeneration)
{
    // Start a note
    toneGenerator->startNote(60, 1.0f, 8192);
    
    // Set waveform to saw
    apvtsHolder->getAPVTS().getParameter("vco_waveform")->setValueNotifyingHost(0.0f);
    
    // Get samples with saw waveform
    float sawSample1 = toneGenerator->getNextSample();
    float sawSample2 = toneGenerator->getNextSample();
    
    // Set waveform to square
    apvtsHolder->getAPVTS().getParameter("vco_waveform")->setValueNotifyingHost(0.25f);
    
    // Get samples with square waveform
    float squareSample1 = toneGenerator->getNextSample();
    float squareSample2 = toneGenerator->getNextSample();
    
    // Check that the samples are different (waveform has changed)
    EXPECT_TRUE(std::abs(sawSample1 - squareSample1) > 0.0001f || std::abs(sawSample2 - squareSample2) > 0.0001f);
    
    // Set waveform to triangle
    apvtsHolder->getAPVTS().getParameter("vco_waveform")->setValueNotifyingHost(0.5f);
    
    // Get samples with triangle waveform
    float triangleSample1 = toneGenerator->getNextSample();
    float triangleSample2 = toneGenerator->getNextSample();
    
    // Check that the samples are different (waveform has changed)
    EXPECT_TRUE(std::abs(squareSample1 - triangleSample1) > 0.0001f || std::abs(squareSample2 - triangleSample2) > 0.0001f);
}

TEST_F(ToneGeneratorTest, Glissando)
{
    // Enable glissando
    toneGenerator->setGlissando(true);
    
    // Start a note
    toneGenerator->startNote(60, 1.0f, 8192);
    
    // Get samples with initial note
    float sample1 = toneGenerator->getNextSample();
    float sample2 = toneGenerator->getNextSample();
    
    // Start a new note (should trigger glissando)
    toneGenerator->startNote(72, 1.0f, 8192);
    
    // Get samples during glissando
    float sample3 = toneGenerator->getNextSample();
    float sample4 = toneGenerator->getNextSample();
    
    // Check that the samples are different (frequency is changing)
    EXPECT_TRUE(std::abs(sample1 - sample3) > 0.0001f || std::abs(sample2 - sample4) > 0.0001f);
    
    // Disable glissando
    toneGenerator->setGlissando(false);
    
    // Start a note
    toneGenerator->startNote(60, 1.0f, 8192);
    
    // Get samples with initial note
    sample1 = toneGenerator->getNextSample();
    sample2 = toneGenerator->getNextSample();
    
    // Start a new note (should not trigger glissando)
    toneGenerator->startNote(72, 1.0f, 8192);
    
    // Get samples with new note
    sample3 = toneGenerator->getNextSample();
    sample4 = toneGenerator->getNextSample();
    
    // Check that the samples are different (frequency has changed immediately)
    EXPECT_TRUE(std::abs(sample1 - sample3) > 0.0001f || std::abs(sample2 - sample4) > 0.0001f);
}

TEST_F(ToneGeneratorTest, LfoModulation)
{
    // Start a note
    toneGenerator->startNote(60, 1.0f, 8192);
    
    // Get samples without LFO modulation
    float sample1 = toneGenerator->getNextSample();
    float sample2 = toneGenerator->getNextSample();
    
    // Apply LFO modulation
    toneGenerator->setLfoModulation(1.0f);
    
    // Get samples with LFO modulation
    float sample3 = toneGenerator->getNextSample();
    float sample4 = toneGenerator->getNextSample();
    
    // Check that the samples are different (frequency has changed)
    EXPECT_TRUE(std::abs(sample1 - sample3) > 0.0001f || std::abs(sample2 - sample4) > 0.0001f);
    
    // Remove LFO modulation
    toneGenerator->setLfoModulation(0.0f);
    
    // Get samples without LFO modulation
    float sample5 = toneGenerator->getNextSample();
    float sample6 = toneGenerator->getNextSample();
    
    // Check that the samples are different (frequency has changed back)
    EXPECT_TRUE(std::abs(sample3 - sample5) > 0.0001f || std::abs(sample4 - sample6) > 0.0001f);
}
