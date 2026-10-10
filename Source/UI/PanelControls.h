#pragma once
#include "IControls.h"
#include <cmath>
#include <functional>
#include <string>
#include <vector>

namespace cs01::ui {
using namespace iplug;
using namespace iplug::igraphics;
inline const IColor background{255, 43, 43, 43};
inline const IColor cyan{255, 0, 255, 255};
inline const IColor dim{255, 136, 136, 136};
inline IText text(float size = 11.f) {
    return IText(size, COLOR_WHITE, "Roboto-Regular");
}

class Section final : public IControl {
   public:
    Section(IRECT bounds, const char* title) : IControl(bounds), title(title) {
        SetIgnoreMouse(true);
    }
    void Draw(IGraphics& g) override {
        const float titleWidth = static_cast<float>(title.size()) * 9.f + 10.f;
        g.DrawText(text(15).WithAlign(EAlign::Near), title.c_str(),
                   IRECT(mRECT.L, mRECT.T, mRECT.L + titleWidth, mRECT.T + 20));
        g.DrawLine(COLOR_WHITE, mRECT.L + titleWidth, mRECT.T + 10, mRECT.R - 10, mRECT.T + 10,
                   nullptr, 2);
        g.DrawLine(IColor(77, 255, 255, 255), mRECT.R, mRECT.T, mRECT.R, mRECT.B);
    }

   private:
    std::string title;
};

// Relative dragging, fine adjustment and double-click reset use iPlug2's
// parameter control base. Only the original panel's drawing is implemented here.
class Fader final : public IKnobControlBase {
   public:
    Fader(IRECT bounds, int parameter, const char* label, bool wheel = false)
        : IKnobControlBase(bounds, parameter, EDirection::Vertical, 1.0),
          label(label),
          wheel(wheel) {
        DisablePrompt(false);
    }
    void Draw(IGraphics& g) override {
        const float top = mRECT.T + 14, bottom = mRECT.B - 44;
        const float x = mRECT.MW(), y = bottom - GetValue() * (bottom - top);
        if (wheel) {
            g.FillRoundRect(COLOR_BLACK, IRECT(x - 18, top - 3, x + 18, bottom + 3), 5);
            g.FillRoundRect(IColor(255, 72, 72, 72), IRECT(x - 15, top, x + 15, bottom), 4);
            for (float rib = top + 5; rib < bottom - 3; rib += 7)
                g.DrawLine(COLOR_BLACK, x - 12, rib, x + 12, rib);
            g.FillRect(cyan, IRECT(x - 13, y - 1, x + 13, y + 1));
        } else {
            g.FillRect(COLOR_BLACK, IRECT(x - 2, top, x + 2, bottom));
            g.FillRect(IColor(255, 211, 211, 211), IRECT(x - 8, y - 12, x + 8, y + 12));
            g.FillRect(dim, IRECT(x - 6, y - 2, x + 6, y + 2));
            g.FillRect(cyan, IRECT(x - 8, y - 1, x + 8, y + 1));
        }
        g.DrawText(text(10), label.c_str(), IRECT(mRECT.L, mRECT.B - 32, mRECT.R, mRECT.B));
        if (GetMouseIsOver()) {
            WDL_String value;
            GetParam()->GetDisplayWithLabel(value);
            g.DrawText(text().WithFGColor(cyan), value.Get(),
                       IRECT(mRECT.L - 15, mRECT.T - 6, mRECT.R + 15, mRECT.T + 10));
        }
    }

   private:
    std::string label;
    bool wheel;
};

class Knob final : public IKnobControlBase {
   public:
    Knob(IRECT bounds, int parameter, const char* label)
        : IKnobControlBase(bounds, parameter), label(label) {
        DisablePrompt(false);
    }
    void Draw(IGraphics& g) override {
        const float radius = std::min(mRECT.W(), mRECT.H() - 20) * .35f;
        const float x = mRECT.MW(), y = (mRECT.T + mRECT.B - 20) * .5f;
        constexpr double start = -2.35, span = 4.7;
        for (int i = 0; i <= 10; ++i) {
            const double a = start + span * i / 10;
            g.DrawLine(COLOR_WHITE, x + std::sin(a) * (radius + 4), y - std::cos(a) * (radius + 4),
                       x + std::sin(a) * (radius + 8), y - std::cos(a) * (radius + 8));
        }
        g.FillCircle(IColor(255, 17, 17, 17), x, y, radius);
        g.FillCircle(IColor(255, 28, 28, 28), x, y, std::max(1.f, radius - 3));
        const double a = start + span * GetValue();
        g.DrawLine(cyan, x, y, x + std::sin(a) * radius * .9, y - std::cos(a) * radius * .9,
                   nullptr, 3);
        WDL_String value;
        GetParam()->GetDisplayWithLabel(value);
        g.DrawText(text(), GetMouseIsOver() ? value.Get() : label.c_str(),
                   IRECT(mRECT.L, mRECT.B - 20, mRECT.R, mRECT.B));
    }

