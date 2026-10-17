#include <mixdebugger/analysis/PhaseAlignmentAnalyzer.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace mixdebugger::analysis
{
namespace
{
float correlationAtLag(const std::span<const float> first,
                       const std::span<const float> second,
                       const std::ptrdiff_t lag)
{
    const auto firstStart = lag < 0 ? static_cast<std::size_t>(-lag) : 0U;
    const auto secondStart = lag > 0 ? static_cast<std::size_t>(lag) : 0U;
    const auto count = std::min(first.size() - std::min(firstStart, first.size()),
                                second.size() - std::min(secondStart, second.size()));
    if (count < 64)
        return 0.0f;

    constexpr std::size_t maximumComparedSamples = 60000;
    const auto stride = std::max<std::size_t>(1, count / maximumComparedSamples);
    const auto comparedCount = (count + stride - 1) / stride;
    double firstMean = 0.0;
    double secondMean = 0.0;
    for (std::size_t index = 0; index < count; index += stride)
    {
        firstMean += first[firstStart + index];
        secondMean += second[secondStart + index];
    }
    firstMean /= static_cast<double>(comparedCount);
    secondMean /= static_cast<double>(comparedCount);

    double cross = 0.0;
    double firstPower = 0.0;
    double secondPower = 0.0;
    for (std::size_t index = 0; index < count; index += stride)
    {
        const auto firstValue = static_cast<double>(first[firstStart + index]) - firstMean;
        const auto secondValue = static_cast<double>(second[secondStart + index]) - secondMean;
        cross += firstValue * secondValue;
        firstPower += firstValue * firstValue;
        secondPower += secondValue * secondValue;
    }

    const auto denominator = std::sqrt(firstPower * secondPower);
    return denominator > 1.0e-12
        ? static_cast<float>(std::clamp(cross / denominator, -1.0, 1.0))
        : 0.0f;
}
}

PhaseAlignmentSummary PhaseAlignmentAnalyzer::analyse(
    const std::span<const float> first,
    const std::span<const float> second,
    const double sampleRate,
    const double maximumLagMilliseconds) const
{
    PhaseAlignmentSummary result;
    const auto count = std::min(first.size(), second.size());
    if (sampleRate <= 0.0 || count < static_cast<std::size_t>(sampleRate * 0.5))
        return result;

    const auto limitedFirst = first.first(count);
    const auto limitedSecond = second.first(count);
    const auto maximumLag = std::max<std::ptrdiff_t>(0, static_cast<std::ptrdiff_t>(
        std::llround(sampleRate * std::max(0.0, maximumLagMilliseconds) / 1000.0)));

    result.zeroLagCorrelation = correlationAtLag(limitedFirst, limitedSecond, 0);
    auto strongestMagnitude = 0.0f;
    auto strongestCorrelation = 0.0f;
    std::ptrdiff_t strongestLag = 0;
    auto bestPositive = -1.0f;

    for (std::ptrdiff_t lag = -maximumLag; lag <= maximumLag; ++lag)
    {
        const auto correlation = correlationAtLag(limitedFirst, limitedSecond, lag);
        bestPositive = std::max(bestPositive, correlation);
        if (std::abs(correlation) > strongestMagnitude)
        {
            strongestMagnitude = std::abs(correlation);
            strongestCorrelation = correlation;
            strongestLag = lag;
        }
    }

    result.bestCorrelation = strongestCorrelation;
    result.bestLagMilliseconds = 1000.0 * static_cast<double>(strongestLag) / sampleRate;
    const auto dataConfidence = std::clamp(
        static_cast<float>(count / sampleRate) / 10.0f, 0.0f, 1.0f);
    result.confidence = strongestMagnitude * (0.65f + 0.35f * dataConfidence);

    if (strongestMagnitude < 0.22f)
    {
        result.assessment = PhaseAlignmentAssessment::lowCoherence;
        return result;
    }

    if (strongestCorrelation < -0.35f
        && std::abs(strongestCorrelation) > bestPositive + 0.08f)
    {
        result.assessment = PhaseAlignmentAssessment::polaritySuspect;
        return result;
    }

    if (bestPositive >= 0.30f && std::abs(result.bestLagMilliseconds) > 1.5)
    {
        result.assessment = PhaseAlignmentAssessment::timingSuspect;
        return result;
    }

    result.assessment = PhaseAlignmentAssessment::aligned;
    return result;
}
} // namespace mixdebugger::analysis
