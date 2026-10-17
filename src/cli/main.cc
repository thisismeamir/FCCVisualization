// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// fccvis-shell: command shell on a session (see fccvis/api/Interpreter.h).
//
//   fccvis-shell                    interactive (line editing when stdin is a terminal)
//   fccvis-shell -f script.fcv      run a script, then exit
//   fccvis-shell -c "command" ...   run commands, then exit
//   fccvis-shell -i -f setup.fcv    run a script, then continue interactively
//
// Non-interactive runs stop at the first failing command (exit status 1)
// unless --keep-going is given.
//
// Interactive terminal: linenoise line editing with history
// ($FCCVIS_HISTORY, default ~/.fccvis_history; empty disables), Tab
// completion from Interpreter::Complete, multi-line editing, `\` at the end
// of a line continues the command. Messages that backends emit from their
// own threads (render failures, picks, stopped windows) are queued and
// printed above the prompt without corrupting the line being edited.
#include "fccvis/FccvisConfig.h"
#include "fccvis/api/Interpreter.h"

extern "C" {
#include "linenoise.h"
}

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fcntl.h>
#include <iostream>
#include <mutex>
#include <string>
#include <sys/select.h>
#include <unistd.h>
#include <vector>

namespace {

std::mutex g_out;  // serialises console output between threads

void Print(const fccvis::api::Reply& reply) {
  std::lock_guard<std::mutex> lock(g_out);
  for (const auto& l : reply.lines) std::cout << l << '\n';
  std::cout.flush();
  for (const auto& e : reply.errors) std::cerr << "error: " << e << '\n';
  std::cerr.flush();
}

void Usage() {
  std::cout << "usage: fccvis-shell [-f script] [-c command]... [-i] [--keep-going] [--help] [--version]\n"
               "  -f FILE        run the commands in FILE (a script or a saved configuration)\n"
               "  -c COMMAND     run COMMAND (may be repeated)\n"
               "  -i             continue interactively after -f/-c\n"
               "  --keep-going   do not stop at the first failing command\n"
               "  --reference [commands|adapters|backends]\n"
               "                 print the generated reference (Markdown)\n"
               "Inside the shell, type 'help' for the commands.\n";
}

/** Messages from other threads, printed by the main loop. */
class AsyncQueue {
 public:
  AsyncQueue() {
    if (pipe(m_pipe) == 0) {
      fcntl(m_pipe[0], F_SETFL, O_NONBLOCK);
      fcntl(m_pipe[1], F_SETFL, O_NONBLOCK);
    } else {
      m_pipe[0] = m_pipe[1] = -1;
    }
  }
  ~AsyncQueue() {
    if (m_pipe[0] >= 0) close(m_pipe[0]);
    if (m_pipe[1] >= 0) close(m_pipe[1]);
  }
  void Push(std::string message) {
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_messages.push_back(std::move(message));
    }
    const char b = 1;
    if (m_pipe[1] >= 0) [[maybe_unused]] auto n = write(m_pipe[1], &b, 1);
  }
  std::vector<std::string> Take() {
    char buf[64];
    while (m_pipe[0] >= 0 && read(m_pipe[0], buf, sizeof buf) > 0) {
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> out(m_messages.begin(), m_messages.end());
    m_messages.clear();
    return out;
  }
  [[nodiscard]] int Fd() const { return m_pipe[0]; }

 private:
  int m_pipe[2];
  std::mutex m_mutex;
  std::deque<std::string> m_messages;
};

fccvis::api::Interpreter* g_interpreter = nullptr;

void Complete(const char* buffer, linenoiseCompletions* lc) {
  if (!g_interpreter) return;
  const std::string line(buffer);
  std::size_t start = line.size();
  while (start > 0 && line[start - 1] != ' ') --start;
  const std::string head = line.substr(0, start);
  const auto candidates = g_interpreter->Complete(line);
  for (const auto& c : candidates) {
    const char last = c.empty() ? ' ' : c.back();
    const bool open = last == '=' || last == '(' || last == '/';
    const std::string full = head + c + (candidates.size() == 1 && !open ? " " : "");
    linenoiseAddCompletion(lc, full.c_str());
  }
}

std::string HistoryFile() {
  if (const char* h = std::getenv("FCCVIS_HISTORY")) return h;
  if (const char* home = std::getenv("HOME")) return std::string(home) + "/.fccvis_history";
  return {};
}

/** Reads one edited line, printing queued messages above the prompt. Returns false at end of input. */
bool ReadLine(const char* prompt, AsyncQueue& queue, std::string& out) {
  linenoiseState ls{};
  std::vector<char> buf(8192);
  if (linenoiseEditStart(&ls, -1, -1, buf.data(), buf.size(), prompt) == -1) return false;
  char* line = linenoiseEditMore;
  while (line == linenoiseEditMore) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(ls.ifd, &fds);
    int maxfd = ls.ifd;
    if (queue.Fd() >= 0) {
      FD_SET(queue.Fd(), &fds);
      maxfd = std::max(maxfd, queue.Fd());
    }
    const int n = select(maxfd + 1, &fds, nullptr, nullptr, nullptr);
    if (n < 0) {
      if (errno == EINTR) continue;
      break;
    }
    if (queue.Fd() >= 0 && FD_ISSET(queue.Fd(), &fds)) {
      const auto messages = queue.Take();
      if (!messages.empty()) {
        std::lock_guard<std::mutex> lock(g_out);
        linenoiseHide(&ls);
        for (const auto& m : messages) std::cout << m << "\r\n";
        std::cout.flush();
        linenoiseShow(&ls);
      }
    }
    if (FD_ISSET(ls.ifd, &fds)) line = linenoiseEditFeed(&ls);
  }
  linenoiseEditStop(&ls);
  if (line == nullptr || line == linenoiseEditMore) return false;
  out = line;
  linenoiseFree(line);
  return true;
}

