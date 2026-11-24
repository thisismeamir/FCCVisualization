// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
#include "Json.h"

#include <cerrno>
#include <cstdlib>
#include <limits>

namespace fccvis::adapters::json {

void Reader::Fail(const std::string& what) const {
  std::size_t line = 1, col = 1;
  for (std::size_t k = 0; k < m_i && k < m_s.size(); ++k) {
    if (m_s[k] == '\n') {
      ++line;
      col = 1;
    } else {
      ++col;
    }
  }
  throw Error("JSON line " + std::to_string(line) + ", column " + std::to_string(col) + ": " + what);
}

void Reader::Blank() {
  while (m_i < m_s.size() && (m_s[m_i] == ' ' || m_s[m_i] == '\n' || m_s[m_i] == '\r' || m_s[m_i] == '\t')) ++m_i;
}

void Reader::Expect(char c) {
  Blank();
  if (m_i >= m_s.size() || m_s[m_i] != c) Fail(std::string("expected '") + c + "'");
  ++m_i;
}

bool Reader::Accept(char c) {
  Blank();
  if (m_i < m_s.size() && m_s[m_i] == c) {
    ++m_i;
    return true;
  }
  return false;
}

namespace {

void AppendUtf8(std::string& out, unsigned code) {
  if (code < 0x80) {
    out += static_cast<char>(code);
  } else if (code < 0x800) {
    out += static_cast<char>(0xC0 | (code >> 6));
    out += static_cast<char>(0x80 | (code & 0x3F));
  } else if (code < 0x10000) {
    out += static_cast<char>(0xE0 | (code >> 12));
    out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (code & 0x3F));
  } else {
    out += static_cast<char>(0xF0 | (code >> 18));
    out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (code & 0x3F));
  }
}

}  // namespace

std::string Reader::String() {
  Expect('"');
  std::string out;
  while (true) {
    if (m_i >= m_s.size()) Fail("unterminated string");
    const char c = m_s[m_i++];
    if (c == '"') return out;
    if (c != '\\') {
      out += c;
      continue;
    }
    if (m_i >= m_s.size()) Fail("unterminated string");
    const char e = m_s[m_i++];
    switch (e) {
      case '"': out += '"'; break;
      case '\\': out += '\\'; break;
      case '/': out += '/'; break;
      case 'b': out += '\b'; break;
      case 'f': out += '\f'; break;
      case 'n': out += '\n'; break;
      case 'r': out += '\r'; break;
      case 't': out += '\t'; break;
      case 'u': {
        auto hex4 = [&]() -> unsigned {
          if (m_i + 4 > m_s.size()) Fail("bad \\u escape");
          unsigned v = 0;
          for (int k = 0; k < 4; ++k) {
            const char h = m_s[m_i++];
            v <<= 4;
            if (h >= '0' && h <= '9') v |= static_cast<unsigned>(h - '0');
            else if (h >= 'a' && h <= 'f') v |= static_cast<unsigned>(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F') v |= static_cast<unsigned>(h - 'A' + 10);
            else Fail("bad \\u escape");
          }
          return v;
        };
        unsigned code = hex4();
        if (code >= 0xD800 && code < 0xDC00 && m_i + 1 < m_s.size() && m_s[m_i] == '\\' && m_s[m_i + 1] == 'u') {
          m_i += 2;
          const unsigned low = hex4();
          code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
        }
        AppendUtf8(out, code);
        break;
      }
      default: Fail("bad escape");
    }
  }
}

Value Reader::Number() {
  const std::size_t start = m_i;
  bool integral = true;
  if (m_i < m_s.size() && m_s[m_i] == '-') ++m_i;
  while (m_i < m_s.size()) {
    const char c = m_s[m_i];
    if (c >= '0' && c <= '9') {
      ++m_i;
    } else if (c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') {
      integral = false;
      ++m_i;
    } else {
      break;
    }
  }
  const std::string text(m_s.substr(start, m_i - start));
  if (text.empty() || text == "-") Fail("bad number");
  if (integral) {
    errno = 0;
    char* end = nullptr;
    const long long v = std::strtoll(text.c_str(), &end, 10);
    if (errno == 0 && end && *end == '\0') return Value{static_cast<std::int64_t>(v)};
  }
  char* end = nullptr;
  const double d = std::strtod(text.c_str(), &end);
  if (!end || *end != '\0') Fail("bad number '" + text + "'");
  return Value{d};
}

Value Reader::Parse() {
  Blank();
  if (m_i >= m_s.size()) Fail("unexpected end of text");
  const char c = m_s[m_i];
  if (c == '{') {
    ++m_i;
    auto obj = std::make_shared<Object>();
    if (Accept('}')) return Value{obj};
    do {
      Blank();
      std::string key = String();
      Expect(':');
      obj->emplace_back(std::move(key), Parse());
    } while (Accept(','));
    Expect('}');
    return Value{obj};
  }
  if (c == '[') {
    ++m_i;
    auto arr = std::make_shared<Array>();
    if (Accept(']')) return Value{arr};
    do arr->push_back(Parse());
    while (Accept(','));
    Expect(']');
    return Value{arr};
  }
  if (c == '"') return Value{String()};
  if (m_s.substr(m_i, 4) == "true") {
    m_i += 4;
    return Value{true};
  }
  if (m_s.substr(m_i, 5) == "false") {
    m_i += 5;
    return Value{false};
  }
  if (m_s.substr(m_i, 4) == "null") {
    m_i += 4;
    return Value{nullptr};
  }
  if (m_s.substr(m_i, 3) == "NaN") {  // written by some exporters; not JSON, read as NaN
    m_i += 3;
    return Value{std::numeric_limits<double>::quiet_NaN()};
  }
  if (c == '-' || (c >= '0' && c <= '9')) return Number();
  Fail(std::string("unexpected character '") + c + "'");
}

std::pair<std::size_t, std::size_t> Reader::SkipValue() {
  Blank();
  const std::size_t begin = m_i;
  if (m_i >= m_s.size()) Fail("unexpected end of text");
  const char c = m_s[m_i];
  if (c == '"') {
    String();
  } else if (c == '{' || c == '[') {
    int depth = 0;
    while (m_i < m_s.size()) {
      const char d = m_s[m_i];
      if (d == '"') {
        String();
        continue;
      }
      ++m_i;
      if (d == '{' || d == '[') ++depth;
      else if ((d == '}' || d == ']') && --depth == 0) break;
    }
    if (depth != 0) Fail("unterminated object or array");
  } else {
    Parse();
  }
  return {begin, m_i};
}

}  // namespace fccvis::adapters::json
