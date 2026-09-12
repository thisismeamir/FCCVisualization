#pragma once

#include "TrackState.h"
#include "TrackerHit.h"

#include <memory>
#include <vector>

namespace fccvis::data::events {
class Track{
public:
    int type = 0;

    double chi2 = 0.0;
    int ndf = 0;
    int holes = 0;

    std::vector<int> subdetectorHitNumbers;
    std::vector<int> subdetectorHoleNumbers;

    std::vector<fccvis::data::events::TrackState> states;

    std::vector<std::shared_ptr<fccvis::data::events::TrackerHit>> hits;
    std::vector<std::shared_ptr<fccvis::data::events::Track>> segments;
};
}
