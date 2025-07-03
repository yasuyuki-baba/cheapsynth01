#include "ProgramPanel.h"

ProgramPanel::ProgramPanel(juce::AudioProcessor& p) : audioProcessor(p)
{
    addAndMakeVisible(programMenu);
    programMenu.addListener(this);

    addAndMakeVisible(prevButton);
    prevButton.onClick = [this]
    {
        const int currentId = programMenu.getSelectedId();
        if (currentId > 1)
            programMenu.setSelectedId(currentId - 1);
    };

    addAndMakeVisible(nextButton);
    nextButton.onClick = [this]
    {
        const int currentId = programMenu.getSelectedId();
        if (currentId < programMenu.getNumItems())
            programMenu.setSelectedId(currentId + 1);
    };

    populateProgramMenu();
    startTimerHz(10); // Check for preset changes 10 times per second
}

ProgramPanel::~ProgramPanel()
{
    stopTimer();
    programMenu.removeListener(this);
}

void ProgramPanel::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void ProgramPanel::resized()
{
    juce::FlexBox flex;
    flex.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;
    flex.alignItems = juce::FlexBox::AlignItems::center;

    flex.items.add(juce::FlexItem(prevButton).withWidth(30).withHeight(30));
    flex.items.add(juce::FlexItem(programMenu).withFlex(1.0f).withHeight(30));
    flex.items.add(juce::FlexItem(nextButton).withWidth(30).withHeight(30));

    flex.performLayout(getLocalBounds().reduced(5));
}

void ProgramPanel::timerCallback()
{
    const int currentProgram = audioProcessor.getCurrentProgram();
    if (currentProgram + 1 != programMenu.getSelectedId())
    {
        programMenu.setSelectedId(currentProgram + 1, juce::dontSendNotification);
    }
}

void ProgramPanel::comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged)
{
    if (comboBoxThatHasChanged == &programMenu)
    {
        const int programIndex = programMenu.getSelectedId() - 1; // 1-based to 0-based
        if (programIndex >= 0 && programIndex < audioProcessor.getNumPrograms())
        {
            audioProcessor.setCurrentProgram(programIndex);
        }
    }
}

void ProgramPanel::populateProgramMenu()
{
    programMenu.clear();
    for (int i = 0; i < audioProcessor.getNumPrograms(); ++i)
    {
        programMenu.addItem(audioProcessor.getProgramName(i), i + 1); // 1-based ID
    }
    programMenu.setSelectedId(audioProcessor.getCurrentProgram() + 1, juce::dontSendNotification);
}