int Interactive(fccvis::api::Interpreter& interpreter, AsyncQueue& queue) {
  g_interpreter = &interpreter;
  linenoiseSetCompletionCallback(Complete);
  linenoiseSetMultiLine(1);
  linenoiseHistorySetMaxLen(1000);
  const std::string history = HistoryFile();
  if (!history.empty()) linenoiseHistoryLoad(history.c_str());
  std::cout << "fccvis " << FCCVIS_VERSION << " shell; Tab completes, 'help' lists commands, 'quit' leaves\n";
  std::string command, line;
  while (true) {
    if (!ReadLine(command.empty() ? "fccvis> " : "     .. ", queue, line)) {
      std::cout << '\n';
      break;
    }
    if (!command.empty()) line.erase(0, std::min(line.size(), line.find_first_not_of(" \t")));  // continuation indent
    if (!line.empty() && line.back() == '\\') {
      line.pop_back();
      command += line.substr(0, line.find_last_not_of(" \t") + 1) + " ";
      continue;
    }
    command += line;
    if (command.find_first_not_of(' ') != std::string::npos) {
      linenoiseHistoryAdd(command.c_str());
      if (!history.empty()) linenoiseHistorySave(history.c_str());
    }
    const auto reply = interpreter.Execute(command);
    command.clear();
    Print(reply);
    for (const auto& m : queue.Take()) {  // messages that arrived while the command ran
      std::lock_guard<std::mutex> lock(g_out);
      std::cout << m << '\n';
    }
    if (reply.quit) break;
  }
  g_interpreter = nullptr;
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  std::vector<std::pair<char, std::string>> jobs;  // ('f', file) or ('c', command)
  bool interactive = false, keepGoing = false;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if ((a == "-f" || a == "-c") && i + 1 < argc) {
      jobs.emplace_back(a[1], argv[++i]);
    } else if (a == "-i") {
      interactive = true;
    } else if (a == "--keep-going") {
      keepGoing = true;
    } else if (a == "-h" || a == "--help") {
      Usage();
      return 0;
    } else if (a == "--reference") {
      const std::string section = i + 1 < argc && argv[i + 1][0] != '-' ? argv[++i] : "";
      std::cout << fccvis::api::Interpreter::Reference(section);
      return 0;
    } else if (a == "--version") {
      std::cout << "fccvis-shell " << FCCVIS_VERSION << '\n';
      return 0;
    } else {
      std::cerr << "fccvis-shell: unknown argument '" << a << "'\n";
      Usage();
      return 2;
    }
  }
  if (jobs.empty()) interactive = true;
  const bool terminal = interactive && isatty(0) && isatty(1);

  fccvis::session::Session session("shell");
  fccvis::api::Interpreter interpreter(session);
  interpreter.SetStopOnError(!keepGoing);
  AsyncQueue queue;
  auto emit = [&queue, terminal](const std::string& text) {
    if (terminal) {
      queue.Push(text);  // printed by the main loop above the prompt
    } else {
      std::lock_guard<std::mutex> lock(g_out);
      std::cerr << text << '\n';
    }
  };
  interpreter.SetLogSink([emit](fccvis::changes::Severity s, const std::string& m) {
    emit((s == fccvis::changes::Severity::Error ? "error: " : "warning: ") + m);
  });
  session.SetPickHandler([emit](const std::string& backend, const fccvis::backend::PickEvent& e) {
    emit("picked in " + backend + ": " + e.scene + "/" + e.layer + " entry " + std::to_string(e.entry) + " element " +
         std::to_string(e.index));
  });

  int status = 0;
  for (const auto& [kind, arg] : jobs) {
    const fccvis::api::Reply reply = kind == 'c' ? interpreter.Run(arg, !keepGoing)
                                                 : interpreter.Execute("source " + fccvis::api::QuoteWord(arg));
    Print(reply);
    if (!reply.ok) {
      status = 1;
      if (!keepGoing) {
        session.Flush();
        return status;
      }
    }
    if (reply.quit) return status;
  }
  if (!interactive) {
    session.Flush();
    return status;
  }
  if (terminal) {
    Interactive(interpreter, queue);
    session.Flush();
    return 0;
  }

  // Input from a pipe or file: plain line reading, `\` continues a line.
  std::string line, command;
  while (std::getline(std::cin, line)) {
    if (!command.empty()) line.erase(0, std::min(line.size(), line.find_first_not_of(" \t")));  // continuation indent
    if (!line.empty() && line.back() == '\\') {
      line.pop_back();
      command += line.substr(0, line.find_last_not_of(" \t") + 1) + " ";
      continue;
    }
    command += line;
    const auto reply = interpreter.Execute(command);
    command.clear();
    Print(reply);
    if (!reply.ok) status = 1;
    if (reply.quit) break;
  }
  session.Flush();
  return status;
}
