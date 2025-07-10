#include "ProgramPanel.h"
#include "../ProgramManager.h"
#include "../CS01AudioProcessor.h"

ProgramPanel::ProgramPanel(juce::AudioProcessor& p) : audioProcessor(p) {
    addAndMakeVisible(programMenu);
    programMenu.addListener(this);

    addAndMakeVisible(prevButton);
    prevButton.onClick = [this] {
        const int currentId = programMenu.getSelectedId();
        if (currentId > 1)
            programMenu.setSelectedId(currentId - 1);
    };

    addAndMakeVisible(nextButton);
    nextButton.onClick = [this] {
        const int currentId = programMenu.getSelectedId();
        if (currentId < programMenu.getNumItems())
            programMenu.setSelectedId(currentId + 1);
    };

    // User preset management buttons
    addAndMakeVisible(saveButton);
    saveButton.onClick = [this] { savePresetButtonClicked(); };

    addAndMakeVisible(deleteButton);
    deleteButton.onClick = [this] { deletePresetButtonClicked(); };

    addAndMakeVisible(renameButton);
    renameButton.onClick = [this] { renamePresetButtonClicked(); };

    // Preset type label
    addAndMakeVisible(presetTypeLabel);
    presetTypeLabel.setFont(juce::Font(12.0f));
    presetTypeLabel.setJustificationType(juce::Justification::centred);

    populateProgramMenu();
    startTimerHz(10);  // Check for preset changes 10 times per second
}

ProgramPanel::~ProgramPanel() {
    stopTimer();
    programMenu.removeListener(this);
}

void ProgramPanel::paint(juce::Graphics& g) {
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void ProgramPanel::resized() {
    auto bounds = getLocalBounds().reduced(3);
    
    // Single row layout: [<] [Program Menu] [>] [Preset Type] [Save][Delete][Rename]
    auto mainRow = bounds.removeFromTop(20);
    
    // Left navigation button
    prevButton.setBounds(mainRow.removeFromLeft(30));
    mainRow.removeFromLeft(5); // Small spacing
    
    // Management buttons (reserve space from right)
    const int buttonWidth = 60;
    const int buttonSpacing = 2;
    
    renameButton.setBounds(mainRow.removeFromRight(buttonWidth));
    mainRow.removeFromRight(buttonSpacing);
    deleteButton.setBounds(mainRow.removeFromRight(buttonWidth));
    mainRow.removeFromRight(buttonSpacing);
    saveButton.setBounds(mainRow.removeFromRight(buttonWidth));
    mainRow.removeFromRight(5); // Spacing before preset type label
    
    // Preset type label
    presetTypeLabel.setBounds(mainRow.removeFromRight(80));
    mainRow.removeFromRight(5); // Spacing before next button
    
    // Right navigation button (next to program menu)
    nextButton.setBounds(mainRow.removeFromRight(30));
    mainRow.removeFromRight(5); // Small spacing
    
    // Program menu takes remaining space
    programMenu.setBounds(mainRow);
}

void ProgramPanel::timerCallback() {
    const int currentProgram = audioProcessor.getCurrentProgram();
    if (currentProgram + 1 != programMenu.getSelectedId()) {
        programMenu.setSelectedId(currentProgram + 1, juce::dontSendNotification);
    }
    
    // Update preset type label and button states
    auto* programManager = getProgramManager();
    if (programManager) {
        bool isUser = programManager->isUserPreset(currentProgram);
        presetTypeLabel.setText(isUser ? "User Preset" : "Factory Preset", 
                               juce::dontSendNotification);
        
        // Enable/disable buttons based on preset type
        deleteButton.setEnabled(isUser);
        renameButton.setEnabled(isUser);
    }
}

void ProgramPanel::comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) {
    if (comboBoxThatHasChanged == &programMenu) {
        const int programIndex = programMenu.getSelectedId() - 1;  // 1-based to 0-based
        if (programIndex >= 0 && programIndex < audioProcessor.getNumPrograms()) {
            audioProcessor.setCurrentProgram(programIndex);
        }
    }
}

void ProgramPanel::populateProgramMenu() {
    programMenu.clear();
    for (int i = 0; i < audioProcessor.getNumPrograms(); ++i) {
        programMenu.addItem(audioProcessor.getProgramName(i), i + 1);  // 1-based ID
    }
    programMenu.setSelectedId(audioProcessor.getCurrentProgram() + 1, juce::dontSendNotification);
}

ProgramManager* ProgramPanel::getProgramManager() {
    // Cast audioProcessor to CS01AudioProcessor to access ProgramManager
    if (auto* cs01Processor = dynamic_cast<CS01AudioProcessor*>(&audioProcessor)) {
        return &cs01Processor->getPresetManager();
    }
    return nullptr;
}

void ProgramPanel::savePresetButtonClicked() {
    showSavePresetDialog();
}

