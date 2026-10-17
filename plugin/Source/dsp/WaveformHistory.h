#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace mixdebugger::dsp
{
struct WaveformPoint
{
    float minimum = 0.0f;
    float maximum = 0.0f;

    bool operator==(const WaveformPoint&) const = default;
};

class WaveformHistory final
{
public:
    static constexpr std::size_t pointsPerSecond = 60;
    static constexpr std::size_t capacityInPoints = 240;

    WaveformHistory()
    {
        clear();
    }

    void prepare(const double sampleRate) noexcept
    {
        samplesPerPoint = static_cast<std::size_t>(
            std::max(1.0, std::round(sampleRate / static_cast<double>(pointsPerSecond))));
        clear();
    }

    void clear() noexcept
    {
        for (auto& point : points)
            point.store(pack({}), std::memory_order_relaxed);

        writePosition = 0;
        pointCount = 0;
        samplesInCurrentPoint = 0;
        currentMinimum = 0.0f;
        currentMaximum = 0.0f;
        publishedWritePosition.store(0, std::memory_order_release);
        publishedPointCount.store(0, std::memory_order_release);
    }

    void push(const float sample) noexcept
    {
        if (samplesInCurrentPoint == 0)
        {
            currentMinimum = sample;
            currentMaximum = sample;
        }
        else
        {
            currentMinimum = std::min(currentMinimum, sample);
            currentMaximum = std::max(currentMaximum, sample);
        }

        ++samplesInCurrentPoint;
        if (samplesInCurrentPoint < samplesPerPoint)
            return;

        points[writePosition].store(pack({ currentMinimum, currentMaximum }),
                                    std::memory_order_release);
        writePosition = (writePosition + 1) % capacityInPoints;
        pointCount = std::min(pointCount + 1, capacityInPoints);
        publishedWritePosition.store(writePosition, std::memory_order_release);
        publishedPointCount.store(pointCount, std::memory_order_release);
        samplesInCurrentPoint = 0;
    }

    [[nodiscard]] std::vector<WaveformPoint> snapshot() const
    {
        const auto count = publishedPointCount.load(std::memory_order_acquire);
        const auto nextWritePosition = publishedWritePosition.load(std::memory_order_acquire);
        const auto firstPoint = (nextWritePosition + capacityInPoints - count) % capacityInPoints;

        std::vector<WaveformPoint> result;
        result.reserve(count);

        for (std::size_t index = 0; index < count; ++index)
        {
            const auto pointIndex = (firstPoint + index) % capacityInPoints;
            result.push_back(unpack(points[pointIndex].load(std::memory_order_acquire)));
        }

        return result;
    }

private:
    static std::uint64_t pack(const WaveformPoint point) noexcept
    {
        const auto minimum = static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(point.minimum));
        const auto maximum = static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(point.maximum));
        return minimum | (maximum << 32U);
    }

    static WaveformPoint unpack(const std::uint64_t packed) noexcept
    {
        return {
            std::bit_cast<float>(static_cast<std::uint32_t>(packed)),
            std::bit_cast<float>(static_cast<std::uint32_t>(packed >> 32U))
        };
    }

    static_assert(std::atomic<std::uint64_t>::is_always_lock_free);

    std::array<std::atomic<std::uint64_t>, capacityInPoints> points{};
    std::atomic<std::size_t> publishedWritePosition{ 0 };
    std::atomic<std::size_t> publishedPointCount{ 0 };

    std::size_t samplesPerPoint = 1;
    std::size_t writePosition = 0;
    std::size_t pointCount = 0;
    std::size_t samplesInCurrentPoint = 0;
    float currentMinimum = 0.0f;
    float currentMaximum = 0.0f;
};
} // namespace mixdebugger::dsp

