#include "../battle/ProjectileSystem.h"

#include <cmath>
#include <iostream>
#include <vector>

namespace {
std::vector<osg::Vec3d> sample(AmmoConfig ammo, unsigned seed, unsigned count, int steps) {
    ProjectileSystem system(ammo, seed);
    for (unsigned id = 0; id < count; ++id)
        system.launch(id, 0, {}, {1, 0, 0}, {});

    std::vector<osg::Vec3d> positions(count);
    for (int i = 0; i < steps; ++i)
        system.step([](unsigned, const auto&, const auto&) { return std::nullopt; },
                    [&](unsigned id, const auto& position) { positions.at(id) = position; },
                    [](unsigned, unsigned, const auto&) {});
    return positions;
}
}

int main() {
    AmmoConfig straight = AmmoConfig::exponentialDemo();
    straight.damping = straight.spread = 0;
    constexpr int steps = 100;
    const double dt = ProjectileSystem::interval;
    const auto position = sample(straight, 528, 1, steps).front();
    const double expectedDrop = straight.downPull * dt * dt * steps * (steps + 1) / 2;
    const bool arc = std::abs(position.x() - straight.launchPace * steps * dt) < 1e-8 &&
                     std::abs(position.z() + expectedDrop) < 1e-8;
    AmmoConfig damped = AmmoConfig::exponentialDemo();
    damped.spread = 0;
    const bool damping = sample(damped, 528, 1, steps).front().x() < position.x();
    const auto spread = sample({}, 528, 8, steps);
    const bool replay = spread == sample({}, 528, 8, steps) &&
                        spread != sample({}, 529, 8, steps) && spread[0] != spread[1];

    AmmoConfig quadratic = AmmoConfig::quadraticDemo();
    quadratic.spread = 0;
    const auto quadraticPos = sample(quadratic, 528, 1, steps).front();
    AmmoConfig quadNoDrag = quadratic;
    quadNoDrag.dragK = 0;
    const auto quadFree = sample(quadNoDrag, 528, 1, steps).front();
    const double expectedQuadDrop = quadNoDrag.downPull * dt * dt * steps * (steps + 1) / 2;
    const bool quadraticDrag = quadraticPos.x() < quadFree.x() &&
        std::abs(quadFree.x() - quadNoDrag.launchPace * steps * dt) < 1e-8 &&
        std::abs(quadFree.z() + expectedQuadDrop) < 1e-8;
    quadratic.spread = 0.0006;
    const bool quadraticReplay = sample(quadratic, 528, 8, steps) ==
        sample(quadratic, 528, 8, steps);
    const double angle = ProjectileSystem(quadratic).aimElevation(1000.0, -6.0);
    const bool aim = std::isfinite(angle) && angle > -0.1 && angle < 0.35;

    ProjectileSystem stopped;
    stopped.launch(1, 0, {}, {1, 0, 0}, {});
    int contacts = 0;
    for (int i = 0; i < 2; ++i)
        stopped.step([](unsigned, const auto& from, const auto& to) {
                         return ProjectileContact{.5, (from + to) * .5, {0, 0, 1}, std::nullopt};
                     }, [](unsigned, const auto&) {},
                     [&](unsigned, unsigned, const auto& hit) { if (hit) ++contacts; });
    const bool singleContact = contacts == 1;
    std::cout << "arc=" << arc << " damping=" << damping << " replay=" << replay
              << " single_contact=" << singleContact << " quadratic=" << quadraticDrag
              << " quadratic_replay=" << quadraticReplay << " aim=" << aim << '\n';
    return arc && damping && replay && singleContact && quadraticDrag && quadraticReplay && aim ? 0 : 1;
}
