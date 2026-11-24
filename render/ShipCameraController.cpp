#include "ShipCameraController.h"

#include <osg/Camera>
#include <osg/Math>

#include <algorithm>
#include <cmath>

namespace {
osg::Vec3d worldDirection(double yaw, double pitch, const osg::Matrixd& shipToWorld) {
    yaw = osg::DegreesToRadians(yaw);
    pitch = osg::DegreesToRadians(pitch);
    const osg::Vec3d forward(std::sin(yaw) * std::cos(pitch),
                            std::cos(yaw) * std::cos(pitch), std::sin(pitch));
    auto direction = osg::Matrixd::transform3x3(forward, shipToWorld);
    direction.normalize();
    return direction;
}
}

void ShipCameraController::follow(ShipID ship, ShipCameraSettings settings) {
    ship_ = ship;
    settings_ = settings;
    binocular_ = false;
    yaw_ = 0;
    pitch_ = std::clamp(settings.pitch, -80., 80.);
    followedOrigin_.reset();
    savedLook_.reset();
}

void ShipCameraController::setBinocular(bool enabled) {
    binocular_ = enabled && settings_.bridgeEye.has_value() && settings_.binocularFov.has_value();
    if (binocular_) followedOrigin_.reset();
}

void ShipCameraController::beginTemporaryLook() {
    if (!savedLook_) savedLook_.emplace(yaw_, pitch_);
}

void ShipCameraController::endTemporaryLook() {
    if (!savedLook_) return;
    yaw_ = savedLook_->first;
    pitch_ = savedLook_->second;
    savedLook_.reset();
}

void ShipCameraController::look(double dx, double dy) {
    yaw_ = std::remainder(yaw_ + dx * .15, 360.);
    pitch_ = std::clamp(pitch_ - dy * .15, -80., 80.);
}

void ShipCameraController::update(osg::Camera& camera, const osg::Matrixd& shipToWorld,
                                  double dt, double aspect) {
    const double yaw = osg::DegreesToRadians(yaw_);
    const osg::Vec3d horizontal(std::sin(yaw), std::cos(yaw), 0);
    ray_.direction = worldDirection(yaw_, pitch_, shipToWorld);

    const auto origin = shipToWorld.getTrans();
    if (binocular_) {
        ray_.origin = *settings_.bridgeEye * shipToWorld;
    } else {
        if (!followedOrigin_ || settings_.smoothingSeconds <= 0)
            followedOrigin_ = origin;
        else
            *followedOrigin_ += (origin - *followedOrigin_) *
                -std::expm1(-std::max(0., dt) / settings_.smoothingSeconds);
        const auto offset = osg::Vec3d(0, 0, settings_.height) - horizontal * settings_.distance;
        ray_.origin = *followedOrigin_ + osg::Matrixd::transform3x3(offset, shipToWorld);
    }
    const auto up = osg::Matrixd::transform3x3(osg::Vec3d(0, 0, 1), shipToWorld);
    camera.setViewMatrixAsLookAt(ray_.origin, ray_.origin + ray_.direction, up);
    camera.setProjectionMatrixAsPerspective(binocular_ ? *settings_.binocularFov : settings_.followFov,
                                             aspect, .15, 14000);
}

osg::Vec3d ShipCameraController::committedDirection(const osg::Matrixd& shipToWorld) const {
    const auto [yaw, pitch] = savedLook_.value_or(std::make_pair(yaw_, pitch_));
    return worldDirection(yaw, pitch, shipToWorld);
}

std::optional<osg::Vec3d> ShipCameraController::intersectSea(const CameraRay& ray) {
    if (std::abs(ray.direction.z()) <= 1e-8) return std::nullopt;
    const double distance = -ray.origin.z() / ray.direction.z();
    if (distance < 0) return std::nullopt;
    auto point = ray.origin + ray.direction * distance;
    point.z() = 0;
    return point;
}
