// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
#include "fccvis/stdlib/Filters.h"

#include <algorithm>
#include <memory>
#include <regex>

namespace fccvis::stdlib {

Selector FromMetadata(std::function<bool(const data::Metadata&)> pred) {
  auto p = std::make_shared<const std::function<bool(const data::Metadata&)>>(std::move(pred));
  return {filter::Filter<data::PointElement>([p](const data::PointElement& e) { return (*p)(e.meta); }),
          filter::Filter<data::LineElement>([p](const data::LineElement& e) { return (*p)(e.meta); }),
          filter::Filter<data::SurfaceElement>([p](const data::SurfaceElement& e) { return (*p)(e.meta); })};
}

Selector FromIndex(std::function<bool(std::size_t)> pred) {
  auto p = std::make_shared<const std::function<bool(std::size_t)>>(std::move(pred));
  auto test = [p](std::size_t i) { return i != filter::kNoIndex && (*p)(i); };
  return {filter::Filter<data::PointElement>([test](const data::PointElement&, std::size_t i) { return test(i); }),
          filter::Filter<data::LineElement>([test](const data::LineElement&, std::size_t i) { return test(i); }),
          filter::Filter<data::SurfaceElement>([test](const data::SurfaceElement&, std::size_t i) { return test(i); })};
}

Selector Between(const std::string& name, double lo, double hi) {
  return FromMetadata([name, lo, hi](const data::Metadata& m) {
    auto v = m.Number(name);
    return v && *v >= lo && *v <= hi;
  });
}

Selector OneOf(const std::string& name, std::vector<double> values) {
  return FromMetadata([name, values = std::move(values)](const data::Metadata& m) {
    auto v = m.Number(name);
    return v && std::find(values.begin(), values.end(), *v) != values.end();
  });
}

Selector OneOf(const std::string& name, std::vector<std::string> values) {
  return FromMetadata([name, values = std::move(values)](const data::Metadata& m) {
    auto v = m.Text(name);
    return v && std::find(values.begin(), values.end(), *v) != values.end();
  });
}

Selector OneOf(const std::string& name, std::initializer_list<const char*> values) {
  return OneOf(name, std::vector<std::string>(values.begin(), values.end()));
}

Selector Exists(const std::string& name) {
  return FromMetadata([name](const data::Metadata& m) { return m.Has(name); });
}

Selector Matches(const std::string& name, const std::string& regex) {
  auto re = std::make_shared<const std::regex>(regex, std::regex::ECMAScript);
  return FromMetadata([name, re](const data::Metadata& m) {
    auto v = m.Text(name);
    return v && std::regex_match(*v, *re);
  });
}

Selector EnergyAbove(double x, const std::string& attribute) { return Attr<double>(attribute) > x; }
Selector EnergyBelow(double x, const std::string& attribute) { return Attr<double>(attribute) < x; }
Selector EnergyBetween(double lo, double hi, const std::string& attribute) {
  return Between(attribute, lo, hi);
}

Selector Index(std::size_t i) {
  return FromIndex([i](std::size_t k) { return k == i; });
}
Selector FirstN(std::size_t n) {
  return FromIndex([n](std::size_t k) { return k < n; });
}
Selector All() { return Selector{}; }
Selector None() { return !Selector{}; }

}  // namespace fccvis::stdlib
