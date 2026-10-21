// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
// T2.2: attribute and energy filters, selection helpers; positional Filter.
#include <catch2/catch_test_macros.hpp>

#include "Filter.h"
#include "Scene.h"
#include "fccvis/stdlib/Filters.h"

#include <regex>
#include <span>
#include <string>
#include <vector>

using namespace fccvis;
using namespace fccvis::stdlib;
using PE = data::PointElement;
using LE = data::LineElement;

namespace {

PE Hit(double energy, std::string subdetector = "ECal", std::int64_t layer = 3) {
  PE e;
  e.meta.Set("energy", energy);
  e.meta.Set("subdetector", std::move(subdetector));
  e.meta.Set("layer", layer);
  e.meta.Set("isPrimary", energy > 1.0);
  return e;
}

PE Bare() { return PE{}; }

template <typename X>
std::vector<std::size_t> Passing(const filter::Filter<X>& f, const std::vector<X>& xs) {
  std::vector<std::size_t> out;
  for (std::size_t i = 0; i < xs.size(); ++i)
    if (f.DoesPass(xs[i], i)) out.push_back(i);
  return out;
}

}  // namespace

TEST_CASE("Filter keeps working with plain predicates and gains positions", "[filter]") {
  filter::Filter<int> even = [](const int& x) { return x % 2 == 0; };
  filter::Filter<int> firstTwo = [](const int&, std::size_t i) { return i < 2; };
  const std::vector<int> xs{2, 4, 5, 6};
  CHECK(even.Apply(std::span<const int>(xs)) == std::vector<int>{2, 4, 6});
  CHECK(firstTwo.Apply(std::span<const int>(xs)) == std::vector<int>{2, 4});
  CHECK((even & !firstTwo).Apply(std::span<const int>(xs)) == std::vector<int>{6});
  CHECK(firstTwo.DoesPass(2, 0));
  CHECK_FALSE(firstTwo.DoesPass(2));  // no index known
  CHECK(even.DoesPass(2));
  CHECK(filter::Filter<int>{}.DoesPass(7));
}

TEST_CASE("Attribute comparisons", "[stdlib][filters]") {
  const std::vector<PE> hits{Hit(0.2), Hit(1.5, "HCal", 7), Hit(3.0, "ECal", 12), Bare()};
  using V = std::vector<std::size_t>;

  filter::Filter<PE> f = Attr<double>("energy") > 1.0;
  CHECK(Passing(f, hits) == V{1, 2});
  CHECK(Passing<PE>(Attr<double>("energy") >= 1.5, hits) == V{1, 2});
  CHECK(Passing<PE>(Attr<double>("energy") < 1.5, hits) == V{0});
  CHECK(Passing<PE>(Attr<double>("energy") <= 1.5, hits) == V{0, 1});
  CHECK(Passing<PE>(Attr<double>("energy") == 3.0, hits) == V{2});
  CHECK(Passing<PE>(Attr<double>("energy") != 3.0, hits) == V{0, 1});  // missing does not pass
  CHECK(Passing<PE>(Attr<std::int64_t>("layer") == 7, hits) == V{1});
  CHECK(Passing<PE>(Attr<std::string>("subdetector") == "ECal", hits) == V{0, 2});
  CHECK(Passing<PE>(Attr<std::string>("subdetector") != "ECal", hits) == V{1});
  CHECK(Passing<PE>(Attr<bool>("isPrimary") == true, hits) == V{1, 2});
  // type mismatch: a text attribute compared numerically does not pass
  CHECK(Passing<PE>(Attr<double>("subdetector") > 0.0, hits).empty());
  // negation is plain logic: missing attributes pass !(...)
  CHECK(Passing<PE>(!(Attr<double>("energy") > 1.0), hits) == V{0, 3});
}

