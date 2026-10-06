#pragma once

#include <JuceHeader.h>

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
#include "UI/SliderValuePopup.h"

#include <memory>

// Forward declarations
class CS01AudioProcessor;
class CS01LookAndFeel;
class IFilter;

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
    void updateDisplayLayout();

    CS01AudioProcessor& audioProcessor;
    juce::TooltipWindow tooltipWindow{this, 700};

    juce::MidiKeyboardComponent midiKeyboard;
    juce::TextButton displayButton{"KEYBOARD + MONITOR"};
    juce::TextButton originalVcfModelButton{"ORIGINAL: LEGACY"};
    juce::TextButton modernVcfModelButton{"MODERN: LEGACY"};

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
    juce::AudioBuffer<float> displayAudio{2, 512};
    std::unique_ptr<SliderValuePopup> sliderValuePopup;

    juce::FlexBox upperFlex;
    juce::FlexBox lowerFlex;
    juce::FlexBox mainFlex;
    juce::FlexBox visualizerFlex;  // FlexBox for waveform display

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CS01AudioProcessorEditor)
};
