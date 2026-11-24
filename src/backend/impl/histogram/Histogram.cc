// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
#include "Histogram.h"

#include "Drawing.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <variant>

namespace fccvis::backend::hist {

namespace {

std::vector<std::string> Split(const std::string& s, char sep) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : s) {
    if (c == sep) {
      out.push_back(cur);
      cur.clear();
    } else {
      cur += c;
    }
  }
  out.push_back(cur);
  return out;
}

std::string Trim(const std::string& s) {
  const auto a = s.find_first_not_of(" \t");
  if (a == std::string::npos) return {};
  return s.substr(a, s.find_last_not_of(" \t") - a + 1);
}

double ParseNumber(const std::string& s, const std::string& item) {
  try {
    std::size_t used = 0;
    const double v = std::stod(s, &used);
    if (used != s.size() || !std::isfinite(v)) throw std::invalid_argument(s);
    return v;
  } catch (const std::exception&) {
    throw std::invalid_argument("histogram '" + item + "': '" + s + "' is not a number");
  }
}

std::string Csv(const std::string& s) {
  if (s.find_first_of(",\"\n") == std::string::npos) return s;
  std::string out = "\"";
  for (char c : s) out += c == '"' ? std::string("\"\"") : std::string(1, c);
  return out + "\"";
}

}  // namespace

std::vector<Spec> ParseSpecs(const std::string& text) {
  std::vector<Spec> out;
  for (const auto& raw : Split(text, ';')) {
    const std::string item = Trim(raw);
    if (item.empty()) continue;
    const auto parts = Split(item, ':');
    if (parts.size() != 2 && parts.size() != 3 && parts.size() != 5)
      throw std::invalid_argument("histogram '" + item + "': expected [scene/]layer:attribute[:bins[:lo:hi]]");
    Spec s;
    std::string where = Trim(parts[0]);
    if (const auto slash = where.find('/'); slash != std::string::npos) {
      s.scene = where.substr(0, slash);
      where = where.substr(slash + 1);
    }
    s.layer = where;
    s.attribute = Trim(parts[1]);
    if (s.layer.empty() || s.attribute.empty())
      throw std::invalid_argument("histogram '" + item + "': layer and attribute must not be empty");
    if (parts.size() >= 3) {
      const double b = ParseNumber(Trim(parts[2]), item);
      if (b < 1 || b > 10000 || b != std::floor(b))
        throw std::invalid_argument("histogram '" + item + "': bins must be an integer in 1..10000");
      s.bins = static_cast<int>(b);
    }
    if (parts.size() == 5) {
      const double lo = ParseNumber(Trim(parts[3]), item), hi = ParseNumber(Trim(parts[4]), item);
      if (!(hi > lo)) throw std::invalid_argument("histogram '" + item + "': range needs lo < hi");
      s.range = {lo, hi};
    }
    out.push_back(std::move(s));
  }
  return out;
}

Histogram Fill(const Spec& spec, const std::string& scene, const std::vector<const data::Metadata*>& selected) {
  Histogram h;
  h.scene = scene;
  h.layer = spec.layer;
  h.attribute = spec.attribute;
  std::vector<double> numbers;
  for (const auto* m : selected) {
    const auto* v = m->Find(spec.attribute);
    if (!v) {
      ++h.missing;
      continue;
    }
    if (h.unit.empty())
      if (auto u = m->Text("unit:" + spec.attribute)) h.unit = *u;
    if (const auto* text = std::get_if<std::string>(v)) {
      h.categorical = true;
      ++h.categories[*text];
    } else {
      numbers.push_back(*m->Number(spec.attribute));
    }
  }
  if (h.categorical) {  // numbers among text values are counted as text
    for (double x : numbers) ++h.categories[draw::FormatNumber(x, 6)];
    for (const auto& [k, n] : h.categories) h.filled += n;
    return h;
  }
  if (spec.range) {
    h.lo = spec.range->first;
    h.hi = spec.range->second;
  } else if (!numbers.empty()) {
    const auto [mn, mx] = std::minmax_element(numbers.begin(), numbers.end());
    h.lo = *mn;
    h.hi = *mx;
    if (h.hi - h.lo <= 0) {
      h.lo -= 0.5;
      h.hi += 0.5;
    } else {
      h.hi = std::nextafter(h.hi, std::numeric_limits<double>::infinity());  // include the maximum
    }
  }
  h.counts.assign(static_cast<std::size_t>(spec.bins), 0);
  const double width = (h.hi - h.lo) / spec.bins;
  for (double x : numbers) {
    if (x < h.lo) {
      ++h.underflow;
    } else if (x >= h.hi) {
      ++h.overflow;
    } else {
      auto bin = static_cast<std::size_t>((x - h.lo) / width);
      bin = std::min(bin, h.counts.size() - 1);
      ++h.counts[bin];
      ++h.filled;
    }
  }
  return h;
}

