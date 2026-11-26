#include "world_internal.h"

#include <cstdio>
#include <cstdlib>

namespace rwrl {

void World::update_flying_states(f32 dt, f32 speed_mult) {
    update_unit_flying_states(dt, speed_mult, 0);
}

void World::update_unit_flying_states(
        f32 dt, f32 speed_mult, u32 only_unit_uid) {
    const f32 game_frames = dt * EngineConstants::FRAME_RATE * speed_mult;
    for (auto& [_, unit] : units_) {
        if (only_unit_uid != 0 && unit.uid != only_unit_uid) continue;
        if (!unit.is_alive) continue;


        if (unit.custom_unit) {
            // custom.j returns from its incomplete branch before advancing
            // idle time, flight phase or visual height.
            if (!unit.is_complete()) continue;
            unit.custom_idle_frames = unit.movement_prediction_enabled
                ? 0.0f : unit.custom_idle_frames + game_frames;
            if (unit.is_active() && unit.movement_prediction_enabled &&
                (unit.move_type == MoveType::OVER_CLIFF ||
                 unit.move_type == MoveType::OVER_CLIFF_WATER)) {
                // custom.j samples cI() after y.a(float), including its
                // displacement. k.l.b(x,y) compares the terrain-only HOVER
                // and OVER_CLIFF_WATER layers; ordinary cliffs do not slow
                // these units. A stopped unit retains its previous j.
                const i32 tx = pathfinder_.unclamped_tile_x(unit.x);
                const i32 ty = pathfinder_.unclamped_tile_y(unit.y);
                const bool outside = tx < 0 || ty < 0 ||
                    tx >= pathfinder_.width() || ty >= pathfinder_.height();
                const bool over_cliff = outside ||
                    (!pathfinder_.is_passable_ignoring_buildings(tx, ty, MoveType::HOVER) &&
                     pathfinder_.is_passable_ignoring_buildings(tx, ty, MoveType::OVER_CLIFF_WATER));
                unit.terrain_speed_multiplier = over_cliff ? 0.7f : 1.0f;
            }
            if (unit.move_type == MoveType::AIR) {
                if (unit.parent_uid != 0) continue;
                const bool was_airborne = unit.height >= 4.0f;
                unit.flight_phase += 2.0f * game_frames;
                while (unit.flight_phase > 360.0f) unit.flight_phase -= 360.0f;
                const u8 terrain = pathfinder_.terrain(
                    pathfinder_.tile_x(unit.x), pathfinder_.tile_y(unit.y));
                const bool landing = unit.land_on_ground &&
                    !unit.movement_prediction_enabled &&
                    (terrain & (TERRAIN_WATER | TERRAIN_LAVA)) == 0 &&
                    unit.custom_idle_frames > 3.0f &&
                    (!unit.land_on_ground_only_idle || unit.order_queue.empty());
                const f32 target = unit.target_height +
                    jar_sine_degrees(unit.flight_phase) * unit.target_height_drift;
                const f32 base_rate = unit.height_change_rate < 0.0f
                    ? std::abs(unit.height - 2.0f) * 0.05f * 0.4f + 0.2f
                    : unit.height_change_rate;
                const f32 error_rate =
                    std::abs(unit.height - target) * 0.05f * 0.3f + 0.1f;
                unit.height = approach_value(
                    unit.height, landing ? 2.0f : target,
                    (landing ? base_rate : std::min(base_rate, error_rate)) * game_frames);
                unit.airborne = unit.height >= 4.0f;
                if (was_airborne != unit.airborne) {
                    unit.collision_moved = true;
                }
                continue;
            }

            const f32 lower_drift_height =
                unit.target_height - unit.target_height_drift;
            if (unit.height < lower_drift_height) {
                unit.height = std::min(
                    lower_drift_height, unit.height + 0.2f * game_frames);
            }
            if (unit.target_height == 0.0f &&
                unit.target_height_drift == 0.0f && unit.height <= 0.0f) {
                continue;
            }
            if (unit.parent_uid != 0) continue;

            if (unit.target_height_drift != 0.0f) {
                unit.flight_phase += 2.0f * game_frames;
                while (unit.flight_phase > 360.0f) unit.flight_phase -= 360.0f;
            }
            const f32 target = unit.target_height +
                jar_sine_degrees(unit.flight_phase) * unit.target_height_drift;
            const f32 base_rate = unit.height_change_rate < 0.0f
                ? std::abs(unit.height - 2.0f) * 0.05f * 0.4f + 0.2f
                : unit.height_change_rate;
            const f32 error_rate =
                std::abs(unit.height - target) * 0.05f * 0.3f + 0.1f;
            unit.height = approach_value(
                unit.height, target,
                std::min(base_rate, error_rate) * game_frames);

            if (unit.height >
                unit.target_height + unit.target_height_drift + 1.0f) {
                unit.vertical_speed += unit.falling_acceleration * game_frames;
                if (unit.height < 0.0f) {
                    // custom.j.a(float) caps f with f.b(f,0.2) underwater.
                    unit.vertical_speed = std::min(unit.vertical_speed, 0.2f);
                }
                unit.height -= unit.vertical_speed * game_frames;
                if (unit.height <=
                    unit.target_height + unit.target_height_drift + 1.0f) {
                    unit.height = std::max(
                        unit.height,
                        unit.target_height + unit.target_height_drift);
                    unit.vertical_speed = 0.0f;
                }
            } else {
                unit.vertical_speed = 0.0f;
            }
            continue;
        }

        if (unit.move_type == MoveType::LAND && unit.can_move) {
            // e.j finishes after y.a(float). bT() excludes incomplete units
            // and cargo; negative height rises before the positive fall gate.
            if (!unit.is_complete() || unit.parent_uid != 0) continue;
            if (unit.height < 0.0f) {
                unit.height += 0.2f * game_frames;
                if (unit.height >= 0.0f) unit.height = 0.0f;
            }
            if (unit.height > 0.0f) {
                unit.vertical_speed += 0.03f * game_frames;
                unit.height -= unit.vertical_speed * game_frames;
                if (unit.height <= 0.0f) {
                    unit.height = 0.0f;
                    unit.vertical_speed = 0.0f;
                }
            }
            continue;
        }

        if (unit.unit_type == "helicopter") {
            unit.vertical_speed = approach_value(
                unit.vertical_speed, 0.5f, 0.003f * game_frames);
            if (unit.vertical_speed > 0.4f) {
                unit.flight_phase += 2.0f * game_frames;
                while (unit.flight_phase > 360.0f) unit.flight_phase -= 360.0f;
                const f32 target_height =
                    20.0f + jar_sine_degrees(unit.flight_phase) * 1.5f;
                unit.height = approach_value(
                    unit.height, target_height, 0.1f * game_frames);
            }
            unit.move_speed = unit.height < 15.0f
                ? 0.0f : 2.2f * EngineConstants::FRAME_RATE;
            continue;
        }

        if (unit.unit_type == "airShip") {
            unit.height = approach_value(
                unit.height, 20.0f, 0.3f * game_frames);
            unit.move_speed = unit.height < 15.0f
                ? 0.0f : 2.4f * EngineConstants::FRAME_RATE;
            continue;
        }

        if (unit.unit_type == "gunShip") {
            unit.flight_phase += 2.0f * game_frames;
            while (unit.flight_phase > 360.0f) unit.flight_phase -= 360.0f;
            const f32 target_height =
                20.0f + jar_sine_degrees(unit.flight_phase) * 1.5f;
            unit.height = approach_value(
                unit.height, target_height, 0.1f * game_frames);
            // b.e.a(float) raises the aircraft after y.a(float). Publish the
            // current z() for queries made before the next object update.
            unit.move_speed = unit.height < 15.0f
                ? 0.0f : 1.4f * EngineConstants::FRAME_RATE;
            continue;
        }

        if (unit.unit_type == "hoverTank" ||
            unit.unit_type == "heavyHoverTank") {
            if (!unit.is_complete() || unit.parent_uid != 0) continue;
            unit.flight_phase += 3.0f * game_frames;
            while (unit.flight_phase > 360.0f) unit.flight_phase -= 360.0f;
            const f32 target_height =
                4.0f + jar_sine_degrees(unit.flight_phase) * 1.5f;
            unit.height = approach_value(
                unit.height, target_height, 0.1f * game_frames);
            continue;
        }

        if (unit.unit_type == "experimentalHoverTank") {
            if (!unit.is_complete() || unit.parent_uid != 0) continue;
            unit.flight_phase += game_frames;
            while (unit.flight_phase > 360.0f) unit.flight_phase -= 360.0f;
            const f32 target_height =
                4.0f + jar_sine_degrees(unit.flight_phase) * 2.0f;
            unit.height = approach_value(
                unit.height, target_height, 0.1f * game_frames);
            // e.c.a(float) refreshes its retained beam from E(0) after the
            // hull's movement and hover update, including the birth turn.
            if (unit.active_beam_projectile_uid != 0) {
                auto beam = projectiles_.find(unit.active_beam_projectile_uid);
                if (beam != projectiles_.end() && !unit.weapon_slots.empty()) {
                    const WeaponSlot& slot = unit.weapon_slots.front();
                    const f32 angle = slot.fixed_turret
                        ? unit.rotation : slot.turret_angle;
                    beam->second.x = unit.x +
                        jar_cosine_degrees(angle) * slot.muzzle_offset;
                    beam->second.y = unit.y +
                        jar_sine_degrees(angle) * slot.muzzle_offset;
                    beam->second.height = unit.height;
                } else {
                    retired_beam_projectiles_.erase(unit.active_beam_projectile_uid);
                    unit.active_beam_projectile_uid = 0;
                }
            }
            continue;
        }

        if (unit.unit_type == "hovercraft") {
            if (!unit.is_complete() || unit.parent_uid != 0) continue;
            unit.flight_phase += 4.0f * game_frames;
            while (unit.flight_phase > 360.0f) unit.flight_phase -= 360.0f;
            const f32 target_height = unit.unloading_all
                ? 0.0f
                : 3.0f + jar_sine_degrees(unit.flight_phase) * 1.5f;
            unit.height = approach_value(
                unit.height, target_height, 0.1f * game_frames);
            continue;
        }

        if (unit.unit_type == "dropship") {
            unit.flight_phase += 2.0f * game_frames;
            while (unit.flight_phase > 360.0f) unit.flight_phase -= 360.0f;
            if (!unit.is_complete() || unit.parent_uid != 0) continue;
            const bool was_airborne = unit.height >= 4.0f;
            // b.d.a(float) descends only when aq() is true and cK() is
            // false. Water and lava keep an idle transport at flight height.
            const u8 terrain = pathfinder_.terrain(
                pathfinder_.tile_x(unit.x), pathfinder_.tile_y(unit.y));
            const bool idle = unit.order_queue.empty() &&
                (terrain & (TERRAIN_WATER | TERRAIN_LAVA)) == 0;
            const f32 target_height = idle
                ? 2.0f
                : 35.0f + jar_sine_degrees(unit.flight_phase) * 1.5f;
            unit.height = approach_value(
                unit.height, target_height,
                (idle ? 0.4f : 0.35f) * game_frames);
            unit.airborne = unit.height >= 4.0f;
            // b.d.a(float) publishes y.ay when i() changes, so an idle
            // transport crossing the height gate enters the collision pass.
            if (unit.airborne != was_airborne) unit.collision_moved = true;
        }
    }
}

void World::update_aircraft_wrecks_(f32 delta_frames, u32 only_unit_uid) {
    if (delta_frames < 0.0f) return;
    for (auto unit_update_it = only_unit_uid == 0
             ? units_.begin() : units_.find(only_unit_uid);
         unit_update_it != units_.end();
         unit_update_it = only_unit_uid == 0
             ? std::next(unit_update_it) : units_.end()) {
        auto& [uid, unit] = *unit_update_it;
        if (!unit.is_dead) continue;
        const bool stock_ground_wreck = !unit.custom_unit &&
            (unit.move_type == MoveType::LAND || unit.move_type == MoveType::HOVER);
        // e.j can raise dK to 0.4 during a zero-delta dead update. Keep the
        // existing zero-delta scheduling of the other wreck classes.
        if (delta_frames == 0.0f && !stock_ground_wreck && !unit.custom_unit) continue;

        // custom.j.a(float) invokes the base object update first and then
        // handles slowDeathFall itself. Sliding custom units retain cc/cd
        // (the native horizontal velocity) after death, so this movement is
        // visible on the next object turn even though the unit is no longer
        // active for combat or navigation.
        if (unit.slow_death_fall && unit.height > 0.0f &&
            (unit.move_speed_factor != 0.0f ||
             unit.vel_x != 0.0f || unit.vel_y != 0.0f)) {
            unit.crash_fall_velocity += 0.017f * delta_frames;
            unit.height -= unit.crash_fall_velocity * delta_frames;
            // custom.j still selects this slow fall when motion is stored,
            // but y.n(delta) suppresses displacement while I() or the wall
            // collision timer prevents movement. Death retains that timer.
            if (unit.is_mobile() && !unit.custom_action_movement_blocked &&
                unit.wall_bump_frames == 0.0f) {
                if (unit.strafes) {
                    unit.x += unit.vel_x * delta_frames;
                    unit.y += unit.vel_y * delta_frames;
                } else if (unit.move_speed_factor != 0.0f) {
                    const f32 move_amount =
                        unit.move_speed_per_frame() *
                        unit.move_speed_factor * delta_frames;
                    unit.x += jar_cosine_degrees(unit.rotation) * move_amount;
                    unit.y += jar_sine_degrees(unit.rotation) * move_amount;
                }
            }
            continue;
        }

        // Custom HOVER units use the aircraft fall. Stock LAND and HOVER
        // share e.j: positive height uses a minimum dK of 0.4, then 0.002
        // acceleration continues through zero over the current liquid tile.
        // b.c inherits the aircraft wreck update even when h() selects
        // WATER during descent or while submerged.
        const bool stock_amphibious_wreck = unit.unit_type == "amphibiousJet";
        const bool standard_wreck_fall =
            unit.custom_unit || unit.move_type == MoveType::AIR ||
            unit.move_type == MoveType::HOVER || stock_ground_wreck || stock_amphibious_wreck;
        if (!standard_wreck_fall || (!unit.can_move && !unit.custom_unit)) continue;

        if (stock_ground_wreck) {
            // e.j.a(float) chooses its terminal height through cK() before
            // falling. Liquid wrecks pass through zero with the same 0.002
            // acceleration and clamp to -10 on the crossing update.
            const i32 tile_x = pathfinder_.unclamped_tile_x(unit.x);
            const i32 tile_y = pathfinder_.unclamped_tile_y(unit.y);
            const u8 terrain = pathfinder_.has_terrain(tile_x, tile_y)
                ? pathfinder_.terrain(tile_x, tile_y) : 0;
            const f32 terminal_height =
                (terrain & (TERRAIN_WATER | TERRAIN_LAVA)) != 0
                    ? -10.0f : 0.0f;
            if (unit.height > terminal_height) {
                if (unit.height > 0.0f && unit.crash_fall_velocity < 0.4f) {
                    unit.crash_fall_velocity = 0.4f;
                }
                unit.crash_fall_velocity += 0.002f * delta_frames;
                unit.height -= unit.crash_fall_velocity * delta_frames;
                if (unit.height <= terminal_height) {
                    unit.height = terminal_height;
                }
            }
            unit.vertical_speed = unit.crash_fall_velocity;
            continue;
        }

        // Stock aircraft and custom HOVER wrecks inherit the 0.06-per-frame
        // fall. Both use cK() to keep sinking over liquid toward -10 with
        // the much smaller 0.0008 acceleration.
        if (unit.height > 0.0f) {
            unit.crash_fall_velocity += 0.06f * delta_frames;
            unit.height -= unit.crash_fall_velocity * delta_frames;
            continue;
        }
        if (!unit.crash_impacted) {
            if (unit.custom_unit && unit.slow_death_fall) {
                // custom.j.a(float) runs ds() before impact effects, after
                // the preceding turn has moved the wreck through height zero.
                release_transport_cargo_on_death_(unit);
            }
            const i32 crash_tile_x = pathfinder_.tile_x(unit.x);
            const i32 crash_tile_y = pathfinder_.tile_y(unit.y);
            if ((unit.move_type == MoveType::AIR || unit.custom_unit ||
                 stock_amphibious_wreck) &&
                pathfinder_.has_terrain(crash_tile_x, crash_tile_y)) {
                const u8 terrain = pathfinder_.terrain(crash_tile_x, crash_tile_y);
                // utility.y.d() accepts the tile's water or lava flag
                // directly, even when another flag blocks its path layer.
                unit.crash_over_water =
                    (terrain & (TERRAIN_WATER | TERRAIN_LAVA)) != 0;
            } else {
                unit.crash_over_water = false;
            }
            unit.crash_impacted = true;
            const f32 impact_velocity = unit.crash_fall_velocity;
            unit.crash_fall_velocity = 0.0f;
            // custom.j emits its ground-collision death effect on the first
            // object turn after the wreck reaches the ground. The effects
            // are visual-only here, but each still consumes the shared JAR
            // UID and object-order counters. Liquid collisions use the
            // very-small effect and do not allocate these objects.
            const bool ground_collision_effect = unit.custom_unit
                ? unit.explode_on_death_ground_collision && impact_velocity > 0.5
                : unit.move_type == MoveType::AIR || stock_amphibious_wreck;
            if (ground_collision_effect && !unit.crash_over_water) {
                for (i32 i = 0; i < 2; ++i) {
                    if (global_next_uid_) {
                        ++(*global_next_uid_);
                    } else {
                        ++next_uid_;
                    }
                    (void)allocate_object_order_();
                }
                if (!unit.hide_scorch_mark &&
                    try_create_scorch_mark_(unit.x, unit.y)) {
                    if (global_next_uid_) {
                        ++(*global_next_uid_);
                    } else {
                        ++next_uid_;
                    }
                    (void)allocate_object_order_();
                }
            }
            continue;
        }
        if (unit.crash_over_water) {
            if (unit.height > -10.0f) {
                unit.crash_fall_velocity += 0.0008f * delta_frames;
                unit.height -= unit.crash_fall_velocity * delta_frames;
            }
        } else {
            unit.height = 0.0f;
        }
    }
}

} // namespace rwrl
