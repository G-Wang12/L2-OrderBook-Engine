#include "strategy_engine.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>

StrategyEngine::StrategyEngine(SpscQueue<MarketTick, 1024> &queue, LimitOrderBook &book) noexcept
    : queue_(queue),
      book_(book)
{
}

void StrategyEngine::stop() noexcept
{
    running_.store(false, std::memory_order_relaxed);
}

void StrategyEngine::run()
{
    MarketTick tick{};

    while (running_.load(std::memory_order_relaxed))
    {
        if (!queue_.pop(tick))
        {
            // Busy-wait (spin) to minimize wake-up latency.
            continue;
        }

        const auto t_pop = std::chrono::high_resolution_clock::now();

        book_.apply_tick(tick);

        const std::uint8_t best_bid = book_.get_best_bid();
        const std::uint8_t best_ask = book_.get_best_ask();

        if (best_bid == 0U || best_ask == 0U)
        {
            continue;
        }

        const int spread = static_cast<int>(best_ask) - static_cast<int>(best_bid);
        if (spread <= 2)
        {
            const auto t_exec = std::chrono::high_resolution_clock::now();
            const auto us = std::chrono::duration_cast<std::chrono::microseconds>(t_exec - t_pop).count();
            std::cout << "mock_exec latency_us=" << us << " spread=" << spread << " bid="
                      << static_cast<unsigned int>(best_bid) << " ask=" << static_cast<unsigned int>(best_ask)
                      << std::endl;
        }
    }
}
