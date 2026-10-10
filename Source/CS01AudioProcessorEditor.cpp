#include "CS01AudioProcessor.h"
#include "Utf8Path.h"
#if IPLUG_EDITOR
using namespace iplug;
using namespace iplug::igraphics;
namespace {
constexpr int index(cs01::Param id) {
    return static_cast<int>(id);
}
}  // namespace
void CS01AudioProcessor::layoutEditor(IGraphics* g) {
    g->AttachPanelBackground(IColor(255, 32, 34, 37));
    g->LoadFont("Roboto-Regular", ROBOTO_FN);
    g->AttachTextEntryControl();
    g->AttachPopupMenuControl();
    g->EnableMouseOver(true);
    const auto style =
        DEFAULT_STYLE.WithDrawShadows(false).WithColor(kFG, IColor(255, 218, 167, 91));
    const IText text(17, IColor(255, 232, 227, 218), "Roboto-Regular");
    g->AttachControl(new ITextControl(IRECT(20, 12, 1060, 47), "CheapSynth01", text.WithSize(28)));
    g->AttachControl(new IVButtonControl(
        IRECT(20, 55, 70, 90),
        [this](IControl*) {
            const int total = static_cast<int>(programs.programs().size());
            if (programs.setCurrentProgram((programs.getCurrentProgram() + total - 1) % total)) {
                syncParameters(true);
                updateProgramLabel();
            }
        },
        "<", style));
    g->AttachControl(new ITextControl(IRECT(80, 55, 350, 90), "Default", text), ProgramLabel);
    g->AttachControl(new IVButtonControl(
        IRECT(355, 55, 405, 90),
        [this](IControl*) {
            const int total = static_cast<int>(programs.programs().size());
            if (programs.setCurrentProgram((programs.getCurrentProgram() + 1) % total)) {
                syncParameters(true);
                updateProgramLabel();
            }
        },
        ">", style));
    g->AttachControl(new IEditableTextControl(IRECT(420, 55, 650, 90), "My preset", text,
                                              IColor(255, 48, 50, 54)),
                     PresetName);
    g->AttachControl(new IVButtonControl(
        IRECT(660, 55, 735, 90),
        [this](IControl*) {
            auto* name = static_cast<ITextControl*>(GetUI()->GetControlWithTag(PresetName));
            if (!programs.saveCurrentStateAsPreset(name->GetStr()))
                reportPresetError();
        },
        "Save", style));
    g->AttachControl(new IVButtonControl(
        IRECT(745, 55, 825, 90),
        [this](IControl*) {
            auto* name = static_cast<ITextControl*>(GetUI()->GetControlWithTag(PresetName));
            if (!programs.renameUserPreset(programs.getCurrentProgram(), name->GetStr()))
                reportPresetError();
            updateProgramLabel();
        },
        "Rename", style));
    g->AttachControl(new IVButtonControl(
        IRECT(835, 55, 915, 90),
        [this](IControl*) {
            if (!programs.deleteUserPreset(programs.getCurrentProgram()))
                reportPresetError();
            else
                syncParameters(true);
            updateProgramLabel();
        },
        "Delete", style));
    g->AttachControl(new IVButtonControl(
        IRECT(925, 55, 1060, 90),
        [this](IControl*) {
            GetUI()->PromptForFile(dialogFile, dialogPath, EFileAction::Open, "xml",
                                   [this](const WDL_String& file, const WDL_String&) {
                                       if (!file.GetLength())
                                           return;
                                       if (programs.loadPresetFile(cs01::utf8Path(file.Get())))
                                           syncParameters(true);
                                       else
                                           reportPresetError();
                                   });
        },
        "Import XML", style));
    // Every sound/performance parameter remains accessible in the panel.
    for (int i = 0; i < cs01::parameterCount; ++i) {
        const int row = i / 8, column = i % 8;
        const IRECT bounds(20 + column * 130.f, 110 + row * 110.f, 140 + column * 130.f,
                           210 + row * 110.f);
        const auto& d = cs01::parameterDefinitions[i];
        if (i == index(cs01::Param::WaveType) || i == index(cs01::Param::Feet) ||
            i == index(cs01::Param::LfoTarget) || i == index(cs01::Param::FilterType))
            g->AttachControl(new IVMenuButtonControl(bounds, i, d.name.data(), style));
        else {
            auto* control = new IVSliderControl(bounds, i, d.name.data(), style, true);
            g->AttachControl(control, i == index(cs01::Param::Resonance) ? Resonance : kNoTag);
        }
    }
    g->AttachControl(new IVToggleControl(
        IRECT(20, 445, 260, 477),
        [this](IControl* control) {
            const bool enabled = control->GetValue() > 0.5;
            monitorEnabled.store(enabled);
            GetUI()->GetControlWithTag(Keyboard)->Hide(!enabled);
            GetUI()->GetControlWithTag(Scope)->Hide(!enabled);
        },
        "", style, "KEYBOARD + MONITOR", "KEYBOARD + MONITOR"));
    g->AttachControl(new IVKeyboardControl(IRECT(20, 490, 745, 630), 41, 72), Keyboard)->Hide(true);
    g->AttachControl(new IVScopeControl<1, 512>(IRECT(765, 490, 1060, 630), "Output", style), Scope)
        ->Hide(true);
    g->SetQwertyMidiKeyHandlerFunc([g](const IMidiMsg& message) {
        if (auto* keyboard = g->GetControlWithTag(Keyboard))
            keyboard->OnMidi(message);
    });
    updateProgramLabel();
    updateFilterControls();
}
void CS01AudioProcessor::OnUIOpen() {
    programs.refreshUserPresets();
    updateProgramLabel();
    updateFilterControls();
}
void CS01AudioProcessor::OnUIClose() {
    monitorEnabled.store(false);
}
void CS01AudioProcessor::updateProgramLabel() {
    if (!GetUI())
        return;
    auto* label = static_cast<ITextControl*>(GetUI()->GetControlWithTag(ProgramLabel));
    const auto list = programs.programs();
    if (label) {
        label->SetStr(
            list[std::clamp(programs.getCurrentProgram(), 0, static_cast<int>(list.size()) - 1)]
                .name.c_str());
        label->SetDirty(false);
    }
}
void CS01AudioProcessor::updateFilterControls() {
    if (!GetUI())
        return;
    // Original resonance has two effective positions; the label explains its threshold.
    if (auto* control = GetUI()->GetControlWithTag(Resonance)) {
        auto* slider = static_cast<IVSliderControl*>(control);
        slider->SetLabelStr(GetParam(index(cs01::Param::FilterType))->Int() == 0
                                ? "Resonance Low/High"
                                : "Resonance");
        slider->SetDirty(false);
    }
}
void CS01AudioProcessor::OnParamChangeUI(int param, EParamSource) {
    if (param == index(cs01::Param::FilterType))
        updateFilterControls();
}
void CS01AudioProcessor::reportPresetError() {
    if (GetUI())
        GetUI()->ShowMessageBox(
            "Could not update the preset. Choose a user preset and a valid name, or check the XML "
            "file and folder permissions.",
            "CheapSynth01", EMsgBoxType::kMB_OK);
}
#endif
