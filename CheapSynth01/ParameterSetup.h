#pragma once
#include "IPlugParameter.h"
#include "ParameterPresentation.h"

namespace cs01 {
struct EnvelopeTimeShape final : iplug::IParam::Shape {
    Shape* Clone() const override {
        return new EnvelopeTimeShape(*this);
    }
    iplug::IParam::EDisplayType GetDisplayType() const override {
        return iplug::IParam::kDisplayLog;
    }
    double NormalizedToValue(double value, const iplug::IParam&) const override {
        return 1000.0 * EGTiming::timeConstant(value);
    }
    double ValueToNormalized(double value, const iplug::IParam&) const override {
        return EGTiming::position(value / 1000.0);
    }
};
inline void initializeHostParameter(iplug::IParam& parameter, Param id) {
    using iplug::IParam;
    const auto& d = definition(id);
    const char* group = parameterGroup(id);
    const int flags = id == Param::PitchBend || id == Param::ModDepth ? IParam::kFlagCannotAutomate
                                                                      : IParam::kFlagsNone;
    switch (id) {
        case Param::WaveType:
            parameter.InitEnum("Wave", 1, {"Triangle", "Sawtooth", "Square", "Pulse", "PWM"}, flags,
                               group);
            return;
        case Param::Feet:
            parameter.InitEnum("Feet", 2, {"32'", "16'", "8'", "4'", "WN"}, flags, group);
            return;
        case Param::LfoTarget:
            parameter.InitEnum("LFO Target", 0, {"VCO", "VCF"}, flags, group);
            return;
        case Param::FilterType:
            parameter.InitEnum("Filter", 0, {"Original", "Modern"}, flags, group);
            return;
        default:
            break;
    }
    const bool milliseconds = hostValueScale(id) == 1000.0;
    const char* label = milliseconds ? (id == Param::Glissando ? "ms/st" : "ms") : d.unit.data();
    const auto unit = milliseconds && id != Param::Glissando ? IParam::kUnitMilliseconds
                      : d.unit == "Hz"                       ? IParam::kUnitFrequency
                      : d.unit == "st"                       ? IParam::kUnitSemitones
                                                             : IParam::kUnitCustom;
    if (d.integer) {
        parameter.InitInt(d.name.data(), static_cast<int>(d.initial), static_cast<int>(d.minimum),
                          static_cast<int>(d.maximum), label, flags, group);
    } else {
        const EnvelopeTimeShape envelopeShape;
        const IParam::ShapePowCurve powerShape(1.0 / d.skew);
        const IParam::Shape& shape = isEnvelopeTime(id)
                                         ? static_cast<const IParam::Shape&>(envelopeShape)
                                         : static_cast<const IParam::Shape&>(powerShape);
        parameter.InitDouble(d.name.data(), toHostValue(id, d.initial), toHostValue(id, d.minimum),
                             toHostValue(id, d.maximum), toHostValue(id, d.step), label, flags,
                             group, shape, unit);
        if (milliseconds || id == Param::Cutoff)
            parameter.SetDisplayPrecision(0);
    }
}
}  // namespace cs01
