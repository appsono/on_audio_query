#ifndef LRU_CACHE_H_
#define LRU_CACHE_H_

#include <unordered_map>
#include <list>
#include <utility>
#include <optional>
#include <mutex>

namespace on_audio_query_linux {

/// Thread-safe LRU (Least Recently Used) cache
template<typename Key, typename Value>
class LRUCache {
 public:
  explicit LRUCache(size_t capacity) : capacity_(capacity) {}

  /// Get value from cache
  std::optional<Value> Get(const Key& key) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = cache_map_.find(key);
    if (it == cache_map_.end()) {
      return std::nullopt;  //not found
    }

    /// Move to front (most recently used)
    cache_list_.splice(cache_list_.begin(), cache_list_, it->second);

    return it->second->second;
  }

  /// Put value into cache
  void Put(const Key& key, const Value& value) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = cache_map_.find(key);

    if (it != cache_map_.end()) {
      //key exists => update value and move to front
      it->second->second = value;
      cache_list_.splice(cache_list_.begin(), cache_list_, it->second);
      return;
    }

    //key doesnt exist => add new entry
    if (cache_list_.size() >= capacity_) {
      //remove least recently used (back of list)
      auto last = cache_list_.back();
      cache_map_.erase(last.first);
      cache_list_.pop_back();
    }

    //add to front
    cache_list_.emplace_front(key, value);
    cache_map_[key] = cache_list_.begin();
  }

  /// Check if key exists
  bool Contains(const Key& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_map_.find(key) != cache_map_.end();
  }

  /// Clear cache
  void Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_map_.clear();
    cache_list_.clear();
  }

  /// Get current size
  size_t Size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_list_.size();
  }

 private:
  size_t capacity_;
  mutable std::mutex mutex_;

  /// List of (key, value) pairs => front is most recently used
  std::list<std::pair<Key, Value>> cache_list_;

  /// Map of key => iterator in cache_list_
  std::unordered_map<Key, typename std::list<std::pair<Key, Value>>::iterator> cache_map_;
};

}  // namespace on_audio_query_linux

#endif  // LRU_CACHE_H_
