#pragma once

#include <osg/MatrixTransform>
#include <string>
#include <vector>

struct MuzzlePose { osg::Vec3d position, forward; };

class Ship {
public:
    explicit Ship(const std::string& modelPath);
    void place(double x, double y, double heading);
    MuzzlePose poseTurret(const osg::Vec3d& cue);
    MuzzlePose poseTurretDirection(const osg::Vec3d& worldDirection);
    osg::MatrixTransform& node() const { return *placement_; }
    void setBridgeHidden(bool hidden);
    const std::string& modelPath() const { return modelPath_; }
    osg::Node& model() const { return *model_; }

private:
    MuzzlePose poseTurretLocal(const osg::Vec3d& direction);
    std::string modelPath_;
    osg::ref_ptr<osg::MatrixTransform> placement_ = new osg::MatrixTransform;
    osg::ref_ptr<osg::Node> model_;
    osg::ref_ptr<osg::MatrixTransform> barrelPitch_ = new osg::MatrixTransform;
    osg::ref_ptr<osg::MatrixTransform> turretYaw_ = new osg::MatrixTransform;
    osg::Vec3d muzzle_;
    osg::ref_ptr<osg::Node> bridge_;
    osg::ref_ptr<osg::Callback> bridgeHider_;
};

class ShipManager {
public:
    explicit ShipManager(osg::Group& root) : root_(root) {}
    unsigned addShip(const std::string& modelPath);
    unsigned count() const { return static_cast<unsigned>(ships_.size()); }
    Ship& ship(unsigned id) { return ships_.at(id); }
    const Ship& ship(unsigned id) const { return ships_.at(id); }

private:
    osg::Group& root_;
    std::vector<Ship> ships_;
};
