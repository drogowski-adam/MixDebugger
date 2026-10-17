#pragma once

#include <mixdebugger/analysis/TrackFeatures.h>

#include <array>
#include <cstddef>
#include <stop_token>
#include <vector>

namespace mixdebugger::analysis
{
enum class ConflictCharacter
{
    continuous,
    intermittent,
    alternating
};

struct PairSummary
{
    float score = 0.0f;
    float confidence = 0.0f;
    float coverage = 0.0f;
    float targetActiveSeconds = 0.0f;
    float coActiveSeconds = 0.0f;
    int worstBand = -1;
    ConflictCharacter character = ConflictCharacter::alternating;
    std::array<float, TrackFeatures::bandCount> scorePerBand{};
    std::array<float, TrackFeatures::bandCount> coveragePerBand{};
    std::array<float, TrackFeatures::bandCount> maskerAdvantageDbPerBand{};
    std::array<float, TrackFeatures::bandCount> targetSaliencePerBand{};
};

struct PairTimeline
{
    PairSummary summary;
    std::size_t frameCount = 0;
    double frameRate = 93.75;
    std::array<float, TrackFeatures::bandCount> centreFrequenciesHz{};
    std::vector<float> intensity;
    std::vector<float> maskerEnvelopeDb;
    std::vector<float> targetEnvelopeDb;

    [[nodiscard]] float at(std::size_t frame, std::size_t band) const noexcept;
};

class PairAnalyzer final
{
public:
    [[nodiscard]] PairSummary summarise(const TrackFeatures& masker,
                                        const TrackFeatures& target,
                                        std::stop_token stopToken = {}) const;

    [[nodiscard]] PairTimeline analyseTimeline(const TrackFeatures& masker,
                                               const TrackFeatures& target,
                                               std::stop_token stopToken = {}) const;

private:
    [[nodiscard]] PairTimeline analyse(const TrackFeatures& masker,
                                       const TrackFeatures& target,
                                       bool includeTimeline,
                                       std::stop_token stopToken) const;
};
} // namespace mixdebugger::analysis
