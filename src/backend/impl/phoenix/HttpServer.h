/**
 * @file HttpServer.h
 * @brief Minimal HTTP/1.0 file server for the Phoenix backend (GET only).
 *
 * Serves the files of one directory (no subdirectories, no `..`) with
 * `Access-Control-Allow-Origin: *`, so a Phoenix page on another origin can
 * load them, and `Cache-Control: no-store`, so reloading fetches new events.
 * `/` returns a small index page. One request per connection, on one thread.
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <thread>

namespace fccvis::backend::phoenix {

/** @brief Serves a directory over HTTP on localhost or all interfaces. */
class HttpServer {
 public:
  /** @brief Index page generator (called per request to `/`). */
  using IndexPage = std::function<std::string()>;

  HttpServer() = default;
  ~HttpServer();
  HttpServer(const HttpServer&) = delete;
  HttpServer& operator=(const HttpServer&) = delete;

  /**
   * @brief Starts serving @p directory.
   * @param port    TCP port; 0 picks a free one (see Port()).
   * @param address Bind address, for example "127.0.0.1" or "0.0.0.0".
   * @throws std::runtime_error if the socket cannot be bound.
   */
  void Start(const std::string& directory, int port, const std::string& address, IndexPage index);
  /** @brief Stops the server thread. */
  void Stop();
  /** @brief Port actually bound (after Start). */
  [[nodiscard]] int Port() const { return m_port; }

 private:
  void Loop();
  void Handle(int client);
  std::string m_dir;
  IndexPage m_index;
  int m_fd = -1;
  int m_port = 0;
  std::atomic<bool> m_stop{false};
  std::thread m_thread;
};

}  // namespace fccvis::backend::phoenix
