#include "../battle/WowsMotion.h"
#include <array>
#include <cmath>
#include <iostream>

namespace b = ocean_gambit::ballistics;

b::MotionState simulate(b::Integrator integrator, double dt, double seconds) {
    b::MotionState state{{0, 0, 15}, {300, 0, 50}};
    b::MotionHistory history;
    b::MotionParameters p{};
    for (int i = 0; i < static_cast<int>(std::round(seconds/dt)); ++i)
        b::advance(state, history, p, integrator, dt);
    return state;
}
int main() {
    const auto reference = simulate(b::Integrator::RungeKutta4, .001, 2);
    const auto euler = simulate(b::Integrator::ForwardEuler, .01, 2);
    const auto rk2 = simulate(b::Integrator::RungeKutta2, .01, 2);
    const auto rk4 = simulate(b::Integrator::RungeKutta4, .01, 2);
    const auto ab5 = simulate(b::Integrator::AdamsBashforth5, .01, 2);
    const auto difference = [&](const b::MotionState& state) {
        return (state.position - reference.position).length();
    };
    const bool atmosphere = b::airDensity(0) > b::airDensity(5000);
    const bool accurate = difference(rk4) < difference(euler) &&
                          difference(rk2) < difference(euler) &&
                          difference(ab5) < difference(euler);
    const bool finite = std::isfinite(rk4.position.x) &&
                        std::isfinite(rk4.position.z) &&
                        rk4.position.x > 0 && rk4.velocity.x < 300;
    std::cout << "euler_error=" << difference(euler)
              << " rk2_error=" << difference(rk2)
              << " rk4_error=" << difference(rk4)
              << " ab5_error=" << difference(ab5) << '\n';
    return atmosphere && accurate && finite ? 0 : 1;
}
