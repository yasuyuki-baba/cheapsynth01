#include "PresetPanel.h"

PresetPanel::PresetPanel(juce::AudioProcessor& p) : audioProcessor(p)
{
    addAndMakeVisible(presetMenu);
    presetMenu.addListener(this);

    addAndMakeVisible(prevButton);
    prevButton.onClick = [this]
    {
        const int currentId = presetMenu.getSelectedId();
        if (currentId > 1)
            presetMenu.setSelectedId(currentId - 1);
    };

    addAndMakeVisible(nextButton);
    nextButton.onClick = [this]
    {
        const int currentId = presetMenu.getSelectedId();
        if (currentId < presetMenu.getNumItems())
            presetMenu.setSelectedId(currentId + 1);
    };

    populatePresetMenu();
    startTimerHz(10); // Check for preset changes 10 times per second
}

PresetPanel::~PresetPanel()
{
    stopTimer();
    presetMenu.removeListener(this);
}

void PresetPanel::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void PresetPanel::resized()
{
    juce::FlexBox flex;
    flex.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;
    flex.alignItems = juce::FlexBox::AlignItems::center;

    flex.items.add(juce::FlexItem(prevButton).withWidth(30).withHeight(30));
    flex.items.add(juce::FlexItem(presetMenu).withFlex(1.0f).withHeight(30));
    flex.items.add(juce::FlexItem(nextButton).withWidth(30).withHeight(30));

    flex.performLayout(getLocalBounds().reduced(5));
}

void PresetPanel::timerCallback()
{
    const int currentProgram = audioProcessor.getCurrentProgram();
    if (currentProgram + 1 != presetMenu.getSelectedId())
    {
        presetMenu.setSelectedId(currentProgram + 1, juce::dontSendNotification);
    }
}

void PresetPanel::comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged)
{
    if (comboBoxThatHasChanged == &presetMenu)
    {
        const int presetIndex = presetMenu.getSelectedId() - 1; // 1-based to 0-based
        if (presetIndex >= 0 && presetIndex < audioProcessor.getNumPrograms())
        {
            audioProcessor.setCurrentProgram(presetIndex);
        }
    }
}

void PresetPanel::populatePresetMenu()
{
    presetMenu.clear();
    for (int i = 0; i < audioProcessor.getNumPrograms(); ++i)
    {
        presetMenu.addItem(audioProcessor.getProgramName(i), i + 1); // 1-based ID
    }
    presetMenu.setSelectedId(audioProcessor.getCurrentProgram() + 1, juce::dontSendNotification);
}
