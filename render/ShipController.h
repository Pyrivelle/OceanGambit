#pragma once

#include "NodeController.h"

#include <memory>

struct MuzzlePose { osg::Vec3d position, forward; };

class ShipController {
public:
    ShipController(const ShipDefinition& definition, const std::string& modelPath);
    void place(double x, double y, double heading);
    MuzzlePose poseTurret(const osg::Vec3d& cue);
    MuzzlePose poseTurretDirection(const osg::Vec3d& worldDirection);
    void setTurretAngles(size_t turret, double yaw, double pitch);
    MuzzlePose muzzlePose(size_t turret, size_t barrel) const;
    std::optional<osg::Vec3d> viewEye(const std::string& id) const;
    void setBridgeHidden(bool hidden);
    osg::MatrixTransform& node() const { return *placement_; }
    osg::Node& model() const { return *model_; }
    const std::string& modelPath() const { return modelPath_; }

private:
    struct Barrel {
        osg::ref_ptr<osg::Node> node;
        osg::Vec3d localMuzzle, localForward;
    };

    struct Turret {
        NodeController* yaw;
        std::vector<NodeController*> pitches;
        std::vector<Barrel> barrels;
    };

    MuzzlePose poseTurretLocal(const osg::Vec3d& direction);
    std::string modelPath_;
    osg::ref_ptr<osg::MatrixTransform> placement_ = new osg::MatrixTransform;
    osg::ref_ptr<osg::Node> model_;
    osg::ref_ptr<osg::Node> bridge_;
    osg::ref_ptr<osg::Callback> bridgeHider_;
    osg::Vec3d restMuzzle_, modelForward_;
    std::vector<ViewDefinition> views_;
    std::vector<std::unique_ptr<NodeController>> controllers_;
    std::vector<Turret> turrets_;
};
