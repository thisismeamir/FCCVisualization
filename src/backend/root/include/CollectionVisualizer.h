#pragma once

#include "Markers.h"
#include <RtypesCore.h>
#include <podio/Frame.h>
#include <TEveElement.h>

#include <string>
#include <string_view>

namespace fccvis::backend::root {

Color_t ToRootColor(fccvis::scene::meta::Color color) {
  using fccvis::scene::meta::Color;
  switch (color) {
    case Color::White:       return kWhite;
    case Color::Black:       return kBlack;
    case Color::Red:         return kRed;
    case Color::Green:       return kGreen;
    case Color::Blue:        return kBlue;
    case Color::Magenta:     return kMagenta;
    case Color::Cyan:        return kCyan;
    case Color::Violet:      return kViolet;
    case Color::Pink:        return kPink;
    case Color::Orange:      return kOrange;
    case Color::Yellow:      return kYellow;
    case Color::Spring:      return kSpring;
    case Color::Teal:        return kTeal;
    case Color::Azure:       return kAzure;
    case Color::Gray:        return kGray;
    case Color::DarkRed:     return kRed + 2;
    case Color::DarkGreen:   return kGreen + 2;
    case Color::DarkBlue:    return kBlue + 2;
    case Color::DarkMagenta: return kMagenta + 2;
    case Color::DarkCyan:    return kCyan + 2;
    case Color::DarkViolet:  return kViolet + 2;
    case Color::DarkPink:    return kPink + 2;
    case Color::DarkOrange:  return kOrange + 2;
    case Color::DarkYellow:  return kYellow + 2;
    case Color::LightRed:    return kRed - 7;
    case Color::LightGreen:  return kGreen - 7;
    case Color::LightBlue:   return kBlue - 7;
    case Color::LightMagenta:return kMagenta - 7;
    case Color::LightCyan:   return kCyan - 7;
    case Color::LightViolet: return kViolet - 7;
    case Color::LightPink:   return kPink - 7;
    case Color::LightOrange: return kOrange - 7;
    case Color::LightYellow: return kYellow - 7;
    default:                 return kWhite;
  }
}
/**
 * @brief Materializes PODIO/EDM4hep collections into unstyled ROOT TEve graphical elements.
 *
 * CollectionVisualizer extracts geometric primitives (calorimeter hits, tracker hits, tracks, clusters)
 * from podio event frames and builds TEveElement trees.
 *
 * @note Visual options and styling (colors, transparencies, marker styles) are NOT applied here.
 * Styling is managed separately downstream by RootScene using SceneOptions.
 */
class CollectionVisualizer {
public:
  CollectionVisualizer() = default;
  ~CollectionVisualizer() = default;

  CollectionVisualizer(const CollectionVisualizer&) = delete;
  CollectionVisualizer& operator=(const CollectionVisualizer&) = delete;
  CollectionVisualizer(CollectionVisualizer&&) noexcept = default;
  CollectionVisualizer& operator=(CollectionVisualizer&&) noexcept = default;

  /**
   * @brief Materialize all collections contained in a podio::Frame.
   *
   * @param frame The PODIO frame containing EDM4hep collections.
   * @return TEveElement* Top-level TEveElementList containing converted collection hierarchies.
   */
  TEveElement* VisualizeFrame(const podio::Frame& frame);

  /**
   * @brief Materialize a single collection from a podio::Frame by collection name.
   *
   * @param frame The PODIO frame containing the target collection.
   * @param collectionName Name of the collection inside the frame.
   * @return TEveElement* TEve graphical primitive (e.g., TEveBoxSet, TEvePointSet, TEveTrackList, or TEveElementList).
   */
  TEveElement* VisualizeCollection(const podio::Frame& frame, std::string_view collectionName);

private:
  TEveElement* ConvertCalorimeterHits(const podio::Frame& frame, std::string_view collectionName);
  TEveElement* ConvertTrackerHits(const podio::Frame& frame, std::string_view collectionName);
  TEveElement* ConvertSimTrackerHits(const podio::Frame& frame, std::string_view collectionName);
  TEveElement* ConvertTracks(const podio::Frame& frame, std::string_view collectionName);
  TEveElement* ConvertClusters(const podio::Frame& frame, std::string_view collectionName);
};

} // namespace fccvis::backend::root
