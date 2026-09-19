#pragma once

#include <TEveElement.h>
#include <podio/Frame.h>
#include <string_view>
namespace fccvis::visualization {

class CollectionVisualizer {
  public:
  virtual ~CollectionVisualizer() = default;
  virtual bool CanVisualize(std::string_view type) const = 0;
  virtual void Visualize(
  const podio::Frame & frame,
  std::string_view collectionName,
  TEveElement* parent
  ) = 0;
};

}
