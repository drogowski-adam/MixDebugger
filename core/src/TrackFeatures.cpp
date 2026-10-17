#include <mixdebugger/analysis/TrackFeatures.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace mixdebugger::analysis
{
float TrackFeatures::powerAt(const std::size_t frame, const std::size_t band) const noexcept
{
    if (frame >= frameCount || band >= bandCount)
        return 0.0f;
    return bandPower[frame * bandCount + band];
}

bool TrackFeatures::activeAt(const std::size_t frame, const std::size_t band) const noexcept
{
    if (frame >= frameCount || band >= bandCount || bandUsable[band] == 0)
        return false;
    return active[frame * bandCount + band] != 0;
}

TrackFeatures TrackFeatureExtractor::extract(dsp::ErbSpectrogram spectrogram) const
{
    TrackFeatures result;
    result.frameCount = spectrogram.frameCount;
    result.centreFrequenciesHz = spectrogram.centreFrequenciesHz;
    result.averageDb = spectrogram.averageDb;
    result.bandPower = std::move(spectrogram.power);
    result.active.assign(result.frameCount * TrackFeatures::bandCount, 0);

    if (result.frameCount == 0)
        return result;

    std::vector<float> levels(result.frameCount);
    for (std::size_t band = 0; band < TrackFeatures::bandCount; ++band)
    {
        for (std::size_t frame = 0; frame < result.frameCount; ++frame)
            levels[frame] = 10.0f * std::log10(result.powerAt(frame, band) + 1.0e-12f);

        const auto percentileIndex = static_cast<std::size_t>(
            std::floor(0.95 * static_cast<double>(result.frameCount - 1)));
        std::nth_element(levels.begin(),
                         levels.begin() + static_cast<std::ptrdiff_t>(percentileIndex),
                         levels.end());
        const auto reference = levels[percentileIndex];
        result.referenceDb[band] = reference;
    }

    const auto strongestReference = *std::max_element(result.referenceDb.begin(),
                                                       result.referenceDb.end());
    for (std::size_t band = 0; band < TrackFeatures::bandCount; ++band)
    {
        // Pełna istotność do 12 dB poniżej głównego pasma, płynne wygaszenie
        // do zera przy -30 dB. Tak zachowujemy harmoniczne, ale odrzucamy bleed.
        const auto relative = result.referenceDb[band] - strongestReference;
        auto salience = std::clamp((relative + 30.0f) / 18.0f, 0.0f, 1.0f);
        salience = salience * salience * (3.0f - 2.0f * salience);
        if (result.referenceDb[band] < -70.0f)
            salience = 0.0f;
        result.bandSalience[band] = salience;

        for (std::size_t frame = 0; frame < result.frameCount; ++frame)
            levels[frame] = 10.0f * std::log10(result.powerAt(frame, band) + 1.0e-12f);

        const auto reference = result.referenceDb[band];
        const auto threshold = std::max(reference - 25.0f, -70.0f);

        auto activeCount = std::size_t{};
        for (std::size_t frame = 0; frame < result.frameCount; ++frame)
        {
            const auto level = 10.0f * std::log10(result.powerAt(frame, band) + 1.0e-12f);
            const auto isActive = level > threshold && level > -70.0f;
            result.active[frame * TrackFeatures::bandCount + band]
                = static_cast<std::uint8_t>(isActive);
            activeCount += static_cast<std::size_t>(isActive);
        }

        if (activeCount >= minimumActiveFrames && salience >= 0.05f)
        {
            result.bandUsable[band] = 1;
        }
        else
        {
            for (std::size_t frame = 0; frame < result.frameCount; ++frame)
                result.active[frame * TrackFeatures::bandCount + band] = 0;
        }
    }

    return result;
}
} // namespace mixdebugger::analysis
