#pragma once

#include "NodeController.h"
#include "../battle/EntityManager.h"

#include <memory>

struct MuzzlePose { osg::Vec3d position, forward; };

class ShipController final : public EntityVisual {
public:
    ShipController(std::shared_ptr<const ShipDefinition> definition, const std::string& modelPath);
    MuzzlePose poseTurret(const osg::Vec3d& cue);
    MuzzlePose poseTurretDirection(const osg::Vec3d& worldDirection);
    void setTurretAngles(size_t turret, double yaw, double pitch);
    MuzzlePose muzzlePose(size_t turret, size_t barrel) const;
    std::optional<osg::Vec3d> viewEye(const std::string& id) const;
    void setBridgeHidden(bool hidden);
    void setSelectionVisible(bool visible);
    const ShipDefinition& definition() const { return *definition_; }
    osg::MatrixTransform& node() const override { return *placement_; }
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
    std::shared_ptr<const ShipDefinition> definition_;
    osg::ref_ptr<osg::Node> selectionRing_;
    std::vector<std::unique_ptr<NodeController>> controllers_;
    std::vector<Turret> turrets_;
};
