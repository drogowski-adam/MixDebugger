#include "AudioValidation.h"

#include <algorithm>
#include <cmath>

namespace mixdebugger
{
bool isUsableAudioPeak(const std::optional<float> peakDb) noexcept
{
    return peakDb.has_value() && std::isfinite(*peakDb)
        && *peakDb > minimumUsablePeakDb;
}

bool renderedAudioIsSuspiciouslySilent(
    const std::span<const std::optional<float>> peakLevelsDb) noexcept
{
    return !peakLevelsDb.empty()
        && std::ranges::none_of(peakLevelsDb, isUsableAudioPeak);
}
} // namespace mixdebugger
