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

namespace {
struct AngleBracket { double lo, hi; };

// The same forward integration is used by both live projectiles and fire control.
}

const char* fireControlStatusName(FireControlStatus status) {
    switch (status) {
    case FireControlStatus::Ready: return "ready";
    case FireControlStatus::FallbackLow: return "high_arc_unavailable_fallback_low";
    case FireControlStatus::OutOfRange: return "out_of_range";
    case FireControlStatus::ElevationLimited: return "elevation_limit";
    case FireControlStatus::NoSolution: return "no_ballistic_solution";
    case FireControlStatus::InvalidTarget: return "invalid_target";
    }
    return "unknown";
}

double ProjectileSystem::aimElevation(double horizontalRange, double heightDifference) const {
    return solveElevation(horizontalRange, heightDifference).value_or(0.0);
}

std::optional<double> ProjectileSystem::solveElevation(
    double horizontalRange, double heightDifference) const {
    return solveElevation(horizontalRange, heightDifference, FireArc::Low, -0.1, 1.2);
}

std::optional<double> ProjectileSystem::solveElevation(
    double horizontalRange, double heightDifference, FireArc arc,
    double minElevationRad, double maxElevationRad) const {
    const auto solution = fireControlSolution(
        horizontalRange, heightDifference, arc, minElevationRad, maxElevationRad);
    // The strict API promises the requested branch; do not expose automatic
    // high-to-low fallback to older callers expecting a high-arc-only result.
    if (solution.status == FireControlStatus::FallbackLow && arc == FireArc::High)
        return std::nullopt;
    return solution.elevation;
}

FireControlSolution ProjectileSystem::fireControlSolution(
    double horizontalRange, double heightDifference, FireArc preferred,
    double minElevationRad, double maxElevationRad) const {
    FireControlSolution result;
    if (!std::isfinite(horizontalRange) || !std::isfinite(heightDifference) ||
        !std::isfinite(minElevationRad) || !std::isfinite(maxElevationRad) ||
        horizontalRange <= 0 || ammo_.launchPace <= 0 || ammo_.lifetime <= 0 ||
        minElevationRad >= maxElevationRad ||
        minElevationRad <= -1.5707963267948966 ||
        maxElevationRad >= 1.5707963267948966)
        return result;

    const auto heightAtRange = [&](double angle) -> std::optional<double> {
        osg::Vec3d p(0, 0, 0);
        osg::Vec3d v(ammo_.launchPace * std::cos(angle), 0,
                     ammo_.launchPace * std::sin(angle));
        ocean_gambit::ballistics::MotionHistory history;
        const int maxSteps = static_cast<int>(std::ceil(ammo_.lifetime / interval));
        for (int step = 0; step < maxSteps; ++step) {
            const osg::Vec3d before = p;
            integrate(p, v, history);
            if (p.x() >= horizontalRange) {
                const double dx = p.x() - before.x();
                if (dx <= 0) return std::nullopt;
                const double fraction = (horizontalRange - before.x()) / dx;
                return before.z() + (p.z() - before.z()) * fraction;
            }
            if (v.x() <= 0) break;
        }
        return std::nullopt;
    };

    const auto findBrackets = [&](double minAngle, double maxAngle) {
        std::vector<AngleBracket> brackets;
        const int intervals = std::max(1, static_cast<int>(
            std::ceil((maxAngle - minAngle) / .015)));
        std::optional<double> previousHeight;
        double previousAngle = minAngle;
        for (int i = 0; i <= intervals; ++i) {
            const double angle = minAngle + (maxAngle - minAngle) * i / intervals;
            const auto h = heightAtRange(angle);
            if (h && previousHeight) {
                const double a = *previousHeight - heightDifference;
                const double b = *h - heightDifference;
                if ((a <= 0 && b >= 0) || (a >= 0 && b <= 0))
                    brackets.push_back({previousAngle, angle});
            }
            previousHeight = h;
            previousAngle = angle;
        }
        return brackets;
    };

    const auto refine = [&](AngleBracket bracket) -> std::optional<double> {
        const auto left = heightAtRange(bracket.lo);
        if (!left) return std::nullopt;
        const bool increasing = *left < heightDifference;
        for (int j = 0; j < 28; ++j) {
            const double mid = (bracket.lo + bracket.hi) * .5;
            const auto h = heightAtRange(mid);
            if (!h) return std::nullopt;
            if ((*h < heightDifference) == increasing) bracket.lo = mid;
            else bracket.hi = mid;
        }
        return (bracket.lo + bracket.hi) * .5;
    };

    const auto brackets = findBrackets(minElevationRad, maxElevationRad);
    if (!brackets.empty()) {
        // A single root is the low arc; high preference only selects the
        // second root when it actually exists inside the weapon envelope.
        const bool highAvailable = brackets.size() >= 2;
        const bool useHigh = preferred == FireArc::High && highAvailable;
        result.selectedArc = useHigh ? FireArc::High : FireArc::Low;
        result.elevation = refine(useHigh ? brackets.back() : brackets.front());
        if (result.elevation) {
            result.status = preferred == FireArc::High && !highAvailable
                ? FireControlStatus::FallbackLow : FireControlStatus::Ready;
            return result;
        }
    }

    // Distinguish a physical firing solution outside the gun's elevation
    // envelope from a projectile that cannot reach the target at all.
    const auto physical = findBrackets(-1.50, 1.569);
    if (!physical.empty()) {
        result.status = FireControlStatus::ElevationLimited;
        return result;
    }

    // Estimate the maximum horizontal distance at which a trajectory descends
    // through the target height (not just whether x reaches the target range).
    // This is a diagnostic only, invoked after regular root solving fails.
    double farthest = 0;
    constexpr int samples = 80;
    for (int sample = 0; sample <= samples; ++sample) {
        const double angle = -0.10 + 1.65 * sample / samples;
        osg::Vec3d p(0, 0, 0);
        osg::Vec3d v(ammo_.launchPace * std::cos(angle), 0,
                     ammo_.launchPace * std::sin(angle));
        ocean_gambit::ballistics::MotionHistory history;
        const int steps = static_cast<int>(std::ceil(ammo_.lifetime / interval));
        for (int step = 0; step < steps; ++step) {
            const auto before = p;
            integrate(p, v, history);
            if (before.z() >= heightDifference && p.z() <= heightDifference &&
                v.z() <= 0 && before.z() != p.z()) {
                const double t = (heightDifference - before.z()) / (p.z() - before.z());
                farthest = std::max(farthest, before.x() + (p.x() - before.x()) * t);
                break;
            }
            if (v.x() <= 0) break;
        }
    }
    result.status = horizontalRange > farthest + 1.0
        ? FireControlStatus::OutOfRange : FireControlStatus::NoSolution;
    return result;
}
