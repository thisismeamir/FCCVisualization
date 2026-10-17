// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
#include "fccvis/api/Config.h"

#include <cctype>
#include <cstdint>
#include <map>
#include <memory>
#include <variant>

namespace fccvis::api {

namespace {

std::string Escape(const std::string& s) {
  std::string out;
  for (unsigned char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\t': out += "\\t"; break;
      case '\r': out += "\\r"; break;
      default:
        if (c < 0x20) {
          static const char* hex = "0123456789abcdef";
          out += "\\u00";
          out += hex[c >> 4];
          out += hex[c & 15];
        } else {
          out += static_cast<char>(c);
        }
    }
  }
  return out;
}

// Minimal JSON reader: values are null, bool, number, string, array, object.
struct Value;
using Array = std::vector<Value>;
using Object = std::map<std::string, Value>;
struct Value {
  std::variant<std::nullptr_t, bool, double, std::string, std::shared_ptr<Array>, std::shared_ptr<Object>> v;
};

class Reader {
 public:
  explicit Reader(std::string_view s) : m_s(s) {}
  Value Document() {
    Value v = Parse();
    Skip();
    if (m_i != m_s.size()) Fail("unexpected text after the JSON value");
    return v;
  }

 private:
  [[noreturn]] void Fail(const std::string& what) const {
    std::size_t line = 1, col = 1;
    for (std::size_t k = 0; k < m_i && k < m_s.size(); ++k) {
      if (m_s[k] == '\n') {
        ++line;
        col = 1;
      } else {
        ++col;
      }
    }
    throw ConfigError("configuration: " + what + " at line " + std::to_string(line) + ", column " + std::to_string(col));
  }
  void Skip() {
    while (m_i < m_s.size() && std::isspace(static_cast<unsigned char>(m_s[m_i]))) ++m_i;
  }
  bool Eat(char c) {
    Skip();
    if (m_i < m_s.size() && m_s[m_i] == c) {
      ++m_i;
      return true;
    }
    return false;
  }
  Value Parse() {
    Skip();
    if (m_i >= m_s.size()) Fail("unexpected end");
    const char c = m_s[m_i];
    if (c == '{') {
      ++m_i;
      auto obj = std::make_shared<Object>();
      if (!Eat('}')) {
        do {
          Skip();
          if (m_i >= m_s.size() || m_s[m_i] != '"') Fail("expected a member name");
          std::string key = String();
          if (!Eat(':')) Fail("expected ':'");
          (*obj)[key] = Parse();
        } while (Eat(','));
        if (!Eat('}')) Fail("expected ',' or '}'");
      }
      return {obj};
    }
    if (c == '[') {
      ++m_i;
      auto arr = std::make_shared<Array>();
      if (!Eat(']')) {
        do arr->push_back(Parse());
        while (Eat(','));
        if (!Eat(']')) Fail("expected ',' or ']'");
      }
      return {arr};
    }
    if (c == '"') return {String()};
    if (m_s.substr(m_i, 4) == "true") {
      m_i += 4;
      return {true};
    }
    if (m_s.substr(m_i, 5) == "false") {
      m_i += 5;
      return {false};
    }
    if (m_s.substr(m_i, 4) == "null") {
      m_i += 4;
      return {nullptr};
    }
    if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
      const std::size_t b = m_i;
      while (m_i < m_s.size() && (std::isdigit(static_cast<unsigned char>(m_s[m_i])) || m_s[m_i] == '-' || m_s[m_i] == '+' ||
                                  m_s[m_i] == '.' || m_s[m_i] == 'e' || m_s[m_i] == 'E'))
        ++m_i;
      try {
        return {std::stod(std::string(m_s.substr(b, m_i - b)))};
      } catch (const std::exception&) {
        m_i = b;
        Fail("bad number");
      }
    }
    Fail(std::string("unexpected character '") + c + "'");
  }
  std::string String() {
    ++m_i;  // opening quote
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
          if (m_i + 4 > m_s.size()) Fail("bad \\u escape");
          std::uint32_t cp = 0;
          for (int k = 0; k < 4; ++k) {
            const char h = m_s[m_i++];
            cp <<= 4;
            if (h >= '0' && h <= '9') cp |= static_cast<std::uint32_t>(h - '0');
            else if (h >= 'a' && h <= 'f') cp |= static_cast<std::uint32_t>(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F') cp |= static_cast<std::uint32_t>(h - 'A' + 10);
            else Fail("bad \\u escape");
          }
          if (cp < 0x80) {
            out += static_cast<char>(cp);
          } else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
          } else {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
          }
          break;
        }
        default: Fail(std::string("bad escape '\\") + e + "'");
      }
    }
  }
  std::string_view m_s;
  std::size_t m_i = 0;
};

}  // namespace

std::string WriteConfig(const std::vector<std::string>& commands) {
  std::string json = "{\n  \"format\": \"fccvis-config\",\n  \"version\": " + std::to_string(kConfigVersion) +
                     ",\n  \"commands\": [";
  for (std::size_t i = 0; i < commands.size(); ++i) json += std::string(i ? ",\n" : "\n") + "    \"" + Escape(commands[i]) + "\"";
  json += commands.empty() ? "]\n}\n" : "\n  ]\n}\n";
  return json;
}

bool IsConfig(std::string_view text) {
  const auto first = text.find_first_not_of(" \t\r\n");
  return first != std::string_view::npos && text[first] == '{';
}

std::vector<std::string> ReadConfig(std::string_view json) {
  const Value doc = Reader(json).Document();
  const auto* obj = std::get_if<std::shared_ptr<Object>>(&doc.v);
  if (!obj) throw ConfigError("configuration: expected a JSON object");
  const auto& o = **obj;
  auto format = o.find("format");
  if (format == o.end() || !std::holds_alternative<std::string>(format->second.v) ||
      std::get<std::string>(format->second.v) != "fccvis-config")
    throw ConfigError("configuration: \"format\" must be \"fccvis-config\"");
  auto version = o.find("version");
  if (version == o.end() || !std::holds_alternative<double>(version->second.v))
    throw ConfigError("configuration: \"version\" is missing");
  const double v = std::get<double>(version->second.v);
  if (v > kConfigVersion)
    throw ConfigError("configuration version " + std::to_string(static_cast<int>(v)) + " was written by a newer fccvis (this one reads version " +
                      std::to_string(kConfigVersion) + ")");
  if (v < 1) throw ConfigError("configuration: bad version");
  auto commands = o.find("commands");
  if (commands == o.end() || !std::holds_alternative<std::shared_ptr<Array>>(commands->second.v))
    throw ConfigError("configuration: \"commands\" must be an array of strings");
  std::vector<std::string> out;
  for (const auto& c : *std::get<std::shared_ptr<Array>>(commands->second.v)) {
    if (!std::holds_alternative<std::string>(c.v)) throw ConfigError("configuration: \"commands\" must be an array of strings");
    out.push_back(std::get<std::string>(c.v));
  }
  return out;
}

}  // namespace fccvis::api
