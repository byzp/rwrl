#include "world_internal.h"

#include <tuple>

namespace rwrl {
void World::update_unit_runtime_precombat(f32 dt, u32 only_unit_uid) {
    Unit* unit = get_unit(only_unit_uid);
    if (!unit || !unit->is_alive) return;

    // y.a(float) clears ay at the start of every object turn. ax makes every
    // newly created orderable object participate in one initial geo update,
    // after it enters the active bT() branch. Incomplete or already loaded
    // objects keep ax pending until their first active turn.
    unit->collision_moved = unit->collision_first_update && unit->is_active();
    if (unit->is_active()) unit->collision_first_update = false;

    const f32 game_frames = dt * EngineConstants::FRAME_RATE;
    if (unit->damage_smoothing != 0.0f) {
        const f32 decay = unit->max_hp * unit->damage_smoothing_velocity *
                          0.005f * game_frames;
        unit->damage_smoothing = approach_value(
            unit->damage_smoothing, 0.0f, decay);
        unit->damage_smoothing_velocity +=
            game_frames + 0.2f * unit->damage_smoothing_velocity * game_frames;
        if (unit->damage_smoothing == 0.0f) {
            unit->damage_smoothing_velocity = 0.0f;
        }
    }
    // y.a(float) updates ap.d and returns idle turrets before k(float) handles
    // orders, targeting, firing, and physical movement for an active object.
    if (!unit->is_active()) return;
    const bool idle_movement_prediction =
        unit->idle_prediction_command_tick == tick_
            ? unit->idle_prediction_before_command
            : unit->movement_prediction_enabled;
    unit->idle_prediction_command_tick = -1;
    for (i32 slot_index = 0;
         slot_index < static_cast<i32>(unit->weapon_slots.size());
         ++slot_index) {
        WeaponSlot& slot = unit->weapon_slots[
            static_cast<std::size_t>(slot_index)];
        if (slot.turret_idle_timer == 0.0f) {
            const f32 idle_direction = weapon_idle_direction(
                *unit, slot, unit->weapon_slots);
            const bool reset_to_idle = update_weapon_idle_behavior(
                *unit, slot, slot_index, game_frames, tick_,
                unit->weapon_slots, idle_movement_prediction);
            if (reset_to_idle &&
                slot.turret_angle != idle_direction) {
                const f32 error = shortest_angle_delta(
                    slot.turret_angle, idle_direction);
                if (std::abs(error) < 0.5f) {
                    slot.turret_idle_timer = 20.0f;
                    slot.turret_turn_velocity = 0.0f;
                } else {
                    turn_weapon_slot_toward(
                        *unit, slot, idle_direction, game_frames);
                }
            }
        } else {
            slot.turret_idle_timer = approach_value(
                slot.turret_idle_timer, 0.0f, game_frames);
        }
    }
}

void World::apply_unit_collision_impulse(u32 only_unit_uid) {
    Unit* unit = get_unit(only_unit_uid);
    if (!unit || !unit->is_alive ||
        (unit->collision_impulse_x == 0.0f &&
         unit->collision_impulse_y == 0.0f)) {
        return;
    }

    // y.a(float) consumes bZ/ca after k(float) has selected commands, aim,
    // and ordinary movement, but before its single map clamp pass.
    unit->x += std::clamp(unit->collision_impulse_x, -9.0f, 9.0f);
    unit->y += std::clamp(unit->collision_impulse_y, -9.0f, 9.0f);
    unit->collision_impulse_x = 0.0f;
    unit->collision_impulse_y = 0.0f;
    unit->collision_moved = true;
    grid_.update(*unit);
}

void World::update_unit_cooldowns(f32 dt, u32 only_unit_uid) {
    Unit* unit = get_unit(only_unit_uid);
    if (!unit || !unit->is_active_for_update(tick_)) return;
    const f32 game_frames = dt * EngineConstants::FRAME_RATE;
    if (unit->cooldown_ticks > 0) {
        unit->cooldown_remainder += game_frames;
        const i32 elapsed_frames = static_cast<i32>(unit->cooldown_remainder);
        if (elapsed_frames > 0) {
            unit->cooldown_ticks = std::max(
                0, unit->cooldown_ticks - elapsed_frames);
            unit->cooldown_remainder -= static_cast<f32>(elapsed_frames);
        }
    }
    for (WeaponSlot& slot : unit->weapon_slots) {
        if (slot.cooldown_remainder <= 0.0f && slot.cooldown_ticks <= 0) {
            slot.cooldown_remainder = 0.0f;
            continue;
        }
        // ap.e is a continuous float in the JAR. cooldown_ticks is only its
        // ceil() projection used by diagnostics and legacy callers.
        slot.cooldown_remainder = std::max(
            0.0f, slot.cooldown_remainder - game_frames);
        slot.cooldown_ticks = static_cast<i32>(
            std::ceil(slot.cooldown_remainder));
    }
    if (!unit->weapon_slots.empty()) {
        unit->cooldown_ticks = unit->weapon_slots.front().cooldown_ticks;
    }
}

void World::update_unit_passive_health(f32 dt, u32 only_unit_uid) {
    Unit* unit = get_unit(only_unit_uid);
    if (!unit || !unit->is_alive) return;
    const f32 game_frames = dt * EngineConstants::FRAME_RATE;
    // These are subclass updates in the JAR and therefore happen after the
    // unit has handled its command/combat and physical movement.
    if (unit->unit_type == "editorOrBuilder") {
        unit->hp = unit->max_hp;
    }
    if (!unit->is_active()) return;
    f32 repair_per_frame = 0.0f;
    if (unit->unit_type == "turret_flamethrower") {
        repair_per_frame = 0.15f;
    } else if (unit->unit_type == "turretT3") {
        repair_per_frame = 0.1f;
    }
    if (repair_per_frame > 0.0f && unit->hp < unit->max_hp) {
        unit->hp = std::min(
            unit->max_hp, unit->hp + repair_per_frame * game_frames);
    }
    if (unit->custom_unit && unit->self_regen_per_frame != 0.0f &&
        (unit->hp < unit->max_hp || unit->self_regen_per_frame < 0.0f)) {
        // custom.j adds its unchanged as.p * deltaFrames before clamping.
        // Converting p to seconds first changes low-health float additions.
        unit->hp += unit->self_regen_per_frame * game_frames;
        if (unit->hp > unit->max_hp) unit->hp = unit->max_hp;
    }
}

void World::update_unit_service_decay(
        f32 delta_frames, u32 only_unit_uid) {
    Unit* unit = get_unit(only_unit_uid);
    if (!unit || !unit->is_alive) return;
    // y.b(l,float) holds every ap.f when any u(i) had zero reload before
    // firing, or the previous nano-beam flag aN has a current X() target.
    // A combat turret can therefore preserve the very first nano increment
    // before e.b.a() has set aN for the following object update.
    bool live_service_target = unit->service_active_this_frame;
    if (live_service_target && unit->service_aim_target_uid != 0) {
        World* target_world = find_order_target_world_(
            unit->service_aim_target_uid, peer_worlds_);
        const Unit* target = target_world
            ? target_world->get_unit(unit->service_aim_target_uid) : nullptr;
        // RECLAIM can kill the aimed target earlier in this object update.
        // X() then returns null although h remains true until the next turn.
        // Completing construction can clear the combat target's team while
        // the REPAIR waypoint still retains its live construction target.
        live_service_target = target && target->is_alive && !target->is_dead;
    }
    const bool combat_holds_warmup = std::any_of(
        unit->weapon_slots.begin(), unit->weapon_slots.end(),
        [](const WeaponSlot& slot) { return slot.warmup_hold_this_frame; });
    if (!combat_holds_warmup &&
        !(unit->service_active_previous_frame && live_service_target) &&
        unit->service_warmup_progress > 0.0f) {
        unit->service_warmup_progress = std::max(
            0.0f,
            unit->service_warmup_progress -
                unit->service_warmup_decay * delta_frames);
    }
    // A repair plugin can insert a new waypoint after y.b(l,delta) has
    // cooled a completed beam. The later nano update sees retained h and
    // the new X() target, so aN is live for the following object turn.
    const bool automatic_beam_restart = unit->service_aim_ready &&
        unit->activate_order_next_frame && !unit->order_queue.empty() &&
        unit->order_queue.front().type == WeaponType::REPAIR &&
        unit->order_queue.front().automatic;
    unit->service_active_previous_frame =
        live_service_target || automatic_beam_restart;
    unit->service_range_extended =
        unit->service_active_this_frame || automatic_beam_restart;
    if (automatic_beam_restart) {
        unit->service_aim_target_uid = unit->order_queue.front().target_uid;
    } else if (!unit->service_active_this_frame) {
        unit->service_aim_ready = false;
        unit->service_aim_target_uid = 0;
    }
}

void World::update_shields(f32 dt) {
    update_unit_shields(dt, 0);
}

void World::update_unit_shields(f32 dt, u32 only_unit_uid) {
    for (auto unit_update_it = only_unit_uid == 0
             ? units_.begin() : units_.find(only_unit_uid);
         unit_update_it != units_.end();
         unit_update_it = only_unit_uid == 0
             ? std::next(unit_update_it) : units_.end()) {
        auto& [uid, u] = *unit_update_it;
        if (!u.is_alive || !u.is_complete() || u.parent_uid != 0) continue;
        u.update_shield(dt);
    }
}

void World::update_energy(f32 dt) {
    update_unit_energy(dt, 0);
}

void World::update_unit_energy(f32 dt, u32 only_unit_uid,
                               const UnitDef* update_definition) {
    for (auto unit_update_it = only_unit_uid == 0
             ? units_.begin() : units_.find(only_unit_uid);
         unit_update_it != units_.end();
         unit_update_it = only_unit_uid == 0
             ? std::next(unit_update_it) : units_.end()) {
        auto& [uid, u] = *unit_update_it;
        if (!u.is_alive || !u.is_complete() || u.parent_uid != 0) continue;
        if (u.custom_unit) {
            if (u.max_energy == 0.0f) continue;
            // custom.j captures lVar before super.a(), which can execute a
            // conversion waypoint. Its live y.d changes immediately, while
            // the regeneration metadata remains from the original definition.
            const f32 rate = u.energy_disabled
                ? (update_definition ? update_definition->energy_recharge_regen_per_frame
                                     : u.energy_recharge_regen_per_frame)
                : (update_definition ? update_definition->energy_regen_per_frame
                                     : u.energy_regen_per_frame);
            if (u.energy < u.max_energy || rate < 0.0f) {
                u.energy += rate * (dt * EngineConstants::FRAME_RATE);
            }
            if (u.energy >= u.max_energy) {
                u.energy = u.max_energy;
                u.energy_disabled = false;
            }
            if (u.energy <= 0.0f) {
                if (update_definition ? update_definition->die_on_zero_energy
                                      : u.die_on_zero_energy) {
                    // bv() runs the normal death effects and returns before
                    // custom.j can clamp cB or advance production/height.
                    u.is_dead = true;
                    remove_unit(uid);
                    continue;
                }
                u.energy = 0.0f;
            }
            continue;
        }
        if (u.unit_type == "laserDefence") {
            // p.a() multiplies the base and T2 addition separately before
            // adding them. Preserve that float sequence and frame units.
            const f32 frames = dt * EngineConstants::FRAME_RATE;
            f32 amount = 0.0004f * frames;
            if (u.tech_level >= 2) amount += 0.0002f * frames;
            if (u.energy < u.max_energy) {
                u.energy = std::min(u.max_energy, u.energy + amount);
            }
            continue;
        }
        if (u.max_energy > 0 && u.energy < u.max_energy) {
            u.energy = std::min(u.max_energy, u.energy + u.energy_regen_rate * dt);
        }
    }
}

void World::update_laser_defence(f32 dt, const Vec<World*>& enemy_worlds) {
    update_laser_defence_(dt, enemy_worlds, 0);
}

void World::update_unit_laser_defence(
    f32 dt, u32 unit_uid, const Vec<World*>& enemy_worlds) {
    update_laser_defence_(dt, enemy_worlds, unit_uid);
}

void World::intercept_tagged_projectile_(
        Unit& unit, WeaponSlot& slot, const Vec<World*>& projectile_worlds) {
    // custom.j.a(bn) checks resources, without aiming, reload or canShoot.
    if ((slot.energy_usage > 0.0f &&
         (slot.energy_usage > unit.energy || unit.energy_disabled)) ||
        (slot.intercept_ammo_usage > 0 && unit.ammo < slot.intercept_ammo_usage) ||
        (unit.custom_flags & slot.intercept_required_flags) != slot.intercept_required_flags ||
        (unit.custom_flags & slot.intercept_missing_flags) != 0) return;

    World* selected_world = nullptr;
    u32 selected_uid = 0;
    u64 selected_order = 0;
    for (World* owner_world : projectile_worlds) {
        for (auto& [pid, projectile] : owner_world->projectiles_) {
            if (projectile.tags.empty() || projectile.height <= slot.intercept_projectile_height ||
                projectile.lifetime <= 0.0f ||
                std::none_of(projectile.tags.begin(), projectile.tags.end(),
                    [&](const Str& tag) {
                        return std::find(slot.intercept_projectile_tags.begin(),
                            slot.intercept_projectile_tags.end(), tag) != slot.intercept_projectile_tags.end();
                    })) continue;
            if (dist2(unit.x, unit.y, projectile.x, projectile.y) >=
                slot.intercept_projectile_distance * slot.intercept_projectile_distance) continue;
            const f32 ground_distance = slot.intercept_ground_target_distance;
            if (ground_distance >= 0.0f &&
                dist2(unit.x, unit.y,
                    projectile.target_x - projectile.target_offset_x,
                    projectile.target_y - projectile.target_offset_y) >=
                    ground_distance * ground_distance) continue;
            // A missing owner is eligible, while an allied or same-player
            // owner is rejected. Tags need not describe a nuclear weapon.
            if (projectile.owner_uid != 0 && is_allied_with(*owner_world)) continue;
            bool claimed = false;
            for (World* world : projectile_worlds) {
                for (const auto& [_, other] : world->projectiles_) {
                    if (other.target_projectile_uid == pid &&
                        other.target_team_id == owner_world->team_id_) {
                        claimed = true;
                        break;
                    }
                }
                if (claimed) break;
            }
            if (!claimed && projectile.creation_order >= selected_order) {
                selected_order = projectile.creation_order;
                selected_world = owner_world;
                selected_uid = pid;
            }
        }
    }
    if (!selected_world) return;
    const Projectile& selected = selected_world->projectiles_.at(selected_uid);
    // custom.b.k pays bn's price and calls the ordinary custom factory.
    // It deliberately leaves the turret's reload and odd-shot state alone.
    if (slot.energy_usage > 0.0f) {
        unit.energy -= slot.energy_usage;
        if (unit.energy < slot.energy_usage && unit.energy_needs_full_recharge) {
            unit.energy_disabled = true;
        }
    }
    unit.ammo -= slot.intercept_ammo_usage;
    Unit point{};
    point.x = selected.x;
    point.y = selected.y;
    const u32 pid = create_weapon_projectile_(unit, slot, point, true);
    auto created = projectiles_.find(pid);
    if (created != projectiles_.end()) {
        created->second.is_antinuke = true;
        created->second.impact_at_target = true;
        created->second.target_team_id = selected_world->team_id_;
        created->second.target_projectile_uid = selected_uid;
    }
}

void World::update_laser_defence_(f32 dt,
                                  const Vec<World*>& enemy_worlds,
                                  u32 only_unit_uid) {
    (void)dt;
    for (auto unit_update_it = only_unit_uid == 0
             ? units_.begin() : units_.find(only_unit_uid);
         unit_update_it != units_.end();
         unit_update_it = only_unit_uid == 0
             ? std::next(unit_update_it) : units_.end()) {
        auto& [uid, unit] = *unit_update_it;
        if (!unit.is_active()) continue;
        if (unit.custom_unit) {
            if (std::none_of(unit.weapon_slots.begin(), unit.weapon_slots.end(),
                    [](const WeaponSlot& slot) {
                        return slot.laser_defence_enabled || !slot.intercept_projectile_tags.empty();
                    })) continue;
        } else {
            if (unit.unit_type != "laserDefence" || unit.max_energy <= 0.0f) continue;
            if (unit.energy >= unit.max_energy) unit.energy_disabled = false;
        }
        Vec<World*> projectile_worlds{this};
        for (World* other_world : enemy_worlds) {
            if (other_world && other_world != this) {
                projectile_worlds.push_back(other_world);
            }
        }
        const auto intercept = [&](f32 origin_x, f32 origin_y,
                                   f32 range, f32 energy_cost) {
            World* selected_world = nullptr;
            u32 selected_pid = 0;
            u64 selected_order = std::numeric_limits<u64>::max();
            // Native p.a(...) returns after the first eligible global-list entry.
            for (World* owner_world : projectile_worlds) {
                for (auto& [pid, projectile] : owner_world->projectiles_) {
                    if (!projectile.active || projectile.laser_defence_excluded ||
                        projectile.laser_defence_immune) continue;
                    if (!(projectile.age_frames > 7.0f ||
                          (projectile.age_frames > 2.0f &&
                           projectile.speed_per_frame > 8.0f))) {
                        continue;
                    }
                    if (dist2(origin_x, origin_y, projectile.x, projectile.y) >=
                        range * range) continue;
                    if (projectile.height < -1.0f) continue;

                    // p.a(...) protects a projectile's live target when that
                    // target is allied with the defence unit. Otherwise it only
                    // intercepts projectiles whose owner is currently hostile.
                    // This distinction matters after diplomacy changes and for
                    // neutral projectile owners.
                    bool protects_allied_target = false;
                    if (projectile.target_uid != 0) {
                        World* target_world = nullptr;
                        if (projectile.target_team_id == team_id_) {
                            target_world = this;
                        } else {
                            for (World* candidate : enemy_worlds) {
                                if (candidate &&
                                    candidate->team_id_ == projectile.target_team_id) {
                                    target_world = candidate;
                                    break;
                                }
                            }
                        }
                        protects_allied_target = target_world &&
                            is_allied_with(*target_world);
                    }
                    if (!protects_allied_target &&
                        (owner_world == this || !is_enemy_with(*owner_world))) {
                        continue;
                    }
                    if (projectile.creation_order < selected_order) {
                        selected_order = projectile.creation_order;
                        selected_world = owner_world;
                        selected_pid = pid;
                    }
                }
            }

            if (!selected_world) return;
            auto projectile_it = selected_world->projectiles_.find(selected_pid);
            if (projectile_it == selected_world->projectiles_.end()) return;
            projectile_it->second.intercept_hp -= 1.01f;
            unit.energy -= energy_cost;
            if (projectile_it->second.intercept_hp <= 0.0f) {
                // p.a(...) calls game.f.d(), which sets aS but does not remove the
                // object immediately. Its already-scheduled object turn still
                // runs once later in this frame.
                projectile_it->second.active = false;
            }
        };
        if (unit.custom_unit) {
            // custom.b.k runs every configured turret, including canShoot=false
            // slots, before custom.j regenerates energy. Exactly zero leaves
            // v unset; only overspending (<0) starts the full-recharge lock.
            for (i32 index = 0; index < static_cast<i32>(unit.weapon_slots.size()); ++index) {
                WeaponSlot& slot = unit.weapon_slots[index];
                if (slot.laser_defence_enabled && unit.energy > 0.0f && !unit.energy_disabled) {
                    const auto origin = custom_weapon_muzzle_origin(unit, index);
                    const f32 range = slot.limiting_range < 99999.0f
                        ? slot.limiting_range : unit.weapon_range;
                    intercept(origin.first, origin.second, range, slot.laser_defence_energy_usage);
                    if (unit.energy < 0.0f) {
                        unit.energy = 0.0f;
                        unit.energy_disabled = true;
                    }
                }
                if (!slot.intercept_projectile_tags.empty()) {
                    intercept_tagged_projectile_(unit, slot, projectile_worlds);
                }
            }
        } else if (!unit.energy_disabled && unit.energy > 0.0f) {
            intercept(unit.x, unit.y - 13.0f, unit.weapon_range, unit.energy_per_shot);
            if (unit.energy < 0.0f) {
                unit.energy = 0.0f;
                unit.energy_disabled = true;
            }
        }
    }
}

void World::update_ammo(f32 dt, f32 speed_mult) {
    update_unit_ammo(dt, speed_mult, 0);
}

void World::update_unit_ammo(
        f32 dt, f32 speed_mult, u32 only_unit_uid) {
    for (auto unit_update_it = only_unit_uid == 0
             ? units_.begin() : units_.find(only_unit_uid);
         unit_update_it != units_.end();
         unit_update_it = only_unit_uid == 0
             ? std::next(unit_update_it) : units_.end()) {
        auto& [uid, u] = *unit_update_it;
        if (!u.is_active()) continue;
        if (u.is_nuke || u.is_antinuke || u.ammo_max <= 1 || u.ammo >= u.ammo_max) continue;
        u.ammo_reload_remainder += dt * EngineConstants::FRAME_RATE * speed_mult;
        const i32 elapsed_frames = static_cast<i32>(u.ammo_reload_remainder);
        if (elapsed_frames == 0) continue;
        u.ammo_reload_remainder -= static_cast<f32>(elapsed_frames);
        u.ammo_reload_timer -= elapsed_frames;
        if (u.ammo_reload_timer <= 0) {
            u.ammo = std::min(u.ammo + 1, u.ammo_max);
            u.ammo_reload_timer = EngineConstants::AMMO_RELOAD_TICKS;
        }
    }
}

void World::update_behaviors(World& enemy_world) {
    // Canonical JAR units perform behavior-specific targeting and firing in
    // their own update. This legacy 250ms hook must not fire a second time;
    // retain the entry point for non-canonical callers only.
    (void)enemy_world;
}

void World::update_patrols() {
    for (auto& [uid, unit] : units_) {
        if (!unit.is_active() || !unit.has_patrol) continue;
        if (unit.arrived && !unit.has_move_target && !unit.patrol_points.empty()) {
            auto& pt = unit.patrol_points[unit.patrol_index];
            move_unit_to(unit, pt.first, pt.second);
            unit.patrol_index = (unit.patrol_index + 1) % static_cast<i32>(unit.patrol_points.size());
        }
    }
}

void World::update_builder_behavior() {
    // y.k() dispatches BUILD/REPAIR once during the builder's object turn.
    // The per-unit build and command passes already perform that work. A
    // second periodic dispatch can request a REPAIR path in the same frame
    // that y.f() placed the building and returned without a proposal.
}

void World::update_repair_bay(f32 delta_frames, u32 only_unit_uid) {
    const Vec<World*> no_other_worlds;
    update_repair_bay(delta_frames, only_unit_uid, no_other_worlds);
}

void World::update_repair_bay(f32 delta_frames, u32 only_unit_uid,
                              const Vec<World*>& other_worlds) {
    update_unit_auto_repair(delta_frames, only_unit_uid, other_worlds);
}

void World::update_unit_auto_repair(f32 delta_frames, u32 only_unit_uid,
                                    const Vec<World*>& other_worlds) {
    for (auto unit_update_it = only_unit_uid == 0
             ? units_.begin() : units_.find(only_unit_uid);
         unit_update_it != units_.end();
         unit_update_it = only_unit_uid == 0
             ? std::next(unit_update_it) : units_.end()) {
        auto& [uid, unit] = *unit_update_it;
        if (!unit.is_active()) continue;
        const bool stock_bay = !unit.custom_unit &&
            unit.is_building && unit.unit_type == "repairbay";
        if (!stock_bay && !(unit.custom_unit && unit.auto_repair)) continue;

        // r.d and custom.j.u advance while busy. Both plugins scan only
        // when aq() sees no waypoint and the elapsed time exceeds 40.
        unit.auto_repair_timer_frames += std::max(0.0f, delta_frames);
        if (!unit.order_queue.empty() ||
            unit.auto_repair_timer_frames <= EngineConstants::REPAIRBAY_SCAN_INTERVAL) continue;
        unit.auto_repair_timer_frames = 0.0f;
        const f32 scan_range = service_range(unit);
        const f32 scan_range_sq = scan_range * scan_range;
        const f32 min_x = unit.x - scan_range;
        const f32 max_x = unit.x + scan_range;
        const f32 min_y = unit.y - scan_range;
        const f32 max_y = unit.y + scan_range;
        const i32 cell_width = std::max(1,
            static_cast<i32>(map_width() * tile_size()) / EngineConstants::SPATIAL_GRID_SIZE);
        const i32 cell_height = std::max(1,
            static_cast<i32>(map_height() * tile_size()) / EngineConstants::SPATIAL_GRID_SIZE);
        const auto cell = [](f32 coordinate, f32 inverse_size) {
            return clamp_i(static_cast<i32>(coordinate * inverse_size),
                           0, EngineConstants::SPATIAL_GRID_SIZE - 1);
        };
        const i32 min_cell_x = cell(min_x, 1.0f / static_cast<f32>(cell_width));
        const i32 max_cell_x = cell(max_x, 1.0f / static_cast<f32>(cell_width));
        const i32 min_cell_y = cell(min_y, 1.0f / static_cast<f32>(cell_height));
        const i32 max_cell_y = cell(max_y, 1.0f / static_cast<f32>(cell_height));

        World* selected_world = nullptr;
        Unit* selected = nullptr;
        struct RepairCandidate {
            World* world;
            Unit* unit;
        };
        Vec<RepairCandidate> candidates;
        Vec<World*> candidate_worlds{this};
        for (World* other_world : other_worlds) {
            if (other_world && other_world != this &&
                is_allied_with(*other_world)) {
                candidate_worlds.push_back(other_world);
            }
        }
        for (World* candidate_world : candidate_worlds) {
            for (auto& [candidate_uid, other] : candidate_world->units_) {
                if ((candidate_world == this && candidate_uid == unit.uid) ||
                    !other.is_alive || other.parent_uid != 0 ||
                    other.unit_type == "crystalResource" ||
                    !can_repair_target(unit, other)) continue;
                if (other.hp >= other.max_hp && other.build_progress >= 1.0f) continue;
                // cc.a() queries the previous global index, then filters
                // current coordinates. A newly created or moved-in object
                // cannot bypass its resident-cell membership.
                if (other.collision_spatial_cell_x < min_cell_x ||
                    other.collision_spatial_cell_x > max_cell_x ||
                    other.collision_spatial_cell_y < min_cell_y ||
                    other.collision_spatial_cell_y > max_cell_y ||
                    other.x < min_x || other.x > max_x ||
                    other.y < min_y || other.y > max_y) continue;
                if (dist2(unit.x, unit.y, other.x, other.y) >=
                    scan_range_sq) continue;
                candidates.push_back({candidate_world, &other});
            }
        }
        // r.s scans cells in x/y order and residents in insertion order.
        // Equal HP retains the first callback candidate across all teams.
        std::sort(candidates.begin(), candidates.end(),
            [](const RepairCandidate& lhs, const RepairCandidate& rhs) {
                return std::tuple(lhs.unit->collision_spatial_cell_x,
                                  lhs.unit->collision_spatial_cell_y,
                                  lhs.unit->collision_spatial_entry_tick,
                                  lhs.unit->object_creation_order) <
                       std::tuple(rhs.unit->collision_spatial_cell_x,
                                  rhs.unit->collision_spatial_cell_y,
                                  rhs.unit->collision_spatial_entry_tick,
                                  rhs.unit->object_creation_order);
            });
        f32 selected_hp = -1.0f;
        for (const RepairCandidate& candidate : candidates) {
            if (selected_hp == -1.0f || candidate.unit->hp < selected_hp) {
                selected_world = candidate.world;
                selected = candidate.unit;
                selected_hp = candidate.unit->hp;
            }
        }
        if (selected && selected_world) {
            // r.a(y,delta,0,false) inserts au.b(target), with (2,2) and
            // m=true. Both callers run after y.a(float), so the waypoint
            // waits for the next object update before service dispatch.
            const u32 main_target_uid = unit.main_target_uid();
            const i32 main_target_team = unit.main_target_team_id();
            clear_current_order_state_(unit, true);
            UnitOrder order;
            order.type = WeaponType::REPAIR;
            order.x = order.y = 2.0f;
            order.target_uid = selected->uid;
            order.target_team_id = selected_world->team_id_;
            order.service_chase_range_extra = 0.0f;
            order.automatic = true;
            unit.order_queue.push_back(std::move(order));
            // r.a()/y.ao() retain R. Choose the storage used by the newly
            // inserted service waypoint before the next activation sees it.
            unit.restore_main_target(main_target_uid, main_target_team);
            unit.activate_order_next_frame = true;
        }
    }
}

} // namespace rwrl
