#include "MovementTrait.h"
#include "EntityManager.h"

#include <osg/Math>
#include <algorithm>
#include <cmath>

void MovementTrait::moveTo(osg::Vec3d destination) {
    destination.z() = 0;
    destination_ = destination;
    state_ = MovementState::Moving;
}

void MovementTrait::step(TransformComponent& transform, double dt) {
    if (state_ != MovementState::Moving || dt <= 0) return;
    const auto delta = *destination_ - transform.position;
    const double distance = std::hypot(delta.x(), delta.y());
    const double remaining = std::max(0., distance - tuning_.arrivalRadius);
    double wantedSpeed = 0;
    if (remaining > 0) {
        const double desiredHeading = -std::atan2(delta.x(), delta.y());
        const double error = std::remainder(desiredHeading - transform.heading, 2 * osg::PI);
        const double maxTurn = tuning_.turnRate * dt;
        const double turn = std::clamp(error, -maxTurn, maxTurn);
        transform.heading = std::remainder(transform.heading + turn, 2 * osg::PI);
        wantedSpeed = std::min(tuning_.maxSpeed, std::sqrt(2 * tuning_.deceleration * remaining)) *
                      std::max(0., std::cos(error - turn));
    }

    const double previous = speed_;
    speed_ += std::clamp(wantedSpeed - speed_, -tuning_.deceleration * dt, tuning_.acceleration * dt);
    const osg::Vec3d forward(-std::sin(transform.heading), std::cos(transform.heading), 0);
    transform.position += forward * ((previous + speed_) * .5 * dt);
    if (remaining == 0 && speed_ == 0) state_ = MovementState::Arrived;
}
