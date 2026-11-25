#pragma once

#include <osg/Vec3d>

#include <optional>

struct TransformComponent;

struct MovementTuning {
    double maxSpeed;       // m/s
    double acceleration;   // m/s^2
    double deceleration;   // m/s^2
    double turnRate;       // radians/s
    double arrivalRadius;  // m
};

enum class MovementState { Idle, Moving, Arrived };

class MovementTrait {
public:
    explicit MovementTrait(MovementTuning tuning) : tuning_(tuning) {}
    void moveTo(osg::Vec3d destination);
    void step(TransformComponent& transform, double dt);
    MovementState state() const { return state_; }
    double speed() const { return speed_; }
    const std::optional<osg::Vec3d>& destination() const { return destination_; }

private:
    MovementTuning tuning_;
    MovementState state_ = MovementState::Idle;
    std::optional<osg::Vec3d> destination_;
    double speed_ = 0;
};
