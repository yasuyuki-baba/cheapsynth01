#include "interaction/EditorSession.h"
#include <gtest/gtest.h>
#include <memory>

TEST(EditorSessionTest, CloseAndReopenCancelOldDialogsWithoutCancelingNewOnes) {
    cs01::EditorSession editor;
    int saves = 0;
    editor.begin();
    auto oldDialog = editor.wrap([&](int amount) { saves += amount; });
    oldDialog(1);
    EXPECT_EQ(saves, 1);
    editor.end();
    oldDialog(10);
    EXPECT_EQ(saves, 1);
    editor.begin();
    oldDialog(10);
    auto newDialog = editor.wrap([&](int amount) { saves += amount; });
    newDialog(2);
    EXPECT_EQ(saves, 3);
    editor.begin();  // Replacing the control hierarchy also cancels pending replies.
    newDialog(20);
    EXPECT_EQ(saves, 3);
}

TEST(EditorSessionTest, DestroyedOwnerIsNotDereferencedAndInstancesRemainIndependent) {
    struct Owner {
        cs01::EditorSession editor;
        int changes = 0;
    };
    auto first = std::make_unique<Owner>();
    Owner second;
    first->editor.begin();
    second.editor.begin();
    auto firstDialog = first->editor.wrap([owner = first.get()] { ++owner->changes; });
    auto secondDialog = second.editor.wrap([&] { ++second.changes; });
    first.reset();
    firstDialog();  // ASan would detect a stale raw-owner dereference.
    secondDialog();
    EXPECT_EQ(second.changes, 1);
}
