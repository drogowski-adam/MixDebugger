#pragma once

#include <mixdebugger/analysis/CompressionAdvisor.h>
#include <mixdebugger/analysis/DynamicsAnalyzer.h>
#include <mixdebugger/analysis/MixRole.h>
#include <mixdebugger/analysis/MixTemplateAdvisor.h>
#include <mixdebugger/analysis/TrackRelationship.h>

#include <string>

namespace mixdebugger::analysis
{
struct TemplateProcessingInput
{
    TrackFamily family = TrackFamily::unknown;
    MixRole role = MixRole::unassigned;
    DynamicsSummary dynamics;
};

struct TemplateProcessingRecommendation
{
    bool available = false;
    std::string stockEqPreset;
    std::string stockCompressorPreset;
    CompressionRecommendation compression;
    bool compressorActive = false;
    float saturationAmountPercent = 0.0f;
};

// Jawna warstwa stylistyczna nakładana dopiero po pomiarze. Nazwy ścieżek nie
// biorą udziału w decyzji; template korzysta z potwierdzonego typu, roli oraz
// zabezpieczeń wynikających z dynamiki materiału.
class TemplateProcessingAdvisor final
{
public:
    // Bezpieczny punkt startowy ReaEQ dla potwierdzonej rodziny. Nie korzysta
    // z nazwy ścieżki i zawsze zwraca preset, również dla typu nieznanego.
    [[nodiscard]] static std::string basicEqPresetFor(TrackFamily family,
                                                       MixRole role);
    [[nodiscard]] static std::string basicCompressorPresetFor(TrackFamily family,
                                                               MixRole role);

    [[nodiscard]] TemplateProcessingRecommendation recommend(
        MixTemplateId templateId,
        const TemplateProcessingInput& input) const;
};
} // namespace mixdebugger::analysis
