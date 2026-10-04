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
    // Lower PC keyboard playback by one octave from JUCE's default (6).
    midiKeyboard.setKeyPressBaseOctave(5);
    midiKeyboard.setKeyWidth(25);  // Mini keys look
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
    addChildComponent(midiKeyboard);
    addChildComponent(oscilloscopeComponent);
    addAndMakeVisible(displayButton);
    displayButton.setClickingTogglesState(true);
    displayButton.onClick = [this] {
        if (!displayButton.getToggleState())
            midiKeyboard.focusLost(juce::Component::focusChangedDirectly);
        midiKeyboard.setVisible(displayButton.getToggleState());
        oscilloscopeComponent.setVisible(displayButton.getToggleState());
        updateDisplayLayout();
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
    setResizeLimits(1240, 400, 2400, 1240);
    setSize(1240, 400);
    setWantsKeyboardFocus(true);
    addPerformanceKeyListeners(*this);
    startTimerHz(30);
}

CS01AudioProcessorEditor::~CS01AudioProcessorEditor() {
    stopTimer();
    midiKeyboard.focusLost(juce::Component::focusChangedDirectly);
    setLookAndFeel(nullptr);
}

void CS01AudioProcessorEditor::addPerformanceKeyListeners(juce::Component& component) {
    // The keyboard already handles its own events; do not process them twice.
    if (&component == &midiKeyboard)
        return;

    component.addKeyListener(this);
    for (auto* child : component.getChildren())
        addPerformanceKeyListeners(*child);
}

bool CS01AudioProcessorEditor::isTextInputFocused() const {
    for (auto* component = juce::Component::getCurrentlyFocusedComponent(); component != nullptr;
         component = component->getParentComponent()) {
        if (dynamic_cast<juce::TextEditor*>(component) != nullptr)
            return true;
        if (component == this)
            break;
    }
    return false;
}

bool CS01AudioProcessorEditor::keyPressed(const juce::KeyPress& key, juce::Component*) {
    return midiKeyboard.isVisible() && !isTextInputFocused() && midiKeyboard.keyPressed(key);
}

bool CS01AudioProcessorEditor::keyStateChanged(bool isKeyDown, juce::Component*) {
    if (!midiKeyboard.isVisible() || isTextInputFocused()) {
        midiKeyboard.focusLost(juce::Component::focusChangedDirectly);
        return false;
    }
    return midiKeyboard.keyStateChanged(isKeyDown);
}

void CS01AudioProcessorEditor::timerCallback() {
    // Release only notes owned by the on-screen keyboard, not external MIDI.
    if (!midiKeyboard.isVisible() || !hasKeyboardFocus(true) || isTextInputFocused())
        midiKeyboard.focusLost(juce::Component::focusChangedDirectly);
}

//==============================================================================
void CS01AudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(CS01LookAndFeel::Palette::background);
}

void CS01AudioProcessorEditor::updateDisplayLayout() {
    const bool expanded = displayButton.getToggleState();
    const int extraHeight = 240;
    const int height = getHeight() + (expanded ? extraHeight : -extraHeight);
    setResizeLimits(1240, expanded ? 640 : 400, 2400, expanded ? 1480 : 1240);
    setSize(getWidth(), height);
    resized();
}

void CS01AudioProcessorEditor::resized() {
    auto bounds = getLocalBounds().reduced(20);
    auto header = bounds.removeFromTop(44);
    displayButton.setBounds(header.removeFromRight(220));
    programPanel->setBounds(header.withSizeKeepingCentre(560, 44));
    bounds.removeFromTop(18);

    const bool expanded = displayButton.getToggleState();
    auto lowerPanel = expanded ? bounds.removeFromBottom(240) : juce::Rectangle<int>();
    auto panel = bounds;
    auto performance = panel.removeFromLeft(124);
    breathControlComponent->setBounds(performance.removeFromTop(performance.getHeight() / 2));
    volumeComponent->setBounds(performance);
    panel.removeFromLeft(12);
    modulationComponent->setBounds(panel.removeFromLeft(204));
    panel.removeFromLeft(12);
    // Every sound control occupies one common column, including across sections.
    juce::Component* sections[] = {lfoComponent.get(), vcoComponent.get(),
                                   vcfComponent.get(), vcaComponent.get(), egComponent.get()};
    const int columns[] = {1, 5, 3, 1, 4};
    const int origin = panel.getX();
    const int totalWidth = panel.getWidth();
    int column = 0;
    for (int i = 0; i < 5; ++i) {
        const int left = origin + totalWidth * column / 14;
        column += columns[i];
        const int right = origin + totalWidth * column / 14;
        sections[i]->setBounds(left, panel.getY(), right - left, panel.getHeight());
    }

    if (expanded) {
        lowerPanel.removeFromTop(20);
        oscilloscopeComponent.setBounds(lowerPanel.removeFromRight(lowerPanel.getWidth() / 3));
        lowerPanel.removeFromRight(12);
        midiKeyboard.setBounds(lowerPanel);
        // Keep the entire playable range visible when the editor is resized.
        midiKeyboard.setKeyWidth(static_cast<float>(lowerPanel.getWidth()) / 19.0f);
    }
}
