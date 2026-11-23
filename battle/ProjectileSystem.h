#pragma once

#include <osg/Vec3d>

#include <functional>
#include <optional>
#include <random>
#include <vector>

// These parameters describe a projectile/ammunition type, not a global battle rule.
enum class DragMode { Exponential, Quadratic };

struct AmmoConfig {
    double launchPace = 670.0;   // m/s
    double downPull = 16.0;      // m/s^2 (legacy demo)
    double damping = 0.06;       // 1/s, exponential model
    double dragK = 0.0;          // 1/m, quadratic model
    double spread = 0.0006;      // radian standard deviation, both axes
    double lifetime = 8.0;       // seconds
    DragMode dragMode = DragMode::Exponential;

    static AmmoConfig exponentialDemo() { return {}; }
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
    const AmmoConfig& ammo() const { return ammo_; }
    static constexpr double interval = .01;

private:
    struct Projectile {
        unsigned id, shooter;
        osg::Vec3d position, velocity;
        double age = 0;
    };

    void integrate(osg::Vec3d& position, osg::Vec3d& velocity) const;
    AmmoConfig ammo_;
    std::mt19937 random_;
    std::normal_distribution<double> spread_;
    std::vector<Projectile> active_;
};