TEST_CASE("Range, membership, existence and regex filters", "[stdlib][filters]") {
  const std::vector<PE> hits{Hit(0.2), Hit(1.5, "HCal", 7), Hit(3.0, "EcalEndcap", 12), Bare()};
  using V = std::vector<std::size_t>;
  CHECK(Passing<PE>(Between("energy", 0.2, 1.5), hits) == V{0, 1});
  CHECK(Passing<PE>(OneOf("layer", {3.0, 12.0}), hits) == V{0, 2});
  CHECK(Passing<PE>(OneOf("subdetector", {"HCal", "Muon"}), hits) == V{1});
  CHECK(Passing<PE>(Exists("energy"), hits) == V{0, 1, 2});
  CHECK(Passing<PE>(Matches("subdetector", "E[cC]al.*"), hits) == V{0, 2});
  CHECK(Passing<PE>(Matches("subdetector", "cal"), hits).empty());  // full match
  CHECK_THROWS_AS(Matches("x", "("), std::regex_error);
}

TEST_CASE("Energy convenience filters", "[stdlib][filters]") {
  const std::vector<PE> hits{Hit(0.2), Hit(1.5), Hit(3.0), Bare()};
  using V = std::vector<std::size_t>;
  CHECK(Passing<PE>(EnergyAbove(1.0), hits) == V{1, 2});
  CHECK(Passing<PE>(EnergyBelow(1.0), hits) == V{0});
  CHECK(Passing<PE>(EnergyBetween(1.0, 3.0), hits) == V{1, 2});
  PE custom;
  custom.meta.Set("E", 5.0);
  CHECK(EnergyAbove(1.0, "E").DoesPass(custom));
  CHECK_FALSE(EnergyAbove(1.0).DoesPass(custom));
}

TEST_CASE("Selection helpers and composition", "[stdlib][filters]") {
  const std::vector<PE> hits{Hit(0.2), Hit(1.5), Hit(3.0), Hit(4.0)};
  using V = std::vector<std::size_t>;
  CHECK(Passing<PE>(Index(2), hits) == V{2});
  CHECK(Passing<PE>(FirstN(2), hits) == V{0, 1});
  CHECK(Passing<PE>(All(), hits) == V{0, 1, 2, 3});
  CHECK(Passing<PE>(None(), hits).empty());
  CHECK(Passing(All<PE>(), hits) == V{0, 1, 2, 3});
  CHECK(Passing(None<PE>(), hits).empty());
  CHECK(Passing<PE>(FirstN(3) & EnergyAbove(1.0), hits) == V{1, 2});
  CHECK(Passing<PE>(Index(0) | EnergyAbove(3.5), hits) == V{0, 3});
  // mixing with an existing Filter<X>
  filter::Filter<PE> custom = [](const PE& e) { return e.meta.Number("energy").value_or(0) < 2; };
  CHECK(Passing(custom & EnergyAbove(1.0), hits) == V{1});
  CHECK(Passing(EnergyAbove(1.0) | custom, hits) == V{0, 1, 2, 3});
  // the same selector works for lines
  LE l;
  l.meta.Set("energy", 2.0);
  filter::Filter<LE> fl = EnergyAbove(1.0);
  CHECK(fl.DoesPass(l));
}

TEST_CASE("Positional filters work inside a scene layer", "[stdlib][filters][scene]") {
  data::SessionData d;
  struct S final : data::Series<PE> {
    std::size_t Size() const override { return 1; }
    std::vector<PE> Load(std::size_t) const override { return {Hit(0.1), Hit(0.2), Hit(5.0)}; }
  };
  REQUIRE(d.Add<PE>("hits", "mem", std::make_unique<S>()));
  scene::Scene sc("a");
  auto* layer = sc.AddLayer<PE>("hits", {"hits", ""});
  layer->filter = std::make_shared<filter::Filter<PE>>(FirstN(2));
  CHECK(sc.Prepare<PE>(d, "hits").size() == 2);
  CHECK(sc.Select<PE>(d, "hits").size() == 2);
  // positional style rule: highlight only the first element
  style::PointStyle big;
  big.size = 9.f;
  layer->filter.reset();
  layer->styles.Add(Index(0), big);
  const auto styled = sc.Prepare<PE>(d, "hits");
  REQUIRE(styled.size() == 3);
  CHECK(styled[0].style.size == 9.f);
  CHECK_FALSE(styled[1].style.size.has_value());
}
