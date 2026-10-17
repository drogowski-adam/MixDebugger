#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace mixdebugger::dsp
{
class RingBuffer final
{
public:
    void prepare(const std::size_t capacityInSamples)
    {
        samples.assign(capacityInSamples, 0.0f);
        clear();
    }

    void clear() noexcept
    {
        writePosition = 0;
        storedSamples.store(0, std::memory_order_release);
        totalSamplesWritten.store(0, std::memory_order_release);
    }

    void push(const float sample) noexcept
    {
        if (samples.empty())
            return;

        samples[writePosition] = sample;
        ++writePosition;

        if (writePosition == samples.size())
            writePosition = 0;

        const auto currentSize = storedSamples.load(std::memory_order_relaxed);
        storedSamples.store(std::min(currentSize + 1, samples.size()),
                            std::memory_order_release);
        totalSamplesWritten.fetch_add(1, std::memory_order_relaxed);
    }

    [[nodiscard]] std::size_t capacity() const noexcept
    {
        return samples.size();
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        return storedSamples.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::uint64_t totalWritten() const noexcept
    {
        return totalSamplesWritten.load(std::memory_order_acquire);
    }

    // Wywoluj dopiero, gdy ten bufor nie jest zapisywany przez watek audio.
    // W kolejnym kroku zapewni to bezblokadowa wymiana dwoch buforow.
    [[nodiscard]] std::vector<float> copyLatest() const
    {
        const auto sampleCount = size();
        std::vector<float> result(sampleCount);

        if (sampleCount == 0)
            return result;

        const auto firstSample = (writePosition + samples.size() - sampleCount) % samples.size();
        const auto firstPartSize = std::min(sampleCount, samples.size() - firstSample);

        std::copy_n(samples.begin() + static_cast<std::ptrdiff_t>(firstSample),
                    static_cast<std::ptrdiff_t>(firstPartSize),
                    result.begin());
        std::copy_n(samples.begin(),
                    static_cast<std::ptrdiff_t>(sampleCount - firstPartSize),
                    result.begin() + static_cast<std::ptrdiff_t>(firstPartSize));
        return result;
    }

private:
    std::vector<float> samples;
    std::size_t writePosition = 0;
    std::atomic<std::size_t> storedSamples{ 0 };
    std::atomic<std::uint64_t> totalSamplesWritten{ 0 };
};
} // namespace mixdebugger::dsp

