#include "ProjectileSystem.h"

#include <algorithm>
#include <cmath>

void ProjectileSystem::launch(unsigned id, unsigned shooter, const osg::Vec3d& origin,
                              osg::Vec3d direction, const osg::Vec3d& shipMotion) {
    if (direction.normalize() == 0) return;
    osg::Vec3d side = direction ^ osg::Vec3d(0, 0, 1);
    if (side.normalize() == 0) side.set(1, 0, 0);
    const auto up = side ^ direction;
    const double lateral = spread_(random_) * ammo_.spread;
    const double vertical = spread_(random_) * ammo_.spread;
    direction += side * lateral + up * vertical;
    direction.normalize();
    active_.push_back({id, shooter, origin, direction * ammo_.launchPace + shipMotion});
}

void ProjectileSystem::integrate(osg::Vec3d& position, osg::Vec3d& velocity) const {
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
        integrate(p.position, p.velocity);
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
    if (horizontalRange <= 0 || ammo_.launchPace <= 0) return 0;
    const auto heightAtRange = [&](double angle) {
        osg::Vec3d p(0, 0, 0);
        osg::Vec3d v(ammo_.launchPace * std::cos(angle), 0,
                     ammo_.launchPace * std::sin(angle));
        for (double age = 0; age < ammo_.lifetime; age += interval) {
            const osg::Vec3d old = p;
            integrate(p, v);
            if (p.x() >= horizontalRange) {
                const double f = (horizontalRange - old.x()) / (p.x() - old.x());
                return old.z() + (p.z() - old.z()) * f;
            }
            if (v.x() <= 0) break;
        }
        return -1e100; // range not reachable with this angle and lifetime
    };
    // The first low-angle crossing is sufficient for the short demo engagement.
    double lo = -0.1, hi = 0.35;
    if (heightAtRange(hi) < heightDifference)
        return std::atan2(heightDifference, horizontalRange);
    for (int i = 0; i < 24; ++i) {
        const double mid = (lo + hi) * 0.5;
        if (heightAtRange(mid) < heightDifference) lo = mid;
        else hi = mid;
    }
    return (lo + hi) * 0.5;
}
