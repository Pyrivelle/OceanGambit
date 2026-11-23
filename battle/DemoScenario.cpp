#include "DemoScenario.h"

#include <cmath>
#include <osg/Math>

std::array<ShipPose, 2> DemoScenario::poses(double now) const {
    constexpr double separation = 1000;
    return {{{0, -separation / 2, osg::PI_4},
             {moving_ ? 4 * std::sin(now * .65) : 0, separation / 2, 3 * osg::PI_4}}};
}

std::optional<ShotCue> DemoScenario::nextShot(double now) {
    if (now < .4 + shotCount_ * 2.4) return std::nullopt;
    const unsigned from = shotCount_ % 2;
    const bool splash = shotCount_ % 3 == 2;
    return ShotCue{++shotCount_, from, 1 - from, splash};
}
