/**
 * @file Executor.h
 * @brief A single worker thread with a task queue, for backends.
 *
 * ARCHITECTURE decision 8: session calls may arrive on any thread and must
 * return fast; the backend does its target work on its own thread. A backend
 * takes what it needs from the SessionView during Sync (the view is only valid
 * for that call), posts the expensive work here, and implements
 * Backend::Flush() with Wait().
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

namespace fccvis::backend {

/** @brief Runs posted tasks in order on one thread. */
class Executor {
 public:
  /** @brief Task signature. */
  using Task = std::function<void()>;
  /** @brief Receives the message of an exception thrown by a task. */
  using ErrorHandler = std::function<void(const std::string&)>;

  /** @param onError Called (on the worker thread) when a task throws. */
  explicit Executor(ErrorHandler onError = {}) : m_onError(std::move(onError)) {
    m_thread = std::thread([this] { Loop(); });
  }

  /** @brief Finishes queued tasks, then stops the thread. */
  ~Executor() {
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_stop = true;
    }
    m_cv.notify_all();
    if (m_thread.joinable()) m_thread.join();
  }

  Executor(const Executor&) = delete;
  Executor& operator=(const Executor&) = delete;

  /** @brief Queues a task; returns immediately. */
  void Post(Task task) {
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_tasks.push_back(std::move(task));
    }
    m_cv.notify_all();
  }

  /** @brief Blocks until every task posted before this call has run. */
  void Wait() {
    if (std::this_thread::get_id() == m_thread.get_id()) return;  // called from a task
    std::unique_lock<std::mutex> lock(m_mutex);
    m_idle.wait(lock, [this] { return m_tasks.empty() && !m_busy; });
  }

  /** @brief True if called from the worker thread. */
  [[nodiscard]] bool OnWorker() const { return std::this_thread::get_id() == m_thread.get_id(); }

 private:
  void Loop() {
    std::unique_lock<std::mutex> lock(m_mutex);
    for (;;) {
      m_cv.wait(lock, [this] { return m_stop || !m_tasks.empty(); });
      if (m_tasks.empty()) {
        if (m_stop) return;
        continue;
      }
      Task t = std::move(m_tasks.front());
      m_tasks.pop_front();
      m_busy = true;
      lock.unlock();
      try {
        t();
      } catch (const std::exception& e) {
        if (m_onError) m_onError(e.what());
      } catch (...) {
        if (m_onError) m_onError("unknown exception");
      }
      lock.lock();
      m_busy = false;
      if (m_tasks.empty()) m_idle.notify_all();
    }
  }

  ErrorHandler m_onError;
  std::mutex m_mutex;
  std::condition_variable m_cv;
  std::condition_variable m_idle;
  std::deque<Task> m_tasks;
  bool m_busy = false;
  bool m_stop = false;
  std::thread m_thread;
};

}  // namespace fccvis::backend
