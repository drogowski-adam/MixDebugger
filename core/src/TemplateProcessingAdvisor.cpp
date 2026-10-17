#include <mixdebugger/analysis/TemplateProcessingAdvisor.h>

namespace mixdebugger::analysis
{
namespace
{
std::string eqPresetFor(const TrackFamily family, const MixRole role)
{
    switch (family)
    {
        case TrackFamily::kick: return "stock - Basic 11 band Kick";
        case TrackFamily::snare: return "stock - Basic 11 band Rock Snare";
        case TrackFamily::cymbals: return "stock - Basic 11 band Cymbals";
        case TrackFamily::toms: return "stock - Basic 11 band Toms";
        case TrackFamily::bass: return "stock - Basic 11 band Rock Bass";
        case TrackFamily::vocal:
            return role == MixRole::background
                ? "stock - Basic 11 band BackGroundVocals"
                : "stock - Wide male vocal";
        case TrackFamily::guitar: return "stock - Basic 11 band Fender or Mesa";
        case TrackFamily::keys: return "stock - Pads locut [snk]";
        case TrackFamily::parallelDrums: return "stock - Basic 11 band";
        // Room nie jest automatycznie traktowany jak overhead. Sam HPF usuwa
        // tylko nieużyteczny sub bez narzucania mu barwy talerzy.
        case TrackFamily::drumRoom: return "stock - Basic 100 HPF";
        case TrackFamily::effect:
        case TrackFamily::unknown: return "stock - Track Default";
    }
    return {};
}

std::string compressorPresetFor(const TrackFamily family, const MixRole role)
{
    switch (family)
    {
        case TrackFamily::kick: return "stock - steady rock kick";
        case TrackFamily::snare: return "stock - Rock Snare - aim for 4dB GR";
        case TrackFamily::cymbals:
        case TrackFamily::drumRoom: return "stock - Vintage Overheads";
        case TrackFamily::bass: return "stock - Driving Rock Bass";
        case TrackFamily::vocal:
            return role == MixRole::background
                ? "stock - BackGroundVocals" : "stock - Modern Vocal";
        case TrackFamily::guitar: return "stock - distorted electrtic guitar";
        case TrackFamily::parallelDrums: return "stock - NY Drum Buss";
        case TrackFamily::toms:
        case TrackFamily::keys:
        case TrackFamily::effect:
        case TrackFamily::unknown: return "__DEFAULT__";
    }
    return {};
}

float saturationFor(const TrackFamily family, const MixRole role)
{
    auto amount = 0.0f;
    switch (family)
    {
        case TrackFamily::kick:
        case TrackFamily::snare: amount = 6.0f; break;
        case TrackFamily::bass: amount = 10.0f; break;
        case TrackFamily::vocal: amount = 7.0f; break;
        case TrackFamily::guitar: amount = 9.0f; break;
        case TrackFamily::parallelDrums: amount = 8.0f; break;
        case TrackFamily::cymbals:
        case TrackFamily::toms:
        case TrackFamily::drumRoom:
        case TrackFamily::keys:
        case TrackFamily::effect:
        case TrackFamily::unknown: break;
    }
    if (role == MixRole::background)
        amount *= 0.5f;
    return amount;
}
}

std::string TemplateProcessingAdvisor::basicEqPresetFor(const TrackFamily family,
                                                         const MixRole role)
{
    return eqPresetFor(family, role);
}

std::string TemplateProcessingAdvisor::basicCompressorPresetFor(
    const TrackFamily family, const MixRole role)
{
    return compressorPresetFor(family, role);
}

TemplateProcessingRecommendation TemplateProcessingAdvisor::recommend(
    const MixTemplateId templateId,
    const TemplateProcessingInput& input) const
{
    TemplateProcessingRecommendation result;
    if (templateId != MixTemplateId::jackieDownTheLine
        || input.role == MixRole::unassigned || input.role == MixRole::ignore
        || input.dynamics.assessment == DynamicsAssessment::insufficientData)
        return result;

    result.available = true;
    result.stockEqPreset = eqPresetFor(input.family, input.role);
    result.stockCompressorPreset = compressorPresetFor(input.family, input.role);
    result.compression = CompressionAdvisor{}.recommend(input.dynamics, input.family);
    result.compressorActive
        = result.compression.advice == CompressionAdvice::considerLeveling
       || result.compression.advice == CompressionAdvice::considerPeakControl;

    // ReaComp może być obecny na każdym kanale, lecz zaczyna pracować wyłącznie
    // wtedy, gdy rozrzut krótkoterminowy albo crest factor przekracza próg dla
    // potwierdzonego typu. W pozostałych przypadkach aplikacja ustawia ratio 1:1.
    const auto dynamicsAllowNonlinearProcessing
        = input.dynamics.assessment != DynamicsAssessment::clippingRisk
        && input.dynamics.assessment != DynamicsAssessment::veryFlat
        && input.dynamics.clippedSampleCount == 0;
    if (dynamicsAllowNonlinearProcessing)
        result.saturationAmountPercent = saturationFor(input.family, input.role);
    else
        result.compressorActive = false;
    return result;
}
} // namespace mixdebugger::analysis
