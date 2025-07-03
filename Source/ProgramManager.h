#pragma once

#include <JuceHeader.h>
#include "Parameters.h"

//==============================================================================
struct Program
{
    juce::String name;
    juce::String filename;
};

class ProgramManager
{
public:
    ProgramManager(juce::AudioProcessorValueTreeState& apvts);
    ~ProgramManager();
    
    // プリセット操作メソッド
    void loadFactoryPreset(int index);
    void loadPresetFromXml(const juce::XmlElement* xml);
    void saveCurrentStateAsPreset(const juce::String& name);
    
    // プログラム（プリセット）管理
    int getNumPrograms() const;
    int getCurrentProgram() const;
    void setCurrentProgram(int index);
    juce::String getProgramName(int index) const;
    
    // 状態の保存と復元
    void getStateInformation(juce::MemoryBlock& destData);
    void setStateInformation(const void* data, int sizeInBytes);
    
private:
    juce::AudioProcessorValueTreeState& apvts;
    std::vector<Program> factoryPresets;
    int currentProgram = 0;
    
    // 特定パラメータをステート保存から除外
    const std::vector<juce::String> stateExcludedParameters = {
        ParameterIds::breathInput,
        ParameterIds::volume,
        ParameterIds::modDepth,
        ParameterIds::pitchBend
    };
    
    bool isStateExcludedParameter(const juce::String& paramId) const;
    
    void initializePresets();
    void loadPresetFromBinaryData(const juce::String& filename);
};
