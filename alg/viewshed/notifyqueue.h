/******************************************************************************
 * (c) 2024 info@hobu.co
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef VIEWSHED_NOTIFYQUEUE_H_INCLUDED
#define VIEWSHED_NOTIFYQUEUE_H_INCLUDED

#include "cpl_port.h"

#include <condition_variable>
#include <mutex>
#include <queue>

namespace gdal
{
namespace viewshed
{

/// This is a thread-safe queue. Things placed in the queue must be move-constructible.
/// Readers will wait until there is something in the queue or the queue is empty or stopped.
/// If the queue is stopped (error), it will never be in the done state. If in the
/// done state (all writers have finished), it will never be in the error state.
template <class T> class NotifyQueue
{
  public:
    /// Destructor
    ~NotifyQueue()
    {
        done();
    }

    /// Push an object on the queue and notify readers.
    /// \param t  Object to be moved onto the queue.
    void push(T &&t)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push(std::move(t));
        }
        m_cv.notify_all();
    }

    /// Get an item from the queue.
    /// \param t  Reference to an item to to which a queued item will be moved.
    /// \return True if an item was popped. False otherwise.  Use isStopped() or isDone()
    ///    to determine the state if you care when false is returned.
    bool pop(T &t)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock,
                  [this] { return !m_queue.empty() || m_done || m_stop; });

        if (m_stop)
            return false;

        if (m_queue.size())
        {
            t = std::move(m_queue.front());
            m_queue.pop();
            return true;
        }

        // m_done must be true and the queue is empty.
        return false;
    }

    /// When we're done putting things in the queue, set the end condition.
    void done()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_done = !m_stop;  // If we're already stopped, we can't be done.
        }
        m_cv.notify_all();
    }

    /// Unblock all readers regardless of queue state.
    void stop()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stop = !m_done;  // If we're already done, we can't be stopped.
        }
        m_cv.notify_all();
    }

    /// Determine if the queue was emptied completely. Call after pop() returns false
    /// to check queue state.
    /// \return  Whether the queue was emptied completely.
    bool isDone()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_done;
    }

    /// Determine if the queue was stopped. Call after pop() returns false
    /// to check queue state.
    /// \return  Whether the queue was stopped.
    bool isStopped()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_stop;
    }

    /// Get the current size of the queue.
    /// \return Current queue size.
    size_t size() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size();
    }

  private:
    std::queue<T> m_queue{};
    mutable std::mutex m_mutex{};
    std::condition_variable m_cv{};
    bool m_done{false};
    bool m_stop{false};
};

}  // namespace viewshed
}  // namespace gdal

#endif
