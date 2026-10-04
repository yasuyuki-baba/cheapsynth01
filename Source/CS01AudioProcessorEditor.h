#pragma once

#include <JuceHeader.h>
// 循環参照を避けるために前方宣言
class CS01AudioProcessor;
class IFilter;
#include "UI/BreathControlComponent.h"
#include "UI/EGComponent.h"
#include "UI/LFOComponent.h"
#include "UI/ModulationComponent.h"
#include "UI/OscilloscopeComponent.h"
#include "UI/ProgramPanel.h"
#include "UI/VCAComponent.h"
#include "UI/VCFComponent.h"
#include "UI/VCOComponent.h"
#include "UI/VolumeComponent.h"

// Forward declarations
class CS01LookAndFeel;

//==============================================================================
class CS01AudioProcessorEditor : public juce::AudioProcessorEditor,
                                 private juce::KeyListener,
                                 private juce::Timer {
   public:
    CS01AudioProcessorEditor(CS01AudioProcessor&);
    ~CS01AudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    using juce::AudioProcessorEditor::keyPressed;
    using juce::AudioProcessorEditor::keyStateChanged;

    // フィルタータイプが変更されたときに呼び出される
    void filterTypeChanged(IFilter* newFilterProcessor);

    OscilloscopeComponent& getOscilloscope() {
        return oscilloscopeComponent;
    }
    juce::AudioVisualiserComponent& getAudioVisualiser() {
        return audioVisualiser;
    }

   private:
    bool keyPressed(const juce::KeyPress&, juce::Component*) override;
    bool keyStateChanged(bool, juce::Component*) override;
    void timerCallback() override;
    void addPerformanceKeyListeners(juce::Component&);
    bool isTextInputFocused() const;

    CS01AudioProcessor& audioProcessor;

    juce::MidiKeyboardComponent midiKeyboard;
    juce::TextButton monitorButton{"MONITOR"};

    std::unique_ptr<ModulationComponent> modulationComponent;
    std::unique_ptr<VCOComponent> vcoComponent;
    std::unique_ptr<LFOComponent> lfoComponent;
    std::unique_ptr<VCFComponent> vcfComponent;
    std::unique_ptr<VCAComponent> vcaComponent;
    std::unique_ptr<EGComponent> egComponent;
    std::unique_ptr<BreathControlComponent> breathControlComponent;
    std::unique_ptr<VolumeComponent> volumeComponent;
    std::unique_ptr<ProgramPanel> programPanel;
    std::unique_ptr<CS01LookAndFeel> lookAndFeel;
    OscilloscopeComponent oscilloscopeComponent;
    juce::AudioVisualiserComponent audioVisualiser;

    juce::FlexBox upperFlex;
    juce::FlexBox lowerFlex;
    juce::FlexBox mainFlex;
    juce::FlexBox visualizerFlex;  // FlexBox for waveform display

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CS01AudioProcessorEditor)
};
