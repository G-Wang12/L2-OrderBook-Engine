#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

template <typename T, std::size_t Size>
class SpscQueue
{
    static_assert(Size >= 2, "Size must be >= 2");
    static_assert((Size & (Size - 1U)) == 0U, "Size must be a power of 2");

public:
    constexpr SpscQueue() noexcept = default;
    SpscQueue(const SpscQueue &) = delete;
    SpscQueue &operator=(const SpscQueue &) = delete;

    ~SpscQueue() noexcept
    {
        // Not thread-safe; destruction must occur after producer/consumer stop.
        std::size_t read = read_index_.load(std::memory_order_relaxed);
        const std::size_t write = write_index_.load(std::memory_order_relaxed);
        while (read != write)
        {
            const std::size_t slot = read & kMask;
            std::destroy_at(slot_ptr(slot));
            ++read;
        }
    }

    [[nodiscard]] bool push(const T &item) noexcept(std::is_nothrow_copy_constructible_v<T>)
    {
        const std::size_t write = write_index_.load(std::memory_order_relaxed);
        const std::size_t read = read_index_.load(std::memory_order_acquire);

        if ((write - read) == Size)
        {
            return false; // full
        }

        const std::size_t slot = write & kMask;
        T *const ptr = slot_ptr(slot);
        std::construct_at(ptr, item);

        write_index_.store(write + 1U, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool pop(T &item) noexcept(std::is_nothrow_move_assignable_v<T> && std::is_nothrow_destructible_v<T>)
    {
        const std::size_t read = read_index_.load(std::memory_order_relaxed);
        const std::size_t write = write_index_.load(std::memory_order_acquire);

        if (read == write)
        {
            return false; // empty
        }

        const std::size_t slot = read & kMask;
        T *const ptr = slot_ptr(slot);
        item = std::move(*ptr);
        std::destroy_at(ptr);

        read_index_.store(read + 1U, std::memory_order_release);
        return true;
    }

private:
    static constexpr std::size_t kMask = Size - 1U;

    using Storage = std::aligned_storage_t<sizeof(T), alignof(T)>;

    alignas(64) std::array<Storage, Size> buffer_{};

    alignas(64) std::atomic<std::size_t> write_index_{0U};
    alignas(64) std::atomic<std::size_t> read_index_{0U};

    [[nodiscard]] T *slot_ptr(std::size_t slot) noexcept
    {
        void *const addr = static_cast<void *>(&buffer_[slot]);
        return std::launder(reinterpret_cast<T *>(addr));
    }

    [[nodiscard]] const T *slot_ptr(std::size_t slot) const noexcept
    {
        const void *const addr = static_cast<const void *>(&buffer_[slot]);
        return std::launder(reinterpret_cast<const T *>(addr));
    }
};
