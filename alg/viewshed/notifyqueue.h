#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>

namespace gdal
{

template <class T> class NotifyQueue
{
  public:
    ~NotifyQueue()
    {
        done();
    }

    // Push an object on the queue and notify readers.
    void push(T &&t)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push(std::move(t));
        }
        m_cv.notify_all();
    }

    // Get an item from the queue.
    // \return True is successful, false on failure.
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

    // When we're done putting things in the queue, set the end condition.
    void done()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_done = !m_stop;  // If we're already stopped, we can't be done.
        }
        m_cv.notify_all();
    }

    // Unblock all readers regardless of queue state.
    void stop()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stop = !m_done;  // If we're already done, we can't be stopped.
        }
        m_cv.notify_all();
    }

    // Determine if the queue was emptied completely. Call after pop() returns false
    // to check queue state.
    // \return  Whether the queue was emptied completely.
    bool isDone()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_done;
    }

    // Determine if the queue was stopped. Call after pop() returns false
    // to check queue state.
    // \return  Whether the queue was stopped.
    bool isStopped()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_stop;
    }

  private:
    std::queue<T> m_queue{};
    std::mutex m_mutex{};
    std::condition_variable m_cv{};
    bool m_done{false};
    bool m_stop{false};
};

}  // namespace gdal
