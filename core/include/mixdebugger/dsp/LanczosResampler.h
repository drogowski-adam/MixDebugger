#pragma once

#include <span>
#include <vector>

namespace mixdebugger::dsp
{
[[nodiscard]] std::vector<float> resampleLanczos(std::span<const float> input,
                                                 double sourceSampleRate,
                                                 double targetSampleRate,
                                                 int lobes = 3);
} // namespace mixdebugger::dsp

