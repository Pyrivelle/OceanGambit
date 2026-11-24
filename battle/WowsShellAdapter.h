#pragma once

#include <vector>

namespace ocean_gambit::ballistics {

// Independent result type: no OSG/Qt/gameplay dependency. Angle in degrees.
struct WowsShellSample {
    double launchAngleDeg = 0.0;
    double rangeMeters = 0.0;
    double flightSeconds = 0.0; // real model time; not WoWS UI time
    double impactSpeedMps = 0.0;
    double rawPenetrationMm = 0.0; // reference WoWS empirical model only
};

enum class WowsShellIntegrator {
    ForwardEuler,
    RungeKutta2,
    RungeKutta4,
    AdamsBashforth5
};

struct WowsShellAmmo {
    double caliberMeters = 0.1;
    double massKg = 50.0;
    double muzzleSpeedMps = 670.0;
    double dragCoefficient = 0.3;
    double krupp = 2000.0; // WoWS-specific empirical penetration factor
};

// Optional reference/calibration path; deliberately not the live projectile simulator.
// Returns samples in ascending launch-angle order (inclusive endpoints).
std::vector<WowsShellSample> sampleWowsShell(
    const WowsShellAmmo& ammo,
    WowsShellIntegrator integrator,
    double minAngleDeg,
    double maxAngleDeg,
    double stepDeg,
    double timeStepSeconds = 0.02);

} // namespace ocean_gambit::ballistics
