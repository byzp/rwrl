#pragma once

#include "../../world_internal.h"

namespace rwrl::custom_actions {

inline bool has_resource_delta(const CustomResourceDelta& delta) {
    return delta.credits != 0 || delta.energy != 0.0f ||
           delta.hp != 0.0f || delta.shield != 0.0f || delta.ammo != 0 ||
           delta.set_flags != 0 || delta.unset_flags != 0;
}

}  // namespace rwrl::custom_actions
