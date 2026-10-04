#include <gtest/gtest.h>
#include <JuceHeader.h>
#include "../../Source/CS01AudioProcessor.h"
#include "../../Source/CS01AudioProcessorEditor.h"
#include "../../Source/UI/ProgramPanel.h"

class ProgramPanelTest : public ::testing::Test {
   protected:
    void SetUp() override {
        oldName = "Rename-Test-" + juce::Uuid().toString();
        newName = oldName + "-Renamed";
        auto& manager = processor.getPresetManager();
        oldFile = manager.getUserPresetsDirectory().getChildFile(oldName + ".xml");
        newFile = manager.getUserPresetsDirectory().getChildFile(newName + ".xml");
        manager.saveCurrentStateAsPreset(oldName);
        ASSERT_TRUE(oldFile.existsAsFile());
        int index = -1;
        for (int i = 0; i < manager.getNumPrograms(); ++i)
            if (manager.getProgramName(i) == oldName)
                index = i;
        ASSERT_GE(index, 0);
        manager.setCurrentProgram(index);
        editor = std::make_unique<CS01AudioProcessorEditor>(processor);
    }

    void TearDown() override {
        editor.reset();
        dispatchCallbacks();
        oldFile.deleteFile();
        newFile.deleteFile();
    }

    void dispatchCallbacks() {
        juce::MessageManager::getInstance()->runDispatchLoopUntil(200);
    }

    juce::AlertWindow* openRenameDialog() {
        for (auto* child : editor->getChildren()) {
            if (auto* panel = dynamic_cast<ProgramPanel*>(child)) {
                for (auto* control : panel->getChildren()) {
                    if (auto* button = dynamic_cast<juce::TextButton*>(control)) {
                        if (button->getButtonText() == "Rename") {
                            button->onClick();
                            return dynamic_cast<juce::AlertWindow*>(
                                juce::Component::getCurrentlyModalComponent());
                        }
                    }
                }
            }
        }
        return nullptr;
    }

    CS01AudioProcessor processor;
    std::unique_ptr<CS01AudioProcessorEditor> editor;
    juce::String oldName, newName;
    juce::File oldFile, newFile;
};

TEST_F(ProgramPanelTest, ConfirmRenamesPresetAfterDialogLeavesModalStack) {
    auto* dialog = openRenameDialog();
    ASSERT_NE(dialog, nullptr);
    juce::Component::SafePointer<juce::AlertWindow> safeDialog(dialog);
    dialog->getTextEditor("presetName")->setText(newName);
    dialog->exitModalState(1);
    EXPECT_FALSE(dialog->isCurrentlyModal());
    dispatchCallbacks();
    EXPECT_FALSE(oldFile.exists());
    EXPECT_TRUE(newFile.existsAsFile());
    EXPECT_EQ(safeDialog.getComponent(), nullptr);
    bool listed = false;
    auto& manager = processor.getPresetManager();
    for (int i = 0; i < manager.getNumPrograms(); ++i)
        listed = listed || manager.getProgramName(i) == newName;
    EXPECT_TRUE(listed);
    EXPECT_EQ(manager.getProgramName(manager.getCurrentProgram()), newName);
}

TEST_F(ProgramPanelTest, CancelPreservesPresetAndAllowsReopening) {
    auto* dialog = openRenameDialog();
    ASSERT_NE(dialog, nullptr);
    dialog->getTextEditor("presetName")->setText(newName);
    dialog->exitModalState(0);
    dispatchCallbacks();
    EXPECT_TRUE(oldFile.existsAsFile());
    EXPECT_FALSE(newFile.exists());
    dialog = openRenameDialog();
    ASSERT_NE(dialog, nullptr);
    EXPECT_EQ(dialog->getTextEditorContents("presetName"), oldName);
    dialog->exitModalState(0);
    dispatchCallbacks();
}

TEST_F(ProgramPanelTest, ClosingEditorDestroysOpenDialog) {
    auto* dialog = openRenameDialog();
    ASSERT_NE(dialog, nullptr);
    juce::Component::SafePointer<juce::AlertWindow> safeDialog(dialog);
    dialog->getTextEditor("presetName")->setText(newName);
    editor.reset();
    EXPECT_EQ(safeDialog.getComponent(), nullptr);
    dispatchCallbacks();
    EXPECT_TRUE(oldFile.existsAsFile());
    EXPECT_FALSE(newFile.exists());
}

TEST_F(ProgramPanelTest, ClosingEditorBeforeConfirmCallbackDoesNotRename) {
    auto* dialog = openRenameDialog();
    ASSERT_NE(dialog, nullptr);
    dialog->getTextEditor("presetName")->setText(newName);
    dialog->exitModalState(1);
    editor.reset();
    dispatchCallbacks();
    EXPECT_TRUE(oldFile.existsAsFile());
    EXPECT_FALSE(newFile.exists());
}