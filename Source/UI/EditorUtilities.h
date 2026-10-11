#pragma once
#include "IControls.h"
#include "IVMeterControl.h"
#include "UI/PanelControls.h"
#include <cmath>
#include <string>

namespace cs01::ui {
class OutputMeterControl final : public IVPeakAvgMeterControl<1> {
   public:
    OutputMeterControl(IRECT bounds, const IVStyle& style)
        : IVPeakAvgMeterControl(bounds, "OUT", style, EDirection::Vertical, {}, 0, -60.f, 0.f,
                                {0, -12, -24, -48}) {
        // The pinned SDK's peak array is not initialized by its constructor.
        mPeakValues.fill(0.f);
        SetTooltip("Output RMS level and held peak. Red indicates a peak above 0 dBFS.");
    }
};

class EditorMenuControl final : public IVButtonControl {
   public:
    EditorMenuControl(IRECT bounds, const IVStyle& style, std::string about)
        : IVButtonControl(bounds, nullptr, "Menu", style), about(std::move(about)) {
        SetTooltip("Display scale, help, and version information.");
    }
    void OnMouseDown(float, float, const IMouseMod&) override {
        menu.Clear();
        auto* scaleMenu = new IPopupMenu();
        for (int percent : {75, 100, 125, 150, 175, 200}) {
            const auto label = std::to_string(percent) + "%";
            const bool current = std::abs(GetUI()->GetDrawScale() * 100.f - percent) < .5f;
            scaleMenu->AddItem(new IPopupMenu::Item(
                label.c_str(), current ? IPopupMenu::Item::kChecked : IPopupMenu::Item::kNoFlags,
                percent));
        }
        const auto label =
            "Display scale (" + std::to_string(std::lround(GetUI()->GetDrawScale() * 100.f)) + "%)";
        menu.AddItem(label.c_str(), scaleMenu);
        menu.AddItem(new IPopupMenu::Item("User guide", 0, 1));
        menu.AddItem(new IPopupMenu::Item("Project website", 0, 2));
        menu.AddItem(new IPopupMenu::Item("Report an issue", 0, 3));
        menu.AddItem(new IPopupMenu::Item("About CheapSynth01", 0, 4));
        GetUI()->CreatePopupMenu(*this, menu, mRECT);
    }
    void OnPopupMenuSelection(IPopupMenu* selected, int) override {
        const auto* item = selected ? selected->GetChosenItem() : nullptr;
        if (!item)
            return;
        const int tag = item->GetTag();
        if (tag >= 75 && tag <= 200) {
            GetUI()->Resize(GetUI()->Width(), GetUI()->Height(), tag / 100.f);
            return;
        }
        switch (tag) {
            case 1:
                GetUI()->OpenURL(
                    "https://github.com/yasuyuki-baba/cheapsynth01/blob/main/docs/User-guide.md");
                break;
            case 2:
                GetUI()->OpenURL("https://github.com/yasuyuki-baba/cheapsynth01");
                break;
            case 3:
                GetUI()->OpenURL("https://github.com/yasuyuki-baba/cheapsynth01/issues");
                break;
            case 4:
                GetUI()->ShowMessageBox(about.c_str(), "About CheapSynth01", EMsgBoxType::kMB_OK);
                break;
            default:
                break;
        }
    }

   private:
    std::string about;
    IPopupMenu menu;
};
}  // namespace cs01::ui
