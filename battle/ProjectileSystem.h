#pragma once

#include <osg/Vec3d>
#include "WowsMotion.h"

#include <functional>
#include <optional>
#include <random>
#include <vector>

// These parameters describe a projectile/ammunition type, not a global battle rule.
enum class DragMode { Exponential, Quadratic, WowsShell };
enum class FireArc { Low, High };
// A failed shot is not always out of range: elevation restrictions and
// numerical/geometry failures must be reported separately.
enum class FireControlStatus {
    Ready, FallbackLow, OutOfRange, ElevationLimited, NoSolution, InvalidTarget
};
struct FireControlSolution {
    std::optional<double> elevation;
    FireArc selectedArc = FireArc::Low;
    FireControlStatus status = FireControlStatus::InvalidTarget;
};
const char* fireControlStatusName(FireControlStatus status);

struct AmmoConfig {
    double launchPace = 670.0;   // m/s
    double downPull = 16.0;      // m/s^2 (legacy demo)
    double damping = 0.06;       // 1/s, exponential model
    double dragK = 0.0;          // 1/m, quadratic model
    double spread = 0.0006;      // radian standard deviation, both axes
    double lifetime = 8.0;       // seconds
    DragMode dragMode = DragMode::Exponential;
    double caliberMeters = 0.1524;
    double massKg = 45.0;
    double dragCoefficient = 0.3;
    double sigma = 2.0; // truncated-normal bound (WoWS-inspired)
    double verticalSpreadRatio = 1.0;
    ocean_gambit::ballistics::Integrator integrator = ocean_gambit::ballistics::Integrator::RungeKutta4;

    static AmmoConfig exponentialDemo() { return {}; }
    static AmmoConfig wowsDemo() {
        AmmoConfig ammo;
        ammo.dragMode = DragMode::WowsShell;
        ammo.downPull = 9.8;
        ammo.spread = 0.0012; // angular maximum, sigma=2 gives similar central width
        // High arc may take considerably longer than the original 30 s demo.
        ammo.lifetime = 120.0;
        return ammo;
    }
    static AmmoConfig quadraticDemo() {
        AmmoConfig ammo;
        ammo.dragMode = DragMode::Quadratic;
        ammo.downPull = 9.80665;
        ammo.dragK = 0.06 / 670.0; // matched initial deceleration, NOT measured ballistics
        ammo.lifetime = 30.0;
        return ammo;
    }
};

// Keep old users source-compatible while the name is migrated to ammunition config.
using ProjectileTuning = AmmoConfig;

struct ProjectileContact {
    double fraction;
    osg::Vec3d position;
    osg::Vec3d normal;
    std::optional<unsigned> ship;
};

class ProjectileSystem {
public:
    explicit ProjectileSystem(AmmoConfig ammo = {}, unsigned seed = 528)
        : ammo_(ammo), random_(seed) {}

    void launch(unsigned id, unsigned shooter, const osg::Vec3d& origin,
                osg::Vec3d direction, const osg::Vec3d& shipMotion);
    void step(const std::function<std::optional<ProjectileContact>(
                  unsigned, const osg::Vec3d&, const osg::Vec3d&)>& trace,
              const std::function<void(unsigned, const osg::Vec3d&)>& onFlight,
              const std::function<void(unsigned, unsigned, const std::optional<ProjectileContact>&)>& onStop);

    // Estimate a low-arc elevation using the same integrator as the live projectile.
    // Target is stationary; no ship motion, dispersion or fire-control lead.
    double aimElevation(double horizontalRange, double heightDifference) const;
    std::optional<double> solveElevation(double horizontalRange, double heightDifference) const;
    std::optional<double> solveElevation(double horizontalRange, double heightDifference,
                                         FireArc arc, double minElevationRad,
                                         double maxElevationRad) const;
    FireControlSolution fireControlSolution(double horizontalRange, double heightDifference,
                                            FireArc preferred, double minElevationRad,
                                            double maxElevationRad) const;
    const AmmoConfig& ammo() const { return ammo_; }
    static constexpr double interval = .01;

private:
    struct Projectile {
        unsigned id, shooter;
        osg::Vec3d position, velocity;
        double age = 0;
        ocean_gambit::ballistics::MotionHistory history;
    };

    void integrate(osg::Vec3d& position, osg::Vec3d& velocity,
                   ocean_gambit::ballistics::MotionHistory& history) const;
    AmmoConfig ammo_;
    std::mt19937 random_;
    std::normal_distribution<double> spread_;
    std::vector<Projectile> active_;
};
