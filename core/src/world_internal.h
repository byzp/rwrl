#pragma once
#include <rwrl/world.h>

namespace rwrl {

inline static f32 jar_direction_degrees(f32 from_x, f32 from_y,
                                        f32 to_x, f32 to_y);
inline static f32 jar_sine_degrees(f32 angle);
inline static f32 jar_cosine_degrees(f32 angle);

inline static f32 jar_build_coordinate(f32 coordinate, f32 offset,
                                      f32 tile_size) {
    // b.b(float,float) snaps the provisional building, using the rounded
    // float reciprocal before truncating its tile coordinate toward zero.
    const i32 tile = static_cast<i32>(
        (coordinate - offset + 1.0f) * (1.0f / tile_size));
    return static_cast<f32>(tile * static_cast<i32>(tile_size)) + offset;
}

inline static f32 approach_value(f32 value, f32 target, f32 amount) {
    // f.a(value, target, amount) tests the rounded target +/- amount
    // boundaries before adding the step. Clamping the rounded result
    // instead can leave ce a few ULPs past the target at an exact boundary.
    if (value > target + amount) return value - amount;
    if (value < target - amount) return value + amount;
    return target;
}

// l.bJ, the map's global seed. game.i seeds it from ad.ay.q when the level
// loads and never touches it again, so every f.a(w,...) draw in a game is
// offset by this one constant. It is a host-side input like the unit
// assets: the core never reads the map or replay itself.
inline static i32 deterministic_unit_random_int(
        const Unit& source, i32 world_tick, i32 minimum, i32 maximum,
        i32 seed_index) {
    if (minimum >= maximum) return minimum;

    // gameFramework.f.a(w,int,int,int). Java converts the accumulating int
    // to float before each coordinate term, so preserve those intermediate
    // roundings rather than summing separately truncated products.
    const u32 seed_clock = static_cast<u32>(world_tick + 1);
    i32 value = static_cast<i32>(
        static_cast<i64>(global_random_seed()) +
        static_cast<i64>(source.uid) * 1313LL);
    u32 wrapped = static_cast<u32>(value);
    value = static_cast<i32>(
        static_cast<f32>(value) + source.x * 13.0f);
    value = static_cast<i32>(
        static_cast<f32>(value) + source.y * 13.0f);
    value = static_cast<i32>(
        static_cast<f32>(value) + source.x * 130.0f);
    value = static_cast<i32>(
        static_cast<f32>(value) + source.y * 130.0f);
    wrapped = static_cast<u32>(value);
    const u32 salt = static_cast<u32>(source.deterministic_random_salt);
    wrapped += salt * 13131u;
    wrapped += salt * seed_clock;
    wrapped += static_cast<u32>(seed_index) * 133u *
               static_cast<u32>(maximum);
    wrapped += static_cast<u32>(seed_index) * source.uid +
               static_cast<u32>(seed_index);
    wrapped += static_cast<u32>(seed_index) * seed_clock * 1313u;
    wrapped += seed_clock * 13u + seed_clock % 10u;

    i32 remainder = static_cast<i32>(wrapped) % (maximum - minimum);
    if (remainder < 0) remainder = -remainder;
    return remainder + minimum;
}

inline static f32 deterministic_unit_random(
        const Unit& source, i32 world_tick, f32 minimum, f32 maximum,
        i32 seed_index) {
    const i32 min_scaled = static_cast<i32>(minimum * 1000.0f);
    const i32 max_scaled = static_cast<i32>(maximum * 1000.0f);
    return static_cast<f32>(deterministic_unit_random_int(
        source, world_tick, min_scaled, max_scaled, seed_index)) * 0.001f;
}

inline static void refresh_weapon_aim_offset(
        Unit& unit, WeaponSlot& slot, const Unit* target,
        i32 world_tick, i32 slot_index) {
    slot.aim_offset_x = 0.0f;
    slot.aim_offset_y = 0.0f;
    if (!target || slot.aim_offset_spread == 0.0f) return;

    const f32 spread = target->radius * slot.aim_offset_spread;
    const i32 minimum = static_cast<i32>(-spread);
    const i32 maximum = static_cast<i32>(spread);
    slot.aim_offset_x = static_cast<f32>(deterministic_unit_random_int(
        unit, world_tick, minimum, maximum, slot_index + 1));
    slot.aim_offset_y = static_cast<f32>(deterministic_unit_random_int(
        unit, world_tick, minimum, maximum, slot_index + 2));
}

inline static void refresh_all_weapon_aim_offsets(
        Unit& unit, const Unit* target, i32 world_tick) {
    for (i32 i = 0; i < static_cast<i32>(unit.weapon_slots.size()); ++i) {
        refresh_weapon_aim_offset(
            unit, unit.weapon_slots[static_cast<std::size_t>(i)],
            target, world_tick, i);
    }
}

inline static f32 shortest_angle_delta(f32 from, f32 to) {
    // gameFramework.f.c(a,b,360) reduces each operand before subtracting;
    // doing the subtraction first changes the final ULP near the +/-180
    // wrap, which matters when a turn snaps exactly to its target.
    const f32 normalized_from = std::fmod(from, 360.0f);
    const f32 normalized_to = std::fmod(to, 360.0f);
    f32 delta = normalized_to - normalized_from;
    if (delta > 180.0f) delta -= 360.0f;
    if (delta < -180.0f) delta += 360.0f;
    return delta;
}

inline static f32 turn_toward(Unit& unit, f32 desired, f32 delta_frames) {
    const f32 error = shortest_angle_delta(unit.rotation, desired);
    if (std::abs(error) < 0.01f || unit.turn_speed <= 0.0f) {
        // y.a(delta, desired, ...): reaching the 0.01-degree dead zone
        // returns without damping ce. The retained angular velocity matters
        // when the requested heading changes on a later update.
        return 0.0f;
    }
    const f32 direction = error > 0.0f ? 1.0f : -1.0f;
    f32 turn_amount = 0.0f;
    if (unit.turn_acceleration <= 0.0f) {
        turn_amount = direction * unit.turn_speed * delta_frames;
    } else {
        const f32 stopping_angle =
            std::abs(unit.turn_velocity) / unit.turn_acceleration;
        const f32 target_velocity = std::abs(error) < stopping_angle
            ? direction * unit.turn_acceleration
            : direction * unit.turn_speed;
        unit.turn_velocity = approach_value(
            unit.turn_velocity, target_velocity,
            unit.turn_acceleration * delta_frames);
        turn_amount = unit.turn_velocity * delta_frames;
    }
    if (std::abs(turn_amount) > std::abs(error)) {
        turn_amount = error;
        // y.a(FFZZ) never writes ce in its B()<=0 constant-rate branch.
        if (unit.turn_acceleration > 0.0f) unit.turn_velocity = 0.0f;
    }
    unit.rotation += turn_amount;
    if (unit.rotation > 180.0f) unit.rotation -= 360.0f;
    if (unit.rotation < -180.0f) unit.rotation += 360.0f;
    return turn_amount;
}

inline static f32 normalize_angle(f32 angle) {
    while (angle > 180.0f) angle -= 360.0f;
    while (angle < -180.0f) angle += 360.0f;
    return angle;
}

inline static f32 rotate_weapon_angle_with_body(f32 angle, f32 delta) {
    // y.i(float) reduces ap.a once in each direction. Accumulated aim
    // angles can span several turns; repeated reduction changes both the
    // next float addition and the signed lookup-table index at the muzzle.
    angle += delta;
    if (angle > 180.0f) angle -= 360.0f;
    if (angle < -180.0f) angle += 360.0f;
    return angle;
}

inline static void set_reversing_with_turret_delay(Unit& unit, bool reversing) {
    if (unit.reversing == reversing) return;
    // y.a(delta, desired, allowReverse, reverseAllowed) calls j(25) on
    // either ci transition. ap.a(25) extends only nonnegative idle delays.
    for (WeaponSlot& slot : unit.weapon_slots) {
        if (slot.turret_idle_timer >= 0.0f && slot.turret_idle_timer < 25.0f) {
            slot.turret_idle_timer = 25.0f;
        }
    }
    unit.reversing = reversing;
}

// y.a(delta, desired, slot) updates ap.c before applying the turn. The
// deceleration branch uses y(slot) both as its target speed and approach
// amount; using the acceleration there changes overshoot and firing frames.
inline static f32 turn_weapon_slot_toward(Unit& unit, WeaponSlot& slot, f32 desired,
                                          f32 delta_frames) {
    f32 error = shortest_angle_delta(slot.turret_angle, desired);
    if (error == 0.0f) return error;

    const f32 direction = error > 0.0f ? 1.0f : -1.0f;
    f32 turn_amount = 0.0f;
    if (slot.turret_turn_acceleration <= 0.0f) {
        turn_amount = std::clamp(
            error,
            -slot.turret_turn_speed * delta_frames,
            slot.turret_turn_speed * delta_frames);
    } else {
        const f32 deceleration = slot.turret_turn_deceleration;
        const f32 stopping_angle =
            std::abs(slot.turret_turn_velocity) / deceleration;
        const bool same_direction =
            (error > 0.0f) == (slot.turret_turn_velocity > 0.0f);
        if (std::abs(error) < stopping_angle && same_direction) {
            slot.turret_turn_velocity = approach_value(
                slot.turret_turn_velocity,
                direction * deceleration,
                deceleration * delta_frames);
        } else {
            slot.turret_turn_velocity = approach_value(
                slot.turret_turn_velocity,
                direction * slot.turret_turn_speed,
                slot.turret_turn_acceleration * delta_frames);
        }
        turn_amount = slot.turret_turn_velocity * delta_frames;
        if (std::abs(turn_amount) > std::abs(error)) {
            slot.turret_turn_velocity = 0.0f;
            turn_amount = error;
        }
    }
    // y.a(int,float) adds directly to ap.a. Keep the accumulated angle in
    // its original representation; reducing it by 360 changes later float
    // additions even though the direction is geometrically equivalent.
    slot.turret_angle += turn_amount;
    if (unit.custom_unit) {
        // custom.j.a(index,amount) carries the same unrounded turn into
        // attached children and extends each nonnegative idle delay to two.
        const i32 index = static_cast<i32>(&slot - unit.weapon_slots.data());
        for (WeaponSlot& child : unit.weapon_slots) {
            if (child.attached_to_slot != index) continue;
            child.turret_angle += turn_amount;
            if (child.turret_idle_timer >= 0.0f && child.turret_idle_timer < 2.0f) {
                child.turret_idle_timer = 2.0f;
            }
        }
    }
    return error - turn_amount;
}

inline static f32 weapon_idle_direction(const Unit& unit,
                                        const WeaponSlot& slot,
                                        const Vec<WeaponSlot>& slots) {
    f32 direction = unit.rotation;
    const i32 parent = slot.attached_to_slot >= 0
        ? slot.attached_to_slot : slot.linked_slot;
    if (parent >= 0 && parent < static_cast<i32>(slots.size())) {
        direction = slots[parent].turret_angle;
    }
    const bool using_reverse_idle =
        unit.reversing && unit.reverse_speed_multiplier > 0.95f;
    direction += using_reverse_idle
        ? slot.turret_idle_direction_reversing
        : slot.turret_idle_direction;
    // custom.j.C(i) returns the direct float sum.  y.a(float) compares that
    // representation before using shortest-angle turning, so normalizing it
    // here can change when the idle timer starts.
    return direction;
}

// custom.j.b(int,float) handles idle spin/sweep before y.a() considers the
// ordinary turret reset target. It returns the shouldResetTurret value only
// when no active custom idle behavior consumed the update.
inline static bool update_weapon_idle_behavior(
        Unit& unit, WeaponSlot& slot, i32 slot_index, f32 game_frames,
        i32 world_tick, const Vec<WeaponSlot>& slots,
        bool movement_prediction) {
    const bool sweep_condition =
        !slot.turret_idle_sweep_requires_stationary ||
        !movement_prediction;
    if (slot.turret_idle_sweep_angle != 0.0f && sweep_condition) {
        f32 sweep_base = 0.0f;
        if (slot.turret_idle_spin != 0.0f) {
            sweep_base = slot.turret_angle;
        } else if (!slot.turret_resets_to_idle) {
            sweep_base = slot.turret_previous_angle;
        } else {
            sweep_base = weapon_idle_direction(unit, slot, slots);
        }

        slot.turret_sweep_timer += game_frames;
        const f32 sweep_frames =
            game_frames * slot.turret_idle_sweep_speed;
        const f32 infinity = std::numeric_limits<f32>::infinity();
        if (slot.turret_sweep_direction > 0.0f) {
            if (slot.turret_sweep_direction < infinity &&
                turn_weapon_slot_toward(
                    unit, slot, sweep_base + slot.turret_sweep_direction,
                    sweep_frames) == 0.0f) {
                slot.turret_sweep_direction = infinity;
            }
        } else if (slot.turret_sweep_direction > -infinity &&
                   turn_weapon_slot_toward(
                       unit, slot, sweep_base + slot.turret_sweep_direction,
                       sweep_frames) == 0.0f) {
            slot.turret_sweep_direction = -infinity;
        }

        if (slot.turret_sweep_timer > slot.turret_idle_sweep_delay) {
            const i32 random_delay = static_cast<i32>(
                slot.turret_idle_sweep_random_delay);
            slot.turret_sweep_timer = -static_cast<f32>(
                deterministic_unit_random_int(
                    unit, world_tick, 0, random_delay, 0));
            f32 sweep_angle = slot.turret_idle_sweep_angle;
            if (slot.turret_idle_sweep_random_angle > 0.0f) {
                sweep_angle += deterministic_unit_random(
                    unit, world_tick, 0.0f,
                    slot.turret_idle_sweep_random_angle, slot_index);
            }
            slot.turret_sweep_direction =
                slot.turret_sweep_direction > 0.0f
                    ? -sweep_angle : sweep_angle;
        }
        return false;
    }

    if (slot.turret_idle_spin != 0.0f) {
        slot.turret_angle += slot.turret_idle_spin * game_frames;
        if (slot.turret_angle > 180.0f) slot.turret_angle -= 360.0f;
        if (slot.turret_angle < -180.0f) slot.turret_angle += 360.0f;
        return false;
    }
    return slot.turret_resets_to_idle;
}

inline static f32 weapon_recoil_distance(const Unit& unit, const WeaponSlot& slot) {
    if (slot.cooldown_remainder <= 0.0f || slot.recoil_offset == 0.0f ||
        slot.fire_delay_frames <= 0.0f ||
        (slot.recoil_out_frames <= 0.0f &&
         slot.recoil_return_frames <= 0.0f)) {
        return 0.0f;
    }

    // custom.j.a(slot,false) obtains b(slot)+t(slot)-ap.e, including
    // the unit's delay multiplier and warmup transfer in the full reload.
    // Keep every intermediate as float, matching strictfp JAR arithmetic.
    const f32 warmup_transfer =
        slot.warmup_shoot_delay_transfer > 0.0f && slot.warmup_frames > 0.0f
        ? slot.warmup_shoot_delay_transfer *
              (slot.warmup_progress / slot.warmup_frames)
        : 0.0f;
    const f32 elapsed = slot.fire_delay_frames * unit.shoot_delay_multiplier -
                        warmup_transfer - slot.cooldown_remainder;
    f32 recoil = 0.0f;
    if (slot.recoil_out_frames > 0.0f && elapsed < slot.recoil_out_frames) {
        recoil = elapsed / slot.recoil_out_frames * slot.recoil_offset;
    } else if (slot.recoil_return_frames > 0.0f &&
               elapsed < slot.recoil_out_frames +
                            slot.recoil_return_frames) {
        recoil = slot.recoil_offset -
            (elapsed - slot.recoil_out_frames) /
                slot.recoil_return_frames * slot.recoil_offset;
    }
    return recoil;
}

inline static std::pair<f32, f32> custom_weapon_mount_origin(
        const Unit& unit, i32 slot_index) {
    const WeaponSlot& slot = unit.weapon_slots.at(slot_index);
    f32 x = unit.x;
    f32 y = unit.y;
    f32 mount_angle = unit.rotation;
    if (slot.attached_to_slot >= 0 &&
        slot.attached_to_slot < static_cast<i32>(unit.weapon_slots.size())) {
        const auto parent = custom_weapon_mount_origin(unit, slot.attached_to_slot);
        x = parent.first;
        y = parent.second;
        mount_angle = unit.weapon_slots[slot.attached_to_slot].turret_angle;
    }
    const f32 recoil = weapon_recoil_distance(unit, slot);
    if (recoil != 0.0f) {
        x += jar_cosine_degrees(slot.turret_angle) * recoil;
        y += jar_sine_degrees(slot.turret_angle) * recoil;
    }
    if (slot.mount_forward != 0.0f || slot.mount_side != 0.0f) {
        x += (jar_cosine_degrees(mount_angle) * slot.mount_forward -
              jar_sine_degrees(mount_angle) * slot.mount_side);
        y += (jar_sine_degrees(mount_angle) * slot.mount_forward +
              jar_cosine_degrees(mount_angle) * slot.mount_side);
    }
    return {x, y};
}

inline static std::pair<f32, f32> custom_weapon_muzzle_origin(
        const Unit& unit, i32 slot_index) {
    // custom.j.E(i) copies D(i), which extends F(i)'s mount/recoil origin.
    const WeaponSlot& slot = unit.weapon_slots.at(slot_index);
    auto origin = custom_weapon_mount_origin(unit, slot_index);
    const f32 angle = unit.attack_follow_e_flag
        ? unit.rotation : slot.turret_angle;
    const f32 side = slot.muzzle_side +
        (slot.odd_shot ? slot.muzzle_side_odd : 0.0f);
    const f32 cosine = jar_cosine_degrees(angle);
    const f32 sine = jar_sine_degrees(angle);
    origin.first += cosine * slot.muzzle_offset - sine * side;
    origin.second += sine * slot.muzzle_offset + cosine * side;
    return origin;
}

inline static std::pair<f32, f32> amphibious_jet_weapon_origin(
        const Unit& unit, f32 base_angle, i32 slot_index) {
    // b.c.G(i) uses the independent cL[2] base angle and the current wing
    // fold, not cg. Preserve d(false)'s add/subtract and the three rounded
    // translations in a(i,false) followed by G(i).
    const f32 angle = (base_angle + 90.0f) - 90.0f;
    const f32 forward_fold = std::clamp(
        unit.wing_fold_progress * 4.0f, 0.0f, 1.0f);
    const f32 side_fold = std::clamp(
        unit.wing_fold_progress * 2.0f - 1.0f, 0.0f, 1.0f);
    const f32 forward = 7.0f - 5.0f * forward_fold;
    const f32 side = 12.0f - 5.0f * side_fold;
    const f32 side_angle = angle + static_cast<f32>(-90 + 180 * slot_index);
    f32 x = unit.x + jar_cosine_degrees(angle) * forward;
    f32 y = unit.y + jar_sine_degrees(angle) * forward;
    x += jar_cosine_degrees(side_angle) * side;
    y += jar_sine_degrees(side_angle) * side;
    x += jar_cosine_degrees(angle) * 5.0f;
    y += jar_sine_degrees(angle) * 5.0f;
    return {x, y};
}

inline static bool aim_weapon_slot(Unit& unit, WeaponSlot& slot, Unit& target,
                            f32 delta_frames, const Vec<WeaponSlot>& slots,
                            const Projectile* beam = nullptr) {
    const bool linked = slot.linked_slot >= 0 &&
        slot.linked_slot < static_cast<i32>(slots.size());
    if (linked) {
        // custom.j slaves inherit the parent's angle and use u(i), which is
        // the parent's ap.g, as their aim gate. Their own mount is used only
        // by G(slot) when the projectile is launched.
        slot.turret_angle = slots[slot.linked_slot].turret_angle;
        return slots[slot.linked_slot].aim_ready;
    }
    const f32 recoil = weapon_recoil_distance(unit, slot);
    // The desired angle is measured from y.G(slot), the same turret origin
    // E(slot) launches the projectile from — so it carries the turret's
    // mount offset, not just the hull centre. Measured on c_helicopter at
    // new-replay 2794: its turret slews the full 15 degrees toward the
    // mount-relative bearing where the hull-relative one was already reached.
    // y.G(slot) adds recoil to the hull origin before stock subclasses add
    // their mount. custom.j.a(slot,false) uses the same order. The rounded
    // origin can decide whether angular acceleration or braking is used.
    f32 origin_x = unit.x;
    f32 origin_y = unit.y;
    if (recoil != 0.0f) {
        origin_x += jar_cosine_degrees(slot.turret_angle) * recoil;
        origin_y += jar_sine_degrees(slot.turret_angle) * recoil;
    }
    origin_x = origin_x +
        jar_cosine_degrees(unit.rotation) * slot.mount_forward -
        jar_sine_degrees(unit.rotation) * slot.mount_side;
    origin_y = origin_y +
        jar_sine_degrees(unit.rotation) * slot.mount_forward +
        jar_cosine_degrees(unit.rotation) * slot.mount_side;
    const i32 slot_index = static_cast<i32>(&slot - slots.data());
    if (unit.custom_unit) {
        // custom.j.G(slot) uses the attached parent even for independently
        // aimed turrets. Aim from the same native mount used for firing.
        const auto origin = custom_weapon_mount_origin(unit, slot_index);
        origin_x = origin.first;
        origin_y = origin.second;
    } else if (unit.unit_type == "amphibiousJet" && slot_index < 2 &&
        slots.size() > 2) {
        const auto origin = amphibious_jet_weapon_origin(
            unit, slots[2].turret_angle, slot_index);
        origin_x = origin.first;
        origin_y = origin.second;
    }
    f32 aim_offset_x = slot.aim_offset_x;
    f32 aim_offset_y = slot.aim_offset_y;
    if (beam) {
        // e.c.K(slot) adds its retained game.f.K/L to y.K(slot) before the
        // target position is added, even while changing to another target.
        aim_offset_x += beam->target_offset_x;
        aim_offset_y += beam->target_offset_y;
    }
    const f32 desired = jar_direction_degrees(
        origin_x, origin_y,
        target.x + aim_offset_x,
        target.y + aim_offset_y);
    f32 remaining_error = 0.0f;
    if (slot.fixed_turret) {
        const f32 error = shortest_angle_delta(unit.rotation, desired);
        if (delta_frames > 0.0f) {
            const f32 body_delta = turn_toward(unit, desired, delta_frames);
            // y.c(delta,desired) calls y.i() for the body turn. Only bm()
            // carries that delta into ap.a; ap.c remains the independently
            // updated turret velocity, not the body's ce.
            if (body_delta != 0.0f) {
                for (WeaponSlot& turret : unit.weapon_slots) {
                    if (turret.turret_rotate_with_body) {
                        turret.turret_angle = rotate_weapon_angle_with_body(
                            turret.turret_angle, body_delta);
                    }
                }
            }
        }
        slot.turret_previous_angle = slot.turret_angle;
        // c() returns its pre-turn error; y.b() uses the strict x(i) gate.
        return std::abs(error) < slot.aim_tolerance;
    } else {
        if (slot.turret_idle_timer >= 0.0f &&
            slot.turret_idle_timer < 70.0f) {
            slot.turret_idle_timer = 70.0f;
        }
        slot.turret_previous_angle = slot.turret_angle;
        if (delta_frames > 0.0f) {
            // y.a(delta, desired, slot) integrates angular acceleration once
            // over the native delta. Subdividing it changes the angle even
            // when the final angular velocity agrees.
            remaining_error = turn_weapon_slot_toward(
                unit, slot, desired, delta_frames);
        } else {
            remaining_error = shortest_angle_delta(slot.turret_angle, desired);
        }
    }
    // y.b() uses the residual returned by a(delta,desired,slot), before
    // ap.a's rounded addition, and its x(slot) comparison is strict.
    return std::abs(remaining_error) < slot.aim_tolerance;
}

inline static void approach_velocity(Unit& unit, f32 target_x, f32 target_y,
                              f32 acceleration, f32 delta_frames) {
    const f32 max_speed_per_frame =
        unit.move_speed_per_frame();
    const f32 dx = target_x - unit.vel_x;
    const f32 dy = target_y - unit.vel_y;
    const f32 distance_sq = dx * dx + dy * dy;
    if (distance_sq > max_speed_per_frame * max_speed_per_frame) {
        // y.a(float) stores fA before damping. The strict stop test keeps
        // that distance; only its direction uses the damped components.
        // The 0.05 expression is evaluated in double and narrowed to float.
        unit.vel_x = static_cast<f32>(
            static_cast<double>(unit.vel_x) -
            static_cast<double>(unit.vel_x) * 0.05 *
                static_cast<double>(delta_frames));
        unit.vel_y = static_cast<f32>(
            static_cast<double>(unit.vel_y) -
            static_cast<double>(unit.vel_y) * 0.05 *
                static_cast<double>(delta_frames));
    }
    const f32 max_delta = acceleration * 1.41f * delta_frames;
    if (distance_sq < max_delta * max_delta) {
        unit.vel_x = target_x;
        unit.vel_y = target_y;
        return;
    }
    const f32 movement_angle = jar_direction_degrees(
        unit.vel_x, unit.vel_y, target_x, target_y);
    unit.vel_x += jar_cosine_degrees(movement_angle) * max_delta;
    unit.vel_y += jar_sine_degrees(movement_angle) * max_delta;
}

inline static i32 remaining_navigation_points(const Unit& unit) {
    // The ordered target is the only logical point when no av/aw path is
    // active. Stale queued storage must not widen the MOVE arrival bucket.
    if (!unit.using_path) return 1;
    const i32 queued = std::max(
        0, static_cast<i32>(unit.waypoints.size()) - unit.current_waypoint);
    return 1 + queued;
}

inline static bool jar_reverse_allowed(const Unit& unit, f32 distance_sq,
                                      i32 navigation_points) {
    const f32 multiplier = unit.reverse_speed_multiplier;
    if (multiplier > 0.95f) return true;

    // y.a(float, ad, au, boolean): the tiered reverse branches additionally
    // require ae=true and ah<=1. A multi-unit formation leader therefore
    // turns forward even when its short remaining path would otherwise make
    // reversing attractive.
    if (!unit.formation_is_leader || unit.formation_group_size > 1) {
        return false;
    }
    // y tests aw before aJ() consumes this frame's node. The split order
    // pass may already have removed it, so use the captured movement count.
    if (navigation_points <= 0) return false;
    if (multiplier > 0.87f) return navigation_points <= 9 && distance_sq < 250000.0f;
    if (multiplier > 0.70f) return navigation_points <= 4 && distance_sq < 40000.0f;
    if (multiplier > 0.40f) return navigation_points <= 2 && distance_sq < 10000.0f;
    return false;
}

// am.bR() assigns collision layers independently of ownership: ground is 1,
// submerged is 2, and airborne is 3. Objects on different layers never reach
// y.a(am,float), so they must not push each other or trigger tree collisions.
inline static i32 jar_collision_layer(const Unit& unit) {
    // am.bR() uses the independent bT collision bit. Stock and custom
    // buildings clear bT even though they remain active simulation objects;
    // incomplete mobile units retain it. Death does not clear bT or recompute
    // bU, so a unit killed after a cached candidate list was built still
    // participates in that frame's pair resolution. cN
    // (transport/attachment) forces the layer off regardless of movement type.
    if (!unit.collision_enabled || unit.is_building || unit.parent_uid != 0) return -1;
    if (unit.airborne) return 3;
    // custom.j.Q() reads eq inclusively at -1; stock submersibles update
    // their separate runtime flag with a strict height < -1 test.
    if (unit.custom_unit ? unit.height <= -1.0f : unit.submerged) return 2;
    return 1;
}

inline static f32 jar_movement_turn_tolerance(const Unit& unit, f32 distance_sq) {
    f32 tolerance = 20.0f;
    if (distance_sq > 361.0f) tolerance = 46.0f;
    if (distance_sq > 3600.0f) tolerance = 89.0f;
    // y.a(float,ad,au,boolean) promotes these fields to double before
    // comparing. The native float 1.4 is just below the double threshold.
    if (unit.turn_speed < 1.4) {
        tolerance = distance_sq > 6400.0f ? tolerance * 0.5f : 17.0f;
    }
    if (unit.move_speed_per_frame() > 5.0f &&
        std::abs(unit.move_speed_factor) < 0.01) {
        tolerance = 1.0f;
    }
    if (unit.turn_speed < 1.1) tolerance *= 0.7f;
    if (unit.move_speed_factor > 0.4 && distance_sq > 16900.0f) {
        tolerance = 180.0f;
    }
    if (unit.move_ignores_body) tolerance = 181.0f;
    return tolerance;
}

inline static f32 jar_sine_degrees(f32 angle) {
    const i32 index = static_cast<i32>(angle * 22.755556f) & 8191;
    const f32 radians =
        (static_cast<f32>(index) + 0.5f) / 8192.0f * 6.2831855f;
    return static_cast<f32>(std::sin(static_cast<double>(radians)));
}

inline static f32 jar_cosine_degrees(f32 angle) {
    const i32 index = static_cast<i32>(angle * 22.755556f) & 8191;
    const f32 radians =
        (static_cast<f32>(index) + 0.5f) / 8192.0f * 6.2831855f;
    return static_cast<f32>(std::cos(static_cast<double>(radians)));
}

inline static f32 jar_atan_lookup(i32 index) {
    const f32 ratio = static_cast<f32>(index) / 1024.0f;
    return static_cast<f32>(
        std::atan(static_cast<double>(ratio)) *
        3.1415927410125732 / 3.14159265358979323846);
}

inline static i32 jar_atan_index(f32 numerator, f32 denominator) {
    if (denominator == 0.0f) return 0;
    const double scaled =
        static_cast<double>(1024.0f * numerator / denominator) + 0.5;
    return std::clamp(static_cast<i32>(scaled), 0, 1024);
}

inline static f32 jar_atan2_radians(f32 y, f32 x) {
    constexpr f32 half_pi = 1.5707964f;
    constexpr f32 pi = 3.1415927f;
    if (x >= 0.0f) {
        if (y >= 0.0f) {
            if (x >= y) {
                return jar_atan_lookup(jar_atan_index(y, x));
            }
            return half_pi - jar_atan_lookup(jar_atan_index(x, y));
        }
        if (x >= -y) {
            return -jar_atan_lookup(jar_atan_index(-y, x));
        }
        return jar_atan_lookup(jar_atan_index(-x, y)) - half_pi;
    }
    if (y >= 0.0f) {
        if (-x >= y) {
            return pi - jar_atan_lookup(jar_atan_index(-y, x));
        }
        return jar_atan_lookup(jar_atan_index(-x, y)) + half_pi;
    }
    if (x <= y) {
        return jar_atan_lookup(jar_atan_index(y, x)) - pi;
    }
    return -half_pi - jar_atan_lookup(jar_atan_index(x, y));
}

inline static f32 jar_direction_degrees(f32 from_x, f32 from_y,
                                        f32 to_x, f32 to_y) {
    return jar_atan2_radians(to_y - from_y, to_x - from_x) * 57.29578f;
}

inline static f32 damage_category_multiplier(const Unit& target,
                                      f32 air_multiplier,
                                      f32 experimental_multiplier) {
    f32 multiplier = 1.0f;
    if (target.airborne) multiplier *= air_multiplier;
    // The stock projectile's second category multiplier is for experimental
    // units only in custom content. Canonical JAR weapons leave it at 1.
    if (target.is_experimental) multiplier *= experimental_multiplier;
    return multiplier;
}

inline static f32 attack_follow_stop_range(const Unit& unit, const Unit& target) {
    f32 range = unit.weapon_range;
    if (unit.attack_range_adds_radii) range += unit.radius + target.radius;
    // y.a(delta, target, ad, stop) gates every stopping adjustment on E().
    // A fixed-firing unit keeps o(target), including its warmup allowance.
    if (!unit.attack_follow_e_flag) {
        range -= 1.0f;
        if (unit.attack_range_adds_radii) range -= 2.0f;
        if (unit.attack_follow_weapon_n > 5.0f) range -= 3.0f;
    }
    return range;
}

inline static std::pair<f32, f32> bomber_attack_target(
        Unit& unit, f32 distance_sq, f32 target_x, f32 target_y) {
    if (!unit.bomber_attack_movement) return {target_x, target_y};
    // y.a(distanceSquared,x,y) captures the incoming approach before
    // movement, then retains it while flying past the target to recharge.
    if (unit.bomber_attack_heading < -900.0f) {
        unit.bomber_attack_heading = jar_direction_degrees(
            unit.x, unit.y, target_x, target_y);
    }
    if (distance_sq < 10000.0f && unit.energy_disabled) {
        unit.bomber_retreating = true;
    }
    if (unit.bomber_retreating) {
        // Native cB and bd() are floats, promoted for the 0.6d comparison.
        const double energy = unit.energy;
        const double maximum = unit.max_energy;
        if (energy < maximum * 0.6 ||
            (distance_sq < 40000.0f && energy < maximum)) {
            const f32 heading = unit.bomber_attack_heading + 180.0f;
            target_x += jar_cosine_degrees(heading) * 600.0f;
            target_y += jar_sine_degrees(heading) * 600.0f;
        } else {
            unit.bomber_retreating = false;
            unit.bomber_attack_heading = -999.0f;
            // aH() drops av/aw, W/X, q and s, but retains a pending aU.
            unit.using_path = false;
            unit.waypoints.clear();
            unit.current_waypoint = 0;
            unit.path_incomplete = false;
            unit.path_target_override_active = false;
            unit.path_request_preserves_active_path = false;
            unit.repath_cooldown_frames = 0.0f;
            unit.repath_cooldown_order_start = -1.0f;
            unit.waypoint_stall_frames = 0.0f;
            unit.last_waypoint_stall_frames = 0.0f;
            unit.single_node_path_counter = 0;
        }
    }
    return {target_x, target_y};
}

inline static bool is_service_unit(const Unit& unit) {
    return unit.can_repair || unit.can_reclaim;
}

inline static f32 service_range(const Unit& unit) {
    return static_cast<f32>(
        unit.nano_range != -1 ? unit.nano_range : unit.base_nano_range);
}

inline static void configure_service_warmup(Unit& unit) {
    unit.service_warmup_max = 0.0f;
    unit.service_warmup_decay = 0.0f;
    if (unit.custom_unit) {
        // custom.j.e(i)/f(i) read the chosen nano turret's bn.n/o.
        const i32 index = unit.nano_turret_index >= 0 ? unit.nano_turret_index : 0;
        if (index < static_cast<i32>(unit.weapon_slots.size())) {
            const WeaponSlot& slot = unit.weapon_slots[index];
            unit.service_warmup_max = slot.warmup_frames;
            unit.service_warmup_decay = slot.warmup_call_down_rate;
        }
    } else if (unit.can_reclaim && !unit.is_building) {
        unit.service_warmup_max = 30.0f;
        unit.service_warmup_decay = 1.3f;
    } else if (unit.can_repair) {
        unit.service_warmup_decay = 4.0f;
    }
}

inline static f32 turn_service_toward(Unit& unit, f32 target_x, f32 target_y,
                                     f32 delta_frames) {
    // d.d.b_()==false skips service aiming for stock buildings.
    if (!unit.custom_unit && unit.is_building) return 0.0f;
    const bool independent_turret = unit.custom_unit
        ? !unit.attack_follow_e_flag : unit.unit_type == "builderShip";
    if (independent_turret) {
        if (unit.weapon_slots.empty()) return 0.0f;
        const i32 index = unit.nano_turret_index >= 0
            ? unit.nano_turret_index : 0;
        WeaponSlot& slot = unit.weapon_slots.at(index);
        f32 origin_x = unit.x;
        f32 origin_y = unit.y;
        f32 mount_angle = unit.rotation;
        if (slot.attached_to_slot >= 0 &&
            slot.attached_to_slot < static_cast<i32>(unit.weapon_slots.size())) {
            const WeaponSlot& parent = unit.weapon_slots[slot.attached_to_slot];
            const f32 recoil = weapon_recoil_distance(unit, parent);
            if (recoil != 0.0f) {
                origin_x += jar_cosine_degrees(parent.turret_angle) * recoil;
                origin_y += jar_sine_degrees(parent.turret_angle) * recoil;
            }
            origin_x += (jar_cosine_degrees(unit.rotation) * parent.mount_forward -
                         jar_sine_degrees(unit.rotation) * parent.mount_side);
            origin_y += (jar_sine_degrees(unit.rotation) * parent.mount_forward +
                         jar_cosine_degrees(unit.rotation) * parent.mount_side);
            mount_angle = parent.turret_angle;
        }
        const f32 recoil = weapon_recoil_distance(unit, slot);
        if (recoil != 0.0f) {
            origin_x += jar_cosine_degrees(slot.turret_angle) * recoil;
            origin_y += jar_sine_degrees(slot.turret_angle) * recoil;
        }
        origin_x += (jar_cosine_degrees(mount_angle) * slot.mount_forward -
                     jar_sine_degrees(mount_angle) * slot.mount_side);
        origin_y += (jar_sine_degrees(mount_angle) * slot.mount_forward +
                     jar_cosine_degrees(mount_angle) * slot.mount_side);
        const f32 desired = jar_direction_degrees(
            origin_x, origin_y, target_x, target_y);
        if (slot.turret_idle_timer >= 0.0f && slot.turret_idle_timer < 70.0f) {
            slot.turret_idle_timer = 70.0f;
        }
        slot.turret_previous_angle = slot.turret_angle;
        // y.b(delta,x,y) uses the independent ap angle for E()==false
        // and returns the error remaining after its angular integration.
        return turn_weapon_slot_toward(unit, slot, desired, delta_frames);
    }
    const f32 desired = jar_direction_degrees(
        unit.x, unit.y, target_x, target_y);
    const f32 error = shortest_angle_delta(unit.rotation, desired);
    turn_toward(unit, desired, delta_frames);
    return error;
}

inline static bool service_action_ready(Unit& unit, const Unit& target,
                                        f32 delta_frames) {
    const f32 aim_error = std::abs(turn_service_toward(
        unit, target.x, target.y, std::max(0.0f, delta_frames)));
    const bool aimed = aim_error < 30.0f;
    // y.i() only marks ad.a while inside the aim window. A nearby target
    // outside that window still lets ap.f decay later in this object turn.
    unit.service_aim_ready = aimed;
    if (!aimed) return false;
    unit.service_active_this_frame = true;
    unit.service_aim_target_uid = target.uid;
    if (unit.service_warmup_progress < unit.service_warmup_max) {
        // ap.f may overshoot e(slot); the JAR clamps and applies service on
        // the following update, rather than when this addition reaches it.
        unit.service_warmup_progress += std::max(0.0f, delta_frames);
        return false;
    }
    unit.service_warmup_progress = unit.service_warmup_max;
    return true;
}

inline static void finish_service_order(Unit& unit) {
    unit.target_uid = 0;
    unit.target_team_id = -1;
    unit.weapon_type = unit.can_reclaim ? WeaponType::BUILD : WeaponType::MOVE;
}

inline static void restore_default_weapon_type(const UnitRegistry& registry, Unit& unit) {
    const UnitDef* def = registry.get(unit.unit_type);
    unit.weapon_type = def ? def->weapon_type : WeaponType::MOVE;
    if (unit.can_attack && unit.weapon_range > 0.0f &&
        (unit.damage > 0.0f || unit.splash_damage > 0.0f) &&
        !unit.is_nuke && !unit.is_antinuke) {
        unit.weapon_type = unit.is_mobile() ? WeaponType::ATTACK : WeaponType::GUARD;
    }
}

inline static void clear_navigation_orders(Unit& unit) {
    unit.target_team_id = -1;
    unit.has_patrol = false;
    unit.patrol_points.clear();
    unit.patrol_index = 0;
    unit.has_guard_pos = false;
    unit.guard_target_uid = 0;
    unit.guard_target_team_id = -1;
    unit.follow_target_uid = 0;
    unit.follow_target_team_id = -1;
    unit.recent_order_chase = false;
    unit.guard_movement_target_hold = false;
    unit.guard_movement_target_uid = 0;
    unit.guard_movement_target_team_id = -1;
}

inline static bool is_map_effect_unit(const Unit& unit) {
    return unit.unit_type == "tree" || unit.unit_type == "fogRevealer" ||
           unit.unit_type == "spreadingFire" || unit.unit_type == "damagingBorder" ||
           unit.unit_type == "zoneMarker" || unit.unit_type == "marker";
}

inline static bool jar_death_persists(const Unit& unit) {
    if (unit.is_building && !unit.custom_unit) {
        // Crystal resources override e() directly. Other stock buildings
        // dispatch d.d.e() to L() only when complete; extractor, supplyDepot,
        // and wall_v are the three registered types whose L() returns false.
        if (unit.unit_type == "crystalResource") return true;
        return unit.build_progress >= 1.0f &&
               unit.unit_type != "extractor" &&
               unit.unit_type != "supplyDepot" &&
               unit.unit_type != "wall_v";
    }
    // Custom units without a wreck image, x (fog/border/marker), ai (fire),
    // and ladybug return false from e(), so am.bv() removes their objects.
    if ((unit.custom_unit && !unit.has_wreck_image &&
         !unit.slow_death_fall) ||
        (unit.is_building &&
         (unit.build_progress < 1.0f ||
          (!unit.has_wreck_image && !unit.slow_death_fall))) ||
        unit.unit_type == "fogRevealer" ||
        unit.unit_type == "spreadingFire" ||
        unit.unit_type == "damagingBorder" ||
        unit.unit_type == "zoneMarker" || unit.unit_type == "marker" ||
        unit.unit_type == "ladybug") {
        return false;
    }
    return true;
}

inline static bool is_victory_key_unit(const Unit& unit) {
    return unit.unit_type == "commandCenter" ||
           unit.unit_type == "landFactory" ||
           unit.unit_type == "airFactory" ||
           unit.unit_type == "seaFactory" ||
           unit.unit_type == "experimentalLandFactory" ||
           unit.unit_type == "builder" ||
           unit.unit_type == "builderShip";
}

} // namespace rwrl
