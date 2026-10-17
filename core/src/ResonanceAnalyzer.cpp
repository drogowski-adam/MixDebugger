#include <mixdebugger/analysis/ResonanceAnalyzer.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace mixdebugger::analysis
{
namespace
{
float powerToDb(const float power)
{
    return 10.0f * std::log10(power + 1.0e-12f);
}
}

std::vector<ResonanceFinding> ResonanceAnalyzer::analyse(
    const TrackFeatures& features,
    const std::size_t maximumFindings) const
{
    std::vector<ResonanceFinding> candidates;
    if (features.frameCount == 0 || maximumFindings == 0)
        return candidates;

    constexpr auto minimumExcessDb = 5.5f;
    constexpr auto minimumPersistence = 0.25f;
    for (std::size_t band = 2; band + 2 < TrackFeatures::bandCount; ++band)
    {
        const auto frequency = features.centreFrequenciesHz[band];
        if (features.bandUsable[band] == 0 || frequency < 80.0f || frequency > 12000.0f)
            continue;

        auto activeFrames = std::size_t{};
        auto resonantFrames = std::size_t{};
        auto excessSum = 0.0f;
        for (std::size_t frame = 0; frame < features.frameCount; ++frame)
        {
            if (!features.activeAt(frame, band))
                continue;
            ++activeFrames;

            const auto centreDb = 10.0f * std::log10(features.powerAt(frame, band) + 1.0e-12f);
            auto neighbourDb = 0.0f;
            for (const auto neighbour : { band - 2, band - 1, band + 1, band + 2 })
                neighbourDb += 10.0f * std::log10(features.powerAt(frame, neighbour) + 1.0e-12f);
            neighbourDb *= 0.25f;
            const auto excess = centreDb - neighbourDb;
            if (excess >= minimumExcessDb)
            {
                ++resonantFrames;
                excessSum += excess;
            }
        }

        if (activeFrames == 0 || resonantFrames == 0)
            continue;
        const auto persistence = static_cast<float>(resonantFrames)
                               / static_cast<float>(activeFrames);
        const auto averageExcess = excessSum / static_cast<float>(resonantFrames);
        if (persistence < minimumPersistence || averageExcess < minimumExcessDb)
            continue;
        candidates.push_back({ band, frequency, averageExcess, persistence,
                               averageExcess * std::sqrt(persistence), 1.0f,
                               -std::clamp(0.30f * (averageExcess - 4.0f), 1.5f, 4.0f),
                               std::clamp((persistence - 0.25f) / 0.55f, 0.0f, 1.0f) });
    }

    std::ranges::sort(candidates, std::greater{}, &ResonanceFinding::score);
    std::vector<ResonanceFinding> result;
    result.reserve(std::min(maximumFindings, candidates.size()));
    for (const auto& candidate : candidates)
    {
        const auto tooClose = std::ranges::any_of(result, [&candidate](const ResonanceFinding& kept)
        {
            const auto distance = kept.band > candidate.band ? kept.band - candidate.band
                                                             : candidate.band - kept.band;
            return distance <= 2;
        });
        if (!tooClose)
            result.push_back(candidate);
        if (result.size() == maximumFindings)
            break;
    }
    return result;
}

std::vector<ResonanceFinding> ResonanceAnalyzer::analyse(
    const dsp::Spectrogram& spectrogram,
    const std::size_t maximumFindings) const
{
    std::vector<ResonanceFinding> candidates;
    if (spectrogram.frameCount == 0 || maximumFindings == 0
        || spectrogram.averagePower.size() < dsp::Spectrogram::binCount)
        return candidates;

    const auto binWidth = static_cast<float>(
        spectrogram.sampleRate / static_cast<double>(dsp::Spectrogram::fftSize));
    if (binWidth <= 0.0f)
        return candidates;

    std::vector<float> averageDb(dsp::Spectrogram::binCount);
    for (std::size_t bin = 0; bin < averageDb.size(); ++bin)
        averageDb[bin] = powerToDb(spectrogram.averagePower[bin]);

    struct SpectralPeak
    {
        std::size_t bin = 0;
        float averageExcessDb = 0.0f;
        std::size_t radius = 0;
    };
    std::vector<SpectralPeak> spectralPeaks;
    const auto firstBin = std::max<std::size_t>(4,
        static_cast<std::size_t>(std::ceil(80.0f / binWidth)));
    const auto lastBin = std::min<std::size_t>(dsp::Spectrogram::binCount - 4,
        static_cast<std::size_t>(std::floor(12000.0f / binWidth)));
    for (auto bin = firstBin; bin <= lastBin; ++bin)
    {
        if (averageDb[bin] < -75.0f
            || averageDb[bin] < averageDb[bin - 1]
            || averageDb[bin] < averageDb[bin + 1])
            continue;

        const auto radius = std::clamp<std::size_t>(
            static_cast<std::size_t>(std::lround(static_cast<double>(bin) * 0.08)),
            4, 48);
        const auto left = bin > radius ? bin - radius : std::size_t{};
        const auto right = std::min(averageDb.size() - 1, bin + radius);
        std::vector<float> neighbours;
        neighbours.reserve(right - left + 1);
        for (auto neighbour = left; neighbour <= right; ++neighbour)
        {
            const auto distance = neighbour > bin ? neighbour - bin : bin - neighbour;
            if (distance >= 3)
                neighbours.push_back(averageDb[neighbour]);
        }
        if (neighbours.size() < 4)
            continue;
        const auto middle = neighbours.begin()
            + static_cast<std::ptrdiff_t>(neighbours.size() / 2);
        std::nth_element(neighbours.begin(), middle, neighbours.end());
        const auto excess = averageDb[bin] - *middle;
        if (excess >= 6.5f)
            spectralPeaks.push_back({ bin, excess, radius });
    }

    std::ranges::sort(spectralPeaks, std::greater{}, &SpectralPeak::averageExcessDb);
    if (spectralPeaks.size() > 24)
        spectralPeaks.resize(24);

    for (const auto& peak : spectralPeaks)
    {
        std::vector<float> levels(spectrogram.frameCount);
        for (std::size_t frame = 0; frame < spectrogram.frameCount; ++frame)
            levels[frame] = powerToDb(spectrogram.at(frame, peak.bin));
        auto percentileLevels = levels;
        const auto percentileIndex = static_cast<std::size_t>(
            std::floor(0.95 * static_cast<double>(percentileLevels.size() - 1)));
        std::nth_element(percentileLevels.begin(),
                         percentileLevels.begin()
                            + static_cast<std::ptrdiff_t>(percentileIndex),
                         percentileLevels.end());
        const auto activeThreshold = std::max(-75.0f,
            percentileLevels[percentileIndex] - 25.0f);

        auto activeFrames = std::size_t{};
        auto resonantFrames = std::size_t{};
        auto excessSum = 0.0f;
        const std::array<std::ptrdiff_t, 4> offsets {
            -static_cast<std::ptrdiff_t>(peak.radius), -3, 3,
             static_cast<std::ptrdiff_t>(peak.radius)
        };
        for (std::size_t frame = 0; frame < spectrogram.frameCount; ++frame)
        {
            const auto level = levels[frame];
            if (level < activeThreshold)
                continue;
            ++activeFrames;
            auto neighbourPower = 0.0f;
            auto neighbourCount = 0;
            for (const auto offset : offsets)
            {
                const auto candidate = static_cast<std::ptrdiff_t>(peak.bin) + offset;
                if (candidate >= 0
                    && candidate < static_cast<std::ptrdiff_t>(dsp::Spectrogram::binCount))
                {
                    neighbourPower += spectrogram.at(frame,
                        static_cast<std::size_t>(candidate));
                    ++neighbourCount;
                }
            }
            const auto localDb = powerToDb(neighbourPower
                / static_cast<float>(std::max(1, neighbourCount)));
            const auto excess = level - localDb;
            if (excess >= 6.0f)
            {
                ++resonantFrames;
                excessSum += excess;
            }
        }
        if (activeFrames < 30 || resonantFrames == 0)
            continue;

        const auto persistence = static_cast<float>(resonantFrames)
                               / static_cast<float>(activeFrames);
        const auto excess = excessSum / static_cast<float>(resonantFrames);
        if (persistence < 0.30f || excess < 6.5f)
            continue;

        const auto threeDbDown = averageDb[peak.bin] - 3.0f;
        auto left = peak.bin;
        auto right = peak.bin;
        while (left > 1 && peak.bin - left < peak.radius
               && averageDb[left] >= threeDbDown)
            --left;
        while (right + 1 < averageDb.size() && right - peak.bin < peak.radius
               && averageDb[right] >= threeDbDown)
            ++right;
        const auto frequency = static_cast<float>(peak.bin) * binWidth;
        const auto bandwidth = std::max(2.0f * binWidth,
            static_cast<float>(right - left) * binWidth);
        const auto q = std::clamp(frequency / bandwidth, 0.8f, 12.0f);
        const auto durationSeconds = static_cast<float>(spectrogram.frameCount)
            * static_cast<float>(dsp::Spectrogram::hopSize / spectrogram.sampleRate);
        const auto dataConfidence = std::clamp(durationSeconds / 20.0f, 0.25f, 1.0f);
        const auto confidence = std::clamp((persistence - 0.25f) / 0.55f, 0.0f, 1.0f)
                              * std::clamp((excess - 5.0f) / 7.0f, 0.0f, 1.0f)
                              * dataConfidence;
        candidates.push_back({ peak.bin, frequency, excess, persistence,
            excess * std::sqrt(persistence), q,
            -std::clamp(0.30f * (excess - 4.0f), 1.5f, 4.0f), confidence });
    }

    std::ranges::sort(candidates, std::greater{}, &ResonanceFinding::score);
    std::vector<ResonanceFinding> result;
    result.reserve(std::min(maximumFindings, candidates.size()));
    for (const auto& candidate : candidates)
    {
        const auto tooClose = std::ranges::any_of(result,
            [&candidate](const ResonanceFinding& kept)
            {
                return std::abs(std::log2(candidate.frequencyHz / kept.frequencyHz)) < 0.12f;
            });
        if (!tooClose)
            result.push_back(candidate);
        if (result.size() == maximumFindings)
            break;
    }
    return result;
}
} // namespace mixdebugger::analysis
