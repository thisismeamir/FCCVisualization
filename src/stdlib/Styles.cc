// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
#include "fccvis/stdlib/Styles.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace fccvis::stdlib {

namespace {

style::Color Mix(style::Color a, style::Color b, double t) {
  auto lerp = [t](std::uint8_t x, std::uint8_t y) {
    return static_cast<std::uint8_t>(std::lround(x + (static_cast<double>(y) - x) * t));
  };
  return {lerp(a.r, b.r), lerp(a.g, b.g), lerp(a.b, b.b), lerp(a.a, b.a)};
}

}  // namespace

Palette::Palette(std::vector<style::Color> stops) : m_stops(std::move(stops)) {
  if (m_stops.empty()) throw std::invalid_argument("Palette needs at least one colour");
}

style::Color Palette::At(double t) const {
  if (m_stops.size() == 1 || !(t > 0)) return m_stops.front();
  if (t >= 1) return m_stops.back();
  const double x = t * static_cast<double>(m_stops.size() - 1);
  const auto i = static_cast<std::size_t>(x);
  return Mix(m_stops[i], m_stops[i + 1], x - static_cast<double>(i));
}

style::Color Palette::Index(std::size_t i) const { return m_stops[i % m_stops.size()]; }

Palette Monochrome(style::Color accent) {
  const style::Color white{255, 255, 255, 255};
  return Palette({Mix(white, accent, 0.15), accent});
}

Palette Categorical(std::size_t n) {
  // IBM-style categorical sequence; accent first.
  static const std::uint32_t kSet[] = {0x7017FF, 0x1192E8, 0x005D5D, 0x9F1853, 0xFA4D56, 0x570408, 0x198038,
                                       0x002D9C, 0xEE538B, 0xB28600, 0x009D9A, 0x012749, 0x8A3800, 0xA56EFF};
  constexpr std::size_t kCount = sizeof(kSet) / sizeof(kSet[0]);
  std::vector<style::Color> c;
  for (std::size_t i = 0; i < std::max<std::size_t>(n, 1); ++i) c.push_back(style::Color::FromHex(kSet[i % kCount]));
  return Palette(std::move(c));
}

Palette Sequential(style::Color lo, style::Color hi) { return Palette({lo, hi}); }

Palette Diverging(style::Color lo, style::Color mid, style::Color hi) { return Palette({lo, mid, hi}); }

namespace detail {
void CheckBinning(Range range, std::size_t bins) {
  if (bins == 0) throw std::invalid_argument("binning needs at least one bin");
  if (!(range.hi > range.lo)) throw std::invalid_argument("binning range needs hi > lo");
}
}  // namespace detail

Selector Bin(const std::string& attribute, Range range, std::size_t i, std::size_t bins) {
  detail::CheckBinning(range, bins);
  const double w = (range.hi - range.lo) / static_cast<double>(bins);
  const double a = range.lo + w * static_cast<double>(i);
  const double b = a + w;
  const bool first = i == 0, last = i + 1 >= bins;
  return FromMetadata([attribute, a, b, first, last](const data::Metadata& m) {
    auto v = m.Number(attribute);
    if (!v) return false;
    return (first || *v >= a) && (last || *v < b);
  });
}

style::StyleSheet<data::SurfaceElement> DetectorOutline() {
  style::StyleSheet<data::SurfaceElement> s;
  s.base.common.color = style::Color::FromHex(0xC6C6C6);
  s.base.common.opacity = 0.15f;
  s.base.edgeColor = style::Color::FromHex(0x525252);
  s.base.edgeWidth = 0.5f;
  s.base.wireframe = false;
  return s;
}

style::StyleSheet<data::PointElement> HitsByEnergy(Range range, const std::string& attribute) {
  style::StyleSheet<data::PointElement> s;
  s.base.size = 2.0f;
  s.base.marker = style::MarkerShape::Circle;
  AddRules(s, ColorByAttribute<data::PointElement>(attribute, Monochrome(), range));
  AddRules(s, SizeByAttribute<data::PointElement>(attribute, 1.5f, 6.0f, range));
  return s;
}

style::StyleSheet<data::LineElement> TracksByCharge(const std::string& attribute) {
  style::StyleSheet<data::LineElement> s;
  s.base.width = 1.5f;
  s.base.common.color = style::Color::FromHex(0x8D8D8D);
  style::LineStyle neg, pos, neutral;
  neg.common.color = style::Color::FromHex(0x1192E8);
  pos.common.color = style::Color::FromHex(kAccent);
  neutral.dash = style::DashPattern::Dashed;
  s.Add(Attr<double>(attribute) < 0.0, neg);
  s.Add(Attr<double>(attribute) > 0.0, pos);
  s.Add(Attr<double>(attribute) == 0.0, neutral);
  return s;
}

style::EnvironmentStyle Publication() {
  style::EnvironmentStyle e;
  e.background = style::Color::FromHex(0xFFFFFF);
  e.axes = true;
  e.grid = false;
  return e;
}

style::EnvironmentStyle DefaultEnvironment() {
  style::EnvironmentStyle e;
  e.background = style::Color::FromHex(0xFFFFFF);
  e.axes = false;
  e.grid = true;
  return e;
}

}  // namespace fccvis::stdlib
