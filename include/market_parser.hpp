#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>

#include <simdjson/ondemand.h>
#include <simdjson/padded_string_view-inl.h>

#include "order_book.hpp"

class MarketParser
{
public:
    static constexpr std::size_t kMaxPayloadBytes = 1024;

    MarketParser() noexcept
    {
        const simdjson::error_code err = parser_.allocate(kMaxPayloadBytes + simdjson::SIMDJSON_PADDING);
        initialized_ = (err == simdjson::SUCCESS);
    }

    [[nodiscard]] bool parse_tick(std::string_view json_payload, MarketTick &out_tick) noexcept
    {
        if (!initialized_)
        {
            return false;
        }

        const std::size_t len = json_payload.size();
        if (len == 0U || len > kMaxPayloadBytes)
        {
            return false;
        }

        std::memcpy(buffer_.data(), json_payload.data(), len);
        std::memset(buffer_.data() + len, 0, simdjson::SIMDJSON_PADDING);

        simdjson::ondemand::document doc;
        const simdjson::padded_string_view view(buffer_.data(), len, buffer_.size());
        if (parser_.iterate(view).get(doc) != simdjson::SUCCESS)
        {
            return false;
        }

        simdjson::ondemand::object obj;
        if (doc.get_object().get(obj) != simdjson::SUCCESS)
        {
            return false;
        }

        // price
        std::uint32_t price_cents = 0U;
        {
            simdjson::ondemand::value price_val;
            if (obj.find_field_unordered("price").get(price_val) != simdjson::SUCCESS)
            {
                return false;
            }

            std::uint64_t price_u64 = 0U;
            if (price_val.get_uint64().get(price_u64) == simdjson::SUCCESS)
            {
                if (price_u64 > static_cast<std::uint64_t>(std::numeric_limits<std::uint8_t>::max()))
                {
                    return false;
                }
                price_cents = static_cast<std::uint32_t>(price_u64);
            }
            else
            {
                double price_d = 0.0;
                if (price_val.get_double().get(price_d) != simdjson::SUCCESS)
                {
                    return false;
                }

                const double scaled = (price_d <= 1.0) ? (price_d * 100.0) : price_d;
                const long long rounded = std::llround(scaled);
                if (rounded < 0LL || rounded > static_cast<long long>(std::numeric_limits<std::uint8_t>::max()))
                {
                    return false;
                }
                price_cents = static_cast<std::uint32_t>(rounded);
            }

            if (price_cents < static_cast<std::uint32_t>(LimitOrderBook::kMinPrice) ||
                price_cents > static_cast<std::uint32_t>(LimitOrderBook::kMaxPrice))
            {
                return false;
            }
        }

        // size
        std::uint32_t size = 0U;
        {
            simdjson::ondemand::value size_val;
            if (obj.find_field_unordered("size").get(size_val) != simdjson::SUCCESS)
            {
                return false;
            }

            std::uint64_t size_u64 = 0U;
            if (size_val.get_uint64().get(size_u64) != simdjson::SUCCESS)
            {
                return false;
            }
            if (size_u64 > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()))
            {
                return false;
            }
            size = static_cast<std::uint32_t>(size_u64);
        }

        // side
        bool is_bid = false;
        {
            simdjson::ondemand::value side_val;
            if (obj.find_field_unordered("side").get(side_val) != simdjson::SUCCESS)
            {
                return false;
            }

            bool side_bool = false;
            if (side_val.get_bool().get(side_bool) == simdjson::SUCCESS)
            {
                is_bid = side_bool;
            }
            else
            {
                std::string_view side_sv;
                if (side_val.get_string().get(side_sv) == simdjson::SUCCESS)
                {
                    if (side_sv == "bid" || side_sv == "buy")
                    {
                        is_bid = true;
                    }
                    else if (side_sv == "ask" || side_sv == "sell")
                    {
                        is_bid = false;
                    }
                    else
                    {
                        return false;
                    }
                }
                else
                {
                    std::uint64_t side_u64 = 0U;
                    if (side_val.get_uint64().get(side_u64) != simdjson::SUCCESS)
                    {
                        return false;
                    }
                    if (side_u64 == 0U)
                    {
                        is_bid = false;
                    }
                    else if (side_u64 == 1U)
                    {
                        is_bid = true;
                    }
                    else
                    {
                        return false;
                    }
                }
            }
        }

        out_tick.price = static_cast<std::uint8_t>(price_cents);
        out_tick.size = size;
        out_tick.is_bid = is_bid;
        return true;
    }

private:
    simdjson::ondemand::parser parser_{};
    bool initialized_{false};

    alignas(64) std::array<char, kMaxPayloadBytes + simdjson::SIMDJSON_PADDING> buffer_{};
};
