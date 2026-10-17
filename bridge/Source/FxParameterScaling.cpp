#include "FxParameterScaling.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace mixdebugger::reaper_bridge
{
std::optional<double> findClosestNormalizedValue(
    const double targetDisplayedValue,
    const std::function<std::optional<double>(double)>& formatNormalizedValue)
{
    if (!std::isfinite(targetDisplayedValue))
        return std::nullopt;

    std::optional<std::pair<double, double>> first;
    std::optional<std::pair<double, double>> last;
    for (auto index = 0; index <= 1024; ++index)
    {
        const auto normalized = static_cast<double>(index) / 1024.0;
        const auto displayed = formatNormalizedValue(normalized);
        if (!displayed || !std::isfinite(*displayed))
            continue;
        first = std::pair { normalized, *displayed };
        break;
    }
    for (auto index = 1024; index >= 0; --index)
    {
        const auto normalized = static_cast<double>(index) / 1024.0;
        const auto displayed = formatNormalizedValue(normalized);
        if (!displayed || !std::isfinite(*displayed))
            continue;
        last = std::pair { normalized, *displayed };
        break;
    }
    if (!first || !last)
        return std::nullopt;

    const auto increasing = last->second >= first->second;
    auto bestNormalized = first->first;
    auto bestDifference = std::abs(first->second - targetDisplayedValue);
    const auto consider = [&](const double normalized,
                              const std::optional<double> displayed)
    {
        if (!displayed || !std::isfinite(*displayed))
            return;
        const auto difference = std::abs(*displayed - targetDisplayedValue);
        if (difference < bestDifference)
        {
            bestDifference = difference;
            bestNormalized = normalized;
        }
    };
    consider(last->first, last->second);

    auto low = first->first;
    auto high = last->first;
    for (auto iteration = 0; iteration < 56; ++iteration)
    {
        const auto middle = 0.5 * (low + high);
        const auto displayed = formatNormalizedValue(middle);
        if (!displayed || !std::isfinite(*displayed))
            return std::nullopt;
        consider(middle, displayed);

        if ((increasing && *displayed < targetDisplayedValue)
            || (!increasing && *displayed > targetDisplayedValue))
            low = middle;
        else
            high = middle;
    }
    consider(low, formatNormalizedValue(low));
    consider(high, formatNormalizedValue(high));
    return std::clamp(bestNormalized, 0.0, 1.0);
}
} // namespace mixdebugger::reaper_bridge
