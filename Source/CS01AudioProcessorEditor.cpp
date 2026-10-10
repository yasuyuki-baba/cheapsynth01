#include "CS01AudioProcessor.h"
#include "Utf8Path.h"
#if IPLUG_EDITOR
#include "UI/PanelControls.h"
#include "IVNumberBoxControl.h"
#endif
#if IPLUG_EDITOR
using namespace iplug;
using namespace iplug::igraphics;
namespace {
constexpr int index(cs01::Param id) {
    return static_cast<int>(id);
}
}  // namespace
void CS01AudioProcessor::layoutEditor(IGraphics* g) {
    using namespace cs01::ui;
    using cs01::Param;
    g->AttachPanelBackground(background);
    g->LoadFont("Roboto-Regular", ROBOTO_FN);
    g->AttachControl(new HardwarePanel());
    g->AttachTextEntryControl();
    g->AttachPopupMenuControl();
    g->EnableMouseOver(true);
    g->EnableTooltips(true);
    g->AttachCornerResizer(EUIResizerMode::Scale, false);
    const auto style = DEFAULT_STYLE.WithDrawShadows(false)
                           .WithRoundness(.08f)
                           .WithColor(kBG, background)
                           .WithColor(kFG, handle)
                           .WithColor(kPR, IColor(255, 36, 104, 99))
                           .WithColor(kX1, cyan)
                           .WithColor(kFR, dim)
                           .WithLabelText(text(11))
                           .WithValueText(text(11));
    const auto parameterStyle =
        style.WithLabelOrientation(EOrientation::South)
            .WithLabelText(text(10).WithVAlign(EVAlign::Bottom))
            .WithValueText(text(10).WithFGColor(cyan).WithVAlign(EVAlign::Top))
            .WithWidgetFrac(.85f);
    const auto choiceStyle =
        style.WithValueText(text(10)).WithColor(kX1, ink).WithColor(kON, cyan).WithShowLabel(false);
    auto label = [g](IRECT bounds, const char* title, float size = 11.f) {
        g->AttachControl(new ITextControl(bounds, title, text(size)));
    };
    auto parameterLabel = [g, parameterStyle](IRECT bounds, const char* title) {
        g->AttachControl(new ITextControl(bounds, title, parameterStyle.labelText));
    };
    auto button = [g, style](IRECT bounds, const char* title, IActionFunction action,
                             int tag = kNoTag) {
        return g->AttachControl(new IVButtonControl(bounds, action, title, style), tag);
    };
    auto section = [g](IRECT bounds, const char* title) {
        g->AttachControl(new Section(bounds, title));
    };
    auto fader = [g, parameterStyle](IRECT bounds, Param parameter, const char* title,
                                     int tag = kNoTag) {
        auto* control = new HardwareSlider(bounds.GetPadded(-2.f, 0.f, -2.f, 0.f), index(parameter),
                                           title, parameterStyle, true);
        const auto help = std::string(cs01::definition(parameter).name) +
                          ". Drag or scroll to adjust; Shift/Ctrl for fine adjustment. "
                          "Click the value to type; double-click the slider to reset.";
        control->SetTooltip(help.c_str());
        return g->AttachControl(control, tag);
    };
    auto knob = [g, parameterStyle](IRECT bounds, Param parameter, const char* title) {
        auto* control = new HardwareKnob(bounds, index(parameter), title, parameterStyle, true);
        const auto help = std::string(cs01::definition(parameter).name) +
                          ". Drag or scroll to adjust; Shift/Ctrl for fine adjustment. "
                          "Click the value to type; double-click the knob to reset.";
        control->SetTooltip(help.c_str());
        return g->AttachControl(control);
    };
    auto choices = [g, choiceStyle](IRECT bounds, Param parameter,
                                    std::initializer_list<const char*> options) {
        auto* control = new IVRadioButtonControl(bounds, index(parameter), options, "", choiceStyle,
                                                 EVShape::Rectangle, EDirection::Vertical, 6.f);
        control->SetTooltip(cs01::definition(parameter).name.data());
        return g->AttachControl(control);
    };
    auto number = [g, style](IRECT bounds, Param parameter) {
        const auto& def = cs01::definition(parameter);
        auto* control = new IVNumberBoxControl(bounds, index(parameter), nullptr, "",
                                               style.WithShowLabel(false), true, def.initial,
                                               def.minimum, def.maximum, "%0.0f", false);
        control->SetTooltip(
            "Pitch bend range in semitones. Double-click the value to type a number.");
        g->AttachControl(control);
    };
    // Original 1240x400 panel: header at y=20, common sound columns at y=82.
    g->AttachControl(new BrandMark(IRECT(20, 16, 205, 64)));
    g->AttachControl(new IPanelControl(IRECT(226, 20, 786, 64), background));
    label(IRECT(265, 20, 525, 35), "MEMORY", 10);
    auto display = [this]() {
        return static_cast<PresetDisplay*>(GetUI()->GetControlWithTag(ProgramLabel));
    };
    g->AttachControl(new PresetDisplay(
                         IRECT(265, 36, 525, 59),
                         [this]() {
                             std::vector<std::string> names;
                             for (const auto& program : programs.programs())
                                 names.push_back(program.name);
                             return names;
                         },
                         [this](int selected) {
                             if (programs.setCurrentProgram(selected)) {
                                 syncParameters(true);
                                 updateProgramLabel();
                                 updateFilterControls();
                             }
                         }),
                     ProgramLabel);
    button(IRECT(236, 36, 260, 59), "<", [this](IControl*) {
        if (programs.setCurrentProgram(programs.getCurrentProgram() - 1)) {
            syncParameters(true);
            updateProgramLabel();
            updateFilterControls();
        }
    });
    button(IRECT(530, 36, 554, 59), ">", [this](IControl*) {
        if (programs.setCurrentProgram(programs.getCurrentProgram() + 1)) {
            syncParameters(true);
            updateProgramLabel();
            updateFilterControls();
        }
    });
    button(IRECT(596, 36, 646, 59), "Save", [this, display](IControl*) {
        display()->PromptName("My Preset", [this](const char* name) {
            if (!programs.saveCurrentStateAsPreset(name))
                reportPresetError();
            updateProgramLabel();
        });
    });
    button(
        IRECT(651, 36, 711, 59), "Rename",
        [this, display](IControl*) {
            const auto list = programs.programs();
            display()->PromptName(
                list[programs.getCurrentProgram()].name.c_str(), [this](const char* name) {
                    if (!programs.renameUserPreset(programs.getCurrentProgram(), name))
                        reportPresetError();
                    updateProgramLabel();
                });
        },
        RenamePreset);
    button(
        IRECT(716, 36, 776, 59), "Delete",
        [this](IControl*) {
            if (!programs.deleteUserPreset(programs.getCurrentProgram()))
                reportPresetError();
            else
                syncParameters(true);
            updateProgramLabel();
            updateFilterControls();
        },
        DeletePreset);
    button(IRECT(866, 28, 972, 56), "Import XML", [this](IControl*) {
        GetUI()->PromptForFile(dialogFile, dialogPath, EFileAction::Open, "xml",
                               [this](const WDL_String& file, const WDL_String&) {
                                   if (!file.GetLength())
                                       return;
                                   if (programs.loadPresetFile(cs01::utf8Path(file.Get()))) {
                                       syncParameters(true);
                                       updateFilterControls();
                                   } else
                                       reportPresetError();
                               });
    });
    g->AttachControl(new IVToggleControl(
        IRECT(1000, 20, 1220, 64),
        [this](IControl* control) {
            const bool enabled = control->GetValue() > .5;
            monitorEnabled.store(enabled);
            GetUI()->GetControlWithTag(Keyboard)->Hide(!enabled);
            GetUI()->GetControlWithTag(Scope)->Hide(!enabled);
            GetUI()->Resize(PLUG_WIDTH, enabled ? 640 : 400, GetUI()->GetDrawScale());
        },
        "", style, "KEYBOARD + MONITOR", "KEYBOARD + MONITOR"));

    section(IRECT(20, 82, 144, 231), "BREATH");
    knob(IRECT(28, 105, 136, 164), Param::BreathVcf, "VCF");
    knob(IRECT(28, 164, 136, 223), Param::BreathVca, "VCA");
    section(IRECT(20, 231, 144, 380), "VOLUME");
    knob(IRECT(28, 259, 136, 371), Param::Volume, "MASTER");
    section(IRECT(156, 82, 360, 380), "CONTROL");
    label(IRECT(167, 110, 248, 134), "BEND");
    label(IRECT(269, 110, 350, 134), "MOD");
    fader(IRECT(167, 134, 248, 286), Param::PitchBend, "");
    fader(IRECT(269, 134, 350, 286), Param::ModDepth, "");
    label(IRECT(167, 286, 248, 304), "UP");
    number(IRECT(167, 304, 248, 326), Param::BendUp);
    label(IRECT(167, 326, 248, 344), "DOWN");
    number(IRECT(167, 344, 248, 366), Param::BendDown);
    label(IRECT(269, 286, 350, 308), "TARGET");
    choices(IRECT(269, 308, 350, 364), Param::LfoTarget, {"VCO", "VCF"});

    constexpr float origin = 372, width = 848;
    auto column = [](int left, int right, float top = 112, float bottom = 370) {
        return IRECT(origin + width * left / 14, top, origin + width * right / 14, bottom);
    };
    section(column(0, 1, 82, 380), "LFO");
    section(column(1, 6, 82, 380), "VCO");
    section(column(6, 9, 82, 380), "VCF");
    section(column(9, 10, 82, 380), "VCA");
    section(column(10, 14, 82, 380), "EG");
    fader(column(0, 1), Param::LfoSpeed, "SPEED");
    fader(column(1, 2), Param::Glissando, "GLISS.");
    fader(column(2, 3), Param::Pitch, "PITCH");
    fader(column(3, 4), Param::PwmSpeed, "PWM SPEED");
    choices(column(4, 5, 114, 274), Param::WaveType,
            {"Triangle", "Sawtooth", "Square", "Pulse", "PWM"});
    parameterLabel(column(4, 5, 338, 370), "WAVEFORM");
    choices(column(5, 6, 114, 274), Param::Feet, {"32'", "16'", "8'", "4'", "WN"});
    parameterLabel(column(5, 6, 338, 370), "FEET");
    const float filterX = origin + width * 6 / 14;
    g->AttachControl(new IPanelControl(IRECT(filterX + 48, 82, filterX + 120, 104), background));
    g->AttachControl(new IVTabSwitchControl(IRECT(filterX + 48, 82, filterX + 120, 104),
                                            index(Param::FilterType), {"I", "II"}, "",
                                            choiceStyle));
    fader(column(6, 7), Param::Cutoff, "CUTOFF");
    fader(column(7, 8), Param::Resonance, "", Resonance);
    g->AttachControl(new IVTabSwitchControl(column(7, 8, 195, 259), index(Param::Resonance),
                                            {"LOW", "HIGH"}, "", choiceStyle, EVShape::Rectangle,
                                            EDirection::Vertical),
                     ResonanceHigh);
    parameterLabel(column(7, 8, 338, 370), "RES");
    fader(column(8, 9), Param::VcfEgDepth, "EG DEPTH");
    fader(column(9, 10), Param::VcaEgDepth, "EG DEPTH");
    fader(column(10, 11), Param::Attack, "A");
    fader(column(11, 12), Param::Decay, "D");
    fader(column(12, 13), Param::Sustain, "S");
    fader(column(13, 14), Param::Release, "R");

    auto* keyboard = new IVKeyboardControl(IRECT(20, 420, 808, 620), 41, 72);
    keyboard->SetBlackToWhiteRatios(.6f, .6f);
    g->AttachControl(keyboard, Keyboard)->Hide(true);
    g->AttachControl(new IVScopeControl<1, 512>(IRECT(820, 420, 1220, 620), "Output",
                                                style.WithColor(kFG, cyan)),
                     Scope)
        ->Hide(true);
    g->SetQwertyMidiKeyHandlerFunc([g](const IMidiMsg& message) {
        if (auto* keyboard = g->GetControlWithTag(Keyboard))
            keyboard->OnMidi(message);
    });
    monitorEnabled.store(false);
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
    const bool user = list[programs.getCurrentProgram()].type == PresetType::User;
    GetUI()->GetControlWithTag(RenamePreset)->SetDisabled(!user);
    GetUI()->GetControlWithTag(DeletePreset)->SetDisabled(!user);
}
void CS01AudioProcessor::updateFilterControls() {
    if (!GetUI())
        return;
    const bool modern = GetParam(index(cs01::Param::FilterType))->Int() != 0;
    GetUI()->GetControlWithTag(Resonance)->Hide(!modern);
    GetUI()->GetControlWithTag(ResonanceHigh)->Hide(modern);
    // The two controls overlap and have different bounds. Repaint the background
    // and the complete panel after swapping them so the previous view is erased.
    GetUI()->SetAllControlsDirty();
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
