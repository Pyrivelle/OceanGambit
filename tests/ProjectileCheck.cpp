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

    // The inverse elevation must agree with the live integrator when
    // dispersion is disabled. Check height at the target range on descent.
    const auto checkAim = [](AmmoConfig ammo, double distance, double height) {
        ammo.spread = 0;
        ammo.lifetime = 30;
        ProjectileSystem system(ammo, 528);
        const auto solution = system.solveElevation(distance, height);
        if (!solution) return false;
        const double angle = *solution;
        system.launch(0, 0, {0, 0, 0},
                      {std::cos(angle), 0, std::sin(angle)}, {});
        osg::Vec3d last;
        bool haveLast = false, reached = false;
        double atRange = 0;
        for (int i = 0; i < 3000 && !reached; ++i) {
            system.step([](unsigned, const auto&, const auto&) { return std::nullopt; },
                        [&](unsigned, const osg::Vec3d& pos) {
                            if (haveLast && last.x() <= distance && pos.x() >= distance) {
                                const double u = (distance - last.x()) / (pos.x() - last.x());
                                atRange = last.z() + (pos.z() - last.z()) * u;
                                reached = true;
                            }
                            last = pos;
                            haveLast = true;
                        }, [](unsigned, unsigned, const auto&) {});
        }
        return reached && std::abs(atRange - height) < 0.05;
    };
    auto legacyAim = AmmoConfig::exponentialDemo();
    legacyAim.lifetime = 30;
    const bool inverseLegacy = checkAim(legacyAim, 1000, -6);
    auto wowsAim = AmmoConfig::wowsDemo();
    const bool inverseWows = checkAim(wowsAim, 1000, -6);
    const bool unreachable = !ProjectileSystem(wowsAim).solveElevation(1000000, 0).has_value();

    // Two branches of the same physical trajectory. Disable atmospheric drag
    // and allow sufficient lifetime so the high branch can reach the target.
    auto doubleArc = AmmoConfig::quadraticDemo();
    doubleArc.dragK = 0;
    doubleArc.spread = 0;
    doubleArc.lifetime = 180;
    ProjectileSystem solver(doubleArc);
    const auto lowArc = solver.solveElevation(1000, 0, FireArc::Low, 0.0, 1.55);
    const auto highArc = solver.solveElevation(1000, 0, FireArc::High, 0.0, 1.55);
    const bool twoArcs = lowArc && highArc && *lowArc < *highArc &&
        *lowArc < .1 && *highArc > 1.4 &&
        std::abs(*lowArc + *highArc - 1.5707963267948966) < .02;
    const auto restrictedHigh = solver.solveElevation(1000, 0, FireArc::High, 0.0, .6);
    // A high-arc profile cannot silently fall back to the low branch.
    const bool restricted = !restrictedHigh.has_value();
    const auto shortLife = AmmoConfig::wowsDemo();
    const bool noLongRange = !ProjectileSystem(shortLife).solveElevation(
        1000000, 0, FireArc::High, 0.0, 1.55).has_value();

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
              << " quadratic_replay=" << quadraticReplay << " aim=" << aim
              << " inverse_legacy=" << inverseLegacy
              << " inverse_wows=" << inverseWows
              << " unreachable=" << unreachable << " two_arcs=" << twoArcs
              << " restricted=" << restricted << " no_long_range=" << noLongRange << '\n';
    return arc && damping && replay && singleContact && quadraticDrag && quadraticReplay && aim && inverseLegacy && inverseWows && unreachable && twoArcs && restricted && noLongRange ? 0 : 1;
}
