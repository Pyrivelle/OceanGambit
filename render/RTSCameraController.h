#pragma once

#include <osg/Matrixd>
#include <osg/Vec2d>
#include <osg/Vec3d>

#include <optional>

struct RTSCameraState {
    osg::Vec3d target;
    double distance;
    double heading; // Degrees clockwise from +Y.
    double pitch;   // Degrees down from horizontal.
};

struct RTSCameraConfig {
    RTSCameraState initial{{0, 0, 0}, 1500, 0, 60};
    double minDistance = 45;
    double maxDistance = 6500;
    double minPitch = 30;
    double maxPitch = 90;
    double verticalFov = 45;
    double zoomPerNotch = .18;
    double zoomDamping = 14; // Exponential response, per second. Zero is immediate.
    double rotationPerPixel = .22;
    double edgeFractionPerSecond = .58;
    int edgePixels = 12;
    bool edgeScroll = true;
    bool wheelForwardZoomsIn = false;
};

class RTSCameraController {
public:
    explicit RTSCameraController(RTSCameraConfig config = {});
    const RTSCameraState& state() const { return state_; }
    osg::Matrixd viewMatrix() const;
    osg::Matrixd projectionMatrix(double aspect) const;
    std::optional<osg::Vec3d> seaPoint(const osg::Vec2d& ndc, double aspect) const;
    void pan(const osg::Vec2d& from, const osg::Vec2d& to, double aspect);
    void rotate(double dx, double dy);
    void zoom(double notches, const osg::Vec2d& cursor, double aspect);
    void home();
    void stopZoom();
    void advance(double dt, double aspect);

    // Pointer coordinates use Qt logical pixels, with +Y down.
    void scrollAtEdge(const osg::Vec2d& pointer, const osg::Vec2d& extent, double dt);

private:
    struct ZoomAnchor { osg::Vec2d cursor; osg::Vec3d world; };
    RTSCameraConfig config_;
    RTSCameraState state_;
    double requestedDistance_;
    std::optional<ZoomAnchor> zoomAnchor_;
};
