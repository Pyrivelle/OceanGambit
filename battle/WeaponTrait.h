#pragma once

#include <cstdint>
#include <optional>

// Weapon ownership/state belongs to an entity, not to its OSG model.
// A target ID is revalidated before every shot; it is never a raw pointer.
struct WeaponTuning {
    double engagementRange = 1000.0; // metres, horizontal distance
    double reloadSeconds = 4.0;
};

class WeaponTrait {
public:
    explicit WeaponTrait(WeaponTuning tuning) : tuning_(tuning) {}
    double range() const { return tuning_.engagementRange; }
    bool ready(double now) const { return now >= nextShotTime_; }
    std::optional<std::uint64_t> target() const { return target_; }
    void track(std::optional<std::uint64_t> target) { target_ = target; }
    void fired(double now) { nextShotTime_ = now + tuning_.reloadSeconds; }

private:
    WeaponTuning tuning_;
    std::optional<std::uint64_t> target_;
    double nextShotTime_ = 0;
};
