#pragma once
/**
 * @file Filter.h
 * @brief Composable, non-mutating element selection (`Filter<X>`).
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#include <concepts>
#include <cstddef>
#include <functional>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace fccvis::filter {

/**
 * @concept PredicateOf
 * @brief Satisfied when `P` is callable with `const X&` and returns a value convertible to `bool`.
 * @tparam P Callable type (lambda, functor, function pointer).
 * @tparam X Element type the predicate is evaluated on.
 */
template <typename P, typename X>
concept PredicateOf = std::predicate<const P&, const X&>;

/**
 * @concept IndexedPredicateOf
 * @brief Satisfied when `P` is callable with `(const X&, std::size_t index)` and
 *        returns a value convertible to `bool`, and is not a plain predicate.
 *
 * The index is the element's position within the entry being filtered (see
 * Filter::DoesPass(const X&, std::size_t)).
 */
template <typename P, typename X>
concept IndexedPredicateOf =
    std::predicate<const P&, const X&, std::size_t> && !PredicateOf<P, X>;

/** @brief Index passed when an element's position is unknown. */
inline constexpr std::size_t kNoIndex = std::numeric_limits<std::size_t>::max();

/**
 * @class Filter
 * @brief Non-mutating selection switch applied before data reaches the visualization engine.
 *
 * A `Filter<X>` decides whether an element of type `X` passes. It never modifies,
 * owns, or reorders loaded data; it only selects what is handed on for rendering.
 *
 * Filters are value types and compose with `&` (and), `|` (or) and `!` (not).
 * Composition is restricted to filters of the same element type. A `Filter<B>`
 * converts implicitly to `Filter<X>` when `X` is convertible to `B`
 * (contravariance), so a filter written for a base type can be used wherever a
 * filter for a derived type is required.
 *
 * @par Example
 * @code
 * Filter<const Hit*> energetic = [](const Hit* h) { return h->energy() > 0.1; };
 * Filter<const Hit*> inner     = [](const Hit* h) { return h->layer() < 5; };
 *
 * auto selected = (energetic & inner).Apply(hits);
 * @endcode
 *
 * @par Lifetime
 * Operands of composed filters are copied into the resulting filter. Any state
 * captured *by reference* by a user predicate must outlive the filter; prefer
 * capture by value or `std::shared_ptr` for filters that outlive their creation scope.
 *
 * @tparam X Element type being filtered. Use a pointer or `std::reference_wrapper`
 *           if copying `X` is expensive.
 */
template <typename X>
class Filter {
public:
    /** @brief Signature of a plain predicate. */
    using Predicate = std::function<bool(const X&)>;

    /** @brief Type-erased predicate stored by the filter: element and its index. */
    using IndexedPredicate = std::function<bool(const X&, std::size_t)>;

    /**
     * @brief Constructs the identity filter, which passes every element.
     * @note This is the neutral element of `operator&`; `!Filter<X>{}` (reject all)
     *       is the neutral element of `operator|`.
     */
    Filter() : fn_([](const X&, std::size_t) { return true; }) {}

    /**
     * @brief Constructs a filter from any callable `X -> bool`.
     * @tparam P Callable type satisfying `PredicateOf<P, X>`.
     * @param p  The predicate; forwarded into the filter.
     */
    template <PredicateOf<X> P>
    Filter(P&& p)
        : fn_([q = std::forward<P>(p)](const X& x, std::size_t) { return static_cast<bool>(q(x)); }) {}

    /**
     * @brief Constructs a positional filter from a callable `(X, index) -> bool`.
     *
     * Positional filters see the element's index within its entry. When the
     * index is unknown (DoesPass(x) without an index) they receive kNoIndex.
     *
     * @tparam P Callable type satisfying `IndexedPredicateOf<P, X>`.
     * @param p  The predicate; forwarded into the filter.
     */
    template <IndexedPredicateOf<X> P>
    Filter(P&& p) : fn_(std::forward<P>(p)) {}

    /**
     * @brief Converting constructor from a filter over a base type.
     *
     * Enables passing a `Filter<B>` where a `Filter<X>` is expected, provided
     * `X` is convertible to `B`. The source filter is copied.
     *
     * @tparam B Element type of the source filter, distinct from `X`.
     * @param f  Source filter.
     */
    template <typename B>
        requires (!std::same_as<B, X> && std::convertible_to<X, B>)
    Filter(const Filter<B>& f)
        : fn_([f](const X& x, std::size_t i) { return f.DoesPass(x, i); }) {}

    /**
     * @brief Tests a single element whose position is unknown.
     * @param x Element to test.
     * @return `true` if `x` passes the filter, `false` otherwise.
     */
    [[nodiscard]] bool DoesPass(const X& x) const { return fn_(x, kNoIndex); }

    /**
     * @brief Tests a single element at a known position of its entry.
     * @param x     Element to test.
     * @param index Position of @p x within its entry (0-based).
     * @return `true` if `x` passes the filter, `false` otherwise.
     */
    [[nodiscard]] bool DoesPass(const X& x, std::size_t index) const { return fn_(x, index); }

    /**
     * @brief Selects the passing elements of a sequence.
     *
     * Preserves input order. The input is not modified; passing elements are
     * copied into a new vector.
     *
     * @param in Input sequence.
     * @return New vector containing the elements of `in` for which `DoesPass` is `true`.
     * @note This is the `[X] -> [X]` operation. It is named `Apply` because a
     *       member function cannot share its class name.
     */
    [[nodiscard]] std::vector<X> Apply(std::span<const X> in) const {
        std::vector<X> out;
        out.reserve(in.size());
        for (std::size_t i = 0; i < in.size(); ++i)
            if (DoesPass(in[i], i)) out.push_back(in[i]);
        return out;
    }

    /**
     * @brief Logical conjunction.
     * @return A filter passing an element only if both `a` and `b` pass it.
     *         `b` is evaluated only if `a` passes (short-circuit).
     */
    friend Filter operator&(const Filter& a, const Filter& b) {
        return Filter([a, b](const X& x, std::size_t i) { return a.DoesPass(x, i) && b.DoesPass(x, i); });
    }

    /**
     * @brief Logical disjunction.
     * @return A filter passing an element if `a` or `b` passes it.
     *         `b` is evaluated only if `a` rejects (short-circuit).
     */
    friend Filter operator|(const Filter& a, const Filter& b) {
        return Filter([a, b](const X& x, std::size_t i) { return a.DoesPass(x, i) || b.DoesPass(x, i); });
    }

    /**
     * @brief Logical negation.
     * @return A filter passing exactly the elements that `a` rejects.
     */
    friend Filter operator!(const Filter& a) {
        return Filter([a](const X& x, std::size_t i) { return !a.DoesPass(x, i); });
    }

private:
    IndexedPredicate fn_;  ///< Type-erased predicate; the single source of truth for filter behaviour.
};



} // namespace fccvis::filter
