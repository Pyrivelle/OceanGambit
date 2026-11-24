#include "../battle/WowsShellAdapter.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

using ocean_gambit::ballistics::WowsShellAmmo;
using ocean_gambit::ballistics::WowsShellIntegrator;
using ocean_gambit::ballistics::sampleWowsShell;

int main() {
    try {
        const WowsShellAmmo ammo{};
        const auto euler = sampleWowsShell(ammo, WowsShellIntegrator::ForwardEuler,
                                          2.0, 10.0, 2.0);
        const auto rk4 = sampleWowsShell(ammo, WowsShellIntegrator::RungeKutta4,
                                        2.0, 10.0, 2.0);
        const bool samples = euler.size() == 5 && rk4.size() == euler.size();
        bool valid = samples;
        for (std::size_t i = 0; i < rk4.size(); ++i) {
            const auto& s = rk4[i];
            valid &= std::abs(s.launchAngleDeg - (2.0 + i * 2.0)) < 1e-6 &&
                     std::isfinite(s.rangeMeters) && s.rangeMeters > 0 &&
                     std::isfinite(s.flightSeconds) && s.flightSeconds > 0 &&
                     std::isfinite(s.impactSpeedMps) && s.impactSpeedMps > 0 &&
                     std::isfinite(s.rawPenetrationMm) && s.rawPenetrationMm >= 0;
        }
        bool rejected = false;
        try {
            sampleWowsShell(ammo, WowsShellIntegrator::ForwardEuler,
                            5.0, 1.0, 1.0);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        std::cout << "wows_shell_samples=" << samples
                  << " finite=" << valid
                  << " rejects_invalid=" << rejected << '\n';
        return valid && rejected ? 0 : 1;
    } catch (const std::exception& ex) {
        std::cerr << "wows_shell error: " << ex.what() << '\n';
        return 1;
    }
}
