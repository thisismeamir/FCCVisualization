// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
#include "HttpServer.h"

#include <arpa/inet.h>
#include <fstream>
#include <netinet/in.h>
#include <poll.h>
#include <sstream>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

namespace fccvis::backend::phoenix {

namespace {

std::string ContentType(const std::string& name) {
  auto ends = [&](const char* s) {
    const std::string x(s);
    return name.size() >= x.size() && name.compare(name.size() - x.size(), x.size(), x) == 0;
  };
  if (ends(".json")) return "application/json";
  if (ends(".obj") || ends(".mtl") || ends(".txt")) return "text/plain";
  if (ends(".html")) return "text/html; charset=utf-8";
  if (ends(".png")) return "image/png";
  if (ends(".svg")) return "image/svg+xml";
  return "application/octet-stream";
}

void SendAll(int fd, const std::string& data) {
  std::size_t sent = 0;
  while (sent < data.size()) {
    const auto n = ::send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
    if (n <= 0) return;
    sent += static_cast<std::size_t>(n);
  }
}

void Reply(int fd, int status, const std::string& reason, const std::string& type, const std::string& body,
           bool headOnly = false) {
  std::string head = "HTTP/1.0 " + std::to_string(status) + " " + reason + "\r\n";
  head += "Content-Type: " + type + "\r\nContent-Length: " + std::to_string(body.size()) +
          "\r\nAccess-Control-Allow-Origin: *\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n";
  SendAll(fd, headOnly ? head : head + body);  // HEAD: the GET headers (same length), no body
}

}  // namespace

HttpServer::~HttpServer() { Stop(); }

void HttpServer::Start(const std::string& directory, int port, const std::string& address, IndexPage index) {
  m_dir = directory;
  m_index = std::move(index);
  m_fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (m_fd < 0) throw std::runtime_error("cannot create socket");
  int one = 1;
  ::setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<std::uint16_t>(port));
  if (::inet_pton(AF_INET, address.c_str(), &addr.sin_addr) != 1) {
    ::close(m_fd);
    m_fd = -1;
    throw std::runtime_error("bad bind address '" + address + "'");
  }
  if (::bind(m_fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0 || ::listen(m_fd, 16) != 0) {
    ::close(m_fd);
    m_fd = -1;
    throw std::runtime_error("cannot listen on " + address + ":" + std::to_string(port));
  }
  socklen_t len = sizeof addr;
  ::getsockname(m_fd, reinterpret_cast<sockaddr*>(&addr), &len);
  m_port = ntohs(addr.sin_port);
  m_stop = false;
  m_thread = std::thread([this] { Loop(); });
}

void HttpServer::Stop() {
  m_stop = true;
  if (m_thread.joinable()) m_thread.join();
  if (m_fd >= 0) ::close(m_fd);
  m_fd = -1;
}

void HttpServer::Loop() {
  while (!m_stop) {
    pollfd p{m_fd, POLLIN, 0};
    if (::poll(&p, 1, 100) <= 0) continue;
    const int client = ::accept(m_fd, nullptr, nullptr);
    if (client < 0) continue;
    Handle(client);
    ::close(client);
  }
}

void HttpServer::Handle(int client) {
  std::string request;
  char buf[2048];
  while (request.find("\r\n\r\n") == std::string::npos && request.size() < 16384) {
    pollfd p{client, POLLIN, 0};
    if (::poll(&p, 1, 1000) <= 0) return;
    const auto n = ::recv(client, buf, sizeof buf, 0);
    if (n <= 0) break;
    request.append(buf, static_cast<std::size_t>(n));
  }
  std::istringstream line(request.substr(0, request.find("\r\n")));
  std::string method, target;
  line >> method >> target;
  if (method == "OPTIONS") {
    Reply(client, 204, "No Content", "text/plain", "");
    return;
  }
  if (method != "GET" && method != "HEAD") {
    Reply(client, 405, "Method Not Allowed", "text/plain", "only GET\n");
    return;
  }
  if (const auto q = target.find('?'); q != std::string::npos) target.erase(q);
  if (target == "/" || target.empty()) {
    Reply(client, 200, "OK", "text/html; charset=utf-8", m_index ? m_index() : std::string());
    return;
  }
  const std::string name = target.substr(1);
  if (name.empty() || name.find('/') != std::string::npos || name.find("..") != std::string::npos || name[0] == '.') {
    Reply(client, 404, "Not Found", "text/plain", "not found\n");
    return;
  }
  std::ifstream in(m_dir + "/" + name, std::ios::binary);
  if (!in) {
    Reply(client, 404, "Not Found", "text/plain", "not found\n");
    return;
  }
  std::stringstream ss;
  ss << in.rdbuf();
  Reply(client, 200, "OK", ContentType(name), ss.str(), method == "HEAD");
}

}  // namespace fccvis::backend::phoenix
