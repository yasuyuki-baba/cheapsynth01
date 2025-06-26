#pragma once

#include <JuceHeader.h>
#include "CS01AudioProcessor.h"
#include "UI/CS01ModulationComponent.h"
#include "UI/CS01VCOComponent.h"
#include "UI/CS01LFOComponent.h"
#include "UI/CS01VCFComponent.h"
#include "UI/CS01VCAComponent.h"
#include "UI/CS01EGComponent.h"
#include "UI/CS01BreathControlComponent.h"
#include "UI/CS01VolumeComponent.h"
#include "UI/PresetPanel.h"
#include "UI/FilterTypeComponent.h"
#include "UI/CS01OscilloscopeComponent.h"

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
    
    CS01OscilloscopeComponent& getOscilloscope() { return oscilloscopeComponent; }
    juce::AudioVisualiserComponent& getAudioVisualiser() { return audioVisualiser; }

private:
    CS01AudioProcessor& audioProcessor;

    juce::MidiKeyboardComponent midiKeyboard;
    
    std::unique_ptr<CS01ModulationComponent> modulationComponent;
    std::unique_ptr<CS01VCOComponent> vcoComponent;
    std::unique_ptr<CS01LFOComponent> lfoComponent;
    std::unique_ptr<CS01VCFComponent> vcfComponent;
    std::unique_ptr<CS01VCAComponent> vcaComponent;
    std::unique_ptr<CS01EGComponent> egComponent;
    std::unique_ptr<CS01BreathControlComponent> breathControlComponent;
    std::unique_ptr<CS01VolumeComponent> volumeComponent;
    std::unique_ptr<PresetPanel> presetPanel;
    std::unique_ptr<FilterTypeComponent> filterTypeComponent;
    std::unique_ptr<CS01LookAndFeel> lookAndFeel;
    CS01OscilloscopeComponent oscilloscopeComponent;
    juce::AudioVisualiserComponent audioVisualiser;

    juce::FlexBox upperFlex;
    juce::FlexBox lowerFlex;
    juce::FlexBox mainFlex;
    juce::FlexBox visualizerFlex; // FlexBox for waveform display

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CS01AudioProcessorEditor)
};
