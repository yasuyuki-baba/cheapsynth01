#pragma once
#include "IControls.h"
#include "ProgramManager.h"
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
        // Outline lettering retains the product name, with round joins and upright strokes.
        const float scale = (mRECT.W() - 12.f) / 212.f;
        struct Outline {
            std::vector<std::pair<float, float>> points;
            bool closed;
            bool striped;
        };
        std::vector<Outline> outlines;
        float offset = 0.f;
        bool striped = false;
        auto stroke = [&](std::initializer_list<std::pair<float, float>> points,
                          bool closed = false) {
            Outline outline{{}, closed, striped};
            for (const auto& [x, y] : points)
                outline.points.emplace_back(mRECT.L + 5.f + (offset + x) * scale,
                                            mRECT.T + 7.f + y * .85f);
            outlines.push_back(std::move(outline));
        };
        for (char letter : std::string("CheapSynth01")) {
            striped = offset >= 90.f && offset < 180.f;
            switch (letter) {
                case 'C':
                    stroke({{12, 0}, {2, 0}, {0, 2}, {0, 26}, {2, 28}, {12, 28}});
                    break;
                case 'h':
                    stroke({{0, 0}, {0, 28}});
                    stroke({{0, 12}, {2, 10}, {10, 10}, {12, 12}, {12, 28}});
                    break;
                case 'e':
                    stroke({{12, 26},
                            {10, 28},
                            {2, 28},
                            {0, 26},
                            {0, 12},
                            {2, 10},
                            {10, 10},
                            {12, 12},
                            {12, 19},
                            {0, 19}});
                    break;
                case 'a':
                    stroke({{0, 10}, {10, 10}, {12, 12}, {12, 28}});
                    stroke({{12, 18}, {2, 18}, {0, 20}, {0, 26}, {2, 28}, {10, 28}, {12, 26}});
                    break;
                case 'p':
                    stroke({{0, 10}, {0, 34}});
                    stroke({{0, 12},
                            {2, 10},
                            {10, 10},
                            {12, 12},
                            {12, 26},
                            {10, 28},
                            {2, 28},
                            {0, 26}});
                    break;
                case 'S':
                    stroke({{12, 0},
                            {2, 0},
                            {0, 2},
                            {0, 12},
                            {2, 14},
                            {10, 14},
                            {12, 16},
                            {12, 26},
                            {10, 28},
                            {0, 28}});
                    break;
                case 'y':
                    stroke({{0, 10}, {0, 24}, {2, 26}, {12, 26}});
                    stroke({{12, 10}, {12, 32}, {10, 34}, {0, 34}});
                    break;
                case 'n':
                    stroke({{0, 28}, {0, 10}});
                    stroke({{0, 12}, {2, 10}, {10, 10}, {12, 12}, {12, 28}});
                    break;
                case 't':
                    stroke({{4, 4}, {4, 26}, {6, 28}, {12, 28}});
                    stroke({{0, 10}, {12, 10}});
                    break;
                case '0':
                    stroke({{2, 0}, {10, 0}, {12, 2}, {12, 26}, {10, 28}, {2, 28}, {0, 26}, {0, 2}},
                           true);
                    break;
                case '1':
                    stroke({{2, 4}, {6, 0}, {8, 0}, {8, 28}});
                    stroke({{2, 28}, {14, 28}});
                    break;
            }
            offset += 18.f;
        }
        IStrokeOptions options;
        options.mJoinOption = ELineJoin::Round;
        // Draw all outer strokes before the inset strokes, so letter junctions remain open.
        for (bool inset : {false, true}) {
            options.mPreserve = false;
            for (const auto& outline : outlines) {
                if (inset && outline.striped)
                    continue;
                g.PathClear();
                bool first = true;
                for (const auto& [x, y] : outline.points) {
                    if (first)
                        g.PathMoveTo(x, y);
                    else
                        g.PathLineTo(x, y);
                    first = false;
                }
                if (outline.closed)
                    g.PathClose();
                g.PathStroke(inset ? handle : ink, (inset ? 3.f : 5.f) * scale, options);
            }
        }
        // Fine horizontal cuts through the filled Synth letters echo the hardware's S.
        for (float y = mRECT.T + 10.f; y < mRECT.T + 37.f; y += 4.f)
            g.FillRect(handle, IRECT(mRECT.L + 5.f + 88.5f * scale, y,
                                     mRECT.L + 5.f + 178.f * scale, y + .8f));
        g.DrawText(text(8), "MICRO MONOPHONIC SYNTHESIZER",
                   IRECT(mRECT.L, mRECT.T + 38.f, mRECT.R, mRECT.B));
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
        g.FillRect(background, IRECT(0, 72, 1240, 392));
        g.DrawLine(cyan, 20, 68, 1220, 68, nullptr, 2);
        // The optional keyboard/monitor drawer continues the casing below the panel.
        g.FillRect(background, IRECT(0, 408, 1240, 640));
        g.DrawLine(bezel, 0, 408, 1240, 408, nullptr, 2);
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
    PresetDisplay(IRECT bounds, std::function<std::vector<Program>()> programs,
                  std::function<Program()> current, std::function<void(const Program&)> select)
        : ITextControl(bounds, "Default", text(13), COLOR_BLACK),
          programs(std::move(programs)),
          current(std::move(current)),
          select(std::move(select)) {
        SetIgnoreMouse(false);
        SetTooltip("Click to choose from all Factory and User presets.");
    }
    void Draw(IGraphics& g) override {
        g.FillRect(COLOR_BLACK, mRECT);
        g.DrawRect(mMouseIsOver ? cyan : dim, mRECT);
        g.DrawText(mText.WithAlign(EAlign::Near), GetStr(),
                   IRECT(mRECT.L + 8.f, mRECT.T, mRECT.R - 28.f, mRECT.B));
        const float x = mRECT.R - 14.f;
        const float y = mRECT.MH();
        g.DrawLine(cyan, x - 4.f, y - 2.f, x, y + 2.f);
        g.DrawLine(cyan, x, y + 2.f, x + 4.f, y - 2.f);
    }
    void OnMouseDown(float, float, const IMouseMod&) override {
        menu.Clear();
        snapshot = programs();
        const auto selected = current();
        for (const auto type : {PresetType::Factory, PresetType::User}) {
            menu.AddItem(new IPopupMenu::Item(type == PresetType::Factory ? "Factory" : "User",
                                              IPopupMenu::Item::kDisabled, -1));
            bool hasPresets = false;
            for (size_t i = 0; i < snapshot.size(); ++i) {
                const auto& program = snapshot[i];
                if (program.type != type)
                    continue;
                hasPresets = true;
                const bool checked =
                    program.type == selected.type && program.filename == selected.filename;
                menu.AddItem(new IPopupMenu::Item(program.name.c_str(),
                                                  checked ? IPopupMenu::Item::kChecked
                                                          : IPopupMenu::Item::kNoFlags,
                                                  static_cast<int>(i)));
            }
            if (!hasPresets)
                menu.AddItem(new IPopupMenu::Item("No presets", IPopupMenu::Item::kDisabled, -1));
        }
        GetUI()->CreatePopupMenu(*this, menu, mRECT);
    }
    void OnPopupMenuSelection(IPopupMenu* selected, int) override {
        const auto* item = selected ? selected->GetChosenItem() : nullptr;
        if (item && item->GetTag() >= 0 && item->GetTag() < static_cast<int>(snapshot.size()))
            select(snapshot[item->GetTag()]);
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
    std::function<std::vector<Program>()> programs;
    std::function<Program()> current;
    std::function<void(const Program&)> select;
    std::vector<Program> snapshot;
    NameAction nameAction;
    IPopupMenu menu;
};
}  // namespace cs01::ui
