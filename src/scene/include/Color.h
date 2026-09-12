#pragma once

#include <array>
#include <string>

namespace fccvis::scene::meta {

enum class Color {
  White,
  Black,
  Red,
  Green,
  Blue,
  Magenta,
  Cyan,
  Violet,
  Pink,
  Orange,
  Yellow,
  Spring,
  Teal,
  Azure,
  Gray,
  DarkRed,
  DarkGreen,
  DarkBlue,
  DarkMagenta,
  DarkCyan,
  DarkViolet,
  DarkPink,
  DarkOrange,
  DarkYellow,
  LightRed,
  LightGreen,
  LightBlue,
  LightMagenta,
  LightCyan,
  LightViolet,
  LightPink,
  LightOrange,
  LightYellow,
  Auto,
  AutoId,
  AutoIter,
  AutoEnergy
};

class Marker {
public:
  Marker(std::string name) : name(std::move(name)) {}

  std::string name;

  std::array<double, 3> position{};

  Color color = Color::White;

  unsigned int size = 1;
};

class Line {
public:
  Line(std::string name) : name(std::move(name)) {}

  std::string name;

  std::array<double, 3> start{};
  std::array<double, 3> end{};

  Color color = Color::White;

  unsigned int width = 1;
  unsigned int style = 1;
};
} // namespace fccvis::scene::meta
