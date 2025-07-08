#pragma once

#include <JuceHeader.h>

class ProgramPanel : public juce::Component, private juce::ComboBox::Listener, private juce::Timer {
   public:
    ProgramPanel(juce::AudioProcessor& p);
    ~ProgramPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

   private:
    void timerCallback() override;
    void comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) override;
    void populateProgramMenu();
    void loadProgram(int programIndex);

    juce::AudioProcessor& audioProcessor;

    juce::ComboBox programMenu;
    juce::TextButton prevButton{"<"};
    juce::TextButton nextButton{">"};
};
