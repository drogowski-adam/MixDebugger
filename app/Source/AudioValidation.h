#pragma once

#include <optional>
#include <span>

namespace mixdebugger
{
inline constexpr float minimumUsablePeakDb = -80.0f;

bool isUsableAudioPeak(std::optional<float> peakDb) noexcept;
bool renderedAudioIsSuspiciouslySilent(
    std::span<const std::optional<float>> peakLevelsDb) noexcept;
} // namespace mixdebugger
