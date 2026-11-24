/**
 * @file Json.h
 * @brief Small JSON reader for the Phoenix adapter: order-preserving objects,
 *        exact 64-bit integers, and a scanner that finds the extent of a value
 *        without building it (so events can be parsed lazily).
 *
 * Internal to the adapter (not installed).
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace fccvis::adapters::json {

/** @brief Malformed JSON; the message gives line and column. */
class Error : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

struct Value;
using Array = std::vector<Value>;
using Object = std::vector<std::pair<std::string, Value>>;  ///< In file order.

/** @brief A JSON value. Integers without fraction or exponent that fit are kept exact. */
struct Value {
  std::variant<std::nullptr_t, bool, std::int64_t, double, std::string, std::shared_ptr<Array>, std::shared_ptr<Object>> v;

  [[nodiscard]] bool IsObject() const { return std::holds_alternative<std::shared_ptr<Object>>(v); }
  [[nodiscard]] bool IsArray() const { return std::holds_alternative<std::shared_ptr<Array>>(v); }
  [[nodiscard]] bool IsNumber() const { return std::holds_alternative<std::int64_t>(v) || std::holds_alternative<double>(v); }
  [[nodiscard]] const Object& AsObject() const { return *std::get<std::shared_ptr<Object>>(v); }
  [[nodiscard]] const Array& AsArray() const { return *std::get<std::shared_ptr<Array>>(v); }
  [[nodiscard]] double AsDouble() const {
    if (const auto* i = std::get_if<std::int64_t>(&v)) return static_cast<double>(*i);
    return std::get<double>(v);
  }
  /** @brief Member of an object, or nullptr. */
  [[nodiscard]] const Value* Find(std::string_view key) const {
    if (!IsObject()) return nullptr;
    for (const auto& [k, val] : AsObject())
      if (k == key) return &val;
    return nullptr;
  }
};

/** @brief Cursor over JSON text. */
class Reader {
 public:
  /** @param text The whole document. @param offset Start position. */
  explicit Reader(std::string_view text, std::size_t offset = 0) : m_s(text), m_i(offset) {}

  /** @brief Parses one value. */
  Value Parse();
  /** @brief Skips one value without building it; returns [begin, end) of its text. */
  std::pair<std::size_t, std::size_t> SkipValue();
  /** @brief Skips blanks. */
  void Blank();
  /** @brief Expects @p c after blanks. */
  void Expect(char c);
  /** @brief True (and consumes it) if the next non-blank character is @p c. */
  bool Accept(char c);
  /** @brief Parses a string (the opening quote is next). */
  std::string String();
  /** @brief Current position. */
  [[nodiscard]] std::size_t Position() const { return m_i; }
  /** @brief True at the end of the text (after blanks). */
  bool AtEnd() {
    Blank();
    return m_i >= m_s.size();
  }
  [[noreturn]] void Fail(const std::string& what) const;

 private:
  Value Number();
  std::string_view m_s;
  std::size_t m_i;
};

}  // namespace fccvis::adapters::json
