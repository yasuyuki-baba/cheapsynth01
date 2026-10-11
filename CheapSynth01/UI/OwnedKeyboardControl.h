#pragma once
#include "IControls.h"
#include "UI/KeyboardInput.h"

namespace cs01::ui {
// Retains iPlug2's vector keyboard drawing and geometry. Mouse events publish
// plugin-owned UI notes instead of sending indistinguishable host MIDI events.
class OwnedKeyboardControl final : public iplug::igraphics::IVKeyboardControl {
   public:
    OwnedKeyboardControl(const iplug::igraphics::IRECT& bounds, UiNoteBridge& notes)
        : IVKeyboardControl(bounds, 41, 72), notes(notes) {
        SetWantsMidi(false);
    }
    void OnMouseDown(float x, float y, const iplug::igraphics::IMouseMod&) override {
        touch(x, y);
    }
    void OnMouseDrag(float x, float y, float, float, const iplug::igraphics::IMouseMod&) override {
        touch(x, y);
    }
    void OnMouseUp(float, float, const iplug::igraphics::IMouseMod&) override {
        releaseMouse();
    }
    void OnMouseOut() override {
        releaseMouse();
    }
    void resetMouse() {
        mLastTouchedKey = mMouseOverKey = -1;
        mLastVelocity = 0;
        refresh();
    }
    void refresh() {
        for (int note = mMinNote; note <= mMaxNote; ++note) {
            const bool pressed = notes.displayed(note);
            if (mPressedKeys.Get()[note - mMinNote] != pressed)
                SetNoteFromMidi(note, pressed);
        }
    }

   private:
    void releaseMouse() {
        if (mLastTouchedKey >= 0)
            notes.set(GetMidiNoteNumberForKey(mLastTouchedKey), 0, UiNoteBridge::Mouse);
        resetMouse();
    }
    void touch(float x, float y) {
        const int key = mRECT.Contains(x, y) ? hitKey(x, y) : -1;
        if (key == mLastTouchedKey)
            return;
        releaseMouse();
        if (key < 0 || key > mMaxNote - mMinNote)
            return;
        mLastTouchedKey = mMouseOverKey = key;
        const float height = mRECT.H() * (mIsBlackKeyList.Get()[key] ? mBKHeightRatio : 1.f);
        mLastVelocity = std::clamp((y - mRECT.T) / (.95f * height), 1.f / 127.f, 1.f);
        notes.set(GetMidiNoteNumberForKey(key), std::max(1, static_cast<int>(mLastVelocity * 127)),
                  UiNoteBridge::Mouse);
        refresh();
    }
    UiNoteBridge& notes;
    int hitKey(float x, float y) const {
        const int count = mMaxNote - mMinNote + 1;
        // Black keys take priority where their rectangles overlap white keys.
        for (int pass = 0; pass < 2; ++pass)
            for (int key = 0; key < count; ++key) {
                const bool black = mIsBlackKeyList.Get()[key];
                if (black != (pass == 0))
                    continue;
                const float left = mKeyXPos.Get()[key];
                const float width = mWKWidth * (black ? mBKWidthRatio : 1.f);
                const float bottom = mRECT.T + mRECT.H() * (black ? mBKHeightRatio : 1.f);
                if (x >= left && x < left + width && y <= bottom)
                    return key;
            }
        return -1;
    }
};
}  // namespace cs01::ui
