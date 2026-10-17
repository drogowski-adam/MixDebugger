#pragma once

#include <mixdebugger/analysis/TrackRelationship.h>

#include <string>
#include <vector>

namespace mixdebugger::analysis
{
struct GroupBlendTrackInput
{
    std::string group;
    TrackFamily family = TrackFamily::unknown;
};

// Konserwatywny punkt startowy dla warstw wewnątrz jawnej grupy. Decyzja
// opiera się wyłącznie na typach potwierdzonych w UI, nigdy na samej nazwie.
class GroupBlendAdvisor final
{
public:
    [[nodiscard]] std::vector<float> recommendInternalTrimsDb(
        const std::vector<GroupBlendTrackInput>& tracks) const;
};
} // namespace mixdebugger::analysis
