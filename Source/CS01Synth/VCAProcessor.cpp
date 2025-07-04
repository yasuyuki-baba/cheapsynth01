#include "VCAProcessor.h"

//==============================================================================
VCAProcessor::VCAProcessor(juce::AudioProcessorValueTreeState& apvts)
    : AudioProcessor (BusesProperties()
                          .withInput  ("AudioInput",  juce::AudioChannelSet::mono(), true)
                          .withInput  ("EGInput", juce::AudioChannelSet::mono(), true)
                          .withOutput ("Output", juce::AudioChannelSet::mono(), true)),
      apvts(apvts)
{
}

VCAProcessor::~VCAProcessor()
{
}

//==============================================================================
void VCAProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Initialize input stage high-pass filter (82K resistor and 1/50 capacitor ~40Hz)
    inputHighPass.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 40.0f);
    inputHighPass.reset();
    inputHighPass.prepare({sampleRate, static_cast<uint32>(samplesPerBlock), 1});
    
    // Initialize DC blocker
    dcBlocker.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 20.0f);
    dcBlocker.reset();
    dcBlocker.prepare({sampleRate, static_cast<uint32>(samplesPerBlock), 1});
    
    // Initialize high frequency rolloff filter
    float cutoffFreq = std::min(15000.0f, static_cast<float>(sampleRate * 0.45f));
    highFreqRolloff.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, cutoffFreq);
    highFreqRolloff.reset();
    highFreqRolloff.prepare({sampleRate, static_cast<uint32>(samplesPerBlock), 1});
    
    // Reset state variables
    capacitorState = 0.0f;
    prevOutput = 0.0f;
    outCapacitorState = 0.0f;
}

void VCAProcessor::releaseResources()
{
    inputHighPass.reset();
    dcBlocker.reset();
    highFreqRolloff.reset();
}

bool VCAProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainIn = layouts.getChannelSet(true, 0);
    const auto& egIn   = layouts.getChannelSet(true, 1);
    const auto& mainOut = layouts.getChannelSet(false, 0);

    if (mainIn != juce::AudioChannelSet::mono()) return false;
    if (egIn != juce::AudioChannelSet::mono()) return false;
    if (mainOut != juce::AudioChannelSet::mono()) return false;

    return true;
}

void VCAProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // CS01はモノラルシンセサイザーなので、モノラルバッファ（チャンネル0）のみを処理
    auto audioInput = getBusBuffer(buffer, true, 0);
    auto egInput = getBusBuffer(buffer, true, 1);

    // パラメータの取得
    auto egDepth = apvts.getRawParameterValue(ParameterIds::vcaEgDepth)->load();
    auto breathInput = apvts.getRawParameterValue(ParameterIds::breathInput)->load();
    auto breathVcaDepth = apvts.getRawParameterValue(ParameterIds::breathVca)->load();
    auto volume = apvts.getRawParameterValue(ParameterIds::volume)->load();

    // モノラルバッファのデータポインタを取得
    const auto* audioData = audioInput.getReadPointer(0);
    const auto* egData = egInput.getReadPointer(0);
    auto* outputData = buffer.getWritePointer(0);

    // サンプルごとに処理
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        // TP3: 入力サンプルを取得
        float inputSample = audioData[sample];
        
        // 入力段のハイパスフィルタ適用（82Kレジスタと1/50コンデンサ）
        inputSample = inputHighPass.processSample(inputSample);
        
        // DCブロッキング適用（追加の安全策）
        inputSample = dcBlocker.processSample(inputSample);
        
        // EG値を取得
        float egValue = egData[sample];
        
        // VCA用の制御電圧を計算
        float controlVoltage = (1.0f - egDepth) + (egValue * egDepth);
        controlVoltage *= (1.0f - breathVcaDepth) + (breathInput * breathVcaDepth);
        
        // IG02600 VCAチップエミュレーションで処理
        float outputSample = processVCA(inputSample, controlVoltage, volume);
        
        // Tr7トランジスタバッファエミュレーションで処理
        outputSample = processTr7Buffer(outputSample);
        
        // 出力カップリングコンデンサ（4.7/25）で処理
        outputSample = processOutputCoupling(outputSample);
        
        // 高周波ロールオフを適用（リアリズム向上のための追加フィルタリング）
        outputSample = highFreqRolloff.processSample(outputSample);
        
        // TP5: 最終出力
        outputData[sample] = outputSample;
    }
}

// IG02600 VCA chip emulation
float VCAProcessor::processVCA(float input, float controlVoltage, float volumeParam)
{
    // Apply logarithmic volume control characteristic (PVR5)
    float volume = std::pow(volumeParam, 2.5f); // Approximate log curve
    
    // Calculate gain based on control voltage and volume
    float gain = controlVoltage * volume;
    
    // Apply gain
    float output = input * gain;
    
    // Emulate IG02600 VCA non-linear response
    if (std::abs(output) > 0.7f) {
        // Soft saturation for higher levels
        float sign = (output > 0.0f) ? 1.0f : -1.0f;
        float excess = std::abs(output) - 0.7f;
        output = sign * (0.7f + excess / (1.0f + excess * 0.5f));
    }
    
    return output;
}

// Tr7 transistor buffer emulation
float VCAProcessor::processTr7Buffer(float input)
{
    // Output coupling capacitor (1/50) - high-pass characteristic
    const float rc1 = 0.997f; // Time constant based on component values
    capacitorState = capacitorState * rc1 + input * (1.0f - rc1);
    float hpOutput = input - capacitorState;
    
    // Tr7 transistor non-linear characteristic
    float transistorOutput;
    if (hpOutput > 0) {
        // NPN transistor is more linear for positive signals
        transistorOutput = hpOutput * 0.95f;
    } else {
        // Slight asymmetry for negative signals
        transistorOutput = hpOutput * 0.92f;
    }
    
    // Transistor high-frequency response (slight treble boost)
    const float rc2 = 0.998f;
    float highFreqComponent = (transistorOutput - prevOutput) * (1.0f - rc2) * 2.0f;
    prevOutput = transistorOutput;
    
    return transistorOutput + highFreqComponent;
}

// Output coupling capacitor emulation (4.7/25)
float VCAProcessor::processOutputCoupling(float input)
{
    // Output coupling capacitor - high-pass characteristic (~7Hz)
    const float rc3 = 0.9995f; // Time constant based on component values
    outCapacitorState = outCapacitorState * rc3 + input * (1.0f - rc3);
    
    return input - outCapacitorState;
}
