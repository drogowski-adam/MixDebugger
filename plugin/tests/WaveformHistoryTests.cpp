#include "dsp/WaveformHistory.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using mixdebugger::dsp::WaveformHistory;
using mixdebugger::dsp::WaveformPoint;

TEST_CASE("WaveformHistory publishes minimum and maximum for each time slice")
{
    WaveformHistory history;
    history.prepare(240.0);

    history.push(-0.25f);
    history.push(0.5f);
    history.push(-0.75f);
    history.push(0.25f);

    CHECK(history.snapshot() == std::vector<WaveformPoint>{ { -0.75f, 0.5f } });
}

TEST_CASE("WaveformHistory does not publish an incomplete time slice")
{
    WaveformHistory history;
    history.prepare(240.0);
    history.push(0.5f);

    CHECK(history.snapshot().empty());
}

TEST_CASE("WaveformHistory keeps approximately four seconds of points")
{
    WaveformHistory history;
    history.prepare(60.0);

    for (std::size_t index = 0; index < WaveformHistory::capacityInPoints + 2; ++index)
        history.push(static_cast<float>(index));

    const auto points = history.snapshot();
    REQUIRE(points.size() == WaveformHistory::capacityInPoints);
    CHECK(points.front().minimum == 2.0f);
    CHECK(points.back().maximum == static_cast<float>(WaveformHistory::capacityInPoints + 1));
}

