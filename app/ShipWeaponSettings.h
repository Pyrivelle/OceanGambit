#pragma once

#include "../battle/ProjectileSystem.h"
#include <string>

// Gun-arc preference belongs to the ship/weapon definition, not the camera.
struct ShipWeaponSettings {
    FireArc arc = FireArc::Low;
    double minElevationDeg = -5.0;
    double maxElevationDeg = 85.0;
    double minElevationRad() const { return minElevationDeg * 0.017453292519943295; }
    double maxElevationRad() const { return maxElevationDeg * 0.017453292519943295; }
};

ShipWeaponSettings loadShipWeaponSettings(const std::string& modelPath);
