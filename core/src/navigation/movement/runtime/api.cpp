#include "world_internal.h"

namespace rwrl {

void World::update_movement(f32 dt) {
    const Vec<World*> no_other_worlds;
    update_movement(dt, no_other_worlds);
}

void World::update_movement(f32 dt, const Vec<World*>& other_worlds) {
    update_unit_movement(dt, 0, other_worlds);
}

} // namespace rwrl
