/*
 * LRUCache11 - a templated C++11 based LRU cache class that allows
 * specification of
 * key, value and optionally the map container type (defaults to
 * std::unordered_map)
 * By using the std::map and a linked list of keys it allows O(1) insert, delete
 * and
 * refresh operations.
 *
 * This is a header-only library and all you need is the LRUCache11.hpp file
 *
 * Github: https://github.com/mohaps/lrucache11
 *
 * This is a follow-up to the LRUCache project -
 * https://github.com/mohaps/lrucache
 *
 * Copyright (c) 2012-22 SAURAV MOHAPATRA <mohaps@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*! @cond Doxygen_Suppress */

#pragma once
#include <algorithm>
#include <cstdint>
#include <list>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_map>

namespace lru11 {
/*
 * a noop lockable concept that can be used in place of std::mutex
 */
class NullLock {
 public:
  void lock() {}
  void unlock() {}
  bool try_lock() { return true; }
};

/**
 * error raised when a key not in cache is passed to get()
 */
class KeyNotFound : public std::invalid_argument {
 public:
  KeyNotFound() : std::invalid_argument("key_not_found") {}
};

template <typename K, typename V>
struct KeyValuePair {
 public:
  K key;
  V value;

  KeyValuePair(const K& k, const V& v) : key(k), value(v) {}
};

/**
 *	The LRU Cache class templated by
 *		Key - key type
 *		Value - value type
 *		MapType - an associative container like std::unordered_map
 *		LockType - a lock type derived from the Lock class (default:
 *NullLock = no synchronization)
 *
 *	The default NullLock based template is not thread-safe, however passing
 *Lock=std::mutex will make it
 *	thread-safe
 */
template <class Key, class Value, class Lock = NullLock,
          class Map = std::unordered_map<
              Key, typename std::list<KeyValuePair<Key, Value>>::iterator>>
class Cache {
 public:
  typedef KeyValuePair<Key, Value> node_type;
  typedef std::list<KeyValuePair<Key, Value>> list_type;
  typedef Map map_type;
  typedef Lock lock_type;
  using Guard = std::lock_guard<lock_type>;
  /**
   * the max size is the hard limit of keys and (maxSize + elasticity) is the
   * soft limit
   * the cache is allowed to grow till maxSize + elasticity and is pruned back
   * to maxSize keys
   * set maxSize = 0 for an unbounded cache (but in that case, you're better off
   * using a std::unordered_map
   * directly anyway! :)
   */
  explicit Cache(size_t maxSize = 64, size_t elasticity = 10)
      : maxSize_(maxSize), elasticity_(elasticity) {}
  virtual ~Cache() = default;
  size_t size() const {
    Guard g(lock_);
    return cache_.size();
  }
  bool empty() const {
    Guard g(lock_);
    return cache_.empty();
  }
  void clear() {
    Guard g(lock_);
    cache_.clear();
    keys_.clear();
  }
  void insert(const Key& k, const Value& v) {
    Guard g(lock_);
    const auto iter = cache_.find(k);
    if (iter != cache_.end()) {
      iter->second->value = v;
      keys_.splice(keys_.begin(), keys_, iter->second);
      return;
    }

    keys_.emplace_front(k, v);
    cache_[k] = keys_.begin();
    prune();
  }
  bool tryGet(const Key& kIn, Value& vOut) {
    Guard g(lock_);
    const auto iter = cache_.find(kIn);
    if (iter == cache_.end()) {
      return false;
    }
    keys_.splice(keys_.begin(), keys_, iter->second);
    vOut = iter->second->value;
    return true;
  }
  /**
   *	The const reference returned here is only
   *    guaranteed to be valid till the next insert/delete
   */
  const Value& get(const Key& k) {
    Guard g(lock_);
    const auto iter = cache_.find(k);
    if (iter == cache_.end()) {
      throw KeyNotFound();
    }
    keys_.splice(keys_.begin(), keys_, iter->second);
    return iter->second->value;
  }
  /**
   * returns a copy of the stored object (if found)
   */
  Value getCopy(const Key& k) {
   return get(k);
  }
  bool remove(const Key& k) {
    Guard g(lock_);
    auto iter = cache_.find(k);
    if (iter == cache_.end()) {
      return false;
    }
    keys_.erase(iter->second);
    cache_.erase(iter);
    return true;
  }
  bool contains(const Key& k) {
    Guard g(lock_);
    return cache_.find(k) != cache_.end();
  }

  bool getOldestEntry(Key& kOut, Value& vOut) {
    Guard g(lock_);
    if( keys_.empty() ) {
        return false;
    }
    kOut = keys_.back().key;
    vOut = keys_.back().value;
    return true;
  }

  size_t getMaxSize() const { return maxSize_; }
  size_t getElasticity() const { return elasticity_; }
  size_t getMaxAllowedSize() const { return maxSize_ + elasticity_; }
  template <typename F>
  void cwalk(F& f) const {
    Guard g(lock_);
    std::for_each(keys_.begin(), keys_.end(), f);
  }

 protected:
  size_t prune() {
    size_t maxAllowed = maxSize_ + elasticity_;
    if (maxSize_ == 0 || cache_.size() <= maxAllowed) { /* ERO: changed < to <= */
      return 0;
    }
    size_t count = 0;
    while (cache_.size() > maxSize_) {
      cache_.erase(keys_.back().key);
      keys_.pop_back();
      ++count;
    }
    return count;
  }

 private:
  // Dissallow copying.
  Cache(const Cache&) = delete;
  Cache& operator=(const Cache&) = delete;

  mutable Lock lock_{};
  Map cache_{};
  list_type keys_{};
  size_t maxSize_;
  size_t elasticity_;
};

} // namespace LRUCache11

/*! @endcond */
