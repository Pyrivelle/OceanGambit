#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>
#include "ShipSelection.h"
#include "../battle/WowsMotion.h"
#include "../battle/EntityManager.h"
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
                bool playerControlled = false, bool extraShips = false);
    ~BattleScene();
    osg::Group& root() const;
    void advance(double now);
    void setRtsMovementEnabled(bool enabled);
    void requestMove(const std::vector<EntityID>& selected, const osg::Vec3d& destination);
    std::optional<EntityID> pickOwnShip(const osg::Vec3d& from, const osg::Vec3d& to) const;
    void setPlayerControlEnabled(bool enabled);
    void setControlledShip(ShipID ship);
    void requestPlayerShot(ShipID ship, const osg::Vec3d& target);
    void requestPlayerFreeShot(ShipID ship, const osg::Vec3d& direction);
    bool oceanReady() const;
    SceneStatus status() const;
    std::optional<std::string> shipModelPath(ShipID id) const;
    std::optional<osg::Vec3d> shipBridgeEye(ShipID id) const;
    std::optional<osg::Matrixd> shipTransform(ShipID id) const;
    std::vector<EntityID> shipsInScreenBox(const ShipSelectionBox& box,
                                        const osg::Matrixd& viewProjection) const;
    void showShipSelection(const std::vector<EntityID>& selected);
    void setBridgeHidden(ShipID id, bool hidden);
    std::optional<CameraTarget> pickTarget(const CameraRay& ray, ShipID exclude) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
