#include <mixdebugger/analysis/PhaseAlignmentAnalyzer.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>
#include <cstdint>
#include <vector>

namespace
{
std::vector<float> testSignal(const std::size_t size)
{
    std::vector<float> result(size);
    std::uint32_t state = 0x12345678U;
    for (auto& sample : result)
    {
        state = state * 1664525U + 1013904223U;
        sample = static_cast<float>((state >> 8U) & 0xffffU) / 32768.0f - 1.0f;
    }
    return result;
}
}

TEST_CASE("Phase alignment reports a delayed copy")
{
    constexpr auto sampleRate = 1000.0;
    const auto first = testSignal(12000);
    std::vector<float> second(first.size(), 0.0f);
    constexpr std::size_t delay = 6;
    for (std::size_t index = 0; index + delay < first.size(); ++index)
        second[index + delay] = first[index];

    const auto result = mixdebugger::analysis::PhaseAlignmentAnalyzer{}.analyse(
        first, second, sampleRate);
    CHECK(result.assessment == mixdebugger::analysis::PhaseAlignmentAssessment::timingSuspect);
    CHECK(result.bestLagMilliseconds == Catch::Approx(6.0).margin(0.1));
    CHECK(result.bestCorrelation > 0.95f);
}

TEST_CASE("Phase alignment detects reversed polarity")
{
    constexpr auto sampleRate = 1000.0;
    const auto first = testSignal(12000);
    std::vector<float> second(first.size());
    for (std::size_t index = 0; index < first.size(); ++index)
        second[index] = -first[index];

    const auto result = mixdebugger::analysis::PhaseAlignmentAnalyzer{}.analyse(
        first, second, sampleRate);
    CHECK(result.assessment == mixdebugger::analysis::PhaseAlignmentAssessment::polaritySuspect);
    CHECK(result.bestCorrelation < -0.95f);
}
