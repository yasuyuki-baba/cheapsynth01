#include "KeyboardFocus.h"
#if defined(__APPLE__)
#import <AppKit/AppKit.h>
#elif defined(_WIN32)
#include <windows.h>
#endif
namespace cs01::ui {
bool keyboardHasFocus(void* view) {
    if (!view)
        return false;
#if defined(__APPLE__)
    auto* editor = static_cast<NSView*>(view);
    NSWindow* window = [editor window];
    if (![NSApp isActive] || ![window isKeyWindow])
        return false;
    NSResponder* responder = [window firstResponder];
    return [responder isKindOfClass:[NSView class]] &&
           (responder == editor || [(NSView*)responder isDescendantOf:editor]);
#elif defined(_WIN32)
    const auto editor = static_cast<HWND>(view);
    const auto focus = GetFocus();
    return focus == editor || (focus && IsChild(editor, focus));
#else
    return false;
#endif
}
}  // namespace cs01::ui
