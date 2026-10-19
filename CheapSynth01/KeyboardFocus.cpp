#include "KeyboardFocus.h"
#if defined(__APPLE__)
#import <AppKit/AppKit.h>
#elif defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <memory>
#include <X11/Xlib.h>
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
#elif defined(__linux__)
    static std::unique_ptr<Display, decltype(&XCloseDisplay)> display{XOpenDisplay(nullptr),
                                                                      XCloseDisplay};
    if (!display)
        return false;
    const auto editor = reinterpret_cast<Window>(view);
    Window focus;
    int revertTo;
    XGetInputFocus(display.get(), &focus, &revertTo);
    while (focus != None && focus != PointerRoot) {
        if (focus == editor)
            return true;
        Window root;
        Window parent;
        Window* children = nullptr;
        unsigned int childCount;
        const auto found = XQueryTree(display.get(), focus, &root, &parent, &children, &childCount);
        if (children)
            XFree(children);
        if (!found)
            break;
        focus = parent;
    }
    return false;
#else
    return false;
#endif
}
}  // namespace cs01::ui
