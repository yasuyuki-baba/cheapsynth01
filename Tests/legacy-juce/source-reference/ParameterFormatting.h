#pragma once

#include "Parameters.h"

#include <cmath>
#include <cstdlib>

namespace ParameterFormatting {
enum class Style { Frequency, Rate, Time, StepTime, Cents, Percent, SignedPercent, Semitones };

inline Style styleFor(const juce::String& id) {
    if (id == ParameterIds::cutoff)
        return Style::Frequency;
    if (id == ParameterIds::lfoSpeed || id == ParameterIds::pwmSpeed)
        return Style::Rate;
    if (id == ParameterIds::attack || id == ParameterIds::decay || id == ParameterIds::release)
        return Style::Time;
    if (id == ParameterIds::glissando)
        return Style::StepTime;
    if (id == ParameterIds::pitch)
        return Style::Cents;
    if (id == ParameterIds::pitchBend)
        return Style::SignedPercent;
    if (id == ParameterIds::pitchBendUpRange || id == ParameterIds::pitchBendDownRange)
        return Style::Semitones;
    return Style::Percent;
}

inline juce::String format(double value, Style style) {
    if (!std::isfinite(value))
        value = 0.0;
    switch (style) {
        case Style::Frequency:
            value = std::round(value);
            return value < 1000.0 ? juce::String(value, 0) + " Hz"
                                  : juce::String(value / 1000.0, 3) + " kHz";
        case Style::Rate:
            return juce::String(value, 2) + " Hz";
        case Style::Time:
            value = std::round(value * 1000.0);
            return value < 1000.0 ? juce::String(value, 0) + " ms"
                                  : juce::String(value / 1000.0, 3) + " s";
        case Style::StepTime:
            return juce::String(std::round(value * 1000.0), 0) + " ms/st";
        case Style::Semitones:
            return juce::String(std::round(value), 0) + " st";
        default:
            value = std::round(value * 1000.0) / 10.0;
            if (value == 0.0)
                value = 0.0;  // Avoid negative zero after rounding.
            const bool signedValue = style == Style::Cents || style == Style::SignedPercent;
            return (signedValue && value > 0.0 ? "+" : "") + juce::String(value, 1) +
                   (style == Style::Cents ? " cent" : " %");
    }
}

// Bare numbers use the displayed base unit: Hz, ms, cent, %, or st.
// Invalid input returns a caller-selected safe fallback instead of an exceptional value.
inline double parse(const juce::String& text, Style style, double fallback = 0.0) {
    const auto input = text.trim().toLowerCase();
    const char* start = input.toRawUTF8();
    char* end = nullptr;
    const double number = std::strtod(start, &end);
    if (end == start || !std::isfinite(number))
        return fallback;
    const auto unit = juce::String(end).trim();
    switch (style) {
        case Style::Frequency:
        case Style::Rate:
            if (unit.isEmpty() || unit == "hz")
                return number;
            if (unit == "khz")
                return number * 1000.0;
            break;
        case Style::Time:
            if (unit.isEmpty() || unit == "ms")
                return number / 1000.0;
            if (unit == "s")
                return number;
            break;
        case Style::StepTime:
            if (unit.isEmpty() || unit == "ms/st")
                return number / 1000.0;
            if (unit == "s/st")
                return number;
            break;
        case Style::Cents:
            if (unit.isEmpty() || unit == "cent" || unit == "cents")
                return number / 100.0;
            if (unit == "st")
                return number;
            break;
        case Style::Semitones:
            if (unit.isEmpty() || unit == "st")
                return number;
            break;
        case Style::Percent:
        case Style::SignedPercent:
            if (unit.isEmpty() || unit == "%")
                return number / 100.0;
            break;
    }
    return fallback;
}

inline std::unique_ptr<juce::AudioParameterFloat>
makeFloat(const juce::ParameterID& id, const juce::String& name,
          juce::NormalisableRange<float> range, float defaultValue,
          juce::AudioParameterFloatAttributes attributes = {}) {
    const auto style = styleFor(id.getParamID());
    return std::make_unique<juce::AudioParameterFloat>(
        id, name, range, defaultValue,
        attributes
            .withStringFromValueFunction([style](float value, int) { return format(value, style); })
            .withValueFromStringFunction([style, range, defaultValue](const juce::String& text) {
                return range.snapToLegalValue(static_cast<float>(
                    juce::jlimit(static_cast<double>(range.start), static_cast<double>(range.end),
                                 parse(text, style, defaultValue))));
            }));
}

inline std::unique_ptr<juce::AudioParameterInt> makeInt(const juce::ParameterID& id,
                                                        const juce::String& name, int minimum,
                                                        int maximum, int defaultValue) {
    return std::make_unique<juce::AudioParameterInt>(
        id, name, minimum, maximum, defaultValue,
        juce::AudioParameterIntAttributes()
            .withStringFromValueFunction(
                [](int value, int) { return format(value, Style::Semitones); })
            .withValueFromStringFunction([minimum, maximum,
                                          defaultValue](const juce::String& text) {
                return juce::roundToInt(juce::jlimit(static_cast<double>(minimum),
                                                     static_cast<double>(maximum),
                                                     parse(text, Style::Semitones, defaultValue)));
            }));
}
}  // namespace ParameterFormatting