#include "ProjectileSystem.h"

#include <algorithm>
#include <cmath>

void ProjectileSystem::launch(unsigned id, unsigned shooter, const osg::Vec3d& origin,
                              osg::Vec3d direction, const osg::Vec3d& shipMotion) {
    direction.normalize();
    osg::Vec3d side = direction ^ osg::Vec3d(0, 0, 1);
    if (side.normalize() == 0) side.set(1, 0, 0);
    const auto up = side ^ direction;
    const double lateral = spread_(random_) * tuning_.spread;
    const double vertical = spread_(random_) * tuning_.spread;
    direction += side * lateral + up * vertical;
    direction.normalize();
    active_.push_back({id, shooter, origin, direction * tuning_.launchPace + shipMotion});
}

void ProjectileSystem::step(const std::function<std::optional<ProjectileContact>(
                                unsigned, const osg::Vec3d&, const osg::Vec3d&)>& trace,
                            const std::function<void(unsigned, const osg::Vec3d&)>& onFlight,
                            const std::function<void(unsigned, unsigned, const std::optional<ProjectileContact>&)>& onStop) {
    const double retain = std::exp(-tuning_.damping * interval);
    active_.erase(std::remove_if(active_.begin(), active_.end(), [&](Projectile& p) {
        const auto old = p.position;
        p.velocity *= retain;
        p.velocity.z() -= tuning_.downPull * interval;
        p.position += p.velocity * interval;
        p.age += interval;
        const auto contact = trace(p.shooter, old, p.position);
        if (contact || p.age >= tuning_.lifetime) {
            onStop(p.id, p.shooter, contact);
            return true;
        }
        onFlight(p.id, p.position);
        return false;
    }), active_.end());
}
