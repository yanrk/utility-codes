#ifndef BITMAP_UID_ALLOCATOR_HPP
#define BITMAP_UID_ALLOCATOR_HPP


#include <cstdint>
#include <array>
#include <limits>

template <uint32_t MIN_UID, uint32_t MAX_UID, bool CYCLIC = true>
class BitmapUIDAllocator {
    static_assert(MIN_UID < MAX_UID, "MIN_UID must be less than MAX_UID");

   private:
    static constexpr uint32_t UID_COUNT = (MAX_UID - MIN_UID) + 1;
    static constexpr uint32_t UID_SLOTS = (UID_COUNT + 63) / 64;

    std::array<uint64_t, UID_SLOTS> bitmap_{};
    uint32_t bit_ = 0;
    uint32_t index_ = 0;
    uint32_t count_ = 0;

   private:
    uint32_t get(uint32_t index, uint32_t bit) const {
        return static_cast<uint32_t>(index * 64 + bit + MIN_UID);
    }

    int find(uint32_t index) const {
        uint64_t v = bitmap_[index];
#if defined(__GNUG__) || defined(__clang__)
        int pos = __builtin_ffsll(~v); // find first 1 bit, 1-based
        return pos > 0 ? pos - 1 : -1; // convert to 0-based
#elif defined(_MSC_VER)
        unsigned long pos = 0;
        return _BitScanForward64(&pos, ~v) ? static_cast<int>(pos) : -1;
#else
        uint64_t nv = ~v;
        uint64_t mk = 1;
        for (int i = 0; i < 64; ++i) {
            if (nv & mk) {
                return i;
            }
            mk <<= 1;
        }
        return -1;
#endif
    }

    void set(uint32_t index, uint32_t bit) {
        bitmap_[index] |= (1ULL << bit);
    }

    bool has(uint32_t index, uint32_t bit) const {
        return (bitmap_[index] & (1ULL << bit)) != 0;
    }

    bool full(uint32_t index) const {
        return bitmap_[index] == std::numeric_limits<uint64_t>::max();
    }

    void clear(uint32_t index, uint32_t bit) {
        bitmap_[index] &= ~(1ULL << bit);
    }

   public:
    uint32_t allocate() {
        if (count_ >= UID_COUNT) {
            return 0;
        }

        uint32_t start = index_;
        if (CYCLIC) {
            if (!full(index_)) {
                for (uint32_t i = bit_; i < 64; ++i) {
                    uint32_t uid = get(index_, i);
                    if (uid > MAX_UID) {
                        break;
                    }

                    if (has(index_, i)) {
                        continue;
                    }

                    set(index_, i);
                    bit_ = i + 1;
                    ++count_;
                    return uid;
                }
            }
            start = (index_ + 1) % UID_SLOTS;
        }

        for (uint32_t i = 0; i < UID_SLOTS; ++i) {
            uint32_t index = (start + i) % UID_SLOTS;
            if (full(index)) {
                continue;
            }

            int bit = find(index);
            if (bit < 0) {
                continue;
            }

            uint32_t uid = get(index, bit);
            if (uid > MAX_UID) {
                continue;
            }

            set(index, bit);
            bit_ = bit + 1;
            index_ = index;
            ++count_;
            return uid;
        }

        return 0;
    }

    void release(uint32_t uid) {
        if (uid < MIN_UID || uid > MAX_UID) {
            return;
        }

        uint32_t zero_based = uid - MIN_UID;
        uint32_t index = zero_based / 64;
        uint32_t bit = zero_based % 64;
        if (!has(index, bit)) {
            return;
        }

        clear(index, bit);
        --count_;
    }

    void clear() {
        bitmap_.fill(0);
        bit_ = 0;
        index_ = 0;
        count_ = 0;
    }

    uint32_t capacity() const {
        return UID_COUNT;
    }

    uint32_t size() const {
        return count_;
    }

    bool empty() const {
        return count_ == 0;
    }

    bool full() const {
        return count_ == UID_COUNT;
    }
};


#endif // BITMAP_UID_ALLOCATOR_HPP
