#include "WowsShellAdapter.h"

#include "../third_party/wows_shell/shellCPP.hpp"

#include <cmath>
#include <stdexcept>

namespace ocean_gambit::ballistics {
namespace {

template <wows_shell::numerical Method>
void calculate(wows_shell::shellCalc& calculator, wows_shell::shell& shell) {
    // One worker avoids SIMD/thread dependencies in the prototype.
    calculator.calculateImpact<Method>(shell, false, 1);
}

} // namespace

std::vector<WowsShellSample> sampleWowsShell(
    const WowsShellAmmo& ammo, WowsShellIntegrator integrator,
    double minAngleDeg, double maxAngleDeg, double stepDeg,
    double timeStepSeconds) {
    if (!std::isfinite(ammo.caliberMeters) || ammo.caliberMeters <= 0 ||
        !std::isfinite(ammo.massKg) || ammo.massKg <= 0 ||
        !std::isfinite(ammo.muzzleSpeedMps) || ammo.muzzleSpeedMps <= 0 ||
        !std::isfinite(ammo.dragCoefficient) || ammo.dragCoefficient < 0 ||
        !std::isfinite(ammo.krupp) || ammo.krupp < 0 ||
        !std::isfinite(minAngleDeg) || !std::isfinite(maxAngleDeg) ||
        !std::isfinite(stepDeg) || !std::isfinite(timeStepSeconds) ||
        minAngleDeg < 0 || maxAngleDeg > 89 || maxAngleDeg < minAngleDeg ||
        stepDeg <= 0 || timeStepSeconds <= 0) {
        throw std::invalid_argument("Invalid wows_shell sampling configuration");
    }
    // The upstream library computes sample count using a division and truncation.
    // Require a grid with an integral number of intervals to avoid surprise values.
    const double n = (maxAngleDeg - minAngleDeg) / stepDeg;
    if (n > 100000 || std::abs(n - std::round(n)) > 1e-7) {
        throw std::invalid_argument("Angle range must be an integral number of steps");
    }

    wows_shell::shellParams parameters{};
    parameters.caliber = ammo.caliberMeters;
    parameters.mass = ammo.massKg;
    parameters.v0 = ammo.muzzleSpeedMps;
    parameters.cD = ammo.dragCoefficient;
    parameters.krupp = ammo.krupp;
    parameters.normalization = 0;
    parameters.fuseTime = 0;
    parameters.threshold = 0;
    parameters.ricochet0 = 0;
    parameters.ricochet1 = 0;
    parameters.nonAP = 0;
    // The upstream 2-argument constructor preprocesses dispersion fields
    // before initializing them. Supply well-formed values even when only
    // trajectory/impact are requested.
    wows_shell::dispersionParams dispersion{};
    dispersion.idealRadius = 1.0;
    dispersion.minRadius = 1.0;
    dispersion.idealDistance = 1000.0;
    dispersion.taperDistance = 1000.0;
    dispersion.delim = 0.5;
    dispersion.zeroRadius = 1.0;
    dispersion.delimRadius = 1.0;
    dispersion.maxRadius = 1.0;
    dispersion.maxDistance = 30000.0;
    dispersion.sigma = 2.0;
    wows_shell::shell projectile(parameters, dispersion, "OceanGambit reference");
    wows_shell::shellCalc calculator(1);
    calculator.set_min(minAngleDeg);
    calculator.set_max(maxAngleDeg);
    calculator.set_precision(stepDeg);
    calculator.set_dt_min(timeStepSeconds);

    switch (integrator) {
    case WowsShellIntegrator::ForwardEuler:
        calculate<wows_shell::numerical::forwardEuler>(calculator, projectile);
        break;
    case WowsShellIntegrator::RungeKutta2:
        calculate<wows_shell::numerical::rungeKutta2>(calculator, projectile);
        break;
    case WowsShellIntegrator::RungeKutta4:
        calculate<wows_shell::numerical::rungeKutta4>(calculator, projectile);
        break;
    case WowsShellIntegrator::AdamsBashforth5:
        calculate<wows_shell::numerical::adamsBashforth5>(calculator, projectile);
        break;
    }

    std::vector<WowsShellSample> result;
    const auto count = static_cast<std::size_t>(std::llround(n)) + 1;
    result.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto read = [&](wows_shell::impact::impactIndices field) {
            return projectile.get_impact(i, field);
        };
        result.push_back({
            read(wows_shell::impact::impactIndices::launchAngle),
            read(wows_shell::impact::impactIndices::distance),
            read(wows_shell::impact::impactIndices::timeToTarget),
            read(wows_shell::impact::impactIndices::impactVelocity),
            read(wows_shell::impact::impactIndices::rawPenetration)
        });
    }
    return result;
}

} // namespace ocean_gambit::ballistics
