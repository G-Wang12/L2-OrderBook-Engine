#pragma once

#include <array>
#include <cstdint>

struct MarketTick
{
    std::uint8_t price;
    std::uint32_t size;
    bool is_bid;
};

class LimitOrderBook
{
public:
    static constexpr std::uint8_t kMinPrice = 1;
    static constexpr std::uint8_t kMaxPrice = 99;
    static constexpr std::size_t kLevels = 100; // index 0 unused

    constexpr LimitOrderBook() noexcept = default;

    constexpr void apply_tick(const MarketTick &tick) noexcept
    {
        if (tick.price < kMinPrice || tick.price > kMaxPrice)
        {
            return;
        }

        const std::size_t idx = static_cast<std::size_t>(tick.price);
        if (tick.is_bid)
        {
            bids_[idx] = tick.size;
        }
        else
        {
            asks_[idx] = tick.size;
        }
    }

    [[nodiscard]] constexpr std::uint8_t get_best_bid() const noexcept
    {
        for (int p = static_cast<int>(kMaxPrice); p >= static_cast<int>(kMinPrice); --p)
        {
            const std::size_t idx = static_cast<std::size_t>(p);
            if (bids_[idx] != 0U)
            {
                return static_cast<std::uint8_t>(p);
            }
        }
        return 0;
    }

    [[nodiscard]] constexpr std::uint8_t get_best_ask() const noexcept
    {
        for (int p = static_cast<int>(kMinPrice); p <= static_cast<int>(kMaxPrice); ++p)
        {
            const std::size_t idx = static_cast<std::size_t>(p);
            if (asks_[idx] != 0U)
            {
                return static_cast<std::uint8_t>(p);
            }
        }
        return 0;
    }

private:
    std::array<std::uint32_t, kLevels> bids_{};
    std::array<std::uint32_t, kLevels> asks_{};
};
