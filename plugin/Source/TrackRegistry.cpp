#include "TrackRegistry.h"

#include <cassert>

namespace mixdebugger
{
TrackRegistry& TrackRegistry::get()
{
    static TrackRegistry registry;
    return registry;
}

int TrackRegistry::registerProbe(ProbeProcessor* probe)
{
    assert(probe != nullptr);

    const std::scoped_lock lock(mutex);
    const auto probeId = nextProbeId++;
    probes.emplace(probeId, probe);
    registeredProbeCount.fetch_add(1, std::memory_order_relaxed);
    return probeId;
}

void TrackRegistry::unregisterProbe(const int probeId)
{
    const std::scoped_lock lock(mutex);
    if (probes.erase(probeId) > 0)
        registeredProbeCount.fetch_sub(1, std::memory_order_relaxed);
}

std::size_t TrackRegistry::probeCount() const
{
    return registeredProbeCount.load(std::memory_order_relaxed);
}
} // namespace mixdebugger
