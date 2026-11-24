#pragma once

// Independent three-dimensional, fixed-dt implementation of the atmospheric
// quadratic-drag equations documented in third_party/wows_shell/shellCPP.hpp.
// The upstream batch calculator cannot advance one live projectile at a time;
// this adapter evaluates the same model per tick. No game-time scaling.
#include <array>
#include <cmath>
#include <algorithm>

namespace ocean_gambit::ballistics {

struct Vector3 {
    double x = 0, y = 0, z = 0;
    Vector3 operator+(Vector3 b) const { return {x+b.x, y+b.y, z+b.z}; }
    Vector3 operator-(Vector3 b) const { return {x-b.x, y-b.y, z-b.z}; }
    Vector3 operator*(double k) const { return {x*k, y*k, z*k}; }
    Vector3& operator+=(Vector3 b) { x+=b.x; y+=b.y; z+=b.z; return *this; }
    double length() const { return std::sqrt(x*x+y*y+z*z); }
};

struct MotionState {
    Vector3 position, velocity;
};

struct MotionDerivative {
    Vector3 position, velocity;
    MotionDerivative operator+(MotionDerivative b) const {
        return {position+b.position, velocity+b.velocity};
    }
    MotionDerivative operator*(double k) const {
        return {position*k, velocity*k};
    }
};

enum class Integrator { ForwardEuler, RungeKutta2, RungeKutta4, AdamsBashforth5 };

struct MotionHistory {
    // Derivatives at four preceding timestamps, newest first.
    std::array<MotionDerivative, 4> previous{};
    unsigned count = 0;
};

struct MotionParameters {
    double caliberMeters = 0.1524;
    double massKg = 45;
    double dragCoefficient = 0.3;
    double gravity = 9.8;
};

inline double airDensity(double heightMeters) {
    // ISA troposphere. Clamp outside its region of validity; live shells should
    // not be modeled above 11 km with this simplified atmosphere.
    constexpr double t0 = 288.15, lapse = 0.0065, p0 = 101325.0;
    constexpr double gasConstant = 8.31447, molarMass = 0.0289644, gravity = 9.8;
    const double h = std::clamp(heightMeters, 0.0, 11000.0);
    const double t = t0 - lapse * h;
    const double pressure = p0 * std::pow(t / t0, gravity * molarMass / (gasConstant * lapse));
    return pressure * molarMass / (gasConstant * t);
}

inline MotionDerivative derivative(const MotionState& s, const MotionParameters& p) {
    constexpr double pi = 3.14159265358979323846;
    const double area = pi * 0.25 * p.caliberMeters * p.caliberMeters;
    const double k = 0.5 * p.dragCoefficient * area / p.massKg;
    const double factor = airDensity(s.position.z) * k * s.velocity.length();
    return {s.velocity, s.velocity * (-factor) + Vector3{0, 0, -p.gravity}};
}

inline MotionState displaced(MotionState s, MotionDerivative d, double dt) {
    s.position += d.position * dt;
    s.velocity += d.velocity * dt;
    return s;
}

inline MotionState rk4(MotionState s, const MotionParameters& p, double dt) {
    auto k1 = derivative(s, p);
    auto k2 = derivative(displaced(s, k1, dt * .5), p);
    auto k3 = derivative(displaced(s, k2, dt * .5), p);
    auto k4 = derivative(displaced(s, k3, dt), p);
    auto combined = (k1 + k2 * 2 + k3 * 2 + k4) * (1.0 / 6.0);
    return displaced(s, combined, dt);
}

inline void advance(MotionState& s, MotionHistory& history,
                    const MotionParameters& params, Integrator method, double dt) {
    const MotionDerivative d = derivative(s, params);
    MotionState next;
    if (method == Integrator::ForwardEuler) {
        next = displaced(s, d, dt);
    } else if (method == Integrator::RungeKutta2) {
        // Improved Euler / Heun; agrees with upstream's two evaluations.
        auto d2 = derivative(displaced(s, d, dt), params);
        next = displaced(s, (d + d2) * .5, dt);
    } else if (method == Integrator::RungeKutta4) {
        next = rk4(s, params, dt);
    } else if (history.count < 4) {
        // Adams-Bashforth 5 needs four earlier steps; bootstrap using RK4.
        next = rk4(s, params, dt);
    } else {
        const auto sum = d * 1901.0 +
            history.previous[0] * (-2774.0) +
            history.previous[1] * 2616.0 +
            history.previous[2] * (-1274.0) +
            history.previous[3] * 251.0;
        next = displaced(s, sum, dt / 720.0);
    }
    if (method == Integrator::AdamsBashforth5) {
        for (int i = 3; i > 0; --i) history.previous[i] = history.previous[i-1];
        history.previous[0] = d;
        if (history.count < 4) ++history.count;
    }
    s = next;
}

} // namespace ocean_gambit::ballistics
