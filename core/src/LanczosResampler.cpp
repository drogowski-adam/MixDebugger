#include <mixdebugger/dsp/LanczosResampler.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace mixdebugger::dsp
{
namespace
{
double sinc(const double value) noexcept
{
    if (std::abs(value) < 1.0e-12)
        return 1.0;

    const auto angle = std::numbers::pi * value;
    return std::sin(angle) / angle;
}

double lanczosKernel(const double distance, const int lobes) noexcept
{
    if (std::abs(distance) >= static_cast<double>(lobes))
        return 0.0;

    return sinc(distance) * sinc(distance / static_cast<double>(lobes));
}
} // namespace

std::vector<float> resampleLanczos(const std::span<const float> input,
                                   const double sourceSampleRate,
                                   const double targetSampleRate,
                                   const int lobes)
{
    if (input.empty() || sourceSampleRate <= 0.0 || targetSampleRate <= 0.0 || lobes < 1)
        return {};

    if (std::abs(sourceSampleRate - targetSampleRate) < 1.0e-9)
        return { input.begin(), input.end() };

    const auto outputSize = static_cast<std::size_t>(std::llround(
        static_cast<double>(input.size()) * targetSampleRate / sourceSampleRate));
    std::vector<float> output(outputSize, 0.0f);
    const auto sourceStep = sourceSampleRate / targetSampleRate;

    for (std::size_t outputIndex = 0; outputIndex < outputSize; ++outputIndex)
    {
        const auto sourcePosition = static_cast<double>(outputIndex) * sourceStep;
        const auto centre = static_cast<std::ptrdiff_t>(std::floor(sourcePosition));
        const auto firstSample = centre - lobes + 1;
        const auto lastSample = centre + lobes;
        auto weightedSample = 0.0;
        auto weightSum = 0.0;

        for (auto sourceIndex = firstSample; sourceIndex <= lastSample; ++sourceIndex)
        {
            if (sourceIndex < 0 || sourceIndex >= static_cast<std::ptrdiff_t>(input.size()))
                continue;

            const auto distance = sourcePosition - static_cast<double>(sourceIndex);
            const auto weight = lanczosKernel(distance, lobes);
            weightedSample += static_cast<double>(input[static_cast<std::size_t>(sourceIndex)]) * weight;
            weightSum += weight;
        }

        if (std::abs(weightSum) > 1.0e-12)
            output[outputIndex] = static_cast<float>(weightedSample / weightSum);
    }

    return output;
}
} // namespace mixdebugger::dsp

