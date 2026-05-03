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

    SpscQueue<MarketTick, 1024> q;
    EXPECT_TRUE(parser.parse_tick(
        R"({"bids":[["0.57","1200"],["0.56","300"]],"asks":[["0.59","500"]]})",
        q));

    MarketTick tick{};
    ASSERT_TRUE(q.pop(tick));
    EXPECT_EQ(tick.price, static_cast<std::uint8_t>(57));
    EXPECT_EQ(tick.size, 1200U);
    EXPECT_TRUE(tick.is_bid);

    ASSERT_TRUE(q.pop(tick));
    EXPECT_EQ(tick.price, static_cast<std::uint8_t>(56));
    EXPECT_EQ(tick.size, 300U);
    EXPECT_TRUE(tick.is_bid);

    ASSERT_TRUE(q.pop(tick));
    EXPECT_EQ(tick.price, static_cast<std::uint8_t>(59));
    EXPECT_EQ(tick.size, 500U);
    EXPECT_FALSE(tick.is_bid);
}

TEST(LimitOrderBookTest, BestBidAskHandleDeletionAndEmpty)
{
    LimitOrderBook book;

    EXPECT_EQ(book.get_best_bid(), LimitOrderBook::kNoBid);
    EXPECT_EQ(book.get_best_ask(), LimitOrderBook::kNoAsk);

    // Add two bid levels; best should be the higher price.
    book.apply_tick(MarketTick{static_cast<std::uint8_t>(57), 100U, true});
    book.apply_tick(MarketTick{static_cast<std::uint8_t>(58), 200U, true});
    EXPECT_EQ(book.get_best_bid(), static_cast<std::uint8_t>(58));

    // Wipe out the best bid with size=0; best should fall back.
    book.apply_tick(MarketTick{static_cast<std::uint8_t>(58), 0U, true});
    EXPECT_EQ(book.get_best_bid(), static_cast<std::uint8_t>(57));

    // Add an ask level then wipe it; ask should return the empty sentinel.
    book.apply_tick(MarketTick{static_cast<std::uint8_t>(59), 123U, false});
    EXPECT_EQ(book.get_best_ask(), static_cast<std::uint8_t>(59));
    book.apply_tick(MarketTick{static_cast<std::uint8_t>(59), 0U, false});
    EXPECT_EQ(book.get_best_ask(), LimitOrderBook::kNoAsk);
}
