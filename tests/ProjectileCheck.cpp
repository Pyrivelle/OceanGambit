#include "../battle/ProjectileSystem.h"

#include <cmath>
#include <iostream>
#include <vector>

namespace {
std::vector<osg::Vec3d> sample(ProjectileTuning tuning, unsigned seed, unsigned count, int steps) {
    ProjectileSystem system(tuning, seed);
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
    ProjectileTuning straight;
    straight.damping = straight.spread = 0;
    constexpr int steps = 100;
    const double dt = ProjectileSystem::interval;
    const auto position = sample(straight, 528, 1, steps).front();
    const double expectedDrop = straight.downPull * dt * dt * steps * (steps + 1) / 2;
    const bool arc = std::abs(position.x() - straight.launchPace * steps * dt) < 1e-8 &&
                     std::abs(position.z() + expectedDrop) < 1e-8;
    ProjectileTuning damped;
    damped.spread = 0;
    const bool damping = sample(damped, 528, 1, steps).front().x() < position.x();
    const auto spread = sample({}, 528, 8, steps);
    const bool replay = spread == sample({}, 528, 8, steps) &&
                        spread != sample({}, 529, 8, steps) && spread[0] != spread[1];

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
              << " single_contact=" << singleContact << '\n';
    return arc && damping && replay && singleContact ? 0 : 1;
}
