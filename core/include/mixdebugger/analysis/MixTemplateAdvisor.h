#pragma once

#include <mixdebugger/analysis/MixRole.h>
#include <mixdebugger/analysis/TrackRelationship.h>

#include <string>
#include <vector>

namespace mixdebugger::analysis
{
enum class MixTemplateId
{
    jackieDownTheLine
};

struct MixTemplateTrackInput
{
    std::string group;
    TrackFamily family = TrackFamily::unknown;
    MixRole currentRole = MixRole::unassigned;
};

struct MixTemplateRecommendation
{
    bool canApply = false;
    std::vector<MixRole> roles;
};

// Template ustawia wyłącznie intencję aranżacyjną. Korzysta z typów i grup
// jawnie potwierdzonych przez użytkownika, nigdy z nazw ścieżek.
class MixTemplateAdvisor final
{
public:
    [[nodiscard]] MixTemplateRecommendation recommend(
        MixTemplateId templateId,
        const std::vector<MixTemplateTrackInput>& tracks) const;
};
} // namespace mixdebugger::analysis
