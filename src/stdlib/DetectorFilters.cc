// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
#include "fccvis/stdlib/DetectorFilters.h"

namespace fccvis::stdlib {

Selector Subdetector(const std::string& name, const std::string& attribute) {
  return Attr<std::string>(attribute) == name;
}

Selector Layer(std::int64_t n, const std::string& attribute) { return Attr<std::int64_t>(attribute) == n; }

Selector LayerRange(std::int64_t first, std::int64_t last, const std::string& attribute) {
  const Attr<std::int64_t> layer(attribute);
  return (layer >= first) & (layer <= last);
}

Selector Volume(const std::string& regex, const std::string& attribute) { return Matches(attribute, regex); }

Selector Material(const std::string& name, const std::string& attribute) {
  return Attr<std::string>(attribute) == name;
}

}  // namespace fccvis::stdlib
