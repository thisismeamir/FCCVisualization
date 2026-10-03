/**
 * @file SessionOptions.h
 * @brief Visualization configuration for a session.
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 */
#pragma once

#include "Camera.h"
#include "Layout.h"
#include "Scene.h"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @copyright Copyright 2026 FCC Project at CERN
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

namespace fccvis::session {

/**
 * @brief Visualization and runtime configuration for a session.
 *
 * SessionOptions contains only visualization configuration: cameras, scenes,
 * and the layout that arranges scenes.
 */
class SessionOptions {
public:
  /**
   * @brief Cameras available to the session.
   */
  std::vector<std::shared_ptr<fccvis::scene::camera::Camera>> cameras;

  /**
   * @brief Scenes available to the session, keyed by unique scene name.
   */
  std::unordered_map<std::string, std::shared_ptr<fccvis::scene::Scene>>
      scenes;

  /**
   * @brief Layout describing how scenes are presented together.
   */
  std::optional<fccvis::scene::layout::LayoutNode> sceneLayout;
};

} // namespace fccvis::session
