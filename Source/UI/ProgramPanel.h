#pragma once

#include <JuceHeader.h>

class ProgramManager; // Forward declaration

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
    
    // User preset management
    void savePresetButtonClicked();
    void deletePresetButtonClicked();
    void renamePresetButtonClicked();
    void showSavePresetDialog();
    void showRenamePresetDialog();
    void savePresetWithName(ProgramManager* programManager, const juce::String& presetName);
    
    ProgramManager* getProgramManager();

    juce::AudioProcessor& audioProcessor;

    juce::ComboBox programMenu;
    juce::TextButton prevButton{"<"};
    juce::TextButton nextButton{">"};
    juce::TextButton saveButton{"Save"};
    juce::TextButton deleteButton{"Delete"};
    juce::TextButton renameButton{"Rename"};
    
    // Add a label to show preset type
    juce::Label presetTypeLabel;
};
