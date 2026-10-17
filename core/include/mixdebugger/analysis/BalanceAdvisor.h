#pragma once

#include <mixdebugger/analysis/MixRole.h>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace mixdebugger::analysis
{
struct BalanceTrackInput
{
    std::string group;
    MixRole role = MixRole::unassigned;
    float activeRmsDb = -100.0f;
    float peakDb = -100.0f;
    std::size_t activeWindowCount = 0;
    // Opcjonalny pomiar rzeczywistej sumy wyrównanych stemów grupy. Gdy jest
    // dostępny, zastępuje przybliżenie polegające na dodawaniu mocy ścieżek.
    std::optional<float> measuredGroupActiveRmsDb;
    std::optional<float> measuredGroupPeakDb;
    std::size_t measuredGroupActiveWindowCount = 0;
    // Jawny, konserwatywny blend warstwy wewnątrz grupy (np. Room).
    // Nie pochodzi z nazwy i jest dodawany do wspólnego ruchu grupy.
    float internalTrimDb = 0.0f;
};

struct BalanceTrackRecommendation
{
    float gainDb = 0.0f;
    float internalTrimDb = 0.0f;
    float predictedActiveRmsDb = -100.0f;
    float predictedPeakDb = -100.0f;
    float confidence = 0.0f;
    bool limitedByPeak = false;
    bool hierarchyLimited = false;
    bool actionable = false;
};

struct BalanceGroupRecommendation
{
    std::string group;
    MixRole role = MixRole::unassigned;
    std::vector<std::size_t> trackIndices;
    float currentActiveRmsDb = -100.0f;
    float targetActiveRmsDb = -100.0f;
    float gainDb = 0.0f;
    float confidence = 0.0f;
    bool limitedByPeak = false;
    bool hierarchyLimited = false;
};

struct MixBalanceSummary
{
    bool available = false;
    bool headroomAvailable = false;
    float globalHeadroomTrimDb = 0.0f;
    float predictedMasterPeakDb = -100.0f;
    bool hierarchyLimited = false;
    std::vector<BalanceTrackRecommendation> tracks;
    std::vector<BalanceGroupRecommendation> groups;
};

// Tworzy neutralny techniczny punkt startowy. Energia jest najpierw dzielona
// między role, a następnie równo między jawne grupy danej roli. Wewnątrz grupy
// zachowane są istniejące proporcje ścieżek. Wszystkie ruchy są względne i
// ograniczone, aby nie wzmacniać pojedynczych szczytów ponad bezpieczny poziom.
class BalanceAdvisor final
{
public:
    [[nodiscard]] MixBalanceSummary recommend(
        const std::vector<BalanceTrackInput>& tracks,
        // Peak sumy przewidziany już PO względnych ruchach grup, ale PRZED
        // wspólnym trimem headroomu. Dzięki temu trim nie opiera się na
        // przybliżeniu największym dodatnim ruchem fadera.
        std::optional<float> predictedPreTrimMasterPeakDb = std::nullopt) const;
};
} // namespace mixdebugger::analysis
