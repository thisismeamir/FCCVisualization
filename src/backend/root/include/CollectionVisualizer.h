#pragma once

#include "Markers.h"
#include <RtypesCore.h>
#include <podio/Frame.h>
#include <TEveElement.h>

#include <string>
#include <string_view>

namespace fccvis::backend::root {

Color_t ToRootColor(fccvis::scene::meta::Color color);
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
