#include <mixdebugger/analysis/DynamicsAnalyzer.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace mixdebugger::analysis
{
namespace
{
float percentile(std::vector<float> values, const float position)
{
    if (values.empty())
        return -100.0f;
    const auto index = static_cast<std::size_t>(std::round(
        std::clamp(position, 0.0f, 1.0f) * static_cast<float>(values.size() - 1)));
    std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(index),
                     values.end());
    return values[index];
}

float toDecibels(const double value)
{
    return value > 1.0e-10 ? static_cast<float>(20.0 * std::log10(value)) : -100.0f;
}
}

DynamicsSummary DynamicsAnalyzer::analyse(const std::span<const float> monoSamples,
                                          const double sampleRate) const
{
    DynamicsSummary result;
    if (monoSamples.empty() || sampleRate <= 0.0)
        return result;

    const auto windowSize = std::max<std::size_t>(1,
        static_cast<std::size_t>(std::round(sampleRate * 0.4)));
    const auto hopSize = std::max<std::size_t>(1, windowSize / 2);
    std::vector<float> windowLevels;
    auto absolutePeak = 0.0f;

    for (const auto sample : monoSamples)
    {
        absolutePeak = std::max(absolutePeak, std::abs(sample));
        if (std::abs(sample) >= 0.999f)
            ++result.clippedSampleCount;
    }

    for (std::size_t first = 0; first < monoSamples.size(); first += hopSize)
    {
        const auto last = std::min(monoSamples.size(), first + windowSize);
        if (last - first < windowSize / 2)
            break;
        auto squareSum = 0.0;
        for (auto index = first; index < last; ++index)
        {
            const auto value = static_cast<double>(monoSamples[index]);
            squareSum += value * value;
        }
        const auto rms = std::sqrt(squareSum / static_cast<double>(last - first));
        windowLevels.push_back(toDecibels(rms));
    }

    if (windowLevels.empty())
        return result;

    const auto loudReference = percentile(windowLevels, 0.9f);
    const auto activityThreshold = std::max(-65.0f, loudReference - 30.0f);
    std::vector<float> activeLevels;
    activeLevels.reserve(windowLevels.size());
    for (const auto level : windowLevels)
    {
        if (level >= activityThreshold)
            activeLevels.push_back(level);
    }

    result.activeWindowCount = activeLevels.size();
    if (activeLevels.size() < 5)
        return result;

    const auto quiet = percentile(activeLevels, 0.1f);
    const auto median = percentile(activeLevels, 0.5f);
    const auto loud = percentile(activeLevels, 0.9f);
    result.quietActiveRmsDb = quiet;
    result.medianActiveRmsDb = median;
    result.loudActiveRmsDb = loud;
    result.shortTermRangeDb = std::max(0.0f, loud - quiet);

    auto activePowerSum = 0.0;
    for (const auto level : activeLevels)
        activePowerSum += std::pow(10.0, static_cast<double>(level) / 10.0);
    const auto activeRms = std::sqrt(activePowerSum / static_cast<double>(activeLevels.size()));
    result.activeRmsDb = toDecibels(activeRms);
    result.absolutePeakDb = toDecibels(absolutePeak);
    result.crestFactorDb = std::max(0.0f, result.absolutePeakDb - result.activeRmsDb);

    if (result.clippedSampleCount > 0)
        result.assessment = DynamicsAssessment::clippingRisk;
    else if (result.shortTermRangeDb < 2.5f && result.crestFactorDb < 6.0f)
        result.assessment = DynamicsAssessment::veryFlat;
    else if (result.shortTermRangeDb > 12.0f)
        result.assessment = DynamicsAssessment::veryUneven;
    else
        result.assessment = DynamicsAssessment::unremarkable;
    return result;
}
} // namespace mixdebugger::analysis
