/**
 * @file RootRuntime.h
 * @brief The one process-wide ROOT GUI runtime shared by all ROOT backend instances.
 *
 * ARCHITECTURE decision 11: all ROOT graphics calls run on exactly one
 * thread. The runtime owns that thread: it creates the TApplication and the
 * TEveManager there, runs posted tasks, pumps the ROOT event loop
 * (`gSystem->ProcessEvents()`, never `Run()`) and calls registered pollers
 * (picks, camera moves, window closed) after every pump.
 *
 * The thread is stopped by an std::atexit handler registered after ROOT's
 * own, so it no longer pumps events while ROOT tears itself down at exit.
 *
 * Classic TEve (Ensure()) needs an X display; other ROOT viewers (REve) only
 * need the thread (Start(), Call()).
 *
 * The TApplication is never destroyed (destroying it after TEve has run
 * crashes in ROOT 6.38; see docs/design/root-experiments.md).
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>

namespace fccvis::backend::root {

/** @brief Process-wide ROOT GUI thread. */
class RootRuntime {
 public:
  /** @brief The runtime (created on first use). */
  static RootRuntime& Instance();

  /** @brief Starts the ROOT thread (with the TApplication) if needed; waits until ready. */
  void Start();

  /**
   * @brief Starts the thread and classic TEve if needed; waits until ready.
   * @throws std::runtime_error when TEve cannot start (no X display, or the
   *         Eve window was closed earlier in this process).
   */
  void Ensure();

  /**
   * @brief Runs @p task on the ROOT thread and waits for it (starts the
   *        thread if needed). An exception thrown by @p task is rethrown here.
   */
  void Call(const std::function<void()>& task);

  /** @brief Runs @p task on the ROOT thread (asynchronously). */
  void Post(std::function<void()> task);

  /** @brief Waits until every task posted so far has run. */
  void Wait();

  /** @brief Calls @p poll on the ROOT thread after every event-loop pump. */
  void AddPoller(const void* owner, std::function<void()> poll);
  /** @brief Removes the poller of @p owner (waits if it is running). */
  void RemovePoller(const void* owner);

  /** @brief True while classic TEve is alive (false before Ensure() and after its main window was closed). */
  [[nodiscard]] bool Alive() const;

  /** @brief Stops the ROOT thread (idempotent); registered with std::atexit by Ensure(). */
  void Shutdown();

  ~RootRuntime();

 private:
  RootRuntime() = default;
  void Loop();

  mutable std::mutex m_mutex;
  std::condition_variable m_cv;
  std::deque<std::function<void()>> m_tasks;
  std::map<const void*, std::function<void()>> m_pollers;
  std::thread m_thread;
  bool m_started = false;
  bool m_ready = false;
  bool m_stop = false;
  bool m_busy = false;
  bool m_alive = false;          // classic TEve alive
  bool m_classic = false;        // classic TEve was started in this process
  std::string m_error;
};

}  // namespace fccvis::backend::root
