#pragma once

/**
 * @file Mergeable.h
 * @brief Field-wise merging of partial records.
 *
 * A *partial record* is a struct whose fields may be undefined. A field is
 * defined when an @c std::optional has a value or a smart pointer is
 * non-null. Records opt in by deriving from @ref fccvis::merge::Mergeable and
 * listing their members in a static @c Members() function.
 *
 * Two operations are provided:
 * - @c a.TakeRef(b): values defined in @c b replace those in @c a.
 * - @c a.CompleteBy(b): values undefined in @c a are filled from @c b.
 *
 * Both return a new record and keep the receiver's non-listed state
 * (for example a scene's identity).
 *
 * Types with their own merge policy (type-keyed collections, style sheets)
 * provide @c TakeRefInto / @c CompleteInto overloads found by ADL.
 */

#include <concepts>
#include <tuple>
#include <utility>

namespace fccvis::merge {

/**
 * @concept PartialRecord
 * @brief Satisfied by types providing @c TakeRef returning the same type.
 */
template <typename T>
concept PartialRecord = requires(const T &a, const T &b) {
  { a.TakeRef(b) } -> std::same_as<T>;
};

/**
 * @brief Merges @p b into @p a, @p b winning where it is defined.
 *
 * Nested partial records are merged recursively; any other type must be
 * contextually convertible to bool (optional, shared_ptr, ...), and a defined
 * value in @p b replaces @p a.
 */
template <typename T> void TakeRefInto(T &a, const T &b) {
  if constexpr (PartialRecord<T>) {
    a = a.TakeRef(b);
  } else {
    if (b)
      a = b;
  }
}

/**
 * @brief Merges @p b into @p a, @p a winning where it is defined.
 *
 * Nested partial records are merged recursively; otherwise @p a is replaced
 * only if it is undefined.
 */
template <typename T> void CompleteInto(T &a, const T &b) {
  if constexpr (PartialRecord<T>) {
    a = a.CompleteBy(b);
  } else {
    if (!a)
      a = b;
  }
}

/**
 * @brief CRTP base providing @c TakeRef and @c CompleteBy.
 *
 * @tparam Derived Record type. Must provide
 *         `static constexpr auto Members()` returning a `std::tuple` of
 *         pointers to the data members that take part in merging.
 */
template <typename Derived> struct Mergeable {
  /**
   * @brief Returns a copy of this record overridden by the values defined in @p
   * b.
   */
  [[nodiscard]] Derived TakeRef(const Derived &b) const {
    Derived out = static_cast<const Derived &>(*this);
    std::apply([&](auto... m) { (TakeRefInto(out.*m, b.*m), ...); },
               Derived::Members());
    return out;
  }

  /**
   * @brief Returns a copy of this record with undefined values filled from @p
   * b.
   */
  [[nodiscard]] Derived CompleteBy(const Derived &b) const {
    Derived out = static_cast<const Derived &>(*this);
    std::apply([&](auto... m) { (CompleteInto(out.*m, b.*m), ...); },
               Derived::Members());
    return out;
  }
};

} // namespace fccvis::merge
