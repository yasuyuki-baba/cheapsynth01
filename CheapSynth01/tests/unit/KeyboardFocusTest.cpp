#include "KeyboardFocus.h"
#include <gtest/gtest.h>
#include <X11/Xlib.h>

class KeyboardFocusTest : public ::testing::Test {
   protected:
    void SetUp() override {
        display = XOpenDisplay(nullptr);
        ASSERT_NE(display, nullptr);
        const auto root = DefaultRootWindow(display);
        editor = XCreateSimpleWindow(display, root, 0, 0, 100, 100, 0, 0, 0);
        descendant = XCreateSimpleWindow(display, editor, 0, 0, 80, 80, 0, 0, 0);
        other = XCreateSimpleWindow(display, root, 120, 0, 100, 100, 0, 0, 0);
        XMapWindow(display, editor);
        XMapWindow(display, descendant);
        XMapWindow(display, other);
        XSync(display, False);
    }

    void TearDown() override {
        if (display) {
            XDestroyWindow(display, editor);
            XDestroyWindow(display, other);
            XCloseDisplay(display);
        }
    }

    void focus(Window window) {
        XSetInputFocus(display, window, RevertToParent, CurrentTime);
        XSync(display, False);
    }

    Display* display = nullptr;
    Window editor = None;
    Window descendant = None;
    Window other = None;
};

TEST_F(KeyboardFocusTest, RecognizesEditorAndDescendant) {
    EXPECT_FALSE(cs01::ui::keyboardHasFocus(nullptr));
    focus(editor);
    EXPECT_TRUE(cs01::ui::keyboardHasFocus(reinterpret_cast<void*>(editor)));
    focus(descendant);
    EXPECT_TRUE(cs01::ui::keyboardHasFocus(reinterpret_cast<void*>(editor)));
}

TEST_F(KeyboardFocusTest, RejectsOtherWindowAndPointerRoot) {
    focus(other);
    EXPECT_FALSE(cs01::ui::keyboardHasFocus(reinterpret_cast<void*>(editor)));
    focus(PointerRoot);
    EXPECT_FALSE(cs01::ui::keyboardHasFocus(reinterpret_cast<void*>(editor)));
}