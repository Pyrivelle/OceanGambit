#include "RTSCameraController.h"

#include <osg/Math>

#include <algorithm>
#include <cmath>

RTSCameraController::RTSCameraController(RTSCameraConfig config)
    : config_(config), state_(config.initial), requestedDistance_(state_.distance) {
    state_.target.z() = 0;
    state_.distance = std::clamp(state_.distance, config_.minDistance, config_.maxDistance);
    state_.pitch = std::clamp(state_.pitch, config_.minPitch, config_.maxPitch);
    requestedDistance_ = state_.distance;
}

osg::Matrixd RTSCameraController::viewMatrix() const {
    const double heading = osg::DegreesToRadians(state_.heading);
    const double pitch = osg::DegreesToRadians(state_.pitch);
    const osg::Vec3d north(std::sin(heading), std::cos(heading), 0);
    const osg::Vec3d vertical(0, 0, 1);
    const auto eye = state_.target + (-north * std::cos(pitch) + vertical * std::sin(pitch)) * state_.distance;
    const auto up = north * std::sin(pitch) + vertical * std::cos(pitch);
    return osg::Matrixd::lookAt(eye, state_.target, up);
}

osg::Matrixd RTSCameraController::projectionMatrix(double aspect) const {
    return osg::Matrixd::perspective(config_.verticalFov, aspect, .15, 20000);
}

std::optional<osg::Vec3d> RTSCameraController::seaPoint(const osg::Vec2d& ndc, double aspect) const {
    osg::Matrixd inverse;
    if (aspect <= 0 || !inverse.invert(viewMatrix() * projectionMatrix(aspect))) return std::nullopt;
    const auto near = osg::Vec3d(ndc.x(), ndc.y(), -1) * inverse;
    const auto far = osg::Vec3d(ndc.x(), ndc.y(), 1) * inverse;
    const auto ray = far - near;
    if (ray.z() >= -1e-8) return std::nullopt;
    const double fraction = -near.z() / ray.z();
    if (fraction < 0 || fraction > 1) return std::nullopt;
    auto hit = near + ray * fraction;
    hit.z() = 0;
    return hit;
}

void RTSCameraController::pan(const osg::Vec2d& from, const osg::Vec2d& to, double aspect) {
    stopZoom();
    const auto before = seaPoint(from, aspect);
    const auto after = seaPoint(to, aspect);
    if (before && after) state_.target += *before - *after;
}

void RTSCameraController::rotate(double dx, double dy) {
    stopZoom();
    state_.heading = std::remainder(state_.heading + dx * config_.rotationPerPixel, 360.);
    state_.pitch = std::clamp(state_.pitch + dy * config_.rotationPerPixel,
                              config_.minPitch, config_.maxPitch);
}

void RTSCameraController::zoom(double notches, const osg::Vec2d& cursor, double aspect) {
    if (const auto world = seaPoint(cursor, aspect))
        zoomAnchor_ = ZoomAnchor{cursor, *world};
    else
        zoomAnchor_.reset();
    const double direction = config_.wheelForwardZoomsIn ? -1. : 1.;
    requestedDistance_ = std::clamp(requestedDistance_ * std::exp(direction * notches * config_.zoomPerNotch),
                                    config_.minDistance, config_.maxDistance);
}

void RTSCameraController::advance(double dt, double aspect) {
    if (dt <= 0 || requestedDistance_ == state_.distance) return;
    const double blend = config_.zoomDamping > 0 ? -std::expm1(-config_.zoomDamping * dt) : 1;
    state_.distance += (requestedDistance_ - state_.distance) * blend;
    if (std::abs(requestedDistance_ - state_.distance) < 1e-4) state_.distance = requestedDistance_;
    if (zoomAnchor_) {
        if (const auto now = seaPoint(zoomAnchor_->cursor, aspect))
            state_.target += zoomAnchor_->world - *now;
    }
    if (requestedDistance_ == state_.distance) zoomAnchor_.reset();
}

void RTSCameraController::home() {
    stopZoom();
    state_.heading = config_.initial.heading;
    state_.pitch = std::clamp(config_.initial.pitch, config_.minPitch, config_.maxPitch);
}

void RTSCameraController::stopZoom() {
    requestedDistance_ = state_.distance;
    zoomAnchor_.reset();
}

void RTSCameraController::scrollAtEdge(const osg::Vec2d& pointer, const osg::Vec2d& extent, double dt) {
    if (!config_.edgeScroll || config_.edgePixels <= 0 || dt <= 0 ||
        pointer.x() < 0 || pointer.y() < 0 || pointer.x() >= extent.x() || pointer.y() >= extent.y()) return;
    const auto edge = [&](double position, double size) {
        const double band = std::min(double(config_.edgePixels), size * .5);
        return std::clamp(1 - (size - position) / band, 0., 1.) -
               std::clamp(1 - position / band, 0., 1.);
    };
    const double x = edge(pointer.x(), extent.x());
    const double y = -edge(pointer.y(), extent.y());
    if (x == 0 && y == 0) return;
    const double heading = osg::DegreesToRadians(state_.heading);
    const osg::Vec3d right(std::cos(heading), -std::sin(heading), 0);
    const osg::Vec3d up(std::sin(heading), std::cos(heading), 0);
    const auto shift = (right * x + up * y) * (state_.distance * config_.edgeFractionPerSecond * dt);
    state_.target += shift;
    if (zoomAnchor_) zoomAnchor_->world += shift;
}
