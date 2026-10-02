#pragma once
#include <concepts>
#include <functional>
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
    /** @brief Type-erased predicate signature stored by the filter. */
    using Predicate = std::function<bool(const X&)>;

    /**
     * @brief Constructs the identity filter, which passes every element.
     * @note This is the neutral element of `operator&`; `!Filter<X>{}` (reject all)
     *       is the neutral element of `operator|`.
     */
    Filter() : fn_([](const X&) { return true; }) {}

    /**
     * @brief Constructs a filter from any callable `X -> bool`.
     * @tparam P Callable type satisfying `PredicateOf<P, X>`.
     * @param p  The predicate; forwarded into the filter.
     */
    template <PredicateOf<X> P>
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
        : fn_([f](const X& x) { return f.DoesPass(x); }) {}

    /**
     * @brief Tests a single element.
     * @param x Element to test.
     * @return `true` if `x` passes the filter, `false` otherwise.
     */
    [[nodiscard]] bool DoesPass(const X& x) const { return fn_(x); }

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
        for (const X& x : in)
            if (DoesPass(x)) out.push_back(x);
        return out;
    }

    /**
     * @brief Logical conjunction.
     * @return A filter passing an element only if both `a` and `b` pass it.
     *         `b` is evaluated only if `a` passes (short-circuit).
     */
    friend Filter operator&(const Filter& a, const Filter& b) {
        return Filter([a, b](const X& x) { return a.DoesPass(x) && b.DoesPass(x); });
    }

    /**
     * @brief Logical disjunction.
     * @return A filter passing an element if `a` or `b` passes it.
     *         `b` is evaluated only if `a` rejects (short-circuit).
     */
    friend Filter operator|(const Filter& a, const Filter& b) {
        return Filter([a, b](const X& x) { return a.DoesPass(x) || b.DoesPass(x); });
    }

    /**
     * @brief Logical negation.
     * @return A filter passing exactly the elements that `a` rejects.
     */
    friend Filter operator!(const Filter& a) {
        return Filter([a](const X& x) { return !a.DoesPass(x); });
    }

private:
    Predicate fn_;  ///< Type-erased predicate; the single source of truth for filter behaviour.
};



} // namespace fccvis
