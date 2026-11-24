# Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
#
# Builds only the k4geo detector drivers that the CLD_o2_v07 compact file
# needs, against DD4hep without Geant4/DDG4 (k4geo's own build requires them).
# For running fccvis's CLD tests where Key4hep is not installed:
#   git clone https://github.com/key4hep/k4geo
#   cmake -S tools/k4geo_cld -B build-cld -DK4GEO=$PWD/k4geo -DCMAKE_PREFIX_PATH="<root>;<dd4hep>"
#   cmake --build build-cld
#   DD4hepINSTALL=<dd4hep> LD_LIBRARY_PATH=$PWD/build-cld:$LD_LIBRARY_PATH \
#   FCCVIS_TEST_COMPACT=$PWD/k4geo/FCCee/CLD/compact/CLD_o2_v07/CLD_o2_v07.xml ctest -L needs-data
# With Key4hep set up, k4geo's own libraries are already on LD_LIBRARY_PATH.
cmake_minimum_required(VERSION 3.20)
project(fccvis_k4geo_cld CXX)
set(CMAKE_CXX_STANDARD 20)
set(K4GEO "" CACHE PATH "k4geo source checkout")
if(NOT EXISTS "${K4GEO}/detector")
  message(FATAL_ERROR "set K4GEO to a k4geo source checkout")
endif()
find_package(ROOT REQUIRED COMPONENTS Geom GenVector)
find_package(DD4hep REQUIRED COMPONENTS DDRec DDParsers)
dd4hep_set_compiler_flags()
dd4hep_add_plugin(fccvis_k4geo_cld SOURCES
  ${K4GEO}/detector/other/ConicalSupport_o1_v01_geo.cpp
  ${K4GEO}/detector/calorimeter/GenericCalBarrel_o1_v01_geo.cpp
  ${K4GEO}/detector/calorimeter/GenericCalEndcap_o1_v01_geo.cpp
  ${K4GEO}/detector/fcal/LumiCal_o1_v01_geo.cpp
  ${K4GEO}/detector/other/TrackerBarrelSupport_o1_v01.cpp
  ${K4GEO}/detector/tracker/TrackerBarrel_o1_v06_geo.cpp
  ${K4GEO}/detector/other/TrackerEndcapSupport_o1_v02.cpp
  ${K4GEO}/detector/tracker/TrackerEndcap_o2_v06_geo.cpp
  ${K4GEO}/detector/other/TubeSupport_o1_v01_geo.cpp
  ${K4GEO}/detector/tracker/VertexEndcap_o1_v06_geo.cpp
  ${K4GEO}/detector/tracker/ZPlanarTracker_geo.cpp
  ${K4GEO}/plugins/LinearSortingPolicy.cpp
  USES DD4hep::DDCore DD4hep::DDRec ROOT::Core ROOT::Geom ROOT::GenVector)
target_include_directories(fccvis_k4geo_cld PRIVATE ${K4GEO}/include ${K4GEO}/detectorCommon/include
                                                     ${K4GEO}/detectorSegmentations/include)
