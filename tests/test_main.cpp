#include <gtest/gtest.h>

#include "market_parser.hpp"
#include "spsc_queue.hpp"

// Dummy test to verify test framework is working.
// This confirms GoogleTest integration for the low-latency engine's unit tests.
TEST(DummyTest, BasicAssertion)
{
    EXPECT_EQ(1 + 1, 2);
    EXPECT_TRUE(true);
    EXPECT_FALSE(false);
}

TEST(SpscQueueTest, EmptyAndFullAndFifo)
{
    SpscQueue<int, 8> q;

    int out = 0;
    EXPECT_FALSE(q.pop(out));

    for (int i = 0; i < 8; ++i)
    {
        EXPECT_TRUE(q.push(i));
    }
    EXPECT_FALSE(q.push(123));

    for (int i = 0; i < 8; ++i)
    {
        EXPECT_TRUE(q.pop(out));
        EXPECT_EQ(out, i);
    }
    EXPECT_FALSE(q.pop(out));
}

TEST(MarketParserTest, ParsesBidAndAsk)
{
    MarketParser parser;
    MarketTick tick{};

    EXPECT_TRUE(parser.parse_tick(R"({"price":0.57,"size":123,"side":"bid"})", tick));
    EXPECT_EQ(tick.price, static_cast<std::uint8_t>(57));
    EXPECT_EQ(tick.size, 123U);
    EXPECT_TRUE(tick.is_bid);

    EXPECT_TRUE(parser.parse_tick(R"({"price":57,"size":1,"side":"ask"})", tick));
    EXPECT_EQ(tick.price, static_cast<std::uint8_t>(57));
    EXPECT_EQ(tick.size, 1U);
    EXPECT_FALSE(tick.is_bid);
}
