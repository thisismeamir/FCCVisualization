// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
#include "RootRuntime.h"

#include <TApplication.h>
#include <TEveBrowser.h>
#include <TEveManager.h>
#include <TROOT.h>
#include <TSystem.h>

#include <chrono>
#include <cstdlib>
#include <stdexcept>

namespace fccvis::backend::root {

RootRuntime& RootRuntime::Instance() {
  static RootRuntime runtime;
  return runtime;
}

RootRuntime::~RootRuntime() { Shutdown(); }

void RootRuntime::Shutdown() {
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stop = true;
    m_alive = false;
    m_tasks.clear();
  }
  m_cv.notify_all();
  if (m_thread.joinable() && m_thread.get_id() != std::this_thread::get_id()) m_thread.join();
}

void RootRuntime::Start() {
  std::unique_lock<std::mutex> lock(m_mutex);
  if (!m_started) {
    m_started = true;
    m_thread = std::thread([this] { Loop(); });
  }
  m_cv.wait(lock, [this] { return m_ready; });
  if (!m_error.empty()) throw std::runtime_error(m_error);
  static bool registered = false;  // guarded by m_mutex
  if (!registered) {
    registered = true;
    std::atexit([] { RootRuntime::Instance().Shutdown(); });  // runs before ROOT's own exit handlers
  }
}

void RootRuntime::Call(const std::function<void()>& task) {
  Start();
  if (std::this_thread::get_id() == m_thread.get_id()) {
    task();
    return;
  }
  std::exception_ptr error;
  bool done = false;
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_stop) throw std::runtime_error("the ROOT thread has stopped");
    m_tasks.push_back([&] {
      try {
        task();
      } catch (...) {
        error = std::current_exception();
      }
      std::lock_guard<std::mutex> l(m_mutex);
      done = true;
    });
  }
  m_cv.notify_all();
  std::unique_lock<std::mutex> lock(m_mutex);
  m_cv.wait(lock, [&] { return done || m_stop; });
  if (!done) throw std::runtime_error("the ROOT thread has stopped");
  lock.unlock();
  if (error) std::rethrow_exception(error);
}

void RootRuntime::Ensure() {
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_classic && !m_alive)
      throw std::runtime_error("the ROOT Eve window was closed; restart the program to open it again");
    if (m_alive) return;
  }
  const char* display = std::getenv("DISPLAY");
  if (!display || !*display)
    throw std::runtime_error("classic TEve needs an X display and DISPLAY is not set (run under X, or under xvfb-run)");
  Call([] {
    gROOT->SetBatch(kFALSE);
    if (!gEve && !TEveManager::Create(kTRUE, "V")) throw std::runtime_error("TEveManager::Create failed");
  });
  std::lock_guard<std::mutex> lock(m_mutex);
  m_classic = true;
  m_alive = true;
}

void RootRuntime::Post(std::function<void()> task) {
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_stop) return;
    m_tasks.push_back(std::move(task));
  }
  m_cv.notify_all();
}

void RootRuntime::Wait() {
  std::unique_lock<std::mutex> lock(m_mutex);
  if (!m_started) return;
  m_cv.wait(lock, [this] { return (m_tasks.empty() && !m_busy) || m_stop; });
}

void RootRuntime::AddPoller(const void* owner, std::function<void()> poll) {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_pollers[owner] = std::move(poll);
}

void RootRuntime::RemovePoller(const void* owner) {
  std::unique_lock<std::mutex> lock(m_mutex);
  m_pollers.erase(owner);
  if (m_thread.get_id() == std::this_thread::get_id()) return;
  m_cv.wait(lock, [this] { return !m_busy || m_stop; });
}

bool RootRuntime::Alive() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_alive;
}

void RootRuntime::Loop() {
  // Start-up on this thread: everything ROOT-graphical lives here.
  ROOT::EnableThreadSafety();
  static int argc = 1;
  static char name[] = "fccvis";
  static char* argv[] = {name, nullptr};
  if (!gApplication) new TApplication("fccvis", &argc, argv);  // never deleted, see header
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_ready = true;
  }
  m_cv.notify_all();

  while (true) {
    std::deque<std::function<void()>> tasks;
    std::map<const void*, std::function<void()>> pollers;
    {
      std::unique_lock<std::mutex> lock(m_mutex);
      m_cv.wait_for(lock, std::chrono::milliseconds(10), [this] { return m_stop || !m_tasks.empty(); });
      if (m_stop) return;
      tasks.swap(m_tasks);
      pollers = m_pollers;
      m_busy = true;
    }
    for (auto& t : tasks) {
      try {
        t();
      } catch (...) {
      }
    }
    gSystem->ProcessEvents();
    for (auto& [owner, poll] : pollers) {
      try {
        poll();
      } catch (...) {
      }
    }
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_busy = false;
      if (m_classic && m_alive && !gEve) m_alive = false;  // main window closed: TEveManager::Terminate deleted gEve
    }
    m_cv.notify_all();
  }
}

}  // namespace fccvis::backend::root
