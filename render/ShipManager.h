#pragma once

#include <osg/MatrixTransform>
#include <string>
#include <vector>

class Ship {
public:
    explicit Ship(const std::string& modelPath);
    void place(double x, double y, double heading);
    osg::Vec3d poseTurret(const osg::Vec3d& cue);
    osg::MatrixTransform& node() const { return *placement_; }
    osg::Node& model() const { return *model_; }

private:
    osg::ref_ptr<osg::MatrixTransform> placement_ = new osg::MatrixTransform;
    osg::ref_ptr<osg::Node> model_;
    osg::ref_ptr<osg::MatrixTransform> barrelPitch_ = new osg::MatrixTransform;
    osg::ref_ptr<osg::MatrixTransform> turretYaw_ = new osg::MatrixTransform;
    osg::Vec3d muzzle_;
};

class ShipManager {
public:
    explicit ShipManager(osg::Group& root) : root_(root) {}
    unsigned addShip(const std::string& modelPath);
    Ship& ship(unsigned id) { return ships_.at(id); }
    const Ship& ship(unsigned id) const { return ships_.at(id); }

private:
    osg::Group& root_;
    std::vector<Ship> ships_;
};
