#include "world_internal.h"

namespace rwrl {

void World::update_unit_legs(f32 delta_frames, u32 unit_uid) {
    Unit* unit = get_unit(unit_uid);
    if (unit) update_legs_(*unit, delta_frames);
}

void World::initialize_legs_(Unit& unit) {
    // custom.j.du() preserves the entire runtime array when a conversion
    // keeps its length, even if all of the configured foot positions change.
    if (unit.legs.size() == unit.leg_definitions.size()) return;
    unit.legs.assign(unit.leg_definitions.size(), LegState{});
    unit.leg_previous_x = unit.x;
    unit.leg_previous_y = unit.y;
    unit.leg_previous_height = unit.height;
    unit.leg_previous_rotation = unit.rotation;
    if (unit.leg_rotation_slot >= 0 &&
        unit.leg_rotation_slot < static_cast<i32>(unit.weapon_slots.size())) {
        unit.leg_previous_rotation =
            unit.weapon_slots[unit.leg_rotation_slot].turret_angle;
    }
    update_legs_(unit, 0.0f);
    for (auto& leg : unit.legs) leg.reset = true;
}

void World::update_legs_(Unit& unit, f32 delta_frames) {
    if (unit.legs.empty()) return;
    if (delta_frames != 0.0f &&
        static_cast<f64>(unit.vertical_speed) > 0.3) return;
    if (unit.parent_uid != 0) {
        for (auto& leg : unit.legs) leg.reset = true;
        return;
    }

    f32 rotation = unit.rotation;
    if (unit.leg_rotation_slot >= 0 &&
        unit.leg_rotation_slot < static_cast<i32>(unit.weapon_slots.size())) {
        rotation = unit.weapon_slots[unit.leg_rotation_slot].turret_angle;
    }
    const f32 dx = unit.x - unit.leg_previous_x;
    const f32 dy = unit.y - unit.leg_previous_y;
    const f32 dh = unit.height - unit.leg_previous_height;
    const f32 dr = rotation - unit.leg_previous_rotation;
    const bool pose_changed = dx != 0.0f || dy != 0.0f || dr != 0.0f;
    unit.leg_previous_x = unit.x;
    unit.leg_previous_y = unit.y;
    unit.leg_previous_height = unit.height;
    unit.leg_previous_rotation = rotation;

    // h asks y.n(15) for a motion estimate, then caps squared-distance*240
    // at 15. Keep that order and the JAR's degree lookup table.
    f32 prediction_x = 0.0f, prediction_y = 0.0f;
    // y.n(15) tests I() and the tile-clamp timer y.b, independently of
    // am.cK. A coasting unit still predicts its feet while a path is pending.
    if (unit.is_mobile() && !unit.custom_action_movement_blocked &&
        unit.wall_bump_frames == 0.0f) {
        if (unit.strafes) {
            prediction_x = unit.vel_x * 15.0f;
            prediction_y = unit.vel_y * 15.0f;
        } else if (unit.move_speed_factor != 0.0f) {
            const f32 speed = unit.move_speed_per_frame() *
                              unit.move_speed_factor * 15.0f;
            prediction_x = jar_cosine_degrees(unit.rotation) * speed;
            prediction_y = jar_sine_degrees(unit.rotation) * speed;
        }
    }
    if (prediction_x != 0.0f || prediction_y != 0.0f) {
        const f32 prediction_sq =
            prediction_x * prediction_x + prediction_y * prediction_y;
        const f32 direction = jar_direction_degrees(
            0.0f, 0.0f, prediction_x, prediction_y);
        const f32 amount = std::min(prediction_sq * 240.0f, 15.0f);
        prediction_x = jar_cosine_degrees(direction) * amount;
        prediction_y = jar_sine_degrees(direction) * amount;
    }

    i32 moving_count = 0;
    f32 furthest_sq = 0.0f;
    std::size_t furthest_index = 0;
    for (std::size_t i = 0; i < unit.legs.size(); ++i) {
        LegState& leg = unit.legs[i];
        const LegDef& definition = unit.leg_definitions[i];
        const bool reset = leg.reset;
        const bool compressed = reset && leg.compressed_reset;
        if (reset) {
            leg.reset = false;
            leg.compressed_reset = false;
        }
        if (!definition.height_relative) leg.height -= dh;
        if (!definition.lock_movement) {
            if (pose_changed) {
                leg.x -= dx;
                leg.y -= dy;
            }
        } else if (pose_changed && dr != 0.0f) {
            const f32 cosine = jar_cosine_degrees(dr);
            const f32 sine = jar_sine_degrees(dr);
            const f32 x = leg.x * cosine - leg.y * sine;
            const f32 y = leg.x * sine + leg.y * cosine;
            leg.x = x;
            leg.y = y;
            leg.rotation += dr;
        }
        if (definition.hidden) continue;

        const f32 cosine = jar_cosine_degrees(rotation);
        const f32 sine = jar_sine_degrees(rotation);
        leg.target_x = cosine * definition.y - sine * definition.x;
        leg.target_y = sine * definition.y + cosine * definition.x;
        if (reset) {
            leg.x = leg.target_x;
            leg.y = leg.target_y;
            leg.rotation = rotation + definition.end_direction;
            if (compressed) {
                leg.x *= 0.6f;
                leg.y *= 0.6f;
                leg.height = -3.0f;
            }
        }
        if (!definition.lock_movement) {
            leg.target_x += prediction_x * definition.prediction_multiplier;
            leg.target_y += prediction_y * definition.prediction_multiplier;
        }
        leg.distance_sq = dist2(
            leg.x, leg.y, leg.target_x, leg.target_y);
        if (leg.distance_sq > furthest_sq) {
            furthest_index = i;
            furthest_sq = leg.distance_sq;
        }
        if (leg.moving && !definition.lock_movement) ++moving_count;
    }

    for (std::size_t i = 0; i < unit.legs.size(); ++i) {
        LegState& leg = unit.legs[i];
        const LegDef& definition = unit.leg_definitions[i];
        if (definition.hidden) continue;
        const f32 target_height = definition.target_height -
            (definition.height_relative ? 0.0f : unit.height);
        const f32 lifted_height = target_height + definition.lifting_height;
        if (leg.distance_sq > 90000.0f) {
            leg.x = definition.x;
            leg.y = definition.y;
        } else if (leg.distance_sq >
                   definition.hard_limit * definition.hard_limit) {
            const f32 direction = jar_direction_degrees(
                leg.target_x, leg.target_y, leg.x, leg.y);
            leg.x = leg.target_x +
                    jar_cosine_degrees(direction) * definition.hard_limit;
            leg.y = leg.target_y +
                    jar_sine_degrees(direction) * definition.hard_limit;
            leg.moving = true;
        }
        if (!leg.moving && leg.height <= target_height + 0.1f &&
            moving_count < definition.max_moving_legs &&
            (i == furthest_index || !definition.move_only_if_furthest)) {
            bool neighbour_moving = false;
            if (definition.check_neighbours) {
                for (i32 index : definition.neighbours) {
                    if (index >= 0 && index < static_cast<i32>(unit.legs.size()) &&
                        unit.legs[index].moving) neighbour_moving = true;
                }
            }
            const f32 hold = neighbour_moving
                ? definition.hold_distance_max : definition.hold_distance_min;
            if (leg.distance_sq > hold * hold) {
                leg.moving = true;
                ++moving_count;
            }
        }
        if (definition.lock_movement) leg.moving = true;
        if (!leg.moving) {
            leg.warmup_frames = 0.0f;
            if (leg.height > target_height) {
                leg.height -= delta_frames * definition.height_speed;
                if (leg.height <= target_height) {
                    leg.height = target_height;
                    const i32 tx = pathfinder_.unclamped_tile_x(unit.x + leg.x);
                    const i32 ty = pathfinder_.unclamped_tile_y(unit.y + leg.y);
                    const bool outside = tx < 0 || ty < 0 ||
                        tx >= pathfinder_.width() || ty >= pathfinder_.height();
                    const u8 terrain = !outside && pathfinder_.has_terrain(tx, ty)
                        ? pathfinder_.terrain(tx, ty) : TERRAIN_NONE;
                    // utility.y.c converts the original foot position to a
                    // tile; k.l.a(int,int) returns true outside the map.
                    // Inside it compares the OVER_CLIFF and WATER terrain
                    // layers. Lava and a standalone bridge do not sink feet.
                    leg.over_liquid = outside || ((terrain & TERRAIN_WATER) != 0 &&
                        (terrain & (TERRAIN_LAVA | TERRAIN_CLIFF |
                         TERRAIN_LARGE_CLIFF | TERRAIN_TREES |
                         TERRAIN_BLOCK_LAND)) == 0);
                }
            } else if (leg.over_liquid &&
                       leg.height > -3.0f + target_height) {
                leg.height -= delta_frames * 0.3f;
            }
        } else if (leg.height > lifted_height || definition.lock_movement) {
            const f32 amount = delta_frames * definition.move_speed;
            if (leg.distance_sq <= amount * amount) {
                leg.x = leg.target_x;
                leg.y = leg.target_y;
                leg.moving = false;
            } else {
                const f32 direction = jar_direction_degrees(
                    leg.x, leg.y, leg.target_x, leg.target_y);
                leg.x += jar_cosine_degrees(direction) * amount;
                leg.y += jar_sine_degrees(direction) * amount;
            }
            if (definition.lock_movement && leg.height > target_height) {
                leg.height = std::max(target_height,
                    leg.height - delta_frames * definition.height_speed);
            }
            const f32 turn = definition.rotate_speed * delta_frames;
            leg.rotation += std::clamp(shortest_angle_delta(
                leg.rotation, rotation + definition.end_direction), -turn, turn);
        } else if (leg.warmup_frames < definition.move_warmup_frames) {
            leg.warmup_frames += delta_frames;
        } else {
            leg.height += delta_frames * definition.height_speed;
        }
    }
}

}  // namespace rwrl
