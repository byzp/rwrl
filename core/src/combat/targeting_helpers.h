#pragma once

namespace rwrl {
namespace {

inline bool within_turret_limiting_angle(const Unit& attacker,
                                        const WeaponSlot& slot,
                                        const Unit& target) {
    if (!attacker.custom_unit || slot.limiting_angle == -1.0f) return true;
    f32 reference = attacker.rotation;
    if (slot.attached_to_slot >= 0 &&
        slot.attached_to_slot < static_cast<i32>(attacker.weapon_slots.size())) {
        reference = attacker.weapon_slots[slot.attached_to_slot].turret_angle;
    }
    reference += slot.turret_idle_direction;
    const f32 bearing = jar_direction_degrees(
        attacker.x, attacker.y, target.x, target.y);
    return std::abs(shortest_angle_delta(reference, bearing)) <= slot.limiting_angle;
}

inline bool within_turret_limiting_range(const Unit& attacker,
                                        const WeaponSlot& slot,
                                        const Unit& target,
                                        bool independent_target = false) {
    const f32 distance_sq = dist2(
        attacker.x, attacker.y, target.x, target.y);
    if (!attacker.custom_unit) {
        if (!independent_target) return true;
        f32 range = attacker.weapon_range;
        if (attacker.attack_range_adds_radii) {
            range += attacker.radius + target.radius;
        }
        // y.a(i,target,false,true) uses h(target)'s strict boundary.
        return distance_sq < range * range;
    }
    // custom.j.a(bn,...) always applies the effective range to independent
    // targets. For R itself, only an explicitly limited turret checks it.
    if (slot.limiting_range >= 99999.0f && !independent_target) return true;
    const f32 range = slot.limiting_range >= 99999.0f
        ? attacker.weapon_range : slot.limiting_range;
    return distance_sq <= range * range;
}

inline bool turret_target_allowed(const Unit& attacker,
                                  const WeaponSlot& slot,
                                  const Unit& target,
                                  bool independent_target = false) {
    return within_turret_limiting_range(attacker, slot, target, independent_target) &&
           within_turret_limiting_angle(attacker, slot, target);
}

inline bool target_layer_allowed(const World& world,
                                 const Unit& attacker, const Unit& target,
                                 bool targets_air, bool targets_ground,
                                 bool targets_underwater = false,
                                 bool targets_not_touching_water = true) {
    if (!target.custom_unit && target.unit_type == "fogRevealer") return false;
    if (attacker.custom_unit) {
        if (target.airborne) return targets_air;
        if (target.is_submerged()) return targets_underwater;
        if (!targets_not_touching_water) {
            // am.cH() admits surface objects on water only up to height 2.
            if (target.height > 2.0f) return false;
            const f32 inverse_tile_size = 1.0f / world.tile_size();
            const i32 tx = static_cast<i32>(target.x * inverse_tile_size);
            const i32 ty = static_cast<i32>(target.y * inverse_tile_size);
            if (tx < 0 || ty < 0 || tx >= world.map_width() ||
                ty >= world.map_height()) return false;
            const Pathfinder& pathfinder = world.pathfinder();
            if (pathfinder.is_passable_ignoring_buildings(
                    tx, ty, MoveType::OVER_CLIFF) ||
                !pathfinder.is_passable_ignoring_buildings(
                    tx, ty, MoveType::WATER)) return false;
        }
        return targets_ground;
    }
    const bool amphibious_submersible =
        attacker.unit_type == "attackSubmarine" ||
        attacker.unit_type == "amphibiousJet";
    // missileShip targets submerged units while surfaced. The amphibious
    // units expose that capability only while they are submerged themselves.
    const bool can_target_submerged =
        attacker.unit_type == "missileShip" ||
        (amphibious_submersible && attacker.is_submerged());
    if (target.is_submerged()) {
        return can_target_submerged && targets_ground;
    }
    if (amphibious_submersible && attacker.is_submerged()) {
        // y.k(am) uses am.cH() when ah() is false. Surface objects on
        // native water tiles remain valid underwater targets up to height 2.
        if (target.airborne || target.height > 2.0f) return false;
        const f32 inverse_tile_size = 1.0f / world.tile_size();
        const i32 tx = static_cast<i32>(target.x * inverse_tile_size);
        const i32 ty = static_cast<i32>(target.y * inverse_tile_size);
        if (tx >= 0 && ty >= 0 && tx < world.map_width() &&
            ty < world.map_height()) {
            const Pathfinder& pathfinder = world.pathfinder();
            if (pathfinder.is_passable_ignoring_buildings(
                    tx, ty, MoveType::OVER_CLIFF) ||
                !pathfinder.is_passable_ignoring_buildings(
                    tx, ty, MoveType::WATER)) {
                return false;
            }
        }
        return targets_ground;
    }
    if (target.airborne) return targets_air;
    return targets_ground;
}

struct AttackTargetWeaponEligibility {
    bool allowed_now;
    bool allowed_without_range;
};

inline AttackTargetWeaponEligibility attack_target_weapon_eligibility(
        const World& world, const Unit& attacker, const Unit& target) {
    if (!attacker.custom_unit) return {true, true};
    // y.i(target) and y.j(target) check shooting slots and their linked
    // parents. Only j ignores range. A target admitted by j but not i must
    // still be approached from inside the unit's ordinary stopping radius.
    const auto eligible = [&](const WeaponSlot& slot, bool ignore_range) {
        return (ignore_range || within_turret_limiting_range(attacker, slot, target)) &&
            within_turret_limiting_angle(attacker, slot, target) &&
            target_layer_allowed(world, attacker, target, slot.targets_air,
                slot.targets_ground, slot.targets_underwater, slot.targets_not_touching_water);
    };
    bool compatible_without_range = false;
    for (const WeaponSlot& slot : attacker.weapon_slots) {
        if (!slot.fire_enabled) continue;
        const WeaponSlot* parent = slot.linked_slot >= 0 &&
            slot.linked_slot < static_cast<i32>(attacker.weapon_slots.size())
            ? &attacker.weapon_slots[slot.linked_slot] : nullptr;
        if (eligible(slot, false) && (!parent || eligible(*parent, false))) return {true, true};
        if (eligible(slot, true) && (!parent || eligible(*parent, true))) {
            compatible_without_range = true;
        }
    }
    return {false, compatible_without_range};
}

inline bool attack_target_needs_approach(const World& world,
                                       const Unit& attacker,
                                       const Unit& target) {
    const auto eligibility = attack_target_weapon_eligibility(world, attacker, target);
    return !eligibility.allowed_now && eligibility.allowed_without_range;
}

inline void set_custom_reload_frames(f32 frames, i32& ticks,
                                     f32& remainder) {
    frames = std::max(0.0f, frames);
    ticks = static_cast<i32>(std::ceil(frames));
    // Keep the exact JAR ap.e value alongside its integer presentation. The
    // old complement representation became stranded when ticks reached zero,
    // allowing a weapon with a small fractional reload to fire one frame
    // early.
    remainder = frames;
}

inline void set_custom_reload(f32 base, f32 multiplier,
                              i32& ticks, f32& remainder) {
    set_custom_reload_frames(base * multiplier, ticks, remainder);
}

} // namespace
} // namespace rwrl
