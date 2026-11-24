#include "../render/RTSCameraController.h"

#include <cmath>
#include <iostream>

namespace {
bool close(const osg::Vec3d& a, const osg::Vec3d& b) { return (a - b).length() < 1e-5; }
}

bool verifyRTSCamera() {
    constexpr double aspect = 1.6;
    RTSCameraController camera;
    const auto center = camera.seaPoint({0, 0}, aspect);
    const auto north = camera.seaPoint({0, .5}, aspect);
    const auto eye = osg::Matrixd::inverse(camera.viewMatrix()).getTrans();
    const bool downFromHorizontal =
        std::abs(eye.z() / camera.state().distance - std::sqrt(3.) / 2.) < 1e-6;
    const bool initial = downFromHorizontal && center && north && close(*center, {0, 0, 0}) &&
        north->y() > center->y() && std::abs(north->x()) < 1e-5 &&
        camera.state().pitch == 60;

    const osg::Vec2d from(-.3, .2), to(.2, -.15);
    const auto held = camera.seaPoint(from, aspect);
    camera.pan(from, to, aspect);
    const auto dragged = camera.seaPoint(to, aspect);
    const bool pan = held && dragged && close(*held, *dragged);

    const auto distance = camera.state().distance;
    camera.zoom(-2, to, aspect);
    camera.advance(.02, aspect);
    const auto zoomed = camera.seaPoint(to, aspect);
    const double firstStep = camera.state().distance;
    camera.advance(.12, aspect);
    const auto settled = camera.seaPoint(to, aspect);
    const bool zoom = dragged && zoomed && settled && close(*dragged, *zoomed) &&
        close(*dragged, *settled) && distance > firstStep && firstStep > camera.state().distance;

    camera.rotate(2000, 900);
    const auto top = camera.seaPoint({0, 0}, aspect);
    const auto rotatedTarget = camera.state().target;
    const auto rotatedDistance = camera.state().distance;
    const bool topDown = camera.state().pitch == 90 && top && close(*top, rotatedTarget);
    camera.rotate(0, -900);
    const bool pitchLimit = camera.state().pitch == 30;
    camera.home();
    const bool home = close(camera.state().target, rotatedTarget) &&
        camera.state().distance == rotatedDistance && camera.state().heading == 0 &&
        camera.state().pitch == 60;

    RTSCameraController once, twice;
    once.scrollAtEdge({1, 1}, {800, 600}, .08);
    twice.scrollAtEdge({1, 1}, {800, 600}, .04);
    twice.scrollAtEdge({1, 1}, {800, 600}, .04);
    const auto corner = once.state().target;
    once.scrollAtEdge({-1, 1}, {800, 600}, .5);
    RTSCameraConfig config;
    config.edgeScroll = false;
    RTSCameraController disabled(config);
    disabled.scrollAtEdge({1, 1}, {800, 600}, 1);
    const bool edge = corner.x() < 0 && corner.y() > 0 && close(corner, twice.state().target) &&
        close(corner, once.state().target) && close(disabled.state().target, {0, 0, 0});

    camera.zoom(-10000, {0, 0}, aspect);
    camera.advance(2, aspect);
    const bool nearLimit = camera.state().distance == config.minDistance;
    camera.zoom(10000, {0, 0}, aspect);
    camera.advance(2, aspect);
    const bool farLimit = camera.state().distance == config.maxDistance;

    std::cout << "north=" << initial << " pan_anchor=" << pan << " zoom_anchor=" << zoom
              << " top_down=" << topDown << " pitch_limit=" << pitchLimit << " home=" << home
              << " edge=" << edge << " distance_limits=" << (nearLimit && farLimit) << std::endl;
    return initial && pan && zoom && topDown && pitchLimit && home && edge && nearLimit && farLimit;
}
