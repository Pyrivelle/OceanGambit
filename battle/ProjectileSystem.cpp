#include "ProjectileSystem.h"

#include <algorithm>
#include <cmath>

void ProjectileSystem::launch(unsigned id, unsigned shooter, const osg::Vec3d& origin,
                              osg::Vec3d direction, const osg::Vec3d& shipMotion) {
    if (direction.normalize() == 0) return;
    osg::Vec3d side = direction ^ osg::Vec3d(0, 0, 1);
    if (side.normalize() == 0) side.set(1, 0, 0);
    const auto up = side ^ direction;
    const auto angular = [this]() {
        if (ammo_.dragMode != DragMode::WowsShell) return spread_(random_) * ammo_.spread;
        // Truncated standard normal scaled to a bounded angular radius.
        const double sigma = std::max(0.01, ammo_.sigma);
        double s;
        do { s = spread_(random_); } while (std::abs(s) > sigma);
        return s / sigma * ammo_.spread;
    };
    const double lateral = angular();
    const double vertical = angular() * ammo_.verticalSpreadRatio;
    direction += side * lateral + up * vertical;
    direction.normalize();
    active_.push_back({id, shooter, origin, direction * ammo_.launchPace + shipMotion});
}

void ProjectileSystem::integrate(osg::Vec3d& position, osg::Vec3d& velocity,
                                 ocean_gambit::ballistics::MotionHistory& history) const {
    if (ammo_.dragMode == DragMode::WowsShell) {
        namespace b = ocean_gambit::ballistics;
        b::MotionState state{{position.x(), position.y(), position.z()},
                             {velocity.x(), velocity.y(), velocity.z()}};
        b::advance(state, history,
                   {ammo_.caliberMeters, ammo_.massKg, ammo_.dragCoefficient, ammo_.downPull},
                   ammo_.integrator, interval);
        position.set(state.position.x, state.position.y, state.position.z);
        velocity.set(state.velocity.x, state.velocity.y, state.velocity.z);
        return;
    }
    if (ammo_.dragMode == DragMode::Exponential) {
        // Preserve the exact original demo damping/integration ordering.
        velocity *= std::exp(-ammo_.damping * interval);
        velocity.z() -= ammo_.downPull * interval;
    } else {
        // dv/dt = g - k |v| v, with constant equivalent quadratic-drag coefficient.
        const double speed = velocity.length();
        const osg::Vec3d acceleration =
            osg::Vec3d(0, 0, -ammo_.downPull) - velocity * (ammo_.dragK * speed);
        velocity += acceleration * interval;
    }
    position += velocity * interval; // semi-implicit Euler (quadratic mode)
}

void ProjectileSystem::step(const std::function<std::optional<ProjectileContact>(
                                unsigned, const osg::Vec3d&, const osg::Vec3d&)>& trace,
                            const std::function<void(unsigned, const osg::Vec3d&)>& onFlight,
                            const std::function<void(unsigned, unsigned, const std::optional<ProjectileContact>&)>& onStop) {
    active_.erase(std::remove_if(active_.begin(), active_.end(), [&](Projectile& p) {
        const auto old = p.position;
        integrate(p.position, p.velocity, p.history);
        p.age += interval;
        const auto contact = trace(p.shooter, old, p.position);
        if (contact || p.age >= ammo_.lifetime) {
            onStop(p.id, p.shooter, contact);
            return true;
        }
        onFlight(p.id, p.position);
        return false;
    }), active_.end());
}

double ProjectileSystem::aimElevation(double horizontalRange, double heightDifference) const {
    const auto result = solveElevation(horizontalRange, heightDifference);
    return result.value_or(0.0); // legacy API; use solveElevation to check reachability
}

std::optional<double> ProjectileSystem::solveElevation(
    double horizontalRange, double heightDifference) const {
    if (horizontalRange <= 0 || ammo_.launchPace <= 0) return std::nullopt;
    const auto heightAtRange = [&](double angle) {
        osg::Vec3d p(0, 0, 0);
        osg::Vec3d v(ammo_.launchPace * std::cos(angle), 0,
                     ammo_.launchPace * std::sin(angle));
        ocean_gambit::ballistics::MotionHistory history;
        const int maxSteps = static_cast<int>(std::ceil(ammo_.lifetime / interval));
        for (int i = 0; i < maxSteps; ++i) {
            const osg::Vec3d old = p;
            integrate(p, v, history);
            if (p.x() >= horizontalRange) {
                const double f = (horizontalRange - old.x()) / (p.x() - old.x());
                return old.z() + (p.z() - old.z()) * f;
            }
            if (v.x() <= 0) break;
        }
        return -1e100;
    };

    // Scan for the first upward crossing to find the low arc. This brackets
    // the root before bisection; out-of-range targets return no firing solution.
    constexpr double start = -0.10, stop = 1.20, step = 0.02;
    double lo = start;
    double errorLo = heightAtRange(lo) - heightDifference;
    for (double hi = lo + step; hi <= stop + 1e-9; hi += step) {
        const double errorHi = heightAtRange(hi) - heightDifference;
        if (errorLo <= 0 && errorHi >= 0) {
            for (int j = 0; j < 28; ++j) {
                const double middle = (lo + hi) * .5;
                if (heightAtRange(middle) < heightDifference) lo = middle;
                else hi = middle;
            }
            return (lo + hi) * .5;
        }
        lo = hi;
        errorLo = errorHi;
    }
    return std::nullopt;
}
