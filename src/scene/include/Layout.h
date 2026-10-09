#pragma once

/**
 * @file Layout.h
 * @brief Backend-independent description of how scenes are arranged in a session.
 *
 * A layout is a value-owned tree of @ref fccvis::layout::LayoutNode. It only
 * *references* scenes by name; scenes exist independently of layouts and may be
 * placed in any number of panes. View state (camera, filters) belongs to the
 * scene, so every pane showing the same scene shows the same view.
 *
 * Nothing here depends on a rendering backend. Backends consume a layout by
 * visiting its three alternatives: Pane, Split and Tabs.
 */

#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace fccvis::scene::layout {

struct LayoutNode;

/**
 * @brief Direction along which a @ref Split arranges its children.
 */
enum class Orientation {
  /** @brief Children are placed side by side. */
  Horizontal,
  /** @brief Children are stacked top to bottom. */
  Vertical
};

/**
 * @brief Entry of a @ref Split: a child node with its share of space.
 *
 * Templated on the node type only to break the circular dependency with
 * @ref LayoutNode, which is incomplete where the slot is declared.
 *
 * @tparam N Node type; always @ref LayoutNode in practice.
 */
template <typename N>
struct SplitSlot {
  /** @brief The child node. */
  N node;

  /**
   * @brief Optional display label.
   *
   * Presentation only: labels need not be unique and are never used to look
   * up layout elements. When empty, a @ref Pane displays its scene name and
   * other nodes display a backend default. The fallback is not stored back.
   */
  std::optional<std::string> label;

  /**
   * @brief Relative share of the available space along the split axis.
   * @pre Must be greater than zero (checked by @ref Validate).
   */
  int weight = 1;
};

/**
 * @brief Entry of a @ref Tabs: a child node presented as one tab.
 *
 * @tparam N Node type; always @ref LayoutNode in practice.
 */
template <typename N>
struct TabSlot {
  /** @brief The child node shown when the tab is selected. */
  N node;

  /**
   * @brief Optional tab title.
   *
   * Presentation only, with the same fallback rule as
   * @ref SplitSlot::label.
   */
  std::optional<std::string> label;
};

/**
 * @brief Leaf of the layout: a placement of one scene.
 *
 * The scene is identified by its unique name. The same scene may appear in
 * any number of panes, across splits and tabs; all placements share the
 * scene's view state.
 */
struct Pane {
  /** @brief Unique name of the referenced scene. */
  std::string sceneName;
};

/**
 * @brief Container arranging its children along one axis with relative weights.
 */
struct Split {
  /** @brief Axis along which children are arranged. Horizontal by default. */
  Orientation orientation = Orientation::Horizontal;

  /** @brief Children in display order. Must not be empty (see @ref Validate). */
  std::vector<SplitSlot<LayoutNode>> children;
};

/**
 * @brief Container presenting its children as alternative tabs.
 */
struct Tabs {
  /** @brief Children in tab order. Must not be empty (see @ref Validate). */
  std::vector<TabSlot<LayoutNode>> children;
};

/**
 * @brief Node of the layout tree.
 *
 * Exactly one of @ref Pane, @ref Split or @ref Tabs. Fields that only make
 * sense for one kind (weights, orientation, scene name) exist only in that
 * kind, so invalid combinations cannot be expressed.
 *
 * The tree is owned by value: copying a node deep-copies its subtree.
 * Consumers dispatch with `std::visit`, which fails to compile if a new kind
 * is added and left unhandled.
 *
 * @par Example
 * @code
 * LayoutNode root{Split{
 *   Orientation::Horizontal,
 *   {
 *     {LayoutNode{Pane{"tracker"}}, std::nullopt, 2},
 *     {LayoutNode{Tabs{{
 *        {LayoutNode{Pane{"calo"}},   std::string("Calorimeter")},
 *        {LayoutNode{Pane{"tracker"}}, std::nullopt},
 *     }}}, std::nullopt, 1},
 *   }
 * }};
 * @endcode
 */
struct LayoutNode {
  /** @brief The concrete node: a Pane, a Split or a Tabs. */
  std::variant<Pane, Split, Tabs> value;
};

/**
 * @brief Checks the structural invariants of a layout.
 *
 * Verified conditions:
 * - every @ref Pane references a scene accepted by @p hasScene;
 * - every @ref Split and @ref Tabs has at least one child;
 * - every @ref SplitSlot::weight is greater than zero.
 *
 * The same scene appearing in several panes is valid and is not reported.
 * All violations are collected in one pass rather than stopping at the first.
 *
 * @param n        Root of the (sub)tree to check.
 * @param hasScene Predicate returning true if a scene name is registered.
 *                 A predicate is used so this module does not depend on the
 *                 scene registry or the session.
 * @param errors   Receives one message per violation; existing entries are kept.
 */
inline void Validate(const LayoutNode& n,
                     const std::function<bool(const std::string&)>& hasScene,
                     std::vector<std::string>& errors) {
  std::visit(
      [&](const auto& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, Pane>) {
          if (!hasScene(v.sceneName))
            errors.push_back("unknown scene: " + v.sceneName);
        } else if constexpr (std::is_same_v<T, Split>) {
          if (v.children.empty()) errors.push_back("empty split");
          for (const auto& s : v.children) {
            if (s.weight <= 0) errors.push_back("non-positive weight in split");
            Validate(s.node, hasScene, errors);
          }
        } else {
          if (v.children.empty()) errors.push_back("empty tabs");
          for (const auto& s : v.children) Validate(s.node, hasScene, errors);
        }
      },
      n.value);
}

/**
 * @brief Convenience overload returning the collected violations.
 *
 * @param n        Root of the (sub)tree to check.
 * @param hasScene Predicate returning true if a scene name is registered.
 * @return All violations found; empty if the layout is valid.
 */
[[nodiscard]] inline std::vector<std::string>
Validate(const LayoutNode& n,
         const std::function<bool(const std::string&)>& hasScene) {
  std::vector<std::string> errors;
  Validate(n, hasScene, errors);
  return errors;
}

/**
 * @brief Reduces a layout to the scenes accepted by @p isOpen.
 *
 * A pane survives if its scene is open. Containers drop closed children,
 * disappear when empty, and collapse into their only remaining child.
 * Weights are kept among the survivors.
 *
 * @return The pruned tree, or nullopt if nothing remains.
 */
inline std::optional<LayoutNode>
Prune(const LayoutNode& n,
      const std::function<bool(const std::string&)>& isOpen) {
  return std::visit(
      [&](const auto& v) -> std::optional<LayoutNode> {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, Pane>) {
          if (!isOpen(v.sceneName)) return std::nullopt;
          return n;
        } else if constexpr (std::is_same_v<T, Split>) {
          Split out;
          out.orientation = v.orientation;
          for (const auto& s : v.children)
            if (auto c = Prune(s.node, isOpen))
              out.children.push_back({std::move(*c), s.label, s.weight});
          if (out.children.empty()) return std::nullopt;
          if (out.children.size() == 1) return std::move(out.children.front().node);
          return LayoutNode{std::move(out)};
        } else {
          Tabs out;
          for (const auto& s : v.children)
            if (auto c = Prune(s.node, isOpen))
              out.children.push_back({std::move(*c), s.label});
          if (out.children.empty()) return std::nullopt;
          if (out.children.size() == 1) return std::move(out.children.front().node);
          return LayoutNode{std::move(out)};
        }
      },
      n.value);
}

}  // namespace fccvis::layout
