// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
#include "Png.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>

namespace fccvis::backend::raster {

namespace {

const std::array<std::uint32_t, 256>& CrcTable() {
  static const auto table = [] {
    std::array<std::uint32_t, 256> t{};
    for (std::uint32_t n = 0; n < 256; ++n) {
      std::uint32_t c = n;
      for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
      t[n] = c;
    }
    return t;
  }();
  return table;
}

std::uint32_t Crc(const std::uint8_t* data, std::size_t n, std::uint32_t crc = 0xFFFFFFFFu) {
  const auto& t = CrcTable();
  for (std::size_t i = 0; i < n; ++i) crc = t[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
  return crc;
}

void Put32(std::vector<std::uint8_t>& out, std::uint32_t v) {
  out.push_back(static_cast<std::uint8_t>(v >> 24));
  out.push_back(static_cast<std::uint8_t>(v >> 16));
  out.push_back(static_cast<std::uint8_t>(v >> 8));
  out.push_back(static_cast<std::uint8_t>(v));
}

std::uint32_t Get32(const std::uint8_t* p) {
  return (static_cast<std::uint32_t>(p[0]) << 24) | (static_cast<std::uint32_t>(p[1]) << 16) |
         (static_cast<std::uint32_t>(p[2]) << 8) | p[3];
}

void Chunk(std::vector<std::uint8_t>& out, const char* type, const std::vector<std::uint8_t>& data) {
  Put32(out, static_cast<std::uint32_t>(data.size()));
  const std::size_t start = out.size();
  out.insert(out.end(), type, type + 4);
  out.insert(out.end(), data.begin(), data.end());
  Put32(out, Crc(out.data() + start, out.size() - start) ^ 0xFFFFFFFFu);
}

// ---------------------------------------------------------------------------
// Deflate (RFC 1951): LZ77 with hash chains + the fixed Huffman code.
// ---------------------------------------------------------------------------
constexpr std::array<std::uint16_t, 29> kLenBase = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                                    31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
constexpr std::array<std::uint8_t, 29> kLenExtra = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                                    2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
constexpr std::array<std::uint16_t, 30> kDistBase = {1,    2,    3,    4,    5,    7,     9,     13,    17,  25,
                                                     33,   49,   65,   97,   129,  193,   257,   385,   513, 769,
                                                     1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
constexpr std::array<std::uint8_t, 30> kDistExtra = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                                     6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

class BitWriter {
 public:
  explicit BitWriter(std::vector<std::uint8_t>& out) : m_out(out) {}
  void Bits(std::uint32_t v, int n) {  // LSB first
    for (int i = 0; i < n; ++i) Bit((v >> i) & 1);
  }
  void Code(std::uint32_t code, int n) {  // Huffman codes: MSB first
    for (int i = n - 1; i >= 0; --i) Bit((code >> i) & 1);
  }
  void Flush() {
    if (m_n) m_out.push_back(m_acc);
    m_acc = 0;
    m_n = 0;
  }

 private:
  void Bit(std::uint32_t b) {
    m_acc = static_cast<std::uint8_t>(m_acc | (b << m_n));
    if (++m_n == 8) Flush();
  }
  std::vector<std::uint8_t>& m_out;
  std::uint8_t m_acc = 0;
  int m_n = 0;
};

void FixedLiteral(BitWriter& w, int sym) {
  if (sym < 144) w.Code(0x30 + sym, 8);
  else if (sym < 256) w.Code(0x190 + (sym - 144), 9);
  else if (sym < 280) w.Code(sym - 256, 7);
  else w.Code(0xC0 + (sym - 280), 8);
}

void Deflate(const std::vector<std::uint8_t>& in, std::vector<std::uint8_t>& out) {
  BitWriter w(out);
  w.Bits(1, 1);  // final block
  w.Bits(1, 2);  // fixed Huffman
  constexpr std::size_t kWindow = 32768, kHash = 1 << 15, kChain = 32;
  std::vector<std::int64_t> head(kHash, -1), prev(in.size(), -1);
  auto hash = [&](std::size_t i) {
    return ((in[i] << 10) ^ (in[i + 1] << 5) ^ in[i + 2]) & (kHash - 1);
  };
  auto insert = [&](std::size_t i) {
    if (i + 2 >= in.size()) return;
    const auto h = hash(i);
    prev[i] = head[h];
    head[h] = static_cast<std::int64_t>(i);
  };
  std::size_t i = 0;
  while (i < in.size()) {
    std::size_t bestLen = 0, bestDist = 0;
    if (i + 2 < in.size()) {
      std::int64_t c = head[hash(i)];
      for (std::size_t n = 0; c >= 0 && n < kChain && i - static_cast<std::size_t>(c) <= kWindow; ++n) {
        const auto cand = static_cast<std::size_t>(c);
        std::size_t len = 0;
        const std::size_t max = std::min<std::size_t>(258, in.size() - i);
        while (len < max && in[cand + len] == in[i + len]) ++len;
        if (len > bestLen) {
          bestLen = len;
          bestDist = i - cand;
          if (len == max) break;
        }
        c = prev[cand];
      }
    }
    if (bestLen >= 3) {
      std::size_t li = 0;
      while (li + 1 < kLenBase.size() && kLenBase[li + 1] <= bestLen) ++li;
      FixedLiteral(w, 257 + static_cast<int>(li));
      w.Bits(static_cast<std::uint32_t>(bestLen - kLenBase[li]), kLenExtra[li]);
      std::size_t di = 0;
      while (di + 1 < kDistBase.size() && kDistBase[di + 1] <= bestDist) ++di;
      w.Code(static_cast<std::uint32_t>(di), 5);
      w.Bits(static_cast<std::uint32_t>(bestDist - kDistBase[di]), kDistExtra[di]);
      for (std::size_t k = 0; k < bestLen; ++k) insert(i + k);
      i += bestLen;
    } else {
      FixedLiteral(w, in[i]);
      insert(i);
      ++i;
    }
  }
  FixedLiteral(w, 256);
  w.Flush();
}

// ---------------------------------------------------------------------------
// Inflate: stored, fixed and dynamic Huffman blocks (any zlib PNG stream).
// ---------------------------------------------------------------------------
class BitReader {
 public:
  BitReader(const std::vector<std::uint8_t>& d, std::size_t pos) : m_d(d), m_pos(pos) {}
  bool Bits(int n, std::uint32_t& v) {
    v = 0;
    for (int i = 0; i < n; ++i) {
      if (m_pos >= m_d.size()) return false;
      v |= static_cast<std::uint32_t>((m_d[m_pos] >> m_bit) & 1) << i;
      if (++m_bit == 8) {
        m_bit = 0;
        ++m_pos;
      }
    }
    return true;
  }
  void Align() {
    if (m_bit) {
      m_bit = 0;
      ++m_pos;
    }
  }
  std::size_t& Pos() { return m_pos; }

 private:
  const std::vector<std::uint8_t>& m_d;
  std::size_t m_pos;
  int m_bit = 0;
};

struct Huffman {
  std::array<std::uint16_t, 16> counts{};
  std::vector<std::uint16_t> symbols;
  bool Build(const std::uint8_t* lengths, std::size_t n) {
    counts.fill(0);
    for (std::size_t i = 0; i < n; ++i) ++counts[lengths[i]];
    counts[0] = 0;
    std::array<std::uint16_t, 16> offs{};
    for (int l = 1; l < 16; ++l) offs[l] = static_cast<std::uint16_t>(offs[l - 1] + counts[l - 1]);
    symbols.assign(n, 0);
    for (std::size_t i = 0; i < n; ++i)
      if (lengths[i]) symbols[offs[lengths[i]]++] = static_cast<std::uint16_t>(i);
    return true;
  }
  bool Decode(BitReader& r, int& sym) const {
    int code = 0, first = 0, index = 0;
    for (int l = 1; l < 16; ++l) {
      std::uint32_t b;
      if (!r.Bits(1, b)) return false;
      code |= static_cast<int>(b);
      const int count = counts[l];
      if (code - count < first) {
        sym = symbols[static_cast<std::size_t>(index + (code - first))];
        return true;
      }
      index += count;
      first += count;
      first <<= 1;
      code <<= 1;
    }
    return false;
  }
};

bool InflateBlock(BitReader& r, const Huffman& lit, const Huffman& dist, std::vector<std::uint8_t>& out,
                  std::string& why) {
  for (;;) {
    int sym;
    if (!lit.Decode(r, sym)) return why = "bad literal code", false;
    if (sym < 256) {
      out.push_back(static_cast<std::uint8_t>(sym));
    } else if (sym == 256) {
      return true;
    } else {
      sym -= 257;
      if (sym >= 29) return why = "bad length code", false;
      std::uint32_t e;
      if (!r.Bits(kLenExtra[sym], e)) return why = "truncated", false;
      const std::size_t len = kLenBase[sym] + e;
      int ds;
      if (!dist.Decode(r, ds) || ds >= 30) return why = "bad distance code", false;
      if (!r.Bits(kDistExtra[ds], e)) return why = "truncated", false;
      const std::size_t d = kDistBase[ds] + e;
      if (d > out.size()) return why = "distance too far back", false;
      for (std::size_t k = 0; k < len; ++k) out.push_back(out[out.size() - d]);
    }
  }
}

bool Inflate(const std::vector<std::uint8_t>& z, std::size_t pos, std::vector<std::uint8_t>& out, std::string& why) {
  BitReader r(z, pos);
  std::uint32_t final = 0, type = 0;
  do {
    if (!r.Bits(1, final) || !r.Bits(2, type)) return why = "truncated deflate stream", false;
    if (type == 0) {
      r.Align();
      auto& q = r.Pos();
      if (q + 4 > z.size()) return why = "truncated stored block", false;
      const std::size_t n = z[q] | (z[q + 1] << 8);
      q += 4;
      if (q + n > z.size()) return why = "truncated stored block", false;
      out.insert(out.end(), z.begin() + static_cast<std::ptrdiff_t>(q), z.begin() + static_cast<std::ptrdiff_t>(q + n));
      q += n;
    } else if (type == 1) {
      std::array<std::uint8_t, 288> l{};
      for (int i = 0; i < 288; ++i) l[i] = i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8;
      std::array<std::uint8_t, 30> d{};
      d.fill(5);
      Huffman lit, dist;
      lit.Build(l.data(), l.size());
      dist.Build(d.data(), d.size());
      if (!InflateBlock(r, lit, dist, out, why)) return false;
    } else if (type == 2) {
      std::uint32_t hlit, hdist, hclen;
      if (!r.Bits(5, hlit) || !r.Bits(5, hdist) || !r.Bits(4, hclen)) return why = "truncated", false;
      hlit += 257;
      hdist += 1;
      hclen += 4;
      static constexpr std::array<int, 19> order = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
      std::array<std::uint8_t, 19> cl{};
      for (std::uint32_t i = 0; i < hclen; ++i) {
        std::uint32_t v;
        if (!r.Bits(3, v)) return why = "truncated", false;
        cl[static_cast<std::size_t>(order[i])] = static_cast<std::uint8_t>(v);
      }
      Huffman clh;
      clh.Build(cl.data(), cl.size());
      std::vector<std::uint8_t> lens;
      while (lens.size() < hlit + hdist) {
        int sym;
        if (!clh.Decode(r, sym)) return why = "bad code length code", false;
        std::uint32_t rep = 0;
        if (sym < 16) {
          lens.push_back(static_cast<std::uint8_t>(sym));
        } else if (sym == 16) {
          if (lens.empty() || !r.Bits(2, rep)) return why = "bad repeat", false;
          lens.insert(lens.end(), 3 + rep, lens.back());
        } else if (sym == 17) {
          if (!r.Bits(3, rep)) return why = "truncated", false;
          lens.insert(lens.end(), 3 + rep, 0);
        } else {
          if (!r.Bits(7, rep)) return why = "truncated", false;
          lens.insert(lens.end(), 11 + rep, 0);
        }
      }
      if (lens.size() != hlit + hdist) return why = "bad code lengths", false;
      Huffman lit, dist;
      lit.Build(lens.data(), hlit);
      dist.Build(lens.data() + hlit, hdist);
      if (!InflateBlock(r, lit, dist, out, why)) return false;
    } else {
      return why = "bad block type", false;
    }
  } while (!final);
  return true;
}

constexpr std::uint8_t kSignature[8] = {137, 80, 78, 71, 13, 10, 26, 10};

}  // namespace

std::uint32_t Image::At(int x, int y) const {
  const auto* p = rgba.data() + (static_cast<std::size_t>(y) * width + x) * 4;
  return (static_cast<std::uint32_t>(p[0]) << 24) | (static_cast<std::uint32_t>(p[1]) << 16) |
         (static_cast<std::uint32_t>(p[2]) << 8) | p[3];
}

std::vector<std::uint8_t> EncodePng(const Image& image) {
  std::vector<std::uint8_t> out(kSignature, kSignature + 8);
  std::vector<std::uint8_t> ihdr;
  Put32(ihdr, static_cast<std::uint32_t>(image.width));
  Put32(ihdr, static_cast<std::uint32_t>(image.height));
  ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0});  // 8-bit, RGBA, deflate, filter 0, no interlace
  Chunk(out, "IHDR", ihdr);

