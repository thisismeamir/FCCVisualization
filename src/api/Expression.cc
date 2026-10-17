// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
#include "fccvis/api/Expression.h"

#include "fccvis/stdlib/ShapeFilters.h"
#include "fccvis/stdlib/Shapes.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <variant>

namespace fccvis::api {

namespace {

// ---------------------------------------------------------------------------
// Lexer
// ---------------------------------------------------------------------------
enum class Tok { Ident, Number, String, Op, LParen, RParen, LBracket, RBracket, Comma, Star, End };

struct Token {
  Tok kind = Tok::End;
  std::string text;
  double number = 0;
  std::size_t column = 1;  // 1-based
};

bool IdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool IdentChar(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.' || c == ':'; }

std::vector<Token> Lex(std::string_view s) {
  std::vector<Token> out;
  std::size_t i = 0;
  while (i < s.size()) {
    const char c = s[i];
    if (std::isspace(static_cast<unsigned char>(c))) {
      ++i;
      continue;
    }
    Token t;
    t.column = i + 1;
    if (IdentStart(c)) {
      const auto b = i;
      while (i < s.size() && IdentChar(s[i])) ++i;
      t.kind = Tok::Ident;
      t.text = std::string(s.substr(b, i - b));
    } else if (std::isdigit(static_cast<unsigned char>(c)) || ((c == '-' || c == '+' || c == '.') && i + 1 < s.size() &&
                                                              (std::isdigit(static_cast<unsigned char>(s[i + 1])) || s[i + 1] == '.'))) {
      const auto b = i;
      ++i;
      while (i < s.size() && (std::isalnum(static_cast<unsigned char>(s[i])) || s[i] == '.' ||
                              ((s[i] == '-' || s[i] == '+') && (s[i - 1] == 'e' || s[i - 1] == 'E'))))
        ++i;
      t.kind = Tok::Number;
      t.text = std::string(s.substr(b, i - b));
      std::string_view num = t.text;
      if (!num.empty() && num[0] == '+') num.remove_prefix(1);
      auto [p, ec] = std::from_chars(num.data(), num.data() + num.size(), t.number);
      if (ec != std::errc() || p != num.data() + num.size()) throw ParseError("bad number '" + t.text + "'", t.column);
    } else if (c == '"') {
      ++i;
      std::string v;
      bool closed = false;
      while (i < s.size()) {
        if (s[i] == '\\' && i + 1 < s.size()) {
          v += s[i + 1];
          i += 2;
        } else if (s[i] == '"') {
          ++i;
          closed = true;
          break;
        } else {
          v += s[i++];
        }
      }
      if (!closed) throw ParseError("unterminated string", t.column);
      t.kind = Tok::String;
      t.text = v;
    } else {
      static const char* ops[] = {"==", "!=", "<=", ">=", "<", ">", "~"};
      bool matched = false;
      for (const char* op : ops) {
        const std::string_view o(op);
        if (s.substr(i, o.size()) == o) {
          t.kind = Tok::Op;
          t.text = std::string(o);
          i += o.size();
          matched = true;
          break;
        }
      }
      if (!matched) {
        switch (c) {
          case '(': t.kind = Tok::LParen; break;
          case ')': t.kind = Tok::RParen; break;
          case '[': t.kind = Tok::LBracket; break;
          case ']': t.kind = Tok::RBracket; break;
          case ',': t.kind = Tok::Comma; break;
          case '*': t.kind = Tok::Star; break;
          case '=': throw ParseError("unexpected '=' (use '==' to compare)", t.column);
          default: throw ParseError(std::string("unexpected character '") + c + "'", t.column);
        }
        t.text = std::string(1, c);
        ++i;
      }
    }
    out.push_back(std::move(t));
  }
  Token end;
  end.kind = Tok::End;
  end.column = s.size() + 1;
  out.push_back(end);
  return out;
}

std::string Describe(const Token& t) {
  switch (t.kind) {
    case Tok::End: return "end of input";
    case Tok::String: return "\"" + t.text + "\"";
    default: return "'" + t.text + "'";
  }
}

// ---------------------------------------------------------------------------
// Filter parser (recursive descent)
// ---------------------------------------------------------------------------
using stdlib::Selector;
using stdlib::ShapePtr;

class FilterParser {
 public:
  explicit FilterParser(std::string_view text) : m_t(Lex(text)) {}

  Selector Parse() {
    if (Peek().kind == Tok::End) throw ParseError("empty filter expression", Peek().column);
    Selector s = Or();
    if (Peek().kind != Tok::End) throw ParseError("unexpected " + Describe(Peek()) + "; expected 'and', 'or' or end", Peek().column);
    return s;
  }

  std::vector<std::string> attributes;

 private:
  const Token& Peek(std::size_t k = 0) const { return m_t[std::min(m_i + k, m_t.size() - 1)]; }
  const Token& Next() { return m_t[std::min(m_i++, m_t.size() - 1)]; }
  bool IsWord(const char* w, std::size_t k = 0) const { return Peek(k).kind == Tok::Ident && Peek(k).text == w; }
  const Token& Expect(Tok kind, const char* what) {
    if (Peek().kind != kind) throw ParseError(std::string("expected ") + what + ", found " + Describe(Peek()), Peek().column);
    return Next();
  }

  Selector Or() {
    Selector s = And();
    while (IsWord("or")) {
      Next();
      s = s | And();
    }
    return s;
  }
  Selector And() {
    Selector s = Not();
    while (IsWord("and")) {
      Next();
      s = s & Not();
    }
    return s;
  }
  Selector Not() {
    if (IsWord("not")) {
      Next();
      return !Not();
    }
    return Primary();
  }

  Selector Primary() {
    const Token& t = Peek();
    if (t.kind == Tok::LParen) {
      Next();
      Selector s = Or();
      Expect(Tok::RParen, "')'");
      return s;
    }
    if (t.kind != Tok::Ident)
      throw ParseError("expected an attribute, a function or '(', found " + Describe(t), t.column);
    if (t.text == "and" || t.text == "or" || t.text == "in")
      throw ParseError("expected an attribute, a function or '(', found '" + t.text + "'", t.column);
    if (Peek(1).kind == Tok::LParen) return Call();
    return Comparison();
  }

  Selector Comparison() {
    const Token name = Next();
    attributes.push_back(name.text);
    if (IsWord("in")) {
      Next();
      Expect(Tok::LBracket, "'['");
      std::vector<double> numbers;
      std::vector<std::string> texts;
      while (true) {
        const Token& v = Next();
        if (v.kind == Tok::Number) numbers.push_back(v.number);
        else if (v.kind == Tok::String) texts.push_back(v.text);
        else throw ParseError("expected a number or a string, found " + Describe(v), v.column);
        if (Peek().kind == Tok::Comma) {
          Next();
          continue;
        }
        Expect(Tok::RBracket, "',' or ']'");
        break;
      }
      if (!numbers.empty() && !texts.empty())
        throw ParseError("list mixes numbers and strings", name.column);
      return texts.empty() ? stdlib::OneOf(name.text, numbers) : stdlib::OneOf(name.text, texts);
    }
    const Token& op = Peek();
    if (op.kind != Tok::Op)
      throw ParseError("expected a comparison (== != < <= > >= ~ in) after '" + name.text + "', found " + Describe(op), op.column);
    Next();
    const Token& v = Next();
    if (op.text == "~") {
      if (v.kind != Tok::String) throw ParseError("'~' needs a \"regular expression\"", v.column);
      try {
        return stdlib::Matches(name.text, v.text);
      } catch (const std::exception& e) {
        throw ParseError(std::string("bad regular expression: ") + e.what(), v.column);
      }
    }
    if (v.kind == Tok::Number) return Compare(stdlib::Attr<double>(name.text), op, v.number);
    if (v.kind == Tok::String) return Compare(stdlib::Attr<std::string>(name.text), op, v.text);
    if (v.kind == Tok::Ident && (v.text == "true" || v.text == "false")) {
      if (op.text != "==" && op.text != "!=") throw ParseError("true/false compare only with == or !=", op.column);
      return Compare(stdlib::Attr<bool>(name.text), op, v.text == "true");
    }
    throw ParseError("expected a number, a \"string\" or true/false, found " + Describe(v), v.column);
  }

  template <typename T>
  static Selector Compare(const stdlib::Attr<T>& a, const Token& op, const T& v) {
    if (op.text == "==") return a == v;
    if (op.text == "!=") return a != v;
    if constexpr (std::is_same_v<T, bool>) {
      throw ParseError("true/false compare only with == or !=", op.column);
    } else {
      if (op.text == "<") return a < v;
      if (op.text == "<=") return a <= v;
      if (op.text == ">") return a > v;
      if (op.text == ">=") return a >= v;
    }
    throw ParseError("unknown operator '" + op.text + "'", op.column);
  }

  double Number() {
    const Token& t = Next();
    if (t.kind != Tok::Number) throw ParseError("expected a number, found " + Describe(t), t.column);
    return t.number;
  }
  std::size_t Count() {
    const Token& t = Peek();
    const double v = Number();
    if (v < 0 || v != std::floor(v)) throw ParseError("expected a non-negative integer", t.column);
    return static_cast<std::size_t>(v);
  }
  std::string AttributeName() {
    const Token& t = Next();
    if (t.kind != Tok::Ident && t.kind != Tok::String) throw ParseError("expected an attribute name, found " + Describe(t), t.column);
    attributes.push_back(t.text);
    return t.text;
  }
  void Comma() { Expect(Tok::Comma, "','"); }

  ShapePtr Shape() {
    const Token name = Expect(Tok::Ident, "a shape (box, sphere, cylinder, cone, translate)");
    Expect(Tok::LParen, "'('");
    ShapePtr out;
    try {
      if (name.text == "box") {
        const double a = Number();
        Comma();
        const double b = Number();
        Comma();
        out = std::make_shared<stdlib::Box>(a, b, Number());
      } else if (name.text == "sphere") {
        out = std::make_shared<stdlib::Sphere>(Number());
      } else if (name.text == "cylinder") {
        const double a = Number();
        Comma();
        const double b = Number();
        Comma();
        out = std::make_shared<stdlib::Cylinder>(a, b, Number());
      } else if (name.text == "cone") {
        double v[5];
        for (int k = 0; k < 5; ++k) {
          if (k) Comma();
          v[k] = Number();
        }
        out = std::make_shared<stdlib::Cone>(v[0], v[1], v[2], v[3], v[4]);
      } else if (name.text == "translate") {
        ShapePtr inner = Shape();
        double v[3];
        for (double& x : v) {
          Comma();
          x = Number();
        }
        out = std::make_shared<stdlib::Transformed>(
            inner, geometry::Transform({v[0], v[1], v[2]}, {1, 0, 0, 0, 1, 0, 0, 0, 1}));
      } else {
        throw ParseError("unknown shape '" + name.text + "' (box, sphere, cylinder, cone, translate)", name.column);
      }
    } catch (const std::invalid_argument& e) {
      throw ParseError(std::string("bad ") + name.text + ": " + e.what(), name.column);
    }
    Expect(Tok::RParen, "')'");
    return out;
  }

  stdlib::LineTest LineMode() {
    if (Peek().kind != Tok::Comma) return stdlib::LineTest::AnyVertex;
    Next();
    const Token& t = Expect(Tok::Ident, "any, all or centroid");
    if (t.text == "any") return stdlib::LineTest::AnyVertex;
    if (t.text == "all") return stdlib::LineTest::AllVertices;
    if (t.text == "centroid") return stdlib::LineTest::Centroid;
    throw ParseError("expected any, all or centroid, found '" + t.text + "'", t.column);
  }

  Selector Call() {
    const Token name = Next();
    Expect(Tok::LParen, "'('");
    Selector s = stdlib::All();
    const std::string& f = name.text;
    if (f == "all") {
      s = stdlib::All();
    } else if (f == "none") {
      s = stdlib::None();
    } else if (f == "exists") {
      s = stdlib::Exists(AttributeName());
    } else if (f == "first") {
      s = stdlib::FirstN(Count());
    } else if (f == "index") {
      s = stdlib::Index(Count());
    } else if (f == "between") {
      const std::string a = AttributeName();
      Comma();
      const double lo = Number();
      Comma();
      s = stdlib::Between(a, lo, Number());
    } else if (f == "inside" || f == "outside") {
      ShapePtr shape = Shape();
      const auto mode = LineMode();
      s = f == "inside" ? stdlib::Inside(shape, mode) : stdlib::Outside(shape, mode);
    } else if (f == "within") {
      const double d = Number();
      Comma();
      ShapePtr shape = Shape();
      s = stdlib::Within(d, shape, LineMode());
    } else {
      throw ParseError("unknown function '" + f +
                           "' (all, none, exists, first, index, between, inside, outside, within)",
                       name.column);
    }
    Expect(Tok::RParen, "')'");
    return s;
  }

  std::vector<Token> m_t;
  std::size_t m_i = 0;
};

// ---------------------------------------------------------------------------
// Style assignments
// ---------------------------------------------------------------------------
struct Assignment {
  std::string key, value;
  std::size_t column;
};

std::vector<Assignment> Assignments(std::string_view text) {
  std::vector<Assignment> out;
  std::size_t i = 0;
  while (i < text.size()) {
    while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) ++i;
    if (i >= text.size()) break;
    const std::size_t col = i + 1;
    std::string key;
    while (i < text.size() && text[i] != '=' && !std::isspace(static_cast<unsigned char>(text[i]))) key += text[i++];
    if (i >= text.size() || text[i] != '=') throw ParseError("expected key=value, found '" + key + "'", col);
    ++i;
    std::string value;
    if (i < text.size() && text[i] == '"') {
      ++i;
      bool closed = false;
      while (i < text.size()) {
        if (text[i] == '\\' && i + 1 < text.size()) {
          value += text[i + 1];
          i += 2;
        } else if (text[i] == '"') {
          ++i;
          closed = true;
          break;
        } else {
          value += text[i++];
        }
      }
      if (!closed) throw ParseError("unterminated string", col);
    } else {
      while (i < text.size() && !std::isspace(static_cast<unsigned char>(text[i]))) value += text[i++];
    }
    if (key.empty()) throw ParseError("empty key", col);
    out.push_back({key, value, col});
  }
  return out;
}

float Float(const Assignment& a, float lo, float hi) {
  float v = 0;
  const auto* b = a.value.data();
  auto [p, ec] = std::from_chars(b, b + a.value.size(), v);
  if (ec != std::errc() || p != b + a.value.size() || !(v >= lo && v <= hi))
    throw ParseError("'" + a.key + "' must be a number in [" + std::to_string(lo).substr(0, 4) + ", " +
                         (hi > 1e6 ? std::string("inf") : std::to_string(hi).substr(0, 5)) + "], got '" + a.value + "'",
                     a.column);
  return v;
}

bool Bool(const Assignment& a) {
  if (a.value == "true" || a.value == "on" || a.value == "yes" || a.value == "1") return true;
  if (a.value == "false" || a.value == "off" || a.value == "no" || a.value == "0") return false;
  throw ParseError("'" + a.key + "' must be true or false, got '" + a.value + "'", a.column);
}

style::Color Colour(const Assignment& a) {
  try {
    return ParseColor(a.value);
  } catch (const ParseError& e) {
    throw ParseError(e.Message(), a.column);
  }
}

template <typename E>
E Enum(const Assignment& a, const std::vector<std::pair<const char*, E>>& names) {
  for (const auto& [n, v] : names)
    if (a.value == n) return v;
  std::string allowed;
  for (const auto& [n, v] : names) allowed += (allowed.empty() ? "" : ", ") + std::string(n);
  throw ParseError("'" + a.key + "' must be one of " + allowed + ", got '" + a.value + "'", a.column);
}

bool Common(const Assignment& a, style::CommonStyle& c) {
  if (a.key == "color") c.color = Colour(a);
  else if (a.key == "opacity") c.opacity = Float(a, 0, 1);
  else if (a.key == "visible") c.visible = Bool(a);
  else return false;
  return true;
}

[[noreturn]] void Unknown(const Assignment& a, const std::string& kind) {
  std::string allowed;
  for (const auto& k : StyleKeys(kind)) allowed += (allowed.empty() ? "" : ", ") + k;
  throw ParseError("unknown " + kind + " style key '" + a.key + "' (" + allowed + ")", a.column);
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------
class LayoutParser {
 public:
  explicit LayoutParser(std::string_view text) : m_t(Lex(text)) {}
  scene::layout::LayoutNode Parse() {
    if (m_t[m_i].kind == Tok::End) throw ParseError("empty layout", 1);
    auto n = Node();
    if (m_t[m_i].kind != Tok::End) throw ParseError("unexpected " + Describe(m_t[m_i]), m_t[m_i].column);
    return n;
  }

 private:
  scene::layout::LayoutNode Node() {
    const Token& t = m_t[m_i];
    if (t.kind != Tok::Ident && t.kind != Tok::String)
      throw ParseError("expected a scene name, split(...), vsplit(...) or tabs(...), found " + Describe(t), t.column);
    ++m_i;
    const bool call = t.kind == Tok::Ident && m_t[m_i].kind == Tok::LParen &&
                      (t.text == "split" || t.text == "vsplit" || t.text == "tabs");
    if (!call) return scene::layout::LayoutNode{scene::layout::Pane{t.text}};
    ++m_i;  // (
    scene::layout::Split split;
    split.orientation = t.text == "vsplit" ? scene::layout::Orientation::Vertical : scene::layout::Orientation::Horizontal;
    scene::layout::Tabs tabs;
    while (true) {
      auto child = Node();
      std::optional<std::string> label;
      int weight = 1;
      while (true) {
        if (m_t[m_i].kind == Tok::Ident && m_t[m_i].text == "as") {
          ++m_i;
          if (m_t[m_i].kind != Tok::String && m_t[m_i].kind != Tok::Ident)
            throw ParseError("expected a label after 'as'", m_t[m_i].column);
          label = m_t[m_i++].text;
        } else if (m_t[m_i].kind == Tok::Star) {
          ++m_i;
          const Token& w = m_t[m_i++];
          if (w.kind != Tok::Number || w.number < 1 || w.number != std::floor(w.number) || w.number > 1000)
            throw ParseError("weight must be an integer in 1..1000", w.column);
          if (t.text == "tabs") throw ParseError("tabs have no weights", w.column);
          weight = static_cast<int>(w.number);
        } else {
          break;
        }
      }
      if (t.text == "tabs") tabs.children.push_back({std::move(child), label});
      else split.children.push_back({std::move(child), label, weight});
      if (m_t[m_i].kind == Tok::Comma) {
        ++m_i;
        continue;
      }
      if (m_t[m_i].kind != Tok::RParen) throw ParseError("expected ',' or ')', found " + Describe(m_t[m_i]), m_t[m_i].column);
      ++m_i;
      break;
    }
    if (t.text == "tabs") return scene::layout::LayoutNode{std::move(tabs)};
    return scene::layout::LayoutNode{std::move(split)};
  }
  std::vector<Token> m_t;
  std::size_t m_i = 0;
};

std::string Quoted(const std::string& s) {
  std::string out = "\"";
  for (char c : s) {
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  return out + "\"";
}

std::string LayoutWord(const std::string& s) {
  bool plain = !s.empty() && IdentStart(s[0]);
  for (char c : s) plain = plain && IdentChar(c);
  if (plain && s != "split" && s != "vsplit" && s != "tabs" && s != "as") return s;
  return Quoted(s);
}

}  // namespace

stdlib::Selector ParseFilter(std::string_view text) { return FilterParser(text).Parse(); }

std::vector<std::string> FilterAttributes(std::string_view text) {
  FilterParser p(text);
  p.Parse();
  std::vector<std::string> out = p.attributes;
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
  return out;
}

std::map<std::string, std::string> ParseAssignments(std::string_view text) {
  std::map<std::string, std::string> out;
  for (auto& a : Assignments(text)) out[a.key] = a.value;
  return out;
}

style::Color ParseColor(std::string_view text) {
  static const std::map<std::string, std::uint32_t, std::less<>> names = {
      {"accent", 0x7017FF}, {"black", 0x000000}, {"blue", 0x0F62FE}, {"cyan", 0x1192E8}, {"gray", 0x8D8D8D},
      {"green", 0x198038},  {"grey", 0x8D8D8D},  {"magenta", 0xD02670}, {"orange", 0xFF832B}, {"purple", 0x8A3FFC},
      {"red", 0xDA1E28},    {"white", 0xFFFFFF}, {"yellow", 0xF1C21B}};
  if (auto it = names.find(text); it != names.end()) return style::Color::FromHex(it->second);
  if (text.empty() || text[0] != '#')
    throw ParseError("expected a colour (#rrggbb or a name such as red, accent), got '" + std::string(text) + "'", 1);
  std::string hex(text.substr(1));
  if (hex.size() == 3) hex = {hex[0], hex[0], hex[1], hex[1], hex[2], hex[2]};
  if (hex.size() != 6 && hex.size() != 8) throw ParseError("expected #rgb, #rrggbb or #rrggbbaa", 1);
  std::uint32_t v = 0;
  auto [p, ec] = std::from_chars(hex.data(), hex.data() + hex.size(), v, 16);
  if (ec != std::errc() || p != hex.data() + hex.size()) throw ParseError("bad hex colour '" + std::string(text) + "'", 1);
  if (hex.size() == 6) return style::Color::FromHex(v);
  style::Color c = style::Color::FromHex(v >> 8);
  c.a = static_cast<std::uint8_t>(v & 0xFF);
  return c;
}

const std::vector<std::string>& StyleKeys(const std::string& kind) {
  static const std::map<std::string, std::vector<std::string>> keys = {
      {"point", {"color", "marker", "opacity", "size", "visible"}},
      {"line", {"color", "dash", "opacity", "visible", "width"}},
      {"surface", {"color", "edgecolor", "edgewidth", "opacity", "visible", "wireframe"}},
      {"environment", {"axes", "background", "grid"}}};
  static const std::vector<std::string> none;
  auto it = keys.find(kind);
  return it == keys.end() ? none : it->second;
}

style::PointStyle ParsePointStyle(std::string_view text) {
  style::PointStyle s;
  for (const auto& a : Assignments(text)) {
    if (Common(a, s.common)) continue;
    if (a.key == "size") s.size = Float(a, 0, 1e9f);
    else if (a.key == "marker")
      s.marker = Enum<style::MarkerShape>(a, {{"circle", style::MarkerShape::Circle},
                                              {"square", style::MarkerShape::Square},
                                              {"diamond", style::MarkerShape::Diamond},
                                              {"triangle", style::MarkerShape::Triangle},
                                              {"cross", style::MarkerShape::Cross}});
    else Unknown(a, "point");
  }
  return s;
}

style::LineStyle ParseLineStyle(std::string_view text) {
  style::LineStyle s;
  for (const auto& a : Assignments(text)) {
    if (Common(a, s.common)) continue;
    if (a.key == "width") s.width = Float(a, 0, 1e9f);
    else if (a.key == "dash")
      s.dash = Enum<style::DashPattern>(a, {{"solid", style::DashPattern::Solid},
                                            {"dashed", style::DashPattern::Dashed},
                                            {"dotted", style::DashPattern::Dotted},
                                            {"dashdot", style::DashPattern::DashDot}});
    else Unknown(a, "line");
  }
  return s;
}

style::SurfaceStyle ParseSurfaceStyle(std::string_view text) {
  style::SurfaceStyle s;
  for (const auto& a : Assignments(text)) {
    if (Common(a, s.common)) continue;
    if (a.key == "edgecolor") s.edgeColor = Colour(a);
    else if (a.key == "edgewidth") s.edgeWidth = Float(a, 0, 1e9f);
    else if (a.key == "wireframe") s.wireframe = Bool(a);
    else Unknown(a, "surface");
  }
  return s;
}

style::EnvironmentStyle ParseEnvironment(std::string_view text) {
  style::EnvironmentStyle s;
  for (const auto& a : Assignments(text)) {
    if (a.key == "background") s.background = Colour(a);
    else if (a.key == "axes") s.axes = Bool(a);
    else if (a.key == "grid") s.grid = Bool(a);
    else Unknown(a, "environment");
  }
  return s;
}

scene::layout::LayoutNode ParseLayout(std::string_view text) { return LayoutParser(text).Parse(); }

std::string FormatLayout(const scene::layout::LayoutNode& node) {
  return std::visit(
      [](const auto& v) -> std::string {
        using V = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<V, scene::layout::Pane>) {
          return LayoutWord(v.sceneName);
        } else {
          std::string out;
          if constexpr (std::is_same_v<V, scene::layout::Split>)
            out = v.orientation == scene::layout::Orientation::Vertical ? "vsplit(" : "split(";
          else
            out = "tabs(";
          for (std::size_t i = 0; i < v.children.size(); ++i) {
            const auto& c = v.children[i];
            out += (i ? ", " : "") + FormatLayout(c.node);
            if (c.label) out += " as " + Quoted(*c.label);
            if constexpr (std::is_same_v<V, scene::layout::Split>)
              if (c.weight != 1) out += " *" + std::to_string(c.weight);
          }
          return out + ")";
        }
      },
      node.value);
}

std::vector<std::string> SplitWords(std::string_view line) {
  std::vector<std::string> out;
  std::size_t i = 0;
  while (i < line.size()) {
    while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) ++i;
    if (i >= line.size()) break;
    std::string w;
    bool any = false;
    while (i < line.size() && !std::isspace(static_cast<unsigned char>(line[i]))) {
      if (line[i] == '"') {
        const std::size_t col = i + 1;
        ++i;
        bool closed = false;
        while (i < line.size()) {
          if (line[i] == '\\' && i + 1 < line.size()) {
            w += line[i + 1];
            i += 2;
          } else if (line[i] == '"') {
            ++i;
            closed = true;
            break;
          } else {
            w += line[i++];
          }
        }
        if (!closed) throw ParseError("unterminated string", col);
        any = true;
      } else {
        w += line[i++];
        any = true;
      }
    }
    if (any) out.push_back(w);
  }
  return out;
}

std::string QuoteWord(const std::string& word) {
  bool plain = !word.empty();
  for (char c : word) plain = plain && !std::isspace(static_cast<unsigned char>(c)) && c != '"' && c != '\\';
  if (plain) return word;
  std::string out = "\"";
  for (char c : word) {
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  return out + "\"";
}

}  // namespace fccvis::api
