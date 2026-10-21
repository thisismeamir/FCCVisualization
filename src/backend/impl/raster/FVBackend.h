/**
 * @file FVBackend.h
 * @brief Former name of the raster backend, kept for existing code.
 *
 * The FV pixel-canvas backend grew into the raster backend (B0, T3.1). The
 * registry name "fv" still creates it.
 *
 * @author Amir H. Ebrahimnezhad <amir.ebh@cern.ch>
 * @copyright Copyright 2026 FCC Project at CERN. Licensed under the Apache
 *            License, Version 2.0; https://www.apache.org/licenses/LICENSE-2.0
 */
#pragma once

#include "RasterBackend.h"

namespace fccvis::backend {

/** @brief Former name of RasterBackend. */
using FVBackend = RasterBackend;

}  // namespace fccvis::backend
