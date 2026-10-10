#pragma once
#include "IControls.h"
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
