#include "CS01AudioProcessor.h"
#include "CS01AudioProcessorEditor.h"
#include "UI/CS01LookAndFeel.h"
#include "UI/ProgramPanel.h"
#include "UI/VCAComponent.h"
#include "UI/VCFComponent.h"
#include "UI/VCOComponent.h"
#include "UI/VolumeComponent.h"
#include "CS01Synth/IFilter.h"

// Use JUCE namespace
using namespace juce;

//==============================================================================
CS01AudioProcessorEditor::CS01AudioProcessorEditor(CS01AudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      midiKeyboard(p.getKeyboardState(),
                   juce::MidiKeyboardComponent::Orientation::horizontalKeyboard),
      oscilloscopeComponent(p.getTotalNumOutputChannels()),
      audioVisualiser(p.getTotalNumOutputChannels()) {
    // Set keyboard range to match CS-01 (F2 - C5, 32 keys)
    midiKeyboard.setAvailableRange(41, 72);
    midiKeyboard.setKeyWidth(25); // Mini keys look
    midiKeyboard.setBlackNoteWidthProportion(0.6f);
    midiKeyboard.setBlackNoteLengthProportion(0.6f);

    lookAndFeel = std::make_unique<CS01LookAndFeel>();
    setLookAndFeel(lookAndFeel.get());

    // Initialize audio visualizer
    audioVisualiser.setBufferSize(512);
    audioVisualiser.setSamplesPerBlock(16);

    // Initialize oscilloscope component
    oscilloscopeComponent.setBufferSize(512);

    // Create and make all components visible
    addAndMakeVisible(midiKeyboard);
    // addAndMakeVisible(audioVisualiser); // Hide spectrum analyzer for vintage look
    addAndMakeVisible(oscilloscopeComponent);
    oscilloscopeComponent.setVisible(false);
    addAndMakeVisible(monitorButton);
    monitorButton.setClickingTogglesState(true);
    monitorButton.onClick = [this] {
        oscilloscopeComponent.setVisible(monitorButton.getToggleState());
        resized();
    };

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

    programPanel.reset(new ProgramPanel(audioProcessor));
    addAndMakeVisible(programPanel.get());

    setResizable(true, true);
    setResizeLimits(1200, 620, 2400, 1240);
    setSize(1280, 680);
}

CS01AudioProcessorEditor::~CS01AudioProcessorEditor() {
    setLookAndFeel(nullptr);
}

//==============================================================================
void CS01AudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(CS01LookAndFeel::Palette::background);
}

void CS01AudioProcessorEditor::resized() {
    auto bounds = getLocalBounds().reduced(20);
    auto header = bounds.removeFromTop(44);
    monitorButton.setBounds(header.removeFromRight(100));
    programPanel->setBounds(header.withSizeKeepingCentre(560, 44));
    bounds.removeFromTop(18);

    auto panel = bounds.removeFromTop(bounds.getHeight() * 3 / 5);
    juce::FlexBox soundPanel;
    soundPanel.flexDirection = juce::FlexBox::Direction::row;
    const auto margin = juce::FlexItem::Margin(0, 6, 0, 6);
    soundPanel.items.add(juce::FlexItem(*lfoComponent).withFlex(1).withMargin(margin));
    soundPanel.items.add(juce::FlexItem(*vcoComponent).withFlex(5).withMargin(margin));
    soundPanel.items.add(juce::FlexItem(*vcfComponent).withFlex(4).withMargin(margin));
    soundPanel.items.add(juce::FlexItem(*vcaComponent).withFlex(1.2f).withMargin(margin));
    soundPanel.items.add(juce::FlexItem(*egComponent).withFlex(3).withMargin(margin));
    soundPanel.performLayout(panel);

    bounds.removeFromTop(20);
    auto performance = bounds.removeFromLeft(180);
    breathControlComponent->setBounds(performance.removeFromTop(performance.getHeight() / 2));
    volumeComponent->setBounds(performance);
    bounds.removeFromLeft(12);
    modulationComponent->setBounds(bounds.removeFromLeft(204));
    bounds.removeFromLeft(12);

    if (monitorButton.getToggleState()) {
        oscilloscopeComponent.setBounds(bounds.removeFromBottom(85));
        bounds.removeFromBottom(10);
    }
    midiKeyboard.setBounds(bounds);
    // Keep the entire playable range visible when the editor is resized.
    midiKeyboard.setKeyWidth(static_cast<float>(bounds.getWidth()) / 19.0f);
}