   private:
    std::string label;
};

class Choices final : public IControl {
   public:
    Choices(IRECT bounds, int parameter, std::initializer_list<const char*> labels,
            bool horizontal = false)
        : IControl(bounds, parameter), labels(labels), horizontal(horizontal) {}
    void Draw(IGraphics& g) override {
        for (size_t i = 0; i < labels.size(); ++i) {
            const auto row = cell(i);
            const float x = row.L + 3, y = row.MH();
            g.FillRect(COLOR_BLACK, IRECT(x, y - 6, x + 14, y + 6));
            const int selected = static_cast<int>(std::lround(GetValue() * (labels.size() - 1)));
            g.FillRect(selected == static_cast<int>(i) ? cyan : dim,
                       IRECT(x + 1, y - 5, x + 13, y + 5));
            g.DrawText(text(row.W() < 80 ? 10.f : 11.f).WithAlign(EAlign::Near), labels[i],
                       IRECT(x + 18, row.T, row.R, row.B));
        }
    }
    void OnMouseDown(float x, float y, const IMouseMod& mod) override {
        if (mod.R) {
            IControl::OnMouseDown(x, y, mod);
            return;
        }
        for (size_t i = 0; i < labels.size(); ++i)
            if (cell(i).Contains(x, y)) {
                SetValue(static_cast<double>(i) / (labels.size() - 1));
                SetDirty();
                break;
            }
    }

   private:
    IRECT cell(size_t i) const {
        if (horizontal) {
            const float width = mRECT.W() / labels.size();
            return IRECT(mRECT.L + i * width, mRECT.T, mRECT.L + (i + 1) * width, mRECT.B);
        }
        const float height = mRECT.H() / labels.size();
        return IRECT(mRECT.L, mRECT.T + i * height, mRECT.R, mRECT.T + (i + 1) * height);
    }
    std::vector<const char*> labels;
    bool horizontal;
};

class ResonanceSwitch final : public IControl {
   public:
    ResonanceSwitch(IRECT bounds, int parameter) : IControl(bounds, parameter) {}
    void Draw(IGraphics& g) override {
        g.FillRect(COLOR_BLACK, IRECT(mRECT.L + 3, mRECT.MH() - 6, mRECT.L + 17, mRECT.MH() + 6));
        g.FillRect(GetValue() >= .5 ? cyan : dim,
                   IRECT(mRECT.L + 4, mRECT.MH() - 5, mRECT.L + 16, mRECT.MH() + 5));
        g.DrawText(text().WithAlign(EAlign::Near), "HIGH",
                   IRECT(mRECT.L + 21, mRECT.T, mRECT.R, mRECT.B));
    }
    void OnMouseDown(float, float, const IMouseMod&) override {
        SetValue(GetValue() >= .5 ? .2 : .7);
        SetDirty();
    }
};

class BendRange final : public IControl {
   public:
    BendRange(IRECT bounds, int parameter) : IControl(bounds, parameter) {
        DisablePrompt(false);
    }
    void Draw(IGraphics& g) override {
        g.FillRect(COLOR_BLACK, mRECT);
        WDL_String value;
        value.SetFormatted(16, "%d", static_cast<int>(GetParam()->FromNormalized(GetValue())));
        g.DrawText(text(12), value.Get(), mRECT);
        g.DrawText(text(14), "-", IRECT(mRECT.L, mRECT.T, mRECT.L + 20, mRECT.B));
        g.DrawText(text(14), "+", IRECT(mRECT.R - 20, mRECT.T, mRECT.R, mRECT.B));
    }
    void OnMouseDown(float x, float y, const IMouseMod& mod) override {
        if (x > mRECT.L + 20 && x < mRECT.R - 20) {
            PromptUserInput();
            return;
        }
        SetValue(GetParam()->ToNormalized(GetParam()->FromNormalized(GetValue()) +
                                          (x < mRECT.MW() ? -1 : 1)));
        SetDirty();
    }
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
