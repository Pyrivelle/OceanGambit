#pragma once

#include <osg/Matrixd>
#include <optional>
#include <utility>

namespace osg { class Camera; }

using ShipID = unsigned;

struct CameraRay {
    osg::Vec3d origin, direction;
};

struct CameraTarget {
    osg::Vec3d point;
    std::optional<ShipID> ship;
};

struct ShipCameraSettings {
    double distance = 90;
    double height = 32;
    double pitch = -13;
    double followFov = 48;
    double smoothingSeconds = 0;
    std::optional<osg::Vec3d> bridgeEye;
    std::optional<double> binocularFov;
};

class ShipCameraController {
public:
    void follow(ShipID ship, ShipCameraSettings settings);
    ShipID followedShip() const { return ship_; }
    bool binocular() const { return binocular_; }
    void setBinocular(bool enabled);
    void look(double dx, double dy);
    void beginTemporaryLook();
    void endTemporaryLook();
    void update(osg::Camera& camera, const osg::Matrixd& shipToWorld, double dt, double aspect);
    osg::Vec3d committedDirection(const osg::Matrixd& shipToWorld) const;
    const CameraRay& centerRay() const { return ray_; }

    static std::optional<osg::Vec3d> intersectSea(const CameraRay& ray);

private:
    ShipID ship_ = 0;
    ShipCameraSettings settings_;
    bool binocular_ = false;
    double yaw_ = 0, pitch_ = -13;
    std::optional<std::pair<double, double>> savedLook_;
    std::optional<osg::Vec3d> followedOrigin_;
    CameraRay ray_;
};
