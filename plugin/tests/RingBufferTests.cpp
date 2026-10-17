#include "dsp/RingBuffer.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using mixdebugger::dsp::RingBuffer;

TEST_CASE("RingBuffer ignores samples before it is prepared")
{
    RingBuffer buffer;
    buffer.push(1.0f);

    CHECK(buffer.capacity() == 0);
    CHECK(buffer.size() == 0);
    CHECK(buffer.totalWritten() == 0);
}

TEST_CASE("RingBuffer returns captured samples in chronological order")
{
    RingBuffer buffer;
    buffer.prepare(4);
    buffer.push(1.0f);
    buffer.push(2.0f);

    CHECK(buffer.copyLatest() == std::vector<float>{ 1.0f, 2.0f });
}

TEST_CASE("RingBuffer keeps only the newest samples after wrapping")
{
    RingBuffer buffer;
    buffer.prepare(4);

    for (int sample = 1; sample <= 6; ++sample)
        buffer.push(static_cast<float>(sample));

    CHECK(buffer.size() == 4);
    CHECK(buffer.totalWritten() == 6);
    CHECK(buffer.copyLatest() == std::vector<float>{ 3.0f, 4.0f, 5.0f, 6.0f });
}

TEST_CASE("RingBuffer can be cleared without reallocating")
{
    RingBuffer buffer;
    buffer.prepare(4);
    buffer.push(1.0f);
    buffer.clear();

    CHECK(buffer.capacity() == 4);
    CHECK(buffer.size() == 0);
    CHECK(buffer.totalWritten() == 0);
    CHECK(buffer.copyLatest().empty());
}

