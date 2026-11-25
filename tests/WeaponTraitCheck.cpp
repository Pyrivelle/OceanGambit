#include "../battle/WeaponTrait.h"

#include <cassert>

int main() {
    WeaponTrait gun({1000.0, 4.0});
    assert(gun.range() == 1000.0);
    assert(gun.ready(0));
    assert(!gun.target());
    gun.track(std::uint64_t{42});
    assert(gun.target() && *gun.target() == 42);
    gun.fired(0.5);
    assert(!gun.ready(4.49));
    assert(gun.ready(4.5));
    gun.track(std::nullopt);
    assert(!gun.target());
    return 0;
}
