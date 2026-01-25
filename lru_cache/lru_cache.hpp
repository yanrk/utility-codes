#ifndef LRU_CACHE_HPP
#define LRU_CACHE_HPP


#include <list>
#include <utility>
#include <unordered_map>

/****************************************************************
 * LRUCache: Least Recently Used Cache
 ****************************************************************/

template<typename Key, typename Value>
class LRUCache {
   public:
    using ListType = std::list<std::pair<Key, Value>>;
    using ListIterator = typename ListType::iterator;
    using MapType = std::unordered_map<Key, ListIterator>;
    using MapIterator = typename MapType::iterator;
    using EmplaceResult = std::pair<MapIterator, bool>;

    explicit LRUCache(size_t capacity) : capacity_(capacity > 0 ? capacity : 1) {

    }

    MapIterator find(const Key& key) {
        return map_.find(key);
    }

    MapIterator end() {
        return map_.end();
    }

    EmplaceResult emplace(const Key& key, Value&& value) {
        auto it = map_.find(key);
        if (it != map_.end()) {
            items_.splice(items_.begin(), items_, it->second);
            it->second->second = std::forward<Value>(value);
            return {it, false};
        }
        if (items_.size() >= capacity_) {
            auto last = items_.end();
            --last;
            map_.erase(last->first);
            items_.pop_back();
        }
        items_.emplace_front(key, std::forward<Value>(value));
        auto mapIt = map_.emplace(key, items_.begin());
        return {mapIt.first, true};
    }

    Value& at(const Key& key) {
        auto it = map_.find(key);
        if (it == map_.end()) {
            throw std::out_of_range("Key not found");
        }
        items_.splice(items_.begin(), items_, it->second);
        return it->second->second;
    }

    void clear() {
        items_.clear();
        map_.clear();
    }

    size_t size() const {
        return items_.size();
    }

    size_t capacity() const {
        return capacity_;
    }

   private:
    size_t   capacity_;
    ListType items_;
    MapType  map_;
};


#endif // LRU_CACHE_HPP
