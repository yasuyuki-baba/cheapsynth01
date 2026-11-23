#pragma once
#include <algorithm>
#include <cmath>
#include <optional>

namespace cs01::ui {
inline constexpr float minimumEditorScale = .75f;
inline constexpr float maximumEditorScale = 2.f;

// Host sizes include the platform coordinate scale; canvas sizes do not.
// Fit both axes so a host with a different aspect ratio cannot stretch controls.
inline std::optional<float> fitEditorScale(int hostWidth, int hostHeight, int canvasWidth,
                                           int canvasHeight, float platformScale) {
    if (hostWidth <= 0 || hostHeight <= 0 || canvasWidth <= 0 || canvasHeight <= 0 ||
        !std::isfinite(platformScale) || platformScale <= 0.f)
        return std::nullopt;
    const double widthScale = hostWidth / (static_cast<double>(canvasWidth) * platformScale);
    const double heightScale = hostHeight / (static_cast<double>(canvasHeight) * platformScale);
    return static_cast<float>(std::clamp(std::min(widthScale, heightScale),
                                         static_cast<double>(minimumEditorScale),
                                         static_cast<double>(maximumEditorScale)));
}
}  // namespace cs01::ui
