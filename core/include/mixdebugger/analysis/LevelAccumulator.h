#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace mixdebugger::analysis
{
struct LevelResult
{
    float rmsDb = -100.0f;
    float peakDb = -100.0f;
    std::uint64_t sampleCount = 0;
    std::optional<float> stereoCorrelation;
    std::optional<float> sideToMidPowerRatio;
};

class LevelAccumulator final
{
public:
    void addMonoSample(const float sample) noexcept
    {
        const auto value = static_cast<double>(sample);
        sumOfSquares += value * value;
        peak = std::max(peak, std::abs(sample));
        ++samples;
    }

    void addStereoBlock(const float* left, const float* right, const std::size_t sampleCount) noexcept
    {
        if (left == nullptr || sampleCount == 0)
            return;

        if (right == nullptr)
            right = left;

        for (std::size_t index = 0; index < sampleCount; ++index)
        {
            const auto leftValue = static_cast<double>(left[index]);
            const auto rightValue = static_cast<double>(right[index]);
            addMonoSample(0.5f * (left[index] + right[index]));
            sumLeftSquares += leftValue * leftValue;
            sumRightSquares += rightValue * rightValue;
            sumLeftRight += leftValue * rightValue;
            const auto side = 0.5 * (leftValue - rightValue);
            sumSideSquares += side * side;
        }
    }

    [[nodiscard]] LevelResult result() const noexcept
    {
        if (samples == 0)
            return {};

        const auto rms = static_cast<float>(std::sqrt(sumOfSquares / static_cast<double>(samples)));
        LevelResult output { toDecibels(rms), toDecibels(peak), samples };
        const auto denominator = std::sqrt(sumLeftSquares * sumRightSquares);
        if (denominator > std::numeric_limits<double>::epsilon())
        {
            output.stereoCorrelation = static_cast<float>(
                std::clamp(sumLeftRight / denominator, -1.0, 1.0));
            output.sideToMidPowerRatio = static_cast<float>(sumSideSquares
                / std::max(sumOfSquares, std::numeric_limits<double>::epsilon()));
        }
        return output;
    }

private:
    static float toDecibels(const float gain) noexcept
    {
        constexpr auto floorDb = -100.0f;
        constexpr auto minimumGain = 1.0e-5f;
        return gain <= minimumGain ? floorDb : 20.0f * std::log10(gain);
    }

    double sumOfSquares = 0.0;
    double sumLeftSquares = 0.0;
    double sumRightSquares = 0.0;
    double sumLeftRight = 0.0;
    double sumSideSquares = 0.0;
    float peak = 0.0f;
    std::uint64_t samples = 0;
};
} // namespace mixdebugger::analysis
