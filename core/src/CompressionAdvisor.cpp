#include <mixdebugger/analysis/CompressionAdvisor.h>

#include <algorithm>

namespace mixdebugger::analysis
{
namespace
{
void finalise(CompressionRecommendation& result, const DynamicsSummary& dynamics) noexcept
{
    if (result.advice != CompressionAdvice::considerLeveling
        && result.advice != CompressionAdvice::considerPeakControl)
        return;
    result.ratio = 0.5f * (result.ratioMinimum + result.ratioMaximum);
    result.attackMilliseconds = 0.5f * (result.attackMinimumMilliseconds
                                      + result.attackMaximumMilliseconds);
    result.releaseMilliseconds = 0.5f * (result.releaseMinimumMilliseconds
                                       + result.releaseMaximumMilliseconds);
    result.targetGainReductionDb = 0.5f * (result.targetGainReductionMinimumDb
                                         + result.targetGainReductionMaximumDb);
    const auto compressionSlope = 1.0f - 1.0f / std::max(1.01f, result.ratio);
    const auto controlLevelDb = dynamics.loudActiveRmsDb > -80.0f
        ? dynamics.loudActiveRmsDb : dynamics.activeRmsDb;
    result.thresholdDb = controlLevelDb
                       - result.targetGainReductionDb / compressionSlope;
}

float shortTermRangeTriggerDb(const TrackFamily family) noexcept
{
    switch (family)
    {
        case TrackFamily::vocal:
        case TrackFamily::bass: return 7.0f;
        case TrackFamily::guitar:
        case TrackFamily::keys: return 8.0f;
        case TrackFamily::kick:
        case TrackFamily::snare:
        case TrackFamily::toms: return 9.0f;
        case TrackFamily::cymbals:
        case TrackFamily::drumRoom: return 10.0f;
        case TrackFamily::effect:
        case TrackFamily::unknown: return 9.0f;
        case TrackFamily::parallelDrums: return 0.0f;
    }
    return 9.0f;
}

float crestTriggerDb(const TrackFamily family) noexcept
{
    switch (family)
    {
        case TrackFamily::vocal: return 12.0f;
        case TrackFamily::bass: return 13.0f;
        case TrackFamily::guitar:
        case TrackFamily::keys: return 14.0f;
        case TrackFamily::kick:
        case TrackFamily::snare:
        case TrackFamily::toms: return 16.0f;
        case TrackFamily::cymbals:
        case TrackFamily::drumRoom: return 18.0f;
        case TrackFamily::effect:
        case TrackFamily::unknown: return 15.0f;
        case TrackFamily::parallelDrums: return 0.0f;
    }
    return 15.0f;
}

CompressionRecommendation levelingForFamily(TrackFamily family) noexcept;

CompressionRecommendation peakControlForFamily(const TrackFamily family,
                                                const float excessDb) noexcept
{
    auto result = levelingForFamily(family);
    result.advice = CompressionAdvice::considerPeakControl;
    result.ratioMinimum = 2.0f;
    result.ratioMaximum = 4.0f;
    result.attackMinimumMilliseconds = 2.0f;
    result.attackMaximumMilliseconds = 8.0f;
    result.releaseMinimumMilliseconds = 45.0f;
    result.releaseMaximumMilliseconds = 110.0f;
    result.targetGainReductionMinimumDb = 1.0f;
    result.targetGainReductionMaximumDb = std::clamp(1.0f + 0.35f * excessDb,
                                                      1.5f, 3.5f);
    if (family == TrackFamily::kick || family == TrackFamily::snare
        || family == TrackFamily::toms)
    {
        // Nie ścinamy całego ataku bębnów. To kontrola nadmiernych pików,
        // nie zamiana close-mikrofonu w płaski sygnał.
        result.attackMinimumMilliseconds = 5.0f;
        result.attackMaximumMilliseconds = 12.0f;
    }
    return result;
}

CompressionRecommendation levelingForFamily(const TrackFamily family) noexcept
{
    CompressionRecommendation result;
    result.advice = CompressionAdvice::considerLeveling;
    result.ratioMinimum = 2.0f;
    result.ratioMaximum = 3.0f;
    result.attackMinimumMilliseconds = 12.0f;
    result.attackMaximumMilliseconds = 35.0f;
    result.releaseMinimumMilliseconds = 70.0f;
    result.releaseMaximumMilliseconds = 180.0f;
    result.targetGainReductionMinimumDb = 2.0f;
    result.targetGainReductionMaximumDb = 4.0f;

    if (family == TrackFamily::vocal || family == TrackFamily::bass)
    {
        result.attackMinimumMilliseconds = 8.0f;
        result.attackMaximumMilliseconds = 25.0f;
        result.releaseMinimumMilliseconds = 60.0f;
        result.releaseMaximumMilliseconds = 150.0f;
        result.targetGainReductionMinimumDb = 3.0f;
        result.targetGainReductionMaximumDb = 5.0f;
    }
    else if (family == TrackFamily::kick || family == TrackFamily::snare
             || family == TrackFamily::toms)
    {
        result.ratioMinimum = 3.0f;
        result.ratioMaximum = 4.0f;
        result.attackMinimumMilliseconds = 15.0f;
        result.attackMaximumMilliseconds = 35.0f;
        result.releaseMinimumMilliseconds = 50.0f;
        result.releaseMaximumMilliseconds = 120.0f;
    }
    else if (family == TrackFamily::parallelDrums)
    {
        result.ratioMinimum = 6.0f;
        result.ratioMaximum = 10.0f;
        result.attackMinimumMilliseconds = 5.0f;
        result.attackMaximumMilliseconds = 15.0f;
        result.releaseMinimumMilliseconds = 40.0f;
        result.releaseMaximumMilliseconds = 100.0f;
        result.targetGainReductionMinimumDb = 6.0f;
        result.targetGainReductionMaximumDb = 10.0f;
    }
    return result;
}
}

CompressionRecommendation CompressionAdvisor::recommend(const DynamicsSummary& dynamics,
                                                          const TrackFamily family) const noexcept
{
    if (dynamics.assessment == DynamicsAssessment::insufficientData)
        return {};

    if (dynamics.assessment == DynamicsAssessment::clippingRisk)
    {
        CompressionRecommendation result;
        result.advice = CompressionAdvice::reduceInputBeforeProcessing;
        return result;
    }

    if (dynamics.assessment == DynamicsAssessment::veryFlat)
    {
        CompressionRecommendation result;
        result.advice = CompressionAdvice::avoidMoreCompression;
        return result;
    }

    // Wybranie profilu Parallel drums jest jawną decyzją użytkownika o
    // technicznej kompresji równoległego busa. Nadal odmawiamy jej dla clippingu,
    // braku danych i sygnału już spłaszczonego (obsłużonych wyżej).
    if (family == TrackFamily::parallelDrums)
    {
        auto result = levelingForFamily(family);
        finalise(result, dynamics);
        return result;
    }

    const auto rangeTrigger = shortTermRangeTriggerDb(family);
    if (dynamics.assessment == DynamicsAssessment::veryUneven
        || dynamics.shortTermRangeDb >= rangeTrigger)
    {
        auto result = levelingForFamily(family);
        const auto rangeExcess = std::max(0.0f,
            dynamics.shortTermRangeDb - rangeTrigger);
        result.targetGainReductionMaximumDb = std::min(
            result.targetGainReductionMaximumDb,
            std::clamp(2.5f + 0.3f * rangeExcess, 3.0f, 5.0f));
        finalise(result, dynamics);
        return result;
    }

    const auto crestTrigger = crestTriggerDb(family);
    if (dynamics.crestFactorDb >= crestTrigger)
    {
        auto result = peakControlForFamily(
            family, dynamics.crestFactorDb - crestTrigger);
        finalise(result, dynamics);
        return result;
    }

    CompressionRecommendation result;
    result.advice = CompressionAdvice::noAction;
    return result;
}
} // namespace mixdebugger::analysis
