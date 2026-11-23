#pragma once

#include <array>
#include <optional>

struct ShipPose { double x, y, heading; };
struct ShotCue { unsigned id, from, to; bool splash; };

class DemoScenario {
public:
    explicit DemoScenario(bool moving) : moving_(moving) {}
    std::array<ShipPose, 2> poses(double now) const;
    std::optional<ShotCue> nextShot(double now);

private:
    bool moving_;
    unsigned shotCount_ = 0;
};
