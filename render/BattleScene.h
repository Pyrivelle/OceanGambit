#pragma once

#include <array>
#include <memory>
#include <string>
#include "../battle/WowsMotion.h"
#include "ShipCameraController.h"

namespace osg { class Group; }
namespace osgViewer { class Viewer; }

struct SceneStatus {
    bool oceanReady;
    std::array<unsigned, 2> decalCounts;
    bool flight, splash, decalAttached, decalMoved;
    unsigned explosions;
    std::array<unsigned, 2> launches;
};

class BattleScene {
public:
    BattleScene(osgViewer::Viewer& viewer, bool moving, bool quadratic = false, bool wows = false,
                ocean_gambit::ballistics::Integrator integrator = ocean_gambit::ballistics::Integrator::RungeKutta4,
                bool playerControlled = false);
    ~BattleScene();
    osg::Group& root() const;
    void advance(double now);
    void requestPlayerShot(const osg::Vec3d& viewDirection);
    bool oceanReady() const;
    SceneStatus status() const;
    std::optional<std::string> shipModelPath(ShipID id) const;
    std::optional<osg::Matrixd> shipTransform(ShipID id) const;
    void setBridgeHidden(ShipID id, bool hidden);
    std::optional<CameraTarget> pickTarget(const CameraRay& ray, ShipID exclude) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
