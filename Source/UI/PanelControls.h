#pragma once
#include "IControls.h"
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace cs01::ui {
using namespace iplug;
using namespace iplug::igraphics;
inline const IColor background{255, 48, 52, 52};
inline const IColor cyan{255, 92, 199, 190};
inline const IColor dim{255, 130, 139, 137};
inline const IColor ink{255, 231, 235, 225};
inline const IColor bezel{255, 154, 162, 159};
inline const IColor handle{255, 23, 27, 27};
inline IText text(float size = 11.f) {
    return IText(size, ink, "Roboto-Regular");
}

class BrandMark final : public IControl {
   public:
    explicit BrandMark(IRECT bounds) : IControl(bounds) {
        SetIgnoreMouse(true);
    }
    void Draw(IGraphics& g) override {
        // Original geometric lettering, drawn as vectors so it scales with the editor.
        const float scale = (mRECT.W() - 8.f) / 174.f;
        auto stroke = [&](std::initializer_list<std::pair<float, float>> points,
                          bool closed = false) {
            g.PathClear();
            bool first = true;
            for (const auto& [x, y] : points) {
                const float px = mRECT.L + 4.f + x * scale;
                const float py = mRECT.T + 3.f + y;
                if (first)
                    g.PathMoveTo(px, py);
                else
                    g.PathLineTo(px, py);
                first = false;
            }
            if (closed)
                g.PathClose();
            g.PathStroke(ink, 5.f);
        };
        stroke({{40, 0}, {8, 0}, {2, 6}, {2, 22}, {8, 28}, {40, 28}});
        stroke({{86, 0},
                {52, 0},
                {46, 6},
                {46, 9},
                {52, 14},
                {80, 14},
                {86, 20},
                {86, 22},
                {80, 28},
                {46, 28}});
        stroke({{100, 0}, {132, 0}, {138, 6}, {138, 22}, {132, 28}, {100, 28}, {94, 22}, {94, 6}},
               true);
        stroke({{150, 6}, {156, 0}, {160, 0}, {160, 28}});
        stroke({{148, 28}, {174, 28}});
        g.DrawText(text(10), "CheapSynth01", IRECT(mRECT.L, mRECT.T + 35.f, mRECT.R, mRECT.B));
    }
};

// Decorative layers and drawing overrides only: parameter interaction stays in iPlug2.
class HardwarePanel final : public IControl {
   public:
    HardwarePanel() : IControl(IRECT(0, 0, 1240, 640)) {
        SetIgnoreMouse(true);
    }
    void Draw(IGraphics& g) override {
        g.FillRect(handle, mRECT);
        g.FillRoundRect(bezel, IRECT(8, 72, 1232, 392), 4);
        g.FillRect(background, IRECT(18, 80, 1222, 382));
        g.DrawLine(ink, 10, 74, 1230, 74);
        g.DrawLine(IColor(255, 76, 84, 82), 10, 390, 1230, 390);
        g.DrawLine(cyan, 20, 68, 1220, 68, nullptr, 2);
        for (float x : {13.f, 1227.f}) {
            for (float y : {78.f, 386.f}) {
                g.FillCircle(handle, x, y, 2.f);
                g.DrawLine(dim, x - 1, y, x + 1, y);
            }
        }
        // The optional keyboard/monitor drawer continues the casing below the panel.
        g.FillRect(background, IRECT(8, 408, 1232, 632));
        g.DrawLine(bezel, 8, 408, 1232, 408, nullptr, 2);
    }
};

class HardwareSlider final : public IVSliderControl {
   public:
    using IVSliderControl::IVSliderControl;
    void DrawTrack(IGraphics& g, const IRECT&) override {
        const float x = mTrackBounds.MW();
        g.FillRect(handle, IRECT(x - 3, mTrackBounds.T - 3, x + 3, mTrackBounds.B + 3), &mBlend);
        g.DrawLine(dim, x + 3, mTrackBounds.T, x + 3, mTrackBounds.B, &mBlend);
        for (int i = 0; i <= 10; ++i) {
            const float y = mTrackBounds.T + mTrackBounds.H() * i / 10.f;
            const float length = i % 5 == 0 ? 7.f : 4.f;
            g.DrawLine(dim, x - 10 - length, y, x - 10, y, &mBlend);
            g.DrawLine(dim, x + 10, y, x + 10 + length, y, &mBlend);
        }
    }
    void DrawHandle(IGraphics& g, const IRECT& bounds) override {
        const auto cap = bounds.GetCentredInside(24.f, 14.f);
        g.FillRoundRect(handle, cap, 1.f, &mBlend);
        g.DrawRoundRect(mMouseIsOver ? ink : dim, cap, 1.f, &mBlend);
        g.FillRect(cyan, cap.GetMidVPadded(2.f).GetHPadded(-2.f), &mBlend);
        g.DrawLine(IColor(255, 68, 76, 74), cap.L + 2, cap.T + 2, cap.R - 2, cap.T + 2, &mBlend);
    }
};

class HardwareKnob final : public IVKnobControl {
   public:
    using IVKnobControl::IVKnobControl;
    void DrawHandle(IGraphics& g, const IRECT& bounds) override {
        g.FillEllipse(handle, bounds, &mBlend);
        g.DrawEllipse(mMouseIsOver ? ink : dim, bounds, &mBlend);
        g.DrawEllipse(IColor(255, 66, 74, 72), bounds.GetPadded(-2.f), &mBlend);
    }
    void DrawIndicatorTrack(IGraphics& g, float, float x, float y, float radius) override {
        for (int i = 0; i <= 10; ++i)
            g.DrawRadialLine(dim, x, y, mAngle1 + (mAngle2 - mAngle1) * i / 10.f, radius - 1.f,
                             radius + 1.f, &mBlend);
    }
    void DrawPointer(IGraphics& g, float angle, float x, float y, float radius) override {
        g.DrawRadialLine(ink, x, y, angle, radius * .45f, radius * .9f, &mBlend, 2.f);
    }
};

class Section final : public IControl {
   public:
    Section(IRECT bounds, const char* title) : IControl(bounds), title(title) {
        SetIgnoreMouse(true);
    }
    void Draw(IGraphics& g) override {
        g.DrawText(text(15).WithAlign(EAlign::Near), title.c_str(),
                   IRECT(mRECT.L + 4, mRECT.T, mRECT.R, mRECT.T + 20));
        g.DrawLine(cyan, mRECT.L + 4, mRECT.T + 22, mRECT.R - 4, mRECT.T + 22);
        g.DrawLine(dim, mRECT.R, mRECT.T, mRECT.R, mRECT.B);
        g.DrawLine(dim, mRECT.L + 4, mRECT.B, mRECT.R - 4, mRECT.B);
    }

   private:
    std::string title;
};

class PresetDisplay final : public ITextControl {
   public:
    using NameAction = std::function<void(const char*)>;
    PresetDisplay(IRECT bounds, std::function<std::vector<std::string>()> names,
                  std::function<void(int)> select)
        : ITextControl(bounds, "Default", text(13), COLOR_BLACK),
          names(std::move(names)),
          select(std::move(select)) {}
    void OnMouseDown(float, float, const IMouseMod&) override {
        menu.Clear();
        for (const auto& name : names())
            menu.AddItem(name.c_str());
        GetUI()->CreatePopupMenu(*this, menu, mRECT);
    }
    void OnPopupMenuSelection(IPopupMenu* selected, int) override {
        if (selected && selected->GetChosenItemIdx() >= 0)
            select(selected->GetChosenItemIdx());
    }
    void PromptName(const char* initial, NameAction action) {
        nameAction = std::move(action);
        GetUI()->CreateTextEntry(*this, text(13), mRECT, initial);
    }
    void OnTextEntryCompletion(const char* name, int) override {
        auto action = std::move(nameAction);
        if (action && name && *name)
            action(name);
    }

   private:
    std::function<std::vector<std::string>()> names;
    std::function<void(int)> select;
    NameAction nameAction;
    IPopupMenu menu;
};
}  // namespace cs01::ui
