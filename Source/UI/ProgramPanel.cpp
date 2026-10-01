#include "CS01LookAndFeel.h"
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
    auto bounds = getLocalBounds().toFloat();

    // Panel Background (Darker Grey/Black strip)
    g.setColour(juce::Colours::darkgrey.darker(0.5f));
    g.fillRoundedRectangle(bounds.reduced(2.0f), 5.0f);

    // Bezel for the "Display" area (where the ComboBox sits)
    auto displayArea = programMenu.getBounds().toFloat().expanded(4.0f, 2.0f);
    g.setColour(juce::Colours::black);
    g.fillRoundedRectangle(displayArea, 4.0f);

    // "MEMORY" Label
    g.setColour(juce::Colours::white.withAlpha(0.7f));
    g.setFont(10.0f);
    g.drawText("MEMORY", displayArea.getX(), 3.0f, displayArea.getWidth(), 12.0f, juce::Justification::centred, false);

    // Border for the whole panel
    g.setColour(juce::Colours::grey);
    g.drawRoundedRectangle(bounds.reduced(2.0f), 5.0f, 1.0f);
}

void ProgramPanel::resized() {
    auto bounds = getLocalBounds().reduced(10, 5).withTrimmedTop(12);

    // Layout: [Prev] [Display (Menu)] [Next]  ...  [Save] [Rename] [Delete]

    auto buttonSize = 24;
    auto margin = 5;

    // Navigation Section (Left/Center)
    auto navArea = bounds.removeFromLeft(300);

    prevButton.setBounds(navArea.removeFromLeft(buttonSize).reduced(0, 2));
    navArea.removeFromLeft(margin);

    nextButton.setBounds(navArea.removeFromRight(buttonSize).reduced(0, 2));
    navArea.removeFromRight(margin);

    programMenu.setBounds(navArea); // Remaining middle part is the menu

    // Management Section (Right)
    auto manageArea = bounds.removeFromRight(180);

    deleteButton.setBounds(manageArea.removeFromRight(50));
    manageArea.removeFromRight(margin);

    renameButton.setBounds(manageArea.removeFromRight(60));
    manageArea.removeFromRight(margin);

    saveButton.setBounds(manageArea.removeFromRight(50));

    // Preset type label (Optional, maybe put it below or hide it if space is tight)
    // For now, let's put it in the remaining space of manageArea or hide it
    presetTypeLabel.setBounds(manageArea);
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