std::string ToCsv(const std::vector<Histogram>& histograms) {
  std::string s = "scene,layer,attribute,kind,bin,low,high,count\n";
  for (const auto& h : histograms) {
    const std::string head = Csv(h.scene) + "," + Csv(h.layer) + "," + Csv(h.attribute) + ",";
    if (h.categorical) {
      for (const auto& [k, n] : h.categories) s += head + "category," + Csv(k) + ",,," + std::to_string(n) + "\n";
    } else {
      const double w = (h.hi - h.lo) / static_cast<double>(std::max<std::size_t>(1, h.counts.size()));
      for (std::size_t i = 0; i < h.counts.size(); ++i)
        s += head + "bin," + std::to_string(i) + "," + draw::FormatNumber(h.lo + w * i, 6) + "," +
             draw::FormatNumber(h.lo + w * (i + 1), 6) + "," + std::to_string(h.counts[i]) + "\n";
      s += head + "underflow,,," + draw::FormatNumber(h.lo, 6) + "," + std::to_string(h.underflow) + "\n";
      s += head + "overflow,," + draw::FormatNumber(h.hi, 6) + ",," + std::to_string(h.overflow) + "\n";
    }
    s += head + "missing,,,," + std::to_string(h.missing) + "\n";
  }
  return s;
}

std::string ToSvg(const std::vector<Histogram>& histograms, int width, int height) {
  using draw::FormatNumber;
  std::string s = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" +
                  std::to_string(width) + "\" height=\"" + std::to_string(height) + "\" viewBox=\"0 0 " +
                  std::to_string(width) + " " + std::to_string(height) + "\" font-family=\"sans-serif\">\n";
  s += "<rect x=\"0\" y=\"0\" width=\"" + std::to_string(width) + "\" height=\"" + std::to_string(height) +
       "\" fill=\"#ffffff\"/>\n";
  if (histograms.empty()) {
    s += "<text x=\"10\" y=\"20\" font-size=\"12\" fill=\"#525252\">no histograms</text>\n</svg>\n";
    return s;
  }
  const int n = static_cast<int>(histograms.size());
  const int cols = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(n))));
  const int rows = (n + cols - 1) / cols;
  const double pw = static_cast<double>(width) / cols, ph = static_cast<double>(height) / rows;
  for (int k = 0; k < n; ++k) {
    const auto& h = histograms[static_cast<std::size_t>(k)];
    const double x0 = (k % cols) * pw, y0 = (k / cols) * ph;
    const double left = x0 + 36, right = x0 + pw - 10, top = y0 + 26, bottom = y0 + ph - 22;
    std::string title = h.layer + ": " + h.attribute + (h.unit.empty() ? "" : " [" + h.unit + "]");
    s += "<g data-layer=\"" + draw::XmlEscape(h.layer) + "\" data-attribute=\"" + draw::XmlEscape(h.attribute) + "\">\n";
    s += "<text x=\"" + draw::FormatNumber(x0 + 8, 2) + "\" y=\"" + draw::FormatNumber(y0 + 16, 2) + "\" font-size=\"12\" fill=\"#161616\">" +
         draw::XmlEscape(title) + "</text>\n";
    std::vector<std::pair<std::string, std::uint64_t>> bars;
    if (h.categorical) {
      for (const auto& [c, cnt] : h.categories) bars.emplace_back(c, cnt);
    } else {
      for (auto c : h.counts) bars.emplace_back("", c);
    }
    std::uint64_t peak = 1;
    for (const auto& b : bars) peak = std::max(peak, b.second);
    const double bw = (right - left) / static_cast<double>(std::max<std::size_t>(1, bars.size()));
    for (std::size_t i = 0; i < bars.size(); ++i) {
      const double bh = (bottom - top) * static_cast<double>(bars[i].second) / static_cast<double>(peak);
      if (bh > 0)
        s += "<rect x=\"" + draw::FormatNumber(left + bw * i + (h.categorical ? bw * 0.1 : 0), 2) + "\" y=\"" +
             draw::FormatNumber(bottom - bh, 2) + "\" width=\"" + draw::FormatNumber(h.categorical ? bw * 0.8 : bw, 2) + "\" height=\"" +
             draw::FormatNumber(bh, 2) + "\" fill=\"#7017ff\" stroke=\"#ffffff\" stroke-width=\"0.5\"/>\n";
      if (h.categorical)
        s += "<text x=\"" + draw::FormatNumber(left + bw * (i + 0.5), 2) + "\" y=\"" + draw::FormatNumber(bottom + 14, 2) +
             "\" font-size=\"10\" text-anchor=\"middle\" fill=\"#525252\">" + draw::XmlEscape(bars[i].first) + "</text>\n";
    }
    s += "<polyline points=\"" + draw::FormatNumber(left, 2) + "," + draw::FormatNumber(top, 2) + " " + draw::FormatNumber(left, 2) + "," + draw::FormatNumber(bottom, 2) + " " +
         draw::FormatNumber(right, 2) + "," + draw::FormatNumber(bottom, 2) + "\" fill=\"none\" stroke=\"#161616\" stroke-width=\"1\"/>\n";
    s += "<text x=\"" + draw::FormatNumber(left - 4, 2) + "\" y=\"" + draw::FormatNumber(top + 8, 2) +
         "\" font-size=\"10\" text-anchor=\"end\" fill=\"#525252\">" + std::to_string(peak) + "</text>\n";
    if (!h.categorical) {
      s += "<text x=\"" + draw::FormatNumber(left, 2) + "\" y=\"" + draw::FormatNumber(bottom + 14, 2) + "\" font-size=\"10\" fill=\"#525252\">" +
           draw::FormatNumber(h.lo, 3) + "</text>\n";
      s += "<text x=\"" + draw::FormatNumber(right, 2) + "\" y=\"" + draw::FormatNumber(bottom + 14, 2) +
           "\" font-size=\"10\" text-anchor=\"end\" fill=\"#525252\">" + draw::FormatNumber(h.hi, 3) + "</text>\n";
    }
    s += "</g>\n";
  }
  return s + "</svg>\n";
}

}  // namespace fccvis::backend::hist
