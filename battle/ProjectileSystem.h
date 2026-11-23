#pragma once

#include <osg/Vec3d>

#include <functional>
#include <optional>
#include <random>
#include <vector>

struct ProjectileTuning {
    // Presentation values tuned for the demo scene.
    double launchPace = 670;
    double downPull = 16;
    double damping = .06;
    double spread = .0006;
    double lifetime = 8;
};

struct ProjectileContact {
    double fraction;
    osg::Vec3d position;
    osg::Vec3d normal;
    std::optional<unsigned> ship;
};

class ProjectileSystem {
public:
    explicit ProjectileSystem(ProjectileTuning tuning = {}, unsigned seed = 528)
        : tuning_(tuning), random_(seed) {}

    void launch(unsigned id, unsigned shooter, const osg::Vec3d& origin,
                osg::Vec3d direction, const osg::Vec3d& shipMotion);
    void step(const std::function<std::optional<ProjectileContact>(
                  unsigned, const osg::Vec3d&, const osg::Vec3d&)>& trace,
              const std::function<void(unsigned, const osg::Vec3d&)>& onFlight,
              const std::function<void(unsigned, unsigned, const std::optional<ProjectileContact>&)>& onStop);

    static constexpr double interval = .01;

private:
    struct Projectile {
        unsigned id, shooter;
        osg::Vec3d position, velocity;
        double age = 0;
    };

    ProjectileTuning tuning_;
    std::mt19937 random_;
    std::normal_distribution<double> spread_;
    std::vector<Projectile> active_;
};
