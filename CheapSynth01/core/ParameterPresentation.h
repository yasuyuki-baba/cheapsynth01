#pragma once
#include "Parameters.h"

namespace cs01 {
// Host units are chosen for editing. DSP and XML values remain in core units.
constexpr double hostValueScale(Param parameter) {
    switch (parameter) {
        case Param::Attack:
        case Param::Decay:
        case Param::Release:
        case Param::Glissando:
            return 1000.0;
        default:
            return 1.0;
    }
}
constexpr double toHostValue(Param parameter, double value) {
    return value * hostValueScale(parameter);
}
constexpr double fromHostValue(Param parameter, double value) {
    return value / hostValueScale(parameter);
}
constexpr const char* parameterGroup(Param parameter) {
    switch (parameter) {
        case Param::WaveType:
        case Param::Feet:
        case Param::PwmSpeed:
        case Param::Pitch:
        case Param::Glissando:
            return "VCO";
        case Param::Cutoff:
        case Param::Resonance:
        case Param::VcfEgDepth:
        case Param::BreathVcf:
        case Param::FilterType:
            return "VCF";
        case Param::VcaEgDepth:
        case Param::BreathVca:
        case Param::Volume:
            return "VCA";
        case Param::Oversampling:
            return "Quality";
        case Param::Attack:
        case Param::Decay:
        case Param::Sustain:
        case Param::Release:
            return "EG";
        case Param::LfoSpeed:
        case Param::LfoTarget:
            return "LFO";
        default:
            return "Performance";
    }
}
}  // namespace cs01
