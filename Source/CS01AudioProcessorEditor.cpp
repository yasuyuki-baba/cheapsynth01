#include "CS01AudioProcessor.h"
#include "CS01AudioProcessorEditor.h"
#include "UI/CS01LookAndFeel.h"

// Use JUCE namespace
using namespace juce;

//==============================================================================
CS01AudioProcessorEditor::CS01AudioProcessorEditor(CS01AudioProcessor& p)
    : AudioProcessorEditor(&p), 
      audioProcessor(p),
      midiKeyboard(p.getKeyboardState(), juce::MidiKeyboardComponent::Orientation::horizontalKeyboard),
      audioVisualiser(p.getTotalNumOutputChannels()),
      oscilloscopeComponent(p.getTotalNumOutputChannels())
{
    lookAndFeel = std::make_unique<CS01LookAndFeel>();
    setLookAndFeel(lookAndFeel.get());

    // Initialize audio visualizer
    audioVisualiser.setBufferSize(512);
    audioVisualiser.setSamplesPerBlock(16);

    // Initialize oscilloscope component
    oscilloscopeComponent.setBufferSize(512);

    // Create and make all components visible
    addAndMakeVisible(midiKeyboard);
    addAndMakeVisible(audioVisualiser);
    addAndMakeVisible(oscilloscopeComponent);
    modulationComponent.reset(new ModulationComponent(audioProcessor));
    addAndMakeVisible(modulationComponent.get());
    vcoComponent.reset(new VCOComponent(audioProcessor.getValueTreeState()));
    addAndMakeVisible(vcoComponent.get());
    lfoComponent.reset(new LFOComponent(audioProcessor.getValueTreeState()));
    addAndMakeVisible(lfoComponent.get());
    vcfComponent.reset(new VCFComponent(audioProcessor.getValueTreeState()));
    addAndMakeVisible(vcfComponent.get());
    vcaComponent.reset(new VCAComponent(audioProcessor.getValueTreeState()));
    addAndMakeVisible(vcaComponent.get());
    egComponent.reset(new EGComponent(audioProcessor.getValueTreeState()));
    addAndMakeVisible(egComponent.get());
    breathControlComponent.reset(new BreathControlComponent(audioProcessor.getValueTreeState()));
    addAndMakeVisible(breathControlComponent.get());
    volumeComponent.reset(new VolumeComponent(audioProcessor.getValueTreeState()));
    addAndMakeVisible(volumeComponent.get());

    presetPanel.reset(new PresetPanel(audioProcessor));
    addAndMakeVisible(presetPanel.get());
    
    filterTypeComponent.reset(new FilterTypeComponent(audioProcessor.getValueTreeState()));
    addAndMakeVisible(filterTypeComponent.get());

    // Define layout structure in the constructor
    upperFlex.flexDirection = juce::FlexBox::Direction::row;
    upperFlex.items.add(juce::FlexItem(*modulationComponent).withFlex(4));
    upperFlex.items.add(juce::FlexItem(*lfoComponent).withFlex(2));
    upperFlex.items.add(juce::FlexItem(*vcoComponent).withFlex(8));
    upperFlex.items.add(juce::FlexItem(*filterTypeComponent).withFlex(3));
    upperFlex.items.add(juce::FlexItem(*vcfComponent).withFlex(5));
    upperFlex.items.add(juce::FlexItem(*vcaComponent).withFlex(2));
    upperFlex.items.add(juce::FlexItem(*egComponent).withFlex(6));
    
    // Configure FlexBox for waveform display (vertical layout)
    visualizerFlex.flexDirection = juce::FlexBox::Direction::column;
    visualizerFlex.items.add(juce::FlexItem(oscilloscopeComponent).withFlex(1));
    visualizerFlex.items.add(juce::FlexItem(audioVisualiser).withFlex(1));
    
    // Add waveform display FlexBox to the upper FlexBox
    upperFlex.items.add(juce::FlexItem(visualizerFlex).withFlex(4));

    lowerFlex.flexDirection = juce::FlexBox::Direction::row;
    lowerFlex.items.add(juce::FlexItem(*breathControlComponent).withFlex(2.0f));
    lowerFlex.items.add(juce::FlexItem(*volumeComponent).withFlex(2.0f));
    lowerFlex.items.add(juce::FlexItem(midiKeyboard).withFlex(27.0f));

    mainFlex.flexDirection = juce::FlexBox::Direction::column;
    mainFlex.items.add(juce::FlexItem(*presetPanel).withHeight(40));
    mainFlex.items.add(juce::FlexItem(upperFlex).withFlex(0.65));
    mainFlex.items.add(juce::FlexItem(lowerFlex).withFlex(0.35));

    setResizable(true, true);
    setResizeLimits(800, 350, 10000, 10000);
    setSize(1200, 500);
}

CS01AudioProcessorEditor::~CS01AudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

//==============================================================================
void CS01AudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
}

void CS01AudioProcessorEditor::resized()
{
    mainFlex.performLayout(getLocalBounds().reduced(10));
}
