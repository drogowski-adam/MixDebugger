#include <mixdebugger/analysis/PairAnalyzer.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace mixdebugger::analysis
{
float PairTimeline::at(const std::size_t frame, const std::size_t band) const noexcept
{
    if (frame >= frameCount || band >= TrackFeatures::bandCount || intensity.empty())
        return 0.0f;
    return intensity[frame * TrackFeatures::bandCount + band];
}

PairSummary PairAnalyzer::summarise(const TrackFeatures& masker,
                                    const TrackFeatures& target,
                                    const std::stop_token stopToken) const
{
    return analyse(masker, target, false, stopToken).summary;
}

PairTimeline PairAnalyzer::analyseTimeline(const TrackFeatures& masker,
                                           const TrackFeatures& target,
                                           const std::stop_token stopToken) const
{
    return analyse(masker, target, true, stopToken);
}

PairTimeline PairAnalyzer::analyse(const TrackFeatures& masker,
                                   const TrackFeatures& target,
                                   const bool includeTimeline,
                                   const std::stop_token stopToken) const
{
    PairTimeline result;
    result.frameCount = std::max(masker.frameCount, target.frameCount);
    result.frameRate = target.frameRate;
    result.centreFrequenciesHz = target.centreFrequenciesHz;

    if (includeTimeline)
    {
        result.intensity.assign(result.frameCount * TrackFeatures::bandCount, 0.0f);
        result.maskerEnvelopeDb.assign(result.frameCount, -120.0f);
        result.targetEnvelopeDb.assign(result.frameCount, -120.0f);
    }

    std::vector<std::uint8_t> targetActiveFrame(result.frameCount, 0);
    std::vector<std::uint8_t> coActiveFrame(result.frameCount, 0);
    std::vector<float> strongestBandScores;
    strongestBandScores.reserve(TrackFeatures::bandCount);

    for (std::size_t band = 0; band < TrackFeatures::bandCount; ++band)
    {
        if (stopToken.stop_requested())
            return {};
        if (target.bandUsable[band] == 0)
            continue;

        auto targetFrames = std::size_t{};
        auto coActiveFrames = std::size_t{};
        auto maskingSum = 0.0f;
        auto advantageSumDb = 0.0f;

        for (std::size_t frame = 0; frame < result.frameCount; ++frame)
        {
            if (!target.activeAt(frame, band))
                continue;

            ++targetFrames;
            targetActiveFrame[frame] = 1;
            if (!masker.activeAt(frame, band))
                continue;

            ++coActiveFrames;
            coActiveFrame[frame] = 1;
            const auto targetPower = target.powerAt(frame, band) + 1.0e-12f;
            const auto maskerPower = masker.powerAt(frame, band) + 1.0e-12f;
            const auto advantageDb = 10.0f * std::log10(maskerPower / targetPower);
            // Równa energia nie jest automatycznie silnym maskowaniem. Wynik
            // rośnie od -4 dB przewagi maskera i osiąga maksimum przy +10 dB.
            auto intensity = std::clamp((advantageDb + 4.0f) / 14.0f, 0.0f, 1.0f);
            intensity = intensity * intensity * (3.0f - 2.0f * intensity);
            maskingSum += intensity;
            advantageSumDb += advantageDb;

            if (includeTimeline)
                result.intensity[frame * TrackFeatures::bandCount + band]
                    = intensity * target.bandSalience[band];
        }

        if (targetFrames == 0 || coActiveFrames < TrackFeatureExtractor::minimumActiveFrames)
            continue;

        const auto coverage = static_cast<float>(coActiveFrames)
                            / static_cast<float>(targetFrames);
        const auto meanMasking = maskingSum / static_cast<float>(coActiveFrames);
        const auto dataConfidence = std::min(1.0f,
            static_cast<float>(coActiveFrames) / 300.0f);
        const auto salience = target.bandSalience[band];
        const auto bandScore = meanMasking * std::sqrt(coverage) * salience
                             * (0.55f + 0.45f * dataConfidence);
        result.summary.scorePerBand[band] = bandScore;
        result.summary.coveragePerBand[band] = coverage;
        result.summary.maskerAdvantageDbPerBand[band]
            = advantageSumDb / static_cast<float>(coActiveFrames);
        result.summary.targetSaliencePerBand[band] = salience;
        strongestBandScores.push_back(bandScore);

        if (result.summary.worstBand < 0
            || bandScore > result.summary.scorePerBand[static_cast<std::size_t>(result.summary.worstBand)])
            result.summary.worstBand = static_cast<int>(band);

    }

    if (!strongestBandScores.empty())
    {
        std::ranges::sort(strongestBandScores, std::greater<>());
        result.summary.score = strongestBandScores[0];
        if (strongestBandScores.size() > 1)
            result.summary.score = 0.72f * strongestBandScores[0]
                                 + 0.28f * strongestBandScores[1];
    }

    const auto targetFrameCount = static_cast<std::size_t>(
        std::count(targetActiveFrame.begin(), targetActiveFrame.end(), std::uint8_t{ 1 }));
    const auto coActiveFrameCount = static_cast<std::size_t>(
        std::count(coActiveFrame.begin(), coActiveFrame.end(), std::uint8_t{ 1 }));

    if (targetFrameCount > 0)
        result.summary.coverage = static_cast<float>(coActiveFrameCount)
                                / static_cast<float>(targetFrameCount);
    if (result.frameRate > 0.0)
    {
        result.summary.targetActiveSeconds = static_cast<float>(
            static_cast<double>(targetFrameCount) / result.frameRate);
        result.summary.coActiveSeconds = static_cast<float>(
            static_cast<double>(coActiveFrameCount) / result.frameRate);
    }
    result.summary.confidence = std::min(1.0f,
        static_cast<float>(coActiveFrameCount) / 600.0f);

    if (result.summary.coverage >= 0.7f)
        result.summary.character = ConflictCharacter::continuous;
    else if (result.summary.coverage >= 0.3f)
        result.summary.character = ConflictCharacter::intermittent;

    if (includeTimeline)
    {
        for (std::size_t frame = 0; frame < result.frameCount; ++frame)
        {
            auto maskerPower = 0.0f;
            auto targetPower = 0.0f;
            for (std::size_t band = 0; band < TrackFeatures::bandCount; ++band)
            {
                maskerPower += masker.powerAt(frame, band);
                targetPower += target.powerAt(frame, band);
            }
            result.maskerEnvelopeDb[frame] = 10.0f * std::log10(maskerPower + 1.0e-12f);
            result.targetEnvelopeDb[frame] = 10.0f * std::log10(targetPower + 1.0e-12f);
        }
    }

    return result;
}
} // namespace mixdebugger::analysis