  // raw scanlines: filter byte 0 + RGBA
  std::vector<std::uint8_t> raw;
  const std::size_t stride = static_cast<std::size_t>(image.width) * 4;
  raw.reserve((stride + 1) * image.height);
  for (int y = 0; y < image.height; ++y) {
    raw.push_back(0);
    const auto* row = image.rgba.data() + stride * y;
    raw.insert(raw.end(), row, row + stride);
  }
  std::vector<std::uint8_t> z{0x78, 0x01};
  Deflate(raw, z);
  std::uint32_t a = 1, b = 0;
  for (auto c : raw) {
    a = (a + c) % 65521;
    b = (b + a) % 65521;
  }
  Put32(z, (b << 16) | a);
  Chunk(out, "IDAT", z);
  Chunk(out, "IEND", {});
  return out;
}

bool WritePng(const std::string& path, const Image& image) {
  const auto bytes = EncodePng(image);
  std::ofstream f(path, std::ios::binary);
  if (!f) return false;
  f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  return static_cast<bool>(f);
}

std::optional<Image> ReadPng(const std::string& path, std::string* error) {
  auto fail = [error](const std::string& why) -> std::optional<Image> {
    if (error) *error = why;
    return std::nullopt;
  };
  std::ifstream f(path, std::ios::binary);
  if (!f) return fail("cannot open " + path);
  const std::vector<std::uint8_t> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  if (d.size() < 8 || !std::equal(kSignature, kSignature + 8, d.begin())) return fail("not a PNG");
  Image img;
  std::vector<std::uint8_t> z;
  for (std::size_t p = 8; p + 12 <= d.size();) {
    const std::uint32_t len = Get32(&d[p]);
    if (p + 12 + len > d.size()) return fail("truncated chunk");
    const std::string type(reinterpret_cast<const char*>(&d[p + 4]), 4);
    const std::uint8_t* body = &d[p + 8];
    if (Get32(body + len) != (Crc(&d[p + 4], len + 4) ^ 0xFFFFFFFFu)) return fail("bad CRC in " + type);
    if (type == "IHDR") {
      img.width = static_cast<int>(Get32(body));
      img.height = static_cast<int>(Get32(body + 4));
      if (body[8] != 8 || body[9] != 6 || body[12] != 0) return fail("only 8-bit RGBA, non-interlaced PNG is supported");
    } else if (type == "IDAT") {
      z.insert(z.end(), body, body + len);
    } else if (type == "IEND") {
      break;
    }
    p += 12 + len;
  }
  if (z.size() < 6) return fail("no image data");
  std::vector<std::uint8_t> raw;
  std::string why;
  if ((z[0] & 0x0F) != 8 || z[1] & 0x20 || ((z[0] << 8) | z[1]) % 31 != 0) return fail("bad zlib header");
  if (!Inflate(z, 2, raw, why)) return fail(why);
  const std::size_t stride = static_cast<std::size_t>(img.width) * 4;
  if (raw.size() < (stride + 1) * img.height) return fail("unexpected image data size");
  img.rgba.resize(stride * img.height);
  for (int y = 0; y < img.height; ++y) {
    const std::uint8_t filter = raw[(stride + 1) * y];
    const std::uint8_t* in = raw.data() + (stride + 1) * y + 1;
    std::uint8_t* out = img.rgba.data() + stride * y;
    const std::uint8_t* up = y > 0 ? out - stride : nullptr;
    for (std::size_t i = 0; i < stride; ++i) {
      const int left = i >= 4 ? out[i - 4] : 0;
      const int above = up ? up[i] : 0;
      const int diag = (up && i >= 4) ? up[i - 4] : 0;
      int pred = 0;
      switch (filter) {
        case 0: pred = 0; break;
        case 1: pred = left; break;
        case 2: pred = above; break;
        case 3: pred = (left + above) / 2; break;
        case 4: {
          const int pp = left + above - diag;
          const int pa = std::abs(pp - left), pb = std::abs(pp - above), pc = std::abs(pp - diag);
          pred = (pa <= pb && pa <= pc) ? left : (pb <= pc ? above : diag);
          break;
        }
        default: return fail("bad filter type " + std::to_string(filter));
      }
      out[i] = static_cast<std::uint8_t>(in[i] + pred);
    }
  }
  return img;
}

double DifferentPixelFraction(const Image& a, const Image& b, int channelTolerance) {
  if (a.width != b.width || a.height != b.height || a.rgba.size() != b.rgba.size()) return 1.0;
  if (a.rgba.empty()) return 0.0;
  std::size_t diff = 0;
  for (std::size_t i = 0; i < a.rgba.size(); i += 4) {
    for (int c = 0; c < 4; ++c) {
      if (std::abs(static_cast<int>(a.rgba[i + c]) - static_cast<int>(b.rgba[i + c])) > channelTolerance) {
        ++diff;
        break;
      }
    }
  }
  return static_cast<double>(diff) / static_cast<double>(a.rgba.size() / 4);
}

}  // namespace fccvis::backend::raster
