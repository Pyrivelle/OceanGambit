#pragma once

#include <osg/Vec3d>
#include <optional>
#include <string>
#include <vector>

struct NodeBinding {
    std::string name, path;
};

struct RotationBinding {
    NodeBinding node;
    std::optional<osg::Vec3d> pivot, axis;
    std::optional<double> sign;
};

struct BarrelDefinition {
    std::string id;
    RotationBinding pitch;
    std::optional<osg::Vec3d> muzzle;
    osg::Vec3d forward;
};

struct TurretDefinition {
    std::string id;
    RotationBinding yaw;
    std::vector<BarrelDefinition> barrels;
};

struct ViewDefinition {
    std::string id;
    osg::Vec3d eye;
    std::optional<NodeBinding> node;
};

struct ShipDefinition {
    std::string code, file;
    NodeBinding hull, bridge;
    std::vector<TurretDefinition> turrets;
    std::vector<ViewDefinition> views;
};

ShipDefinition loadShipDefinition(const std::string& code);
