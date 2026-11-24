#pragma once

#include "../render/ShipCameraController.h"

#include <string>

ShipCameraSettings loadShipCameraSettings(const std::string& modelPath, std::optional<osg::Vec3d> bridgeEye);
