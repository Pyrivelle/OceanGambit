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
    return solveElevation(horizontalRange, heightDifference, FireArc::Low, -0.1, 1.2);
}

std::optional<double> ProjectileSystem::solveElevation(
    double horizontalRange, double heightDifference, FireArc arc,
    double minElevationRad, double maxElevationRad) const {
    if (!std::isfinite(horizontalRange) || !std::isfinite(heightDifference) ||
        !std::isfinite(minElevationRad) || !std::isfinite(maxElevationRad) ||
        horizontalRange <= 0 || ammo_.launchPace <= 0 ||
        minElevationRad >= maxElevationRad ||
        minElevationRad <= -1.5707963267948966 ||
        maxElevationRad >= 1.5707963267948966)
        return std::nullopt;

    const auto heightAtRange = [&](double angle) -> std::optional<double> {
        osg::Vec3d p(0, 0, 0);
        osg::Vec3d v(ammo_.launchPace * std::cos(angle), 0,
                     ammo_.launchPace * std::sin(angle));
        ocean_gambit::ballistics::MotionHistory history;
        const int maxSteps = static_cast<int>(std::ceil(ammo_.lifetime / interval));
        for (int i = 0; i < maxSteps; ++i) {
            const osg::Vec3d old = p;
            integrate(p, v, history);
            if (p.x() >= horizontalRange) {
                const double dx = p.x() - old.x();
                if (dx <= 0) return std::nullopt;
                const double t = (horizontalRange - old.x()) / dx;
                return old.z() + (p.z() - old.z()) * t;
            }
            if (v.x() <= 0) break;
        }
        return std::nullopt;
    };

    // Scan the full allowed gun envelope for signed crossings. The first
    // crossing is the low arc and the last is the high arc. A missing trajectory
    // (range not reached before expiration) must NOT become a fake high root.
    struct Bracket { double lo, hi; };
    std::vector<Bracket> brackets;
    const double scanStep = 0.025;
    std::optional<double> previous;
    double previousAngle = minElevationRad;
    const int count = std::max(1, static_cast<int>(
        std::ceil((maxElevationRad - minElevationRad) / scanStep)));
    for (int i = 0; i <= count; ++i) {
        const double angle = minElevationRad +
            (maxElevationRad - minElevationRad) * i / count;
        const auto height = heightAtRange(angle);
        if (height && previous) {
            const double a = *previous - heightDifference;
            const double b = *height - heightDifference;
            if ((a <= 0 && b >= 0) || (a >= 0 && b <= 0))
                brackets.push_back({previousAngle, angle});
        }
        previous = height;
        previousAngle = angle;
    }
    if (brackets.empty() || (arc == FireArc::High && brackets.size() < 2))
        return std::nullopt;
    auto bracket = arc == FireArc::Low ? brackets.front() : brackets.back();
    const auto leftHeight = heightAtRange(bracket.lo);
    if (!leftHeight) return std::nullopt;
    const bool leftBelow = *leftHeight < heightDifference;
    for (int iteration = 0; iteration < 30; ++iteration) {
        const double mid = (bracket.lo + bracket.hi) * .5;
        const auto height = heightAtRange(mid);
        if (!height) return std::nullopt;
        if ((*height < heightDifference) == leftBelow) bracket.lo = mid;
        else bracket.hi = mid;
    }
    return (bracket.lo + bracket.hi) * .5;
}
