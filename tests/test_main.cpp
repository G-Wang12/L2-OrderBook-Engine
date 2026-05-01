#include <gtest/gtest.h>

// Dummy test to verify test framework is working.
// This confirms GoogleTest integration for the low-latency engine's unit tests.
TEST(DummyTest, BasicAssertion)
{
    EXPECT_EQ(1 + 1, 2);
    EXPECT_TRUE(true);
    EXPECT_FALSE(false);
}
