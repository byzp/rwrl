#include "world_internal.h"

namespace rwrl {

void World::update_dynamic_map_units(f32 delta_frames,
                                     const Vec<World*>& enemy_worlds,
                                     u32 only_unit_uid) {
    if (delta_frames <= 0.0f) return;

    Vec<std::pair<f32, f32>> fire_spawns;
    Vec<World*> all_worlds = enemy_worlds;
    all_worlds.push_back(this);

    auto fire_at = [&](f32 x, f32 y) {
        for (World* world : all_worlds) {
            if (!world) continue;
            for (const auto& [_, other] : world->units_) {
                if (other.is_alive && other.unit_type == "spreadingFire" &&
                    dist2(other.x, other.y, x, y) <
                        other.radius * other.radius) {
                    return true;
                }
            }
        }
        return false;
    };

    // The JAR stores these objects in the same global unit list as ordinary
    // units. On its first update a border hands its center/radius to the first
    // other live border of the same kind and removes itself. This also means
    // the first object is removed when duplicate borders are created before
    // either one has received an update.
    Vec<u32> dynamic_uids;
    for (auto unit_update_it = only_unit_uid == 0
             ? units_.begin() : units_.find(only_unit_uid);
         unit_update_it != units_.end();
         unit_update_it = only_unit_uid == 0
             ? std::next(unit_update_it) : units_.end()) {
        const auto& [uid, unit] = *unit_update_it;
        if (unit.is_alive &&
            (unit.unit_type == "spreadingFire" ||
             unit.unit_type == "damagingBorder" ||
             unit.unit_type == "zoneMarker")) {
            dynamic_uids.push_back(uid);
        }
    }
    std::sort(dynamic_uids.begin(), dynamic_uids.end());
    for (u32 uid : dynamic_uids) {
        Unit* unit_ptr = get_unit(uid);
        if (!unit_ptr || !unit_ptr->is_alive) continue;
        Unit& unit = *unit_ptr;

        if (unit.unit_type == "spreadingFire") {
            const f32 tile_size = pathfinder_.tile_size();
            const f32 world_width = pathfinder_.width() * tile_size;
            const f32 world_height = pathfinder_.height() * tile_size;
            if (!unit.fire_initialized) {
                unit.fire_initialized = true;
                unit.fire_intensity += deterministic_unit_random(
                    unit, tick_, 0.0f, 0.01f, 10);
                if (unit.x < 0.0f || unit.y < 0.0f ||
                    unit.x >= world_width || unit.y >= world_height) {
                    unit.fire_growth = 0.0f;
                    unit.fire_max_intensity = 0.0f;
                    unit.fire_spread_threshold = 2.0f;
                } else {
                    // ai.f() rejects g.e/g.f/g.h/g.k: water, water bridges,
                    // ordinary/lava cliffs, and large cliffs/trees. Plain
                    // lava (g.g) and block-land cost (g.j) do not disable it.
                    const u8 terrain = pathfinder_.terrain(
                        pathfinder_.tile_x(unit.x), pathfinder_.tile_y(unit.y));
                    constexpr u8 non_burning = TERRAIN_WATER |
                        TERRAIN_WATER_BRIDGE | TERRAIN_CLIFF |
                        TERRAIN_LARGE_CLIFF | TERRAIN_TREES;
                    if ((terrain & non_burning) != 0) {
                        unit.fire_growth = 0.0f;
                        unit.fire_max_intensity = 0.0f;
                        unit.fire_spread_threshold = 2.0f;
                    }
                }
            }

            if (unit.fire_intensity < unit.fire_max_intensity) {
                unit.fire_intensity = std::min(
                    unit.fire_max_intensity,
                    unit.fire_intensity + unit.fire_growth * delta_frames);
            }
            if (unit.fire_intensity > unit.fire_spread_threshold) {
                unit.fire_spread_timer += 0.01f * delta_frames;
                if ((!unit.fire_spread_exhausted && unit.fire_spread_timer > 1.0f) ||
                    unit.fire_spread_timer > 8.0f) {
                    unit.fire_spread_timer = deterministic_unit_random(
                        unit, tick_, 0.0f, 0.01f, 10);
                    unit.fire_spread_exhausted = true;
                    bool spawned = false;
                    for (i32 dx = -1; dx <= 1; ++dx) {
                        for (i32 dy = -1; dy <= 1; ++dy) {
                            if (dx == 0 && dy == 0) continue;
                            // ai.b(int,int) truncates the world-space tile
                            // coordinate before looking up/creating the fire.
                            const f32 sx = static_cast<f32>(static_cast<i32>(
                                unit.x + dx * tile_size));
                            const f32 sy = static_cast<f32>(static_cast<i32>(
                                unit.y + dy * tile_size));
                            if (sx < 0.0f || sy < 0.0f ||
                                sx >= world_width || sy >= world_height) {
                                continue;
                            }
                            if (!fire_at(sx, sy)) {
                                fire_spawns.push_back({sx, sy});
                                spawned = true;
                            }
                        }
                    }
                    if (spawned) unit.fire_spread_exhausted = false;
                }
            }
            unit.fire_animation_timer += delta_frames;
            if (unit.fire_animation_timer > 10.0f) {
                unit.fire_animation_timer = 0.0f;
            }
            if (unit.fire_intensity < 0.0f) {
                remove_unit(uid);
            }
            continue;
        }

        if (unit.unit_type != "damagingBorder" && unit.unit_type != "zoneMarker") {
            continue;
        }

        if (unit.border_initialized) {
            unit.border_initialized = false;
            Unit* existing = nullptr;
            for (World* world : all_worlds) {
                if (!world) continue;
                for (auto& [other_uid, other] : world->units_) {
                    if ((world == this && other_uid == uid) || !other.is_alive ||
                        other.zone_marker != unit.zone_marker ||
                        (other.unit_type != "damagingBorder" &&
                         other.unit_type != "zoneMarker")) continue;
                    existing = &other;
                    break;
                }
                if (existing) break;
            }
            if (existing) {
                existing->border_target_x = unit.x;
                existing->border_target_y = unit.y;
                existing->border_target_radius = unit.border_target_radius;
                remove_unit(uid);
                continue;
            }
            unit.border_target_x = unit.x;
            unit.border_target_y = unit.y;
        }

        if (unit.zone_marker) {
            unit.border_current_radius = unit.border_target_radius;
            unit.x = unit.border_target_x;
            unit.y = unit.border_target_y;
            continue;
        }

        if (unit.border_current_radius > unit.border_target_radius) {
            unit.border_shrink_speed += 0.00025f * delta_frames;
            // f.a(float) subtracts the newly accumulated speed once per
            // update; the frame delta is applied only to the acceleration.
            unit.border_current_radius -= unit.border_shrink_speed;
            unit.border_moving = true;

            const f32 dx = unit.border_target_x - unit.x;
            const f32 dy = unit.border_target_y - unit.y;
            const f32 center_distance = std::sqrt(dx * dx + dy * dy);
            if (center_distance > 1.0f) {
                f32 movement = unit.border_shrink_speed;
                movement = std::min(movement, center_distance * delta_frames);
                const f32 heading = jar_direction_degrees(
                    unit.x, unit.y, unit.border_target_x,
                    unit.border_target_y);
                unit.x += movement * jar_cosine_degrees(heading) * delta_frames;
                unit.y += movement * jar_sine_degrees(heading) * delta_frames;
            } else {
                unit.border_moving = false;
            }
        } else {
            unit.border_moving = false;
            unit.x += (unit.border_target_x - unit.x) *
                      0.003f * delta_frames;
            unit.y += (unit.border_target_y - unit.y) *
                      0.003f * delta_frames;
        }
        if (unit.border_current_radius < unit.border_target_radius) {
            unit.border_current_radius = unit.border_target_radius;
            unit.border_shrink_speed = 0.0f;
        }
        if (unit.border_target_radius < 0.0f) {
            remove_unit(uid);
            continue;
        }

        unit.border_damage_timer -= delta_frames;
        if (unit.border_damage_timer > 0.0f) continue;
        unit.border_damage_timer = 2.0f;
        const f32 radius = std::max(0.0f, unit.border_current_radius);
        const f32 radius_sq = radius * radius;
        for (World* target_world : all_worlds) {
            if (!target_world) continue;
            Vec<u32> killed;
            for (auto& [target_uid, target] : target_world->units_) {
                if (!target.is_alive || target.parent_uid != 0 ||
                    (target_world == this && target_uid == uid) ||
                    (is_map_effect_unit(target) &&
                     target.unit_type != "spreadingFire")) continue;
                const f32 dx = target.x - unit.x;
                const f32 dy = target.y - unit.y;
                if (dx * dx + dy * dy < radius_sq) continue;
                const f32 damage = (0.5f + target.hp * 0.002f +
                                    target.max_hp * 0.001f) * unit.border_damage_multiplier;
                target.apply_damage(damage, unit.uid, tick_);
                if (target.is_dead) killed.push_back(target_uid);
            }
            for (u32 target_uid : killed) {
                World* owner_world = target_world;
                owner_world->remove_unit(target_uid);
            }
        }
    }

    for (const auto& [x, y] : fire_spawns) {
        // Spreading the same tile from multiple fires is coalesced by the
        // occupancy check above for existing units; retain the JAR's one
        // canonical fire object per tile for this update as well.
        if (!fire_at(x, y)) spawn_unit("spreadingFire", x, y, true);
    }
}

} // namespace rwrl