void ProgramPanel::deletePresetButtonClicked() {
    auto* programManager = getProgramManager();
    if (!programManager) return;
    
    const int currentProgram = audioProcessor.getCurrentProgram();
    if (!programManager->isUserPreset(currentProgram)) {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                             "Cannot Delete",
                                             "Factory presets cannot be deleted.");
        return;
    }
    
    auto presetName = audioProcessor.getProgramName(currentProgram);
    
    juce::AlertWindow::showOkCancelBox(juce::AlertWindow::QuestionIcon,
                                     "Delete Preset",
                                     "Are you sure you want to delete preset \"" + presetName + "\"?",
                                     "Delete", "Cancel",
                                     this,
                                     juce::ModalCallbackFunction::create([this, programManager, currentProgram](int result) {
                                         if (result == 1) { // OK was clicked
                                             if (programManager->deleteUserPreset(currentProgram)) {
                                                 populateProgramMenu();
                                             }
                                         }
                                     }));
}

void ProgramPanel::renamePresetButtonClicked() {
    auto* programManager = getProgramManager();
    if (!programManager) return;
    
    const int currentProgram = audioProcessor.getCurrentProgram();
    if (!programManager->isUserPreset(currentProgram)) {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                             "Cannot Rename",
                                             "Factory presets cannot be renamed.");
        return;
    }
    
    showRenamePresetDialog();
}

void ProgramPanel::showSavePresetDialog() {
    auto* programManager = getProgramManager();
    if (!programManager) return;
    
    // Determine default preset name based on current selection
    const int currentProgram = audioProcessor.getCurrentProgram();
    juce::String defaultName = "My Preset";
    
    if (programManager->isUserPreset(currentProgram)) {
        // If current preset is a user preset, use its name as default
        defaultName = audioProcessor.getProgramName(currentProgram);
    }
    
    auto alertWindow = std::make_unique<juce::AlertWindow>("Save Preset", 
                                                         "Enter a name for the new preset:", 
                                                         juce::AlertWindow::NoIcon);
    alertWindow->addTextEditor("presetName", defaultName, "Preset Name:");
    alertWindow->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    alertWindow->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    
    // Store the AlertWindow pointer for access in callback
    auto* alertWindowPtr = alertWindow.get();
    
    alertWindow->enterModalState(true, 
        juce::ModalCallbackFunction::create([this, programManager, alertWindowPtr](int result) {
            if (result == 1) {
                auto presetName = alertWindowPtr->getTextEditorContents("presetName");
                
                if (presetName.isNotEmpty()) {
                    // Check if preset already exists
                    auto userPresetsDir = programManager->getUserPresetsDirectory();
                    auto presetFile = userPresetsDir.getChildFile(presetName + ".xml");
                    
                    if (presetFile.exists()) {
                        // Show overwrite confirmation
                        juce::NativeMessageBox::showOkCancelBox(
                            juce::MessageBoxIconType::QuestionIcon,
                            "Overwrite Preset",
                            "A preset named \"" + presetName + "\" already exists.\n\nDo you want to overwrite it?",
                            this,
                            juce::ModalCallbackFunction::create([this, programManager, presetName](int overwriteResult) {
                                if (overwriteResult == 1) { // Overwrite confirmed
                                    this->savePresetWithName(programManager, presetName);
                                }
                            })
                        );
                    } else {
                        // No existing preset, save directly
                        savePresetWithName(programManager, presetName);
                    }
                }
            }
        }), true);
    
    alertWindow.release(); // AlertWindow will be deleted automatically
}

void ProgramPanel::showRenamePresetDialog() {
    auto* programManager = getProgramManager();
    if (!programManager) return;
    
    const int currentProgram = audioProcessor.getCurrentProgram();
    auto currentName = audioProcessor.getProgramName(currentProgram);
    
    auto alertWindow = std::make_unique<juce::AlertWindow>("Rename Preset", 
                                                         "Enter a new name for the preset:", 
                                                         juce::AlertWindow::NoIcon);
    alertWindow->addTextEditor("presetName", currentName, "Preset Name:");
    alertWindow->addButton("Rename", 1, juce::KeyPress(juce::KeyPress::returnKey));
    alertWindow->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    
    alertWindow->enterModalState(true, 
        juce::ModalCallbackFunction::create([this, programManager, currentProgram, currentName](int result) {
            if (result == 1) {
                auto* alertWindow = dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
                if (alertWindow) {
                    auto newName = alertWindow->getTextEditorContents("presetName");
                    if (newName.isNotEmpty() && newName != currentName) {
                        if (programManager->renameUserPreset(currentProgram, newName)) {
                            populateProgramMenu();
                        }
                    }
                }
            }
        }), true);
    
    alertWindow.release(); // AlertWindow will be deleted automatically
}

void ProgramPanel::savePresetWithName(ProgramManager* programManager, const juce::String& presetName) {
    programManager->saveCurrentStateAsPreset(presetName);
    
    // Find and select the saved preset BEFORE repopulating the menu
    for (int i = 0; i < audioProcessor.getNumPrograms(); ++i) {
        if (audioProcessor.getProgramName(i) == presetName) {
            audioProcessor.setCurrentProgram(i);
            break;
        }
    }
    
    // Now populate the menu - this will use the updated current program
    populateProgramMenu();
}
