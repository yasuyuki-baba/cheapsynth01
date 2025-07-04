#include "CS01VCFProcessor.h"

//==============================================================================
CS01VCFProcessor::CS01VCFProcessor(juce::AudioProcessorValueTreeState& apvts)
    : AudioProcessor (BusesProperties()
                          .withInput  ("AudioInput",  juce::AudioChannelSet::mono(), true)
                          .withInput  ("EGInput", juce::AudioChannelSet::mono(), true)
                          .withInput  ("LFOInput", juce::AudioChannelSet::mono(), true)
                          .withOutput ("Output", juce::AudioChannelSet::mono(), true)),
      apvts(apvts)
{
}

CS01VCFProcessor::~CS01VCFProcessor()
{
}

//==============================================================================
void CS01VCFProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Initialize filter
    filter.reset();
    filter.prepare(sampleRate);
    
    // Pre-allocate buffer for modulation values
    modulationBuffer.allocate(samplesPerBlock, true);
}

void CS01VCFProcessor::releaseResources()
{
    // Reset filter
    filter.reset();
}

bool CS01VCFProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainIn = layouts.getChannelSet(true, 0);
    const auto& egIn   = layouts.getChannelSet(true, 1);
    const auto& lfoIn  = layouts.getChannelSet(true, 2);
    const auto& mainOut = layouts.getChannelSet(false, 0);

    if (mainIn != juce::AudioChannelSet::mono()) return false;
    if (egIn != juce::AudioChannelSet::mono()) return false;
    if (lfoIn != juce::AudioChannelSet::mono()) return false;
    if (mainOut != juce::AudioChannelSet::mono()) return false;

    return true;
}

void CS01VCFProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // CS01は完全にモノラルなので、チャンネル0のみで処理
    auto audioInput = getBusBuffer(buffer, true, 0);
    auto egInput = getBusBuffer(buffer, true, 1);
    auto lfoInput = getBusBuffer(buffer, true, 2);

    // Get parameters
    auto cutoffParam = apvts.getRawParameterValue(ParameterIds::cutoff)->load();
    auto resonanceParam = apvts.getRawParameterValue(ParameterIds::resonance)->load();
    auto egDepth = apvts.getRawParameterValue(ParameterIds::vcfEgDepth)->load();
    auto modDepth = apvts.getRawParameterValue(ParameterIds::modDepth)->load();
    auto breathInput = apvts.getRawParameterValue(ParameterIds::breathInput)->load();
    auto breathVcfDepth = apvts.getRawParameterValue(ParameterIds::breathVcf)->load();

    // Cutoff frequency calculation
    float cutoff = calculateCutoffFrequency(cutoffParam);
    
    // Ensure minimum cutoff frequency
    cutoff = juce::jmax(20.0f, cutoff);
    
    // Resonance calculation
    float resonance = calculateResonance(resonanceParam);

    // モノラル処理のためのポインタ取得
    const auto* egData = egInput.getReadPointer(0);
    const auto* lfoData = lfoInput.getNumSamples() > 0 ? lfoInput.getReadPointer(0) : nullptr;
    const auto* audioData = audioInput.getReadPointer(0);
    auto* outputData = buffer.getWritePointer(0);
    
    // モジュレーションバッファのリサイズ
    if (buffer.getNumSamples() > 0) {
        modulationBuffer.realloc(buffer.getNumSamples());
        modulationBuffer.clear(buffer.getNumSamples());
    }
    
    // サンプル毎にカットオフ周波数の計算
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        float egValue = egData[sample];
        float lfoValue = (lfoData != nullptr) ? lfoData[sample] : 0.0f;

        // 変調効果の計算定数
        const float egModRangeSemitones = 36.0f; // 3 octaves
        const float lfoModRangeSemitones = 24.0f; // 2 octaves
        const float breathModRangeSemitones = 24.0f; // 2 octaves

        // ベースカットオフ周波数
        float baseCutoff = cutoff;
        
        // EG変調
        float egMod = egValue * egDepth * egModRangeSemitones;
        float egModFreqRatio = std::pow(2.0f, egMod / 12.0f);
        
        // LFO変調
        float lfoMod = lfoValue * modDepth * lfoModRangeSemitones;
        float lfoModFreqRatio = std::pow(2.0f, lfoMod / 12.0f);
        
        // ブレス変調
        float breathMod = breathInput * breathVcfDepth * breathModRangeSemitones;
        float breathModFreqRatio = std::pow(2.0f, breathMod / 12.0f);
        
        // すべての変調を適用
        float modulatedCutoffHz = baseCutoff * egModFreqRatio * lfoModFreqRatio * breathModFreqRatio;
        
        // NaNや無限大のチェック
        if (std::isnan(modulatedCutoffHz) || std::isinf(modulatedCutoffHz)) {
            modulatedCutoffHz = baseCutoff;
        }
        
        modulatedCutoffHz = juce::jlimit(20.0f, 20000.0f, modulatedCutoffHz);
        
        // このサンプルの変調されたカットオフを保存
        modulationBuffer[sample] = modulatedCutoffHz;
    }
    
    // オーディオ入力を出力バッファにコピー
    buffer.copyFrom(0, 0, audioData, buffer.getNumSamples());
    
    // フィルタを使用して処理
    filter.processBlock(outputData, buffer.getNumSamples(), modulationBuffer, resonance);
}
