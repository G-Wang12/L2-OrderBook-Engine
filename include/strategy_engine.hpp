#pragma once

#include <atomic>

#include "order_book.hpp"
#include "spsc_queue.hpp"

class StrategyEngine
{
public:
    StrategyEngine(SpscQueue<MarketTick, 1024> &queue, LimitOrderBook &book) noexcept;

    StrategyEngine(const StrategyEngine &) = delete;
    StrategyEngine &operator=(const StrategyEngine &) = delete;

    void run();
    void stop() noexcept;

private:
    SpscQueue<MarketTick, 1024> &queue_;
    LimitOrderBook &book_;
    std::atomic<bool> running_{true};
};
