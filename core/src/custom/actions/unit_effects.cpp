#include "internal.h"

#include <tuple>

namespace rwrl {

void World::spawn_custom_unit_list_(
        Unit& source, const Vec<CustomUnitSpawn>& entries, bool produced,
        const Vec<World*>& other_worlds) {
    if (entries.empty()) return;

    i32 accepted_count = 0;
    i32 random_index = 0;
    bool over_cap = false;
    Vec<u32> produced_uids;

    auto total_live_units = [&]() {
        i32 count = 0;
        for (const auto& [_, candidate] : units_) {
            if (candidate.is_alive) ++count;
        }
        return count;
    };

    auto can_accept_cargo = [&](const Unit& transport, const Unit& cargo) {
        if (transport.uid == cargo.uid || !transport.is_alive ||
            !cargo.is_alive || !cargo.transportable ||
            transport.team_id != cargo.team_id ||
            transport.cargo_capacity <= 0 || transport.unloading_all) {
            return false;
        }
        i32 used = 0;
        for (u32 cargo_uid : transport.cargo_units) {
            const Unit* loaded = get_unit(cargo_uid);
            if (loaded) used += loaded->cargo_weight;
        }
        return used + cargo.cargo_weight <= transport.cargo_capacity;
    };

    for (const CustomUnitSpawn& entry : entries) {
        if (!over_cap && count_active_units() > unit_cap() + 300) {
            over_cap = true;
        }
        if (over_cap || total_live_units() > unit_cap() + 25000) continue;

        const UnitDef* definition = registry_.get(entry.unit_type);
        if (!definition) continue;

        for (i32 copy_index = 0; copy_index < entry.count; ++copy_index) {
            ++random_index;
            if (entry.spawn_chance < 1.0f) {
                const f32 roll = deterministic_unit_random(
                    source, tick_, 0.0f, 1.0f, random_index);
                if (roll > entry.spawn_chance) continue;
            }
            if (accepted_count >= entry.max_spawn_limit) continue;

            const f32 base_x = produced ? 0.0f : source.x;
            const f32 base_y = produced ? 0.0f : source.y;
            const f32 base_height = produced ? 0.0f : source.height;
            const f32 base_rotation = produced ? 0.0f : source.rotation;
            Unit* spawned = spawn_unit_from_def_(
                *definition, entry.unit_type, base_x, base_y, true);
            if (!spawned) continue;

            spawned->height = base_height + entry.offset_height;
            if (!spawned->is_building &&
                !entry.always_start_dir_at_zero) {
                spawned->rotation = base_rotation;
            }
            if (entry.tech_level != -1 && spawned->custom_unit) {
                spawned->tech_level = entry.tech_level;
            }

            f32 direction_offset = entry.offset_dir;
            if (entry.offset_random_dir != 0.0f) {
                direction_offset += deterministic_unit_random(
                    source, tick_, -entry.offset_random_dir,
                    entry.offset_random_dir, random_index * 4 + 3);
            }
            if (direction_offset != 0.0f) {
                spawned->rotation = normalize_angle(
                    spawned->rotation + direction_offset);
            }

            spawned->x += static_cast<f32>(copy_index);
            if (entry.offset_random_x != 0.0f) {
                spawned->x += deterministic_unit_random(
                    source, tick_, -entry.offset_random_x,
                    entry.offset_random_x, random_index * 2 + 1);
            }
            if (entry.offset_random_y != 0.0f) {
                spawned->y += deterministic_unit_random(
                    source, tick_, -entry.offset_random_y,
                    entry.offset_random_y, random_index * 3 + 2);
            }
            if (entry.grid_align) {
                spawned->x = pathfinder_.world_x(
                    pathfinder_.tile_x(spawned->x));
                spawned->y = pathfinder_.world_y(
                    pathfinder_.tile_y(spawned->y));
            }
            spawned->x += entry.offset_x;
            spawned->y += entry.offset_y;
            grid_.update(*spawned);
            if (spawned->is_building) {
                rebuild_local_buildings_();
            }

            // bp increments its shared maxSpawnLimit counter after offsets
            // but before skipIfOverlapping rejects the newly created object.
            ++accepted_count;

            bool invalid_placement = false;
            if (entry.skip_if_overlapping) {
                if (!spawned->is_building &&
                    !pathfinder_.is_passable_world(
                        spawned->x, spawned->y, spawned->move_type)) {
                    invalid_placement = true;
                }
                if (!invalid_placement) {
                    for (const auto& [other_uid, other] : units_) {
                        if (other_uid == spawned->uid || !other.is_alive ||
                            other.parent_uid != 0) {
                            continue;
                        }
                        const bool different_layer =
                            (spawned->move_type == MoveType::AIR) !=
                            (other.move_type == MoveType::AIR);
                        if (different_layer) continue;
                        const f32 minimum_distance =
                            spawned->radius + other.radius;
                        if (dist2(spawned->x, spawned->y,
                                  other.x, other.y) <
                            minimum_distance * minimum_distance) {
                            invalid_placement = true;
                            break;
                        }
                    }
                }
            }

            if (invalid_placement) {
                const u32 discarded_uid = spawned->uid;
                remove_unit(discarded_uid);
                units_.erase(discarded_uid);
                if (units_built_ > 0) --units_built_;
                continue;
            }

            if (entry.falling && spawned->custom_unit) {
                spawned->height = 170.0f;
                spawned->vertical_speed = 1.5f;
            }

            const CustomResourceDelta& resources = entry.add_resources;
            if (custom_actions::has_resource_delta(resources)) {
                credits_ += static_cast<f32>(resources.credits);
                spawned->energy += resources.energy;
                spawned->hp += resources.hp;
                spawned->shield += resources.shield;
                spawned->ammo += resources.ammo;
                spawned->custom_flags &= ~resources.unset_flags;
                spawned->custom_flags |= resources.set_flags;
                clamp_custom_resources_(*spawned);
            }

            for (i32 transfer = 0;
                 transfer < entry.transported_units_to_transfer;
                 ++transfer) {
                i32 selected_slot = -1;
                for (std::size_t slot = source.cargo_units.size();
                     slot-- > 0;) {
                    Unit* cargo = get_unit(source.cargo_units[slot]);
                    if (cargo && can_accept_cargo(*spawned, *cargo)) {
                        selected_slot = static_cast<i32>(slot);
                        break;
                    }
                }
                if (selected_slot < 0) break;

                const u32 cargo_uid = source.cargo_units[
                    static_cast<std::size_t>(selected_slot)];
                source.cargo_units.erase(
                    source.cargo_units.begin() + selected_slot);
                Unit* cargo = get_unit(cargo_uid);
                if (!cargo) continue;
                cargo->parent_uid = 0;
                cargo->x = spawned->x;
                cargo->y = spawned->y;
                cargo->rotation = spawned->rotation;
                cargo->target_uid = 0;
                cargo->target_team_id = -1;
                cargo->order_queue.clear();
                cargo->active_order_elapsed_frames = 0.0f;
                cargo->order_point_dwell_frames = 0.0f;
                cargo->has_move_target = false;
                cargo->arrived = true;
                cargo->using_path = false;
                cargo->waypoints.clear();
                cargo->current_waypoint = 0;
                clear_navigation_orders(*cargo);
                grid_.add(*cargo);
                load_into(*spawned, *cargo);
                if (cargo->parent_uid != spawned->uid) {
                    remove_unit(cargo_uid);
                }
            }

            if (entry.copy_waypoints_from_self) {
                clear_current_order_state_(*spawned);
                spawned->order_queue = source.order_queue;
                spawned->active_order_elapsed_frames = 0.0f;
                spawned->order_point_dwell_frames = 0.0f;
                if (!spawned->order_queue.empty()) {
                    activate_order_(
                        *spawned, spawned->order_queue.front(), other_worlds);
                }
            }

            if (produced) produced_uids.push_back(spawned->uid);
        }
    }

    // custom.a.a.k applies custom.j.E/F only after bp has finished creating
    // the full output list. These are the default factory-exit values used
    // when the custom definition has no explicit exit offsets.
    for (u32 produced_uid : produced_uids) {
        Unit* spawned = get_unit(produced_uid);
        if (!spawned || !spawned->is_alive) continue;
        spawned->rotation = 90.0f;
        spawned->x = source.x;
        spawned->y = source.y;
        if (!source.is_building) spawned->height = source.height;
        grid_.update(*spawned);

        const f32 side = source.custom_factory_exit_side ? 0.0f : 1.0f;
        const f32 exit_x = source.x +
            jar_cosine_degrees(spawned->rotation) * 70.0f -
            jar_sine_degrees(spawned->rotation) * side;
        const f32 exit_y = source.y +
            jar_sine_degrees(spawned->rotation) * 70.0f +
            jar_cosine_degrees(spawned->rotation) * side;
        UnitOrder exit_order;
        exit_order.type = WeaponType::MOVE;
        exit_order.x = exit_x;
        exit_order.y = exit_y;
        const bool activate_now = spawned->order_queue.empty();
        if (spawned->order_queue.size() < 29) {
            spawned->order_queue.push_back(exit_order);
            if (activate_now) {
                activate_order_(
                    *spawned, spawned->order_queue.front(), other_worlds);
            }
        }
        source.custom_factory_exit_side =
            !source.custom_factory_exit_side;
    }
}

bool World::apply_take_resources_(
        Unit& source, const CustomTakeResourcesEffect& effect,
        const Vec<World*>& other_worlds,
        const CustomActionContext& context, i32 recursion_depth) {
    if (!effect.enabled) return false;

    struct Candidate {
        World* world = nullptr;
        Unit* unit = nullptr;
    };
    Vec<Candidate> candidates;

    if (effect.include_units_in_transport) {
        for (u32 uid : source.cargo_units) {
            Unit* cargo = get_unit(uid);
            if (cargo && cargo->is_alive) candidates.push_back({this, cargo});
        }
    }

    if (effect.include_parent && source.parent_uid != 0) {
        // custom.a.a.m only reaches the cO (transport/orderable parent)
        // branch. Its decompiled cN fallback appends null and is not a usable
        // parent reference.
        if (Unit* parent = get_unit(source.parent_uid)) {
            candidates.push_back({this, parent});
        }
    }

    if (effect.include_units_within_range > 0.0f) {
        Vec<Candidate> ranged;
        Vec<World*> worlds;
        worlds.reserve(other_worlds.size() + 1);
        worlds.push_back(this);
        for (World* world : other_worlds) {
            if (world && world != this) worlds.push_back(world);
        }

        auto relation_matches = [&](const World& candidate_world) {
            const bool own = candidate_world.team_id_ == team_id_;
            const bool candidate_neutral = candidate_world.team_id_ == -1;
            const bool source_neutral = team_id_ == -1;
            const bool allied = candidate_neutral || source_neutral
                ? candidate_neutral && source_neutral
                : candidate_world.alliance_id_ == alliance_id_;
            switch (effect.range_team) {
            case CustomTakeResourcesTeam::OWN:
                return own;
            case CustomTakeResourcesTeam::ALLY:
                return allied;
            case CustomTakeResourcesTeam::ALLY_NOT_OWN:
                return !own && allied;
            case CustomTakeResourcesTeam::ENEMY:
                return !candidate_neutral && !source_neutral && !allied;
            case CustomTakeResourcesTeam::NEUTRAL:
                return candidate_neutral;
            case CustomTakeResourcesTeam::ANY:
                return true;
            case CustomTakeResourcesTeam::NOT_OWN:
                return !own;
            }
            return false;
        };

        const f32 range_squared =
            effect.include_units_within_range *
            effect.include_units_within_range;
        for (World* candidate_world : worlds) {
            if (!relation_matches(*candidate_world)) continue;
            for (auto& [_, candidate] : candidate_world->units_) {
                if (!candidate.is_alive || !candidate.is_complete()) continue;
                if (dist2(source.x, source.y, candidate.x, candidate.y) >=
                    range_squared) {
                    continue;
                }
                ranged.push_back({candidate_world, &candidate});
            }
        }

        // The JAR's global geo index scans x cells, y cells, then each cell's
        // insertion list. Global unit ids preserve that list order for units
        // which have not crossed a cell boundary, the normal action case.
        std::sort(ranged.begin(), ranged.end(), [](const Candidate& lhs,
                                                   const Candidate& rhs) {
            constexpr i32 cells = EngineConstants::SPATIAL_GRID_SIZE;
            auto key = [&](const Candidate& value) {
                const f32 width = std::max(
                    1.0f, value.world->map_width() *
                              value.world->tile_size());
                const f32 height = std::max(
                    1.0f, value.world->map_height() *
                              value.world->tile_size());
                const i32 cx = clamp_i(
                    static_cast<i32>(value.unit->x * cells / width),
                    0, cells - 1);
                const i32 cy = clamp_i(
                    static_cast<i32>(value.unit->y * cells / height),
                    0, cells - 1);
                return std::tuple<i32, i32, u32>(cx, cy, value.unit->uid);
            };
            return key(lhs) < key(rhs);
        });
        candidates.insert(candidates.end(), ranged.begin(), ranged.end());
    }

    auto has_all = [](const Candidate& candidate,
                      const CustomResourceDelta& resources) {
        if (!candidate.world || !candidate.unit) return false;
        const Unit& unit = *candidate.unit;
        if (resources.credits > 0 &&
            candidate.world->credits_ < resources.credits) return false;
        if (resources.energy > 0.0f && unit.energy < resources.energy)
            return false;
        if (resources.hp > 0.0f && unit.hp < resources.hp) return false;
        if (resources.shield > 0.0f && unit.shield < resources.shield)
            return false;
        if (resources.ammo > 0 && unit.ammo < resources.ammo) return false;
        return true;
    };

    for (std::size_t index = candidates.size(); index-- > 0;) {
        const Candidate& candidate = candidates[index];
        if ((effect.exclude_units_without_all_resources &&
             !has_all(candidate, effect.resources)) ||
            (effect.has_exclude_units_with_resources &&
             has_all(candidate, effect.exclude_units_with_resources))) {
            candidates.erase(candidates.begin() +
                             static_cast<std::ptrdiff_t>(index));
        }
    }

    auto apply_bundle = [&](Candidate target,
                            const CustomResourceDelta& resources,
                            f32 sign) {
        target.world->credits_ += sign *
            static_cast<f32>(resources.credits);
        target.unit->energy += sign * resources.energy;
        target.unit->hp += sign * resources.hp;
        target.unit->shield += sign * resources.shield;
        target.unit->ammo += static_cast<i32>(sign) * resources.ammo;
        // Resource-bundle flag operations are state assignments, so both the
        // target subtraction and source addition use the same unset/set order.
        target.unit->custom_flags &= ~resources.unset_flags;
        target.unit->custom_flags |= resources.set_flags;
        target.world->clamp_custom_resources_(*target.unit);
    };

    i32 processed = 0;
    bool collected = false;
    for (const Candidate& candidate : candidates) {
        if (!candidate.world || !candidate.unit) continue;
        if (!effect.keep_resources_on_target) {
            apply_bundle(candidate, effect.resources, -1.0f);
        }
        if (!effect.discard_collected) {
            apply_bundle({this, &source}, effect.resources, 1.0f);
        }
        if (!effect.trigger_action_for_each.empty()) {
            CustomActionContext each_context;
            each_context.has_position = true;
            each_context.x = candidate.unit->x;
            each_context.y = candidate.unit->y;
            each_context.target_uid = candidate.unit->uid;
            each_context.target_team_id = candidate.world->team_id_;
            execute_custom_action_(
                source, effect.trigger_action_for_each, other_worlds,
                nullptr, &each_context, recursion_depth + 1);
        }
        collected = true;
        if (++processed >= effect.max_units) break;
    }

    const Str& followup = collected
        ? effect.trigger_action_if_any_collected
        : effect.trigger_action_if_none_collected;
    if (!followup.empty()) {
        execute_custom_action_(
            source, followup, other_worlds, nullptr, &context,
            recursion_depth + 1);
    }
    return collected;
}

}  // namespace rwrl
