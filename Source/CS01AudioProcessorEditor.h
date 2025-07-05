#pragma once

#include <JuceHeader.h>
// 循環参照を避けるために前方宣言
class CS01AudioProcessor;
class IFilter;
#include "UI/ModulationComponent.h"
#include "UI/VCOComponent.h"
#include "UI/LFOComponent.h"
#include "UI/VCFComponent.h"
#include "UI/VCAComponent.h"
#include "UI/EGComponent.h"
#include "UI/BreathControlComponent.h"
#include "UI/VolumeComponent.h"
#include "UI/ProgramPanel.h"
#include "UI/FilterTypeComponent.h"
#include "UI/OscilloscopeComponent.h"

// Forward declarations
class CS01LookAndFeel;

//==============================================================================
class CS01AudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    CS01AudioProcessorEditor(CS01AudioProcessor&);
    ~CS01AudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    
    // フィルタータイプが変更されたときに呼び出される
    void filterTypeChanged(IFilter* newFilterProcessor);
    
    OscilloscopeComponent& getOscilloscope() { return oscilloscopeComponent; }
    juce::AudioVisualiserComponent& getAudioVisualiser() { return audioVisualiser; }

private:
    CS01AudioProcessor& audioProcessor;

    juce::MidiKeyboardComponent midiKeyboard;
    
    std::unique_ptr<ModulationComponent> modulationComponent;
    std::unique_ptr<VCOComponent> vcoComponent;
    std::unique_ptr<LFOComponent> lfoComponent;
    std::unique_ptr<VCFComponent> vcfComponent;
    std::unique_ptr<VCAComponent> vcaComponent;
    std::unique_ptr<EGComponent> egComponent;
    std::unique_ptr<BreathControlComponent> breathControlComponent;
    std::unique_ptr<VolumeComponent> volumeComponent;
    std::unique_ptr<ProgramPanel> programPanel;
    std::unique_ptr<FilterTypeComponent> filterTypeComponent;
    std::unique_ptr<CS01LookAndFeel> lookAndFeel;
    OscilloscopeComponent oscilloscopeComponent;
    juce::AudioVisualiserComponent audioVisualiser;

    juce::FlexBox upperFlex;
    juce::FlexBox lowerFlex;
    juce::FlexBox mainFlex;
    juce::FlexBox visualizerFlex; // FlexBox for waveform display

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CS01AudioProcessorEditor)
};
