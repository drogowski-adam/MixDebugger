#pragma once

#include <atomic>
#include <cstddef>
#include <mutex>
#include <unordered_map>

namespace mixdebugger
{
class ProbeProcessor;

class TrackRegistry final
{
public:
    static TrackRegistry& get();

    int registerProbe(ProbeProcessor* probe);
    void unregisterProbe(int probeId);
    [[nodiscard]] std::size_t probeCount() const;

private:
    TrackRegistry() = default;

    mutable std::mutex mutex;
    std::unordered_map<int, ProbeProcessor*> probes;
    std::atomic<std::size_t> registeredProbeCount{ 0 };
    int nextProbeId = 1;
};
} // namespace mixdebugger
