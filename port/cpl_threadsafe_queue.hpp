/******************************************************************************
 *
 * Project:  CPL
 * Purpose:  Implementation of a thread-safe queue
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef CPL_THREADSAFE_QUEUE_INCLUDED
#define CPL_THREADSAFE_QUEUE_INCLUDED

#include <condition_variable>
#include <mutex>
#include <queue>

namespace cpl
{
template <class T> class ThreadSafeQueue
{
  private:
    mutable std::mutex m_mutex{};
    std::condition_variable m_cv{};
    std::queue<T> m_queue{};

  public:
    ThreadSafeQueue() = default;

    void clear()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        while (!m_queue.empty())
            m_queue.pop();
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }

    size_t size() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size();
    }

    void push(const T &value)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(value);
        m_cv.notify_one();
    }

    void push(T &&value)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(std::move(value));
        m_cv.notify_one();
    }

    T get_and_pop_front()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (m_queue.empty())
        {
            m_cv.wait(lock);
        }
        T val = m_queue.front();
        m_queue.pop();
        return val;
    }
};

}  // namespace cpl

#endif  // CPL_THREADSAFE_QUEUE_INCLUDED
