#ifndef SPSC_QUEUE_HPP
#define SPSC_QUEUE_HPP


#include <cstdint>
#include <array>
#include <thread>
#include <atomic>
#include <memory>
#include <variant>
#include <optional>
#include <type_traits>

/****************************************************************
 * UniquePtrSPSCQueue: Single Producer Single Consumer Queue
 ****************************************************************/

template<size_t QueueSize, typename... DataTypes>
class UniquePtrSPSCQueue {
   private:
    using UniquePtrVariant = std::variant<std::unique_ptr<DataTypes>...>;

    struct QueueItem {
        template<typename T>
        QueueItem(std::unique_ptr<T>&& ptr) noexcept
            : data(std::move(ptr)) {
            static_assert((std::is_same_v<T, DataTypes> || ...), "Invalid type");
        }

        QueueItem() noexcept = default;
        QueueItem(const QueueItem&) = delete;
        QueueItem(QueueItem&&) noexcept = default;
        QueueItem& operator=(const QueueItem&) = delete;
        QueueItem& operator=(QueueItem&&) noexcept = default;
        ~QueueItem() = default;

        UniquePtrVariant data;
    };

    struct alignas(64) Slot {
        Slot() noexcept : ready(false), value() {
            static_assert(offsetof(Slot, value) == 64, "value must start at cache line boundary");
        }

        Slot(const Slot&) = delete;
        Slot(Slot&&) = delete;
        Slot& operator=(const Slot&) = delete;
        Slot& operator=(Slot&&) = delete;
        ~Slot() = default;

        alignas(64) std::atomic<bool> ready;
        alignas(64) QueueItem         value;
    };

    std::array<Slot, QueueSize>     slots_; // must use array to ensure memory contiguous and aligned, do not use vector or other container
    size_t                          capacity_;
    size_t                          cap_mask_;
    alignas(64) std::atomic<size_t> writer_idx_;
    alignas(64) std::atomic<size_t> reader_idx_;

   public:
    UniquePtrSPSCQueue() {
        static_assert(std::is_nothrow_move_constructible_v<QueueItem>, "QueueItem must be nothrow move constructible");
        static_assert(std::is_nothrow_move_assignable_v<QueueItem>,"QueueItem must be nothrow move assignable");
        static_assert(sizeof(Slot) > 64 && sizeof(Slot) % 64 == 0, "Slot size must be multiple of cache line size");
        static_assert(QueueSize > 0 && (QueueSize & (QueueSize - 1)) == 0, "QueueSize must be positive power of two");
        capacity_ = QueueSize;
        cap_mask_ = capacity_ - 1;
        writer_idx_.store(0, std::memory_order_relaxed);
        reader_idx_.store(0, std::memory_order_relaxed);
    }

    UniquePtrSPSCQueue(const UniquePtrSPSCQueue&) = delete;
    UniquePtrSPSCQueue(UniquePtrSPSCQueue&&) = delete;
    UniquePtrSPSCQueue& operator=(const UniquePtrSPSCQueue&) = delete;
    UniquePtrSPSCQueue& operator=(UniquePtrSPSCQueue&&) = delete;
    ~UniquePtrSPSCQueue() = default;

    template<typename T>
    bool try_produce(std::unique_ptr<T>&& ptr) {
        static_assert((std::is_same_v<T, DataTypes> || ...), "Type T must be one of DataTypes");

        if (!ptr) {
            return false;
        }

        size_t writer_idx = writer_idx_.load(std::memory_order_relaxed);
        Slot& slot = slots_[writer_idx & cap_mask_];

        if (slot.ready.load(std::memory_order_acquire)) {
            return false;
        }

        slot.value = QueueItem(std::move(ptr));
        slot.ready.store(true, std::memory_order_release);
        writer_idx_.store(writer_idx + 1, std::memory_order_release);
        return true;
    }

    template<typename T>
    void produce(std::unique_ptr<T>&& ptr) {
        if (!ptr) {
            return;
        }

        while (!try_produce(std::move(ptr))) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    std::optional<UniquePtrVariant> try_consume() {
        size_t reader_idx = reader_idx_.load(std::memory_order_relaxed);
        Slot& slot = slots_[reader_idx & cap_mask_];

        if (!slot.ready.load(std::memory_order_acquire)) {
            return std::nullopt;
        }

        UniquePtrVariant data = std::move(slot.value.data);
        slot.ready.store(false, std::memory_order_release);
        reader_idx_.store(reader_idx + 1, std::memory_order_release);
        return std::move(data);
    }

    UniquePtrVariant consume() {
        std::optional<UniquePtrVariant> result;
        while (!(result = try_consume())) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return std::move(*result);
    }

    template<typename Visitor>
    bool try_visit(Visitor&& visitor) {
        auto data = try_consume();
        if (!data) {
            return false;
        }

        std::visit([&visitor](auto&& ptr) {
            if (ptr) {
                visitor(std::move(ptr));
            }
        }, std::move(*data));

        return true;
    }

    template<typename Visitor>
    void visit(Visitor&& visitor) {
        auto data = consume();
        std::visit([&visitor](auto&& ptr) {
            if (ptr) {
                visitor(std::move(ptr));
            }
        }, std::move(data));
    }

    template<typename T>
    std::optional<std::unique_ptr<T>> try_consume_as() {
        static_assert(sizeof...(DataTypes) == 1, "try_consume_as is only available when queue has exactly one data type");
        static_assert(std::is_same_v<T, std::tuple_element_t<0, std::tuple<DataTypes...>>>, "try_consume_as<T> requires T to be the queue's single data type");

        auto data = try_consume();
        if (!data) {
            return std::nullopt;
        }

        try {
            return std::get<std::unique_ptr<T>>(std::move(*data));
        } catch (...) {
            return std::nullopt;
        }
    }

    template<typename T>
    std::unique_ptr<T> consume_as() {
        static_assert(sizeof...(DataTypes) == 1, "try_consume_as is only available when queue has exactly one data type");
        static_assert(std::is_same_v<T, std::tuple_element_t<0, std::tuple<DataTypes...>>>, "try_consume_as<T> requires T to be the queue's single data type");

        std::optional<std::unique_ptr<T>> result;
        while (!(result = try_consume_as<T>())) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return std::move(*result);
    }

    bool empty() const {
        return reader_idx_.load(std::memory_order_acquire) == writer_idx_.load(std::memory_order_acquire);
    }

    size_t size() const {
        return writer_idx_.load(std::memory_order_acquire) - reader_idx_.load(std::memory_order_acquire);
    }

    bool full() const {
        return writer_idx_.load(std::memory_order_acquire) - reader_idx_.load(std::memory_order_acquire) == capacity_;
    }

    size_t capacity() {
        return capacity_;
    }

    void clear() {
        while (!empty()) {
            consume();
        }
    }
};


#endif // SPSC_QUEUE_HPP
