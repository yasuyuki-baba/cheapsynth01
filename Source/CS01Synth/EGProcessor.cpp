#include "EGProcessor.h"

//==============================================================================
EGProcessor::EGProcessor(juce::AudioProcessorValueTreeState& apvts)
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::mono(), true)),
      apvts(apvts)
{
}

EGProcessor::~EGProcessor()
{
}

//==============================================================================
void EGProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    adsr.setSampleRate(sampleRate);
    updateADSR();
}

void EGProcessor::releaseResources()
{
}

bool EGProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled())
        return false;

    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono())
        return false;

    return true;
}

void EGProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    updateADSR();

    // CS01はモノラルシンセサイザーなので、モノラル出力のみを生成
    buffer.clear();

    // MIDIメッセージ処理はスキップ (MidiProcessorですでに処理済み)
    // MIDIメッセージはtriggerNoteOn/triggerNoteOffメソッドで処理される

    // モノラル出力（チャンネル0）のみを処理
    auto* channelData = buffer.getWritePointer(0);
    
    // エンベロープ整形用の静的変数
    static float prevSample = 0.0f;
    
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        // 生のエンベロープサンプルを取得
        float envSample = adsr.getNextSample();
        
        // FET非線形特性の適用 (回路中のFET1)
        // 1. 低レベルでの軽い圧縮 (FETのしきい値効果)
        if (envSample < 0.1f)
            envSample = envSample * 0.7f + 0.03f * std::sqrt(envSample);
            
        // 2. 中レベルでの軽い拡張 (FETの二乗法則領域)
        else if (envSample < 0.7f)
            envSample = envSample * (1.0f + (envSample - 0.1f) * 0.15f);
            
        // 3. 高レベルでのソフト飽和 (FET飽和領域)
        else
            envSample = 0.7f + (1.0f - 0.7f) * std::tanh((envSample - 0.7f) / (1.0f - 0.7f) * 2.0f);
        
        // 4. トランジスタバッファ効果の適用 (Tr14)
        // - カップリングによる軽いハイパス特性
        // - 高速なトランジェントのための小さな時定数
        const float alpha = 0.99f; // 時定数
        
        // シンプルな一次ハイパスフィルタ
        float highPassComponent = (envSample - prevSample) * (1.0f - alpha);
        prevSample = envSample * alpha + prevSample * (1.0f - alpha);
        
        // トランジェント強調のための少量のハイパス成分追加
        envSample = envSample * 0.95f + highPassComponent * 2.0f;
        
        // 出力が有効範囲内に収まることを保証
        envSample = juce::jlimit(0.0f, 1.0f, envSample);
        
        channelData[sample] = envSample;
    }
}

void EGProcessor::updateADSR()
{
    juce::ADSR::Parameters adsrParams;
    adsrParams.attack  = apvts.getRawParameterValue(ParameterIds::attack)->load();
    adsrParams.decay   = apvts.getRawParameterValue(ParameterIds::decay)->load();
    adsrParams.sustain = apvts.getRawParameterValue(ParameterIds::sustain)->load();
    adsrParams.release = apvts.getRawParameterValue(ParameterIds::release)->load();
    
    adsr.setParameters(adsrParams);
}
