// Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
#include "fccvis/stdlib/MeshExport.h"

#include <cstdio>

namespace fccvis::stdlib {

namespace {

std::string Num(double v) {
  char buf[32];
  std::snprintf(buf, sizeof buf, "%.4g", v);
  return buf;
}

std::string Material(std::size_t i) { return "m" + std::to_string(i); }

}  // namespace

void ObjWriter::Add(const std::string& name, const std::vector<std::array<double, 3>>& vertices,
                    const std::vector<std::array<std::uint32_t, 3>>& triangles, std::optional<style::Color> color) {
  std::string clean = name.empty() ? std::string("object") : name;
  for (char& c : clean)
    if (c == ' ' || c == '\t' || c == '\n') c = '_';
  m_body += "o " + clean + "\n";
  if (color) {
    std::size_t i = 0;
    auto same = [](const style::Color& a, const style::Color& b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; };
    while (i < m_materials.size() && !same(m_materials[i], *color)) ++i;
    if (i == m_materials.size()) m_materials.push_back(*color);
    m_body += "usemtl " + Material(i) + "\n";
  }
  for (const auto& v : vertices) m_body += "v " + Num(v[0]) + " " + Num(v[1]) + " " + Num(v[2]) + "\n";
  for (const auto& t : triangles) {
    if (t[0] >= vertices.size() || t[1] >= vertices.size() || t[2] >= vertices.size()) continue;
    m_body += "f " + std::to_string(m_vertexCount + t[0] + 1) + " " + std::to_string(m_vertexCount + t[1] + 1) + " " +
              std::to_string(m_vertexCount + t[2] + 1) + "\n";
  }
  m_vertexCount += vertices.size();
  ++m_objects;
}

void ObjWriter::Add(const std::string& name, const geometry::Mesh& mesh, const geometry::Transform& transform,
                    std::optional<style::Color> color) {
  std::vector<std::array<double, 3>> world;
  world.reserve(mesh.vertices.size());
  for (const auto& v : mesh.vertices) world.push_back(transform.Apply({v[0], v[1], v[2]}));
  Add(name, world, mesh.triangles, color);
}

std::string ObjWriter::Obj(const std::string& mtlFile) const {
  std::string s = "# fccvis OBJ export, millimetres\n";
  if (!mtlFile.empty() && !m_materials.empty()) s += "mtllib " + mtlFile + "\n";
  return s + m_body;
}

std::string ObjWriter::Mtl() const {
  std::string s = "# fccvis MTL export\n";
  for (std::size_t i = 0; i < m_materials.size(); ++i) {
    const auto& c = m_materials[i];
    s += "newmtl " + Material(i) + "\nKd " + Num(c.r / 255.0) + " " + Num(c.g / 255.0) + " " + Num(c.b / 255.0) +
         "\nd " + Num(c.a / 255.0) + "\n";
  }
  return s;
}

}  // namespace fccvis::stdlib
