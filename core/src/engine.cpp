#include <rwrl/engine.h>

#include <stdexcept>

namespace rwrl {

namespace {
i32 g_global_random_seed = 0;

// GameSaver creates fresh shells and restores only serialized runtime fields.
// In particular, aU, command caches, collision candidates and ap.b/m are
// transient. Keep this equivalent shared by replay system action 200 and the
// ordinary engine's next-update path.
void copy_serialized_unit_state(Unit& unit, const Unit& saved) {
#define COPY_FIELD(name) unit.name = saved.name
    COPY_FIELD(hp); COPY_FIELD(max_hp); COPY_FIELD(shield); COPY_FIELD(max_shield);
    COPY_FIELD(total_shield_absorbed); COPY_FIELD(shield_disabled); COPY_FIELD(energy);
    COPY_FIELD(energy_disabled); COPY_FIELD(build_progress); COPY_FIELD(build_progress_secondary);
    COPY_FIELD(rotation); COPY_FIELD(height); COPY_FIELD(vel_x); COPY_FIELD(vel_y);
    COPY_FIELD(move_speed_factor); COPY_FIELD(turn_velocity); COPY_FIELD(reversing);
    COPY_FIELD(spawn_delay_frames); COPY_FIELD(spawn_clock_ms); COPY_FIELD(tree_type);
    COPY_FIELD(movement_prediction_enabled); COPY_FIELD(has_move_target); COPY_FIELD(arrived);
    COPY_FIELD(target_x); COPY_FIELD(target_y); COPY_FIELD(collision_impulse_x);
    COPY_FIELD(collision_impulse_y); COPY_FIELD(collision_enabled);
    COPY_FIELD(collision_ignore_uid); COPY_FIELD(collision_ignore_frames);
    COPY_FIELD(wall_bump_frames); COPY_FIELD(formation_path_fallback_frames);
    COPY_FIELD(formation_path_failure_frames); COPY_FIELD(formation_projection_disabled_frames);
    COPY_FIELD(formation_leader_uid); COPY_FIELD(formation_is_leader); COPY_FIELD(formation_preferred);
    COPY_FIELD(formation_group_size); COPY_FIELD(formation_offset_x); COPY_FIELD(formation_offset_y);
    COPY_FIELD(formation_heading); COPY_FIELD(formation_tick); COPY_FIELD(formation_controller_id);
    COPY_FIELD(formation_idle_leader_frames); COPY_FIELD(last_request_x); COPY_FIELD(last_request_y);
    COPY_FIELD(repath_cooldown_frames); COPY_FIELD(path_request_target_size);
    COPY_FIELD(using_path); COPY_FIELD(waypoints); COPY_FIELD(current_waypoint); COPY_FIELD(path_incomplete);
    COPY_FIELD(path_target_override_active); COPY_FIELD(path_target_override_x); COPY_FIELD(path_target_override_y);
    COPY_FIELD(waypoint_stall_frames); COPY_FIELD(last_waypoint_stall_frames);
    COPY_FIELD(active_order_elapsed_frames); COPY_FIELD(deterministic_random_salt);
    COPY_FIELD(target_scan_timer_frames);
    // r.d and custom.j.u are constructor-only scan clocks. Native unit
    // saves retain the separate y.U nano-effect clock, not either timer.
    unit.auto_repair_timer_frames = 0.0f;
    COPY_FIELD(parent_uid); COPY_FIELD(target_uid); COPY_FIELD(target_team_id);
    if (!unit.can_attack) {
        // These internal fields encode the serialized y.R pointer when the
        // freshly loaded unarmed unit has a MOVE scheduling type.
        unit.unarmed_main_target_uid = saved.is_dead ? 0 : saved.main_target_uid();
        unit.unarmed_main_target_team_id = unit.unarmed_main_target_uid != 0
            ? saved.main_target_team_id() : -1;
    }
    COPY_FIELD(service_combat_target_uid); COPY_FIELD(service_combat_target_team_id);
    COPY_FIELD(service_order_target_x); COPY_FIELD(service_order_target_y);
    COPY_FIELD(ammo); COPY_FIELD(custom_flags);
    if (unit.unit_type == "fogRevealer") {
        COPY_FIELD(lifetime_frames); COPY_FIELD(sight_range);
    }
    COPY_FIELD(weapon_type);
    COPY_FIELD(is_alive); COPY_FIELD(is_dead); COPY_FIELD(death_clock_ms);
    COPY_FIELD(income_timer); COPY_FIELD(custom_idle_frames);
    COPY_FIELD(custom_factory_exit_side);
    // Only custom.j.e and the transport subclasses serialize their bobbing
    // phase. Stock gunship, hover tank and amphibious jet constructors reset
    // it on load, independently of the saved height and vertical speed.
    if (saved.custom_unit || saved.unit_type == "hovercraft" ||
        saved.unit_type == "dropship") {
        COPY_FIELD(flight_phase);
    }
    COPY_FIELD(vertical_speed); COPY_FIELD(wing_fold_progress); COPY_FIELD(submerge_requested);
    COPY_FIELD(unload_timer_frames); COPY_FIELD(unloading_all); COPY_FIELD(behavior);
    COPY_FIELD(service_warmup_progress); COPY_FIELD(tech_level); COPY_FIELD(cost); COPY_FIELD(reclaim_cost);
    unit.sync_stock_laser_defence_specs();
    COPY_FIELD(cargo_units); COPY_FIELD(bomber_attack_heading); COPY_FIELD(bomber_retreating);
    COPY_FIELD(bomber_target_search_done); COPY_FIELD(guard_combat_chase_outer);
    COPY_FIELD(last_damage_tick); COPY_FIELD(last_damage_age_ms); COPY_FIELD(last_attacker_uid);
    COPY_FIELD(guard_target_uid); COPY_FIELD(guard_target_team_id);
    COPY_FIELD(follow_target_uid); COPY_FIELD(follow_target_team_id);
    COPY_FIELD(has_guard_pos); COPY_FIELD(guard_x); COPY_FIELD(guard_y); COPY_FIELD(has_patrol);
    COPY_FIELD(attack_order_target_x); COPY_FIELD(attack_order_target_y);
    COPY_FIELD(transport_order_target_x); COPY_FIELD(transport_order_target_y);
    COPY_FIELD(patrol_logical_target_active); COPY_FIELD(patrol_logical_target_x); COPY_FIELD(patrol_logical_target_y);
    COPY_FIELD(attack_move_combat_target_valid); COPY_FIELD(attack_move_combat_target_x); COPY_FIELD(attack_move_combat_target_y);
    COPY_FIELD(guard_movement_target_valid); COPY_FIELD(guard_movement_target_x); COPY_FIELD(guard_movement_target_y);
    // custom.j writes only the planted pose, k/m/n flags and the visual
    // dust flag. The limb warmup, liquid cache and target are fresh-shell
    // state; retaining e advances the next lift after a quick resync.
    for (size_t i = 0; i < unit.legs.size() && i < saved.legs.size(); ++i) {
        LegState& leg = unit.legs[i];
        const LegState& old = saved.legs[i];
        leg.x = old.x; leg.y = old.y; leg.height = old.height;
        leg.rotation = old.rotation; leg.moving = old.moving;
        leg.reset = old.reset; leg.compressed_reset = old.compressed_reset;
    }
    unit.leg_previous_x = unit.x;
    unit.leg_previous_y = unit.y;
    unit.leg_previous_height = unit.height;
    unit.leg_previous_rotation = unit.rotation;
    for (UnitOrder order : saved.order_queue) {
        UnitOrder loaded;
        loaded.type = order.type; loaded.x = order.x; loaded.y = order.y;
        loaded.target_uid = order.target_uid; loaded.target_team_id = order.target_team_id;
        loaded.unit_type = order.unit_type; loaded.action_id = order.action_id;
        loaded.build_tech_level = order.build_tech_level;
        loaded.relaxed_formation = order.relaxed_formation;
        loaded.service_chase_range_extra = order.service_chase_range_extra;
        loaded.max_time_frames = order.max_time_frames; loaded.automatic = order.automatic;
        unit.order_queue.push_back(std::move(loaded));
    }
    if (!unit.order_queue.empty()) {
        const UnitOrder& active = unit.order_queue.front();
        if (active.type == WeaponType::REPAIR ||
            active.type == WeaponType::RECLAIM) {
            // The native save stores au, not this internal scheduling tag.
            // A BUILD that became REPAIR can still use its construction task
            // before reload; afterwards dispatch the serialized waypoint.
            unit.weapon_type = active.type;
            unit.target_uid = active.target_uid;
            unit.target_team_id = active.target_team_id;
        }
    }
    if (unit.is_dead) {
        unit.cleanup_done = true;
        unit.native_unit_list_member = false;
        unit.death_persistent = true;
        unit.target_uid = 0;
        unit.target_team_id = -1;
    }
    for (size_t i = 0; i < unit.weapon_slots.size() && i < saved.weapon_slots.size(); ++i) {
        WeaponSlot& slot = unit.weapon_slots[i];
        const WeaponSlot& old = saved.weapon_slots[i];
        slot.turret_angle = old.turret_angle;
        slot.turret_turn_velocity = old.turret_turn_velocity;
        slot.turret_idle_timer = old.turret_idle_timer;
        slot.cooldown_remainder = old.cooldown_remainder;
        slot.cooldown_ticks = old.cooldown_ticks;
        slot.warmup_progress = old.warmup_progress;
        slot.aim_offset_x = old.aim_offset_x; slot.aim_offset_y = old.aim_offset_y;
        slot.target_uid = old.target_uid; slot.target_team_id = old.target_team_id;
    }
    if (saved.custom_unit) {
        COPY_FIELD(crash_fall_velocity);
        // custom.j.k/l are transient. A loaded wreck performs its first
        // impact turn again before choosing a liquid or land terminal state.
    } else if (saved.move_type == MoveType::AIR ||
        saved.move_type == MoveType::WATER || saved.unit_type == "amphibiousJet") {
        COPY_FIELD(crash_fall_velocity); COPY_FIELD(crash_impacted);
        // The tile cache is rebuilt from terrain after loading.
        COPY_FIELD(crash_over_water);
    }
#undef COPY_FIELD
}
}

void set_global_random_seed(i32 seed) { g_global_random_seed = seed; }
i32 global_random_seed() { return g_global_random_seed; }

Engine::Engine() {
    registry_.register_builtin_units();
}

u64 Engine::combined_building_revision_() const {
    u64 revision = 0;
    for (const auto& [_, world] : worlds_) revision += world.building_revision_;
    return revision;
}

void Engine::sync_building_pathing_() {
    Vec<World*> worlds;
    worlds.reserve(worlds_.size());
    for (auto& [_, world] : worlds_) worlds.push_back(&world);
    for (World* world : worlds) world->rebuild_pathing_from_worlds_(worlds);
}

World& Engine::create_world(i32 team_id) {
    auto [it, inserted] = worlds_.emplace(
        team_id, World(team_id, registry_, &next_unit_uid_,
                       &next_object_order_, object_cost_refresh_frames_,
                       object_cost_layers_, &scorch_marks_, clearance_state_));
    if (inserted && map_data_) it->second.apply_map(*map_data_);
    if (inserted) {
        const auto alliance = player_alliances_.find(team_id);
        if (alliance != player_alliances_.end()) {
            it->second.set_alliance_id(alliance->second);
        }
    }
    it->second.set_unit_cap(unit_cap_);
    it->second.set_fog_mode(fog_mode_);
    it->second.set_singleplayer_mode(singleplayer_mode_);
    it->second.set_nukes_enabled(nukes_enabled_);
    return it->second;
}

void Engine::set_singleplayer_mode(bool enabled) {
    singleplayer_mode_ = enabled;
    for (auto& [_, world] : worlds_) world.set_singleplayer_mode(enabled);
}

void Engine::set_nukes_enabled(bool enabled) {
    nukes_enabled_ = enabled;
    for (auto& [_, world] : worlds_) world.set_nukes_enabled(enabled);
}

void Engine::load_map(const Str& path, bool spawn_map_units,
                      const Vec<i32>& active_team_ids) {
    MapData loaded = load_tmx_map(path);
    const Set<i32> active_teams(active_team_ids.begin(), active_team_ids.end());
    const auto participating = [&](const MapUnitSpawn& spawn) {
        return spawn.team_id < 0 || active_teams.empty() ||
               active_teams.count(spawn.team_id) != 0;
    };

    for (const MapUnitSpawn& spawn : loaded.units) {
        if (!spawn_map_units) break;
        if (!participating(spawn)) continue;
        const bool known = spawn.custom_unit
            ? registry_.is_custom(spawn.unit_type)
            : registry_.get_map_unit(spawn.unit_type) != nullptr;
        if (!known) {
            throw std::runtime_error(
                (spawn.custom_unit
                    ? "custom map unit is not registered in the C++ engine: "
                    : "map references an unknown stock unit: ") +
                spawn.unit_type);
        }
    }

    // Parse before mutating Engine so malformed maps leave the current game
    // intact. A successful map load replaces the global JAR unit list.
    worlds_.clear();
    player_alliances_.clear();
    clearance_state_ = mk<PathClearanceState>();
    next_unit_uid_ = 1;
    next_object_order_ = 1;
    scorch_marks_.clear();
    global_tick_ = 0;
    object_update_sweep_counter_ = 0;
    game_time_ = 0.0f;
    game_over_ = false;
    winner_team_ = -1;
    map_data_ = std::move(loaded);
    if (map_data_->fog_mode) set_fog_mode(*map_data_->fog_mode);
    if (!spawn_map_units) return;

    Set<i32> teams;
    teams.insert(active_teams.begin(), active_teams.end());
    for (const MapUnitSpawn& spawn : map_data_->units) {
        if (participating(spawn)) teams.insert(spawn.team_id);
    }
    for (i32 team_id : teams) create_world(team_id);

    for (const MapUnitSpawn& spawn : map_data_->units) {
        if (!participating(spawn)) continue;
        World* world = get_world(spawn.team_id);
        if (!world) continue;
        Unit* unit = world->spawn_map_unit(spawn.unit_type, spawn.x, spawn.y);
        if (!unit) {
            throw std::runtime_error("map references an unknown unit: " +
                                     spawn.unit_type);
        }
        if (spawn.random_rotate) {
            // f.a(am,-180,180) is deterministic and depends on object id,
            // world position, player id and the current simulation seed/tick.
            // Map loading starts at seed/tick zero in this engine.
            u32 wrapped = unit->uid * 1313u;
            wrapped += static_cast<u32>(static_cast<i32>(unit->x * 13.0f));
            wrapped += static_cast<u32>(static_cast<i32>(unit->y * 13.0f));
            wrapped += static_cast<u32>(static_cast<i32>(unit->x * 130.0f));
            wrapped += static_cast<u32>(static_cast<i32>(unit->y * 130.0f));
            wrapped += static_cast<u32>(spawn.team_id * 13131 + spawn.team_id);
            wrapped += 14u;
            constexpr i32 range = 360;
            i32 value = static_cast<i32>(wrapped);
            value %= range;
            if (value < 0) value = -value;
            unit->rotation = static_cast<f32>(value - 180);
        }
        if (unit->unit_type == "commandCenter") world->set_cc_uid(unit->uid);
    }
    // Unit spawning rebuilds occupancy incrementally. Reapply the static map
    // once after all buildings so every World sees the same final layer.
    for (auto& [_, world] : worlds_) world.apply_map(*map_data_);
    sync_building_pathing_();
    for (auto& [_, world] : worlds_) {
        world.pathfinder_.initialize_clearance_layers(true);
        world.pathfinder_.reset_clearance_budget();
    }
}

Unit* Engine::restore_unit(const Str& name, bool custom, i32 team_id,
                           u32 uid, f32 x, f32 y) {
    if (!map_data_ || uid == 0) {
        throw std::invalid_argument("saved unit requires loaded terrain and a nonzero UID");
    }
    for (const auto& [_, world] : worlds_) {
        if (world.get_unit(uid)) {
            throw std::invalid_argument("duplicate saved unit UID");
        }
    }
    Str resolved = name;
    if (custom && !registry_.is_custom(name)) resolved = "missing";
    const UnitDef* definition = custom ? registry_.get(resolved)
                                       : registry_.get_map_unit(resolved);
    if (!definition || (custom && !registry_.is_custom(resolved))) {
        throw std::invalid_argument("saved unit definition is not registered: " + resolved);
    }
    World& world = create_world(team_id);
    if (team_id < 0) world.credits_ = 0.0;
    next_unit_uid_ = uid;
    Unit* unit = custom ? world.spawn_unit(resolved, x, y, true)
                        : world.spawn_map_unit(resolved, x, y);
    if (unit->unit_type == "commandCenter") world.set_cc_uid(uid);
    return unit;
}

void Engine::finish_unit_restore(u32 next_uid, u64 formation_next_id) {
    if (!map_data_ || next_uid == 0) {
        throw std::invalid_argument("invalid saved object allocator");
    }
    next_unit_uid_ = next_uid;
    // game.i.b(false) resets j during native save loading; j is not saved.
    object_update_sweep_counter_ = 0;
    for (auto& [_, world] : worlds_) {
        world.object_update_sweep_counter_ = 0;
        world.next_formation_controller_id_ = formation_next_id;
        for (auto& [uid, unit] : world.units_) {
            // Saved incomplete units resume through their builders; merely
            // loading their shell must not start synthetic construction.
            if (unit.is_alive && unit.build_progress < 1.0f) {
                world.pending_builds_.push_back(
                    {unit.build_progress, uid, true, false});
            }
        }
    }
    // gameSaver's final k.l.a(null) rebuilds occupancy and the four ground
    // clearance layers from the restored units. WATER keeps the map's
    // original clearance layer, since k.l.a never regenerates it.
    sync_building_pathing_();
    for (auto& [_, world] : worlds_) {
        world.pathfinder_.initialize_clearance_layers(true, true);
        world.pathfinder_.reset_clearance_budget();
    }
    refresh_spatial_index_();
}

void Engine::restore_auxiliary_object(f32 x, f32 y, bool scorch_mark,
                                      bool nuclear_scorch) {
    ++next_object_order_;
    if (scorch_mark) scorch_marks_.push_back({x, y, nuclear_scorch});
}

void Engine::restore_projectile(const Projectile& saved) {
    if (saved.uid == 0) throw std::invalid_argument("invalid saved projectile UID");
    World& world = create_world(saved.team_id);
    Projectile projectile = saved;
    projectile.creation_order = next_object_order_++;
    world.projectiles_.emplace(projectile.uid, std::move(projectile));
}

void Engine::restore_timeline(i32 frame, i32 clock_ms, f32 game_time,
                              bool clock_already_advanced) {
    if (frame < 0 || clock_ms < 0 || game_time < 0.0f) {
        throw std::invalid_argument("invalid saved simulation timeline");
    }
    global_tick_ = frame;
    game_time_ = game_time;
    for (auto& [_, world] : worlds_) {
        world.tick_ = frame;
        world.simulation_clock_ms_ = clock_ms;
        world.next_simulation_clock_ms_ = clock_already_advanced
            ? std::optional<i32>(clock_ms) : std::nullopt;
    }
}

void Engine::flush_quick_resync() {
    if (!quick_resync_pending_ || global_tick_ <= quick_resync_queued_frame_) return;
    quick_resync_pending_ = false;
    if (!map_data_) return;
    struct TeamSave {
        i32 team, alliance, clock;
        f64 credits;
        bool ai, surrendered;
        i32 difficulty, surrender_start_clock;
        Map<u32, FactoryQueue> queues;
    };
    Vec<TeamSave> teams;
    Vec<Unit> units;
    Vec<Projectile> projectiles;
    for (const auto& [team_id, world] : worlds_) {
        teams.push_back({team_id, world.alliance_id_, world.simulation_clock_ms_,
                         static_cast<f64>(static_cast<i32>(world.credits_)),
                         world.ai_player_, world.surrendered_, world.ai_difficulty_,
                         world.surrender_start_clock_ms_, world.factory_queues_});
        for (const auto& [_, unit] : world.units_) {
            if (unit.list_removal_tick < 0) units.push_back(unit);
        }
        for (const auto& [_, projectile] : world.projectiles_) projectiles.push_back(projectile);
    }
    const u32 allocator = next_unit_uid_;
    const u64 object_allocator = next_object_order_;
    u64 formation_allocator = 1;
    for (const auto& [_, world] : worlds_) {
        formation_allocator = std::max(formation_allocator, world.next_formation_controller_id_);
    }
    worlds_.clear();
    for (i32 i = 0; i < 5; ++i) {
        object_cost_refresh_frames_[i] = -99;
        object_cost_layers_[i].clear();
    }
    clearance_state_ = mk<PathClearanceState>();
    for (const TeamSave& team : teams) {
        World& world = create_world(team.team);
        world.credits_ = team.credits; world.alliance_id_ = team.alliance;
        world.ai_player_ = team.ai; world.ai_difficulty_ = team.difficulty;
        // Native quick resync reuses n objects. E is serialized and its av
        // surrender timestamp survives on that existing team object.
        world.surrendered_ = team.surrendered;
        world.surrender_start_clock_ms_ = team.surrender_start_clock;
        world.tick_ = global_tick_; world.simulation_clock_ms_ = team.clock;
    }
    std::sort(units.begin(), units.end(), [](const Unit& a, const Unit& b) {
        return a.object_creation_order < b.object_creation_order;
    });
    for (const Unit& saved : units) {
        Unit* unit = restore_unit(saved.unit_type, saved.custom_unit, saved.team_id,
                                  saved.uid, saved.x, saved.y);
        copy_serialized_unit_state(*unit, saved);
        unit->object_creation_order = saved.object_creation_order;
    }
    for (Projectile saved : projectiles) {
        // u/v/w, I and the cached retarget/sweep controller are not written.
        saved.unguided_speed_x = saved.unguided_speed_y = saved.unguided_speed_height = 0.0f;
        saved.random_phase = 0.0f;
        World& world = create_world(saved.team_id);
        world.projectiles_.emplace(saved.uid, std::move(saved));
    }
    for (const TeamSave& team : teams) {
        get_world(team.team)->factory_queues_ = team.queues;
    }
    next_object_order_ = object_allocator;
    finish_unit_restore(allocator, formation_allocator);
}

void Engine::refresh_spatial_index_() {
    for (auto& [_, world] : worlds_) {
        for (auto& [uid, unit] : world.units_) {
            if (!unit.native_unit_list_member) continue;
            world.refresh_unit_spatial_index_(unit);
        }
    }
}

void Engine::setup_teams(i32 team_a, i32 team_b) {
    game_over_ = false;
    winner_team_ = -1;
    auto& wa = create_world(team_a);
    auto& wb = create_world(team_b);

    auto* cc_a = wa.spawn_unit("commandCenter", 400, 400, true);
    auto* cc_b = wb.spawn_unit("commandCenter", 1200, 800, true);
    if (cc_a) wa.set_cc_uid(cc_a->uid);
    if (cc_b) wb.set_cc_uid(cc_b->uid);
}

void Engine::setup_teams_vec(const Vec<i32>& team_ids) {
    game_over_ = false;
    winner_team_ = -1;
    f32 spacing = 800.0f;
    for (i32 i = 0; i < static_cast<i32>(team_ids.size()); ++i) {
        auto& w = create_world(team_ids[i]);
        f32 x = 400.0f + i * spacing;
        f32 y = 400.0f + (i % 2) * 400.0f;
        auto* cc = w.spawn_unit("commandCenter", x, y, true);
        if (cc) w.set_cc_uid(cc->uid);
    }
}

void Engine::tick(f32 dt) {
    if (paused_) return;

    flush_quick_resync();

    global_tick_++;
    game_time_ += dt;

    Vec<World*> world_ptrs;
    world_ptrs.reserve(worlds_.size());
    for (auto& [_, world] : worlds_) world_ptrs.push_back(&world);

    auto other_worlds_for = [&](World* world, bool hostile_only) {
        Vec<World*> result;
        result.reserve(world_ptrs.size() > 0 ? world_ptrs.size() - 1 : 0);
        for (World* other : world_ptrs) {
            if (world == other) continue;
            if (hostile_only && !world->is_enemy_with(*other)) continue;
            result.push_back(other);
        }
        return result;
    };

    for (World* world : world_ptrs) {
        world->begin_step(
            dt, speed_mult_, income_mult_, other_worlds_for(world, false));
    }

    const f32 game_dt = dt * speed_mult_;
    auto update_unit = [&](World* world, u32 uid) {
        Unit* current = world->get_unit(uid);
        if (!current) return;
        if (!world->update_unit_lifecycle(
                game_dt * EngineConstants::FRAME_RATE, uid)) return;
        Vec<World*> others = other_worlds_for(world, false);
        const UnitDef* custom_energy_definition = current->custom_unit
            ? registry_.get(current->unit_type) : nullptr;
        // Most stock flight updates precede orders; hover units, transports
        // and submersibles finish after generic movement. Custom height belongs
        // to custom.j after dL advances production.
        if (!current->custom_unit) world->update_unit_shields(game_dt, uid);
        if (!current->custom_unit && current->unit_type != "laserDefence") {
            world->update_unit_energy(game_dt, uid);
        }
        world->update_unit_ammo(dt, speed_mult_, uid);
        const bool stock_transport = !current->custom_unit &&
            (current->unit_type == "hovercraft" || current->unit_type == "dropship");
        const bool stock_post_movement_height = stock_transport ||
            (!current->custom_unit &&
             (current->unit_type == "gunShip" ||
              current->move_type == MoveType::HOVER ||
              (current->move_type == MoveType::LAND && current->can_move)));
        if (!current->custom_unit && !stock_post_movement_height) {
            world->update_unit_flying_states(dt, speed_mult_, uid);
        }
        world->update_unit_runtime_precombat(game_dt, uid);

        // y.k() advances one-shot waypoints and executes custom actions before
        // combat. A zero-delta order pass preserves that phase without moving
        // the unit; the normal pass below then applies physical movement.
        world->update_unit_movement(0.0f, uid, others);
        world->publish_single_attack_move_proposal_(uid, others);
        world->update_unit_builds(dt, speed_mult_, uid);
        world->update_unit_command_behaviors(
            game_dt * EngineConstants::FRAME_RATE, uid, others);
        const bool armed_service = current->has_armed_service_order();
        const u32 service_target = current->target_uid;
        const i32 service_target_team = current->target_team_id;
        const WeaponType service_weapon_type = current->weapon_type;
        if (armed_service) {
            current->target_uid = current->service_combat_target_uid;
            current->target_team_id = current->service_combat_target_team_id;
            current->weapon_type = current->is_mobile() ? WeaponType::ATTACK : WeaponType::GUARD;
        }
        world->update_unit_combat(uid, others);
        // y.a(float) runs k(float), including aiming and firing, before it
        // decrements the per-slot ap.e reload timers.
        world->update_unit_cooldowns(game_dt, uid);
        if (armed_service) {
            current->service_combat_target_uid = current->target_uid;
            current->service_combat_target_team_id = current->target_team_id;
            current->target_uid = service_target;
            current->target_team_id = service_target_team;
            current->weapon_type = service_weapon_type;
        }
        world->update_unit_movement(game_dt, uid, others);
        // h.f and b.c change height after y.a(float). A submerge boundary
        // therefore changes movement speed on the next object turn.
        world->update_unit_submersible_states(dt, speed_mult_, uid);
        // custom.b.h runs after y.a(float)'s movement and before custom.j's
        // production, income and vertical movement.
        if (current->custom_unit && current->is_active()) {
            world->update_legs_(
                *current, game_dt * EngineConstants::FRAME_RATE);
        }
        // Inactive custom.j dispatches b.a(j,delta), which is a no-op for
        // the leg controller. Passenger turns preserve the existing feet;
        // resetting them here changes exit poses and death-scorch allocation.
        if (current->custom_unit) {
            world->update_unit_auto_repair(
                game_dt * EngineConstants::FRAME_RATE, uid, others);
            world->update_unit_laser_defence(game_dt, uid, others);
            world->update_unit_energy(game_dt, uid, custom_energy_definition);
            if (!current->is_alive) return;
            // custom.j checks bT() again after y.a(). A passenger loaded
            // during its order turn skips regeneration on that same turn.
            world->update_unit_shields(game_dt, uid);
        }
        const bool custom_production = current->custom_unit;
        // custom.j advances dL before generation_resources; an upgrade that
        // completes on an income boundary uses the new resource bundle.
        if (custom_production) {
            // custom.j applies y.p regeneration before dL can convertTo.
            world->update_unit_passive_health(game_dt, uid);
            world->update_unit_factory_production(dt, speed_mult_, uid, others);
        }
        world->update_unit_income(dt, speed_mult_, income_mult_, uid);
        if (custom_production) {
            // custom.j unloads against its current height before the flight
            // update. A later passenger turn can then finish its own fall.
            world->update_unit_transport_unloading(dt, speed_mult_, uid);
            world->update_unit_flying_states(dt, speed_mult_, uid);
        }
        world->update_unit_action_building_state_(
            game_dt * EngineConstants::FRAME_RATE, uid);
        world->update_unit_auto_actions_(
            game_dt * EngineConstants::FRAME_RATE, uid, others);

        // Per-unit follow-up systems run after the command/combat phase and
        // physical movement. Their state is then visible to later objects.
        if (!custom_production) {
            world->update_unit_passive_health(game_dt, uid);
            world->update_repair_bay(
                game_dt * EngineConstants::FRAME_RATE, uid, others);
        }
        world->update_dynamic_map_units(
            game_dt * EngineConstants::FRAME_RATE, others, uid);
        if (!custom_production) {
            world->update_unit_factory_production(dt, speed_mult_, uid, others);
        }
        if (!custom_production) {
            // p.a() regenerates after its factory superclass can finish an
            // upgrade, then intercepts with the resulting tier and energy.
            if (current->unit_type == "laserDefence") world->update_unit_energy(game_dt, uid);
            world->update_unit_laser_defence(game_dt, uid, others);
        }
        world->update_unit_antinuke_interception(game_dt, uid, others);
        if (!custom_production) world->update_unit_transport_unloading(dt, speed_mult_, uid);
        // Hover subclasses check bT() after y.a(float), so cargo that loads
        // during its own movement turn keeps the preceding height and phase.
        if (stock_post_movement_height) {
            world->update_unit_flying_states(dt, speed_mult_, uid);
        }
        world->update_unit_service_decay(
            game_dt * EngineConstants::FRAME_RATE, uid);
    };

    enum class ObjectKind : u8 { UNIT, PROJECTILE };
    struct ObjectUpdate {
        u64 creation_order;
        u32 uid;
        World* world;
        ObjectKind kind;
    };
    auto sort_objects = [](Vec<ObjectUpdate>& objects) {
        std::sort(objects.begin(), objects.end(),
                  [](const ObjectUpdate& lhs, const ObjectUpdate& rhs) {
                      if (lhs.creation_order != rhs.creation_order) {
                          return lhs.creation_order < rhs.creation_order;
                      }
                      if (lhs.kind != rhs.kind) return lhs.kind < rhs.kind;
                      if (lhs.world->team_id() != rhs.world->team_id()) {
                          return lhs.world->team_id() < rhs.world->team_id();
                      }
                      return lhs.uid < rhs.uid;
                  });
    };
    auto update_object = [&](const ObjectUpdate& update) {
        const u64 building_revision = combined_building_revision_();
        if (update.kind == ObjectKind::UNIT) {
            update_unit(update.world, update.uid);
        } else {
            update.world->update_projectile(
                game_dt, update.uid,
                other_worlds_for(update.world, false));
        }
        if (combined_building_revision_() != building_revision) {
            sync_building_pathing_();
        }
    };

    Set<u64> known_object_orders;
    auto collect_new_objects = [&]() {
        Vec<ObjectUpdate> result;
        for (World* world : world_ptrs) {
            for (const auto& [uid, unit] : world->units()) {
                if (known_object_orders.insert(
                        unit.object_creation_order).second) {
                    result.push_back({unit.object_creation_order, uid, world,
                                      ObjectKind::UNIT});
                }
            }
            for (const auto& [uid, projectile] : world->projectiles()) {
                if (known_object_orders.insert(
                        projectile.creation_order).second) {
                    result.push_back({projectile.creation_order, uid, world,
                                      ObjectKind::PROJECTILE});
                }
            }
        }
        sort_objects(result);
        return result;
    };

    // w.dK() first updates a creation-ordered snapshot, then processes the
    // additions that were queued while that snapshot ran. Objects spawned by
    // the additions themselves remain queued for the following frame.
    Vec<ObjectUpdate> frame_objects = collect_new_objects();
    for (const ObjectUpdate& update : frame_objects) update_object(update);
    Vec<ObjectUpdate> pending_objects = collect_new_objects();
    for (const ObjectUpdate& update : pending_objects) update_object(update);

    // game.units.y.g(f2) is the global collision pass immediately after the
    // creation-ordered object turns.  Keep cross-world collision in that
    // same phase; finish_step contains AI/removal bookkeeping that belongs
    // after collision resolution.
    if (!world_ptrs.empty()) {
        world_ptrs.front()->update_collision(game_dt, world_ptrs);
    }
    // game.i.j increments once per native update, independently of delta.
    // Its global sweep runs after collisions and before the team update.
    if (++object_update_sweep_counter_ >= 1000) {
        object_update_sweep_counter_ = 0;
        World::sweep_wrecks_(world_ptrs);
    }

    for (World* world : world_ptrs) {
        world->set_disable_ai(disable_ai_);
        world->set_raw_commands(raw_commands_);
        world->finish_step(dt, speed_mult_, other_worlds_for(world, false));
    }

    std::size_t playable_worlds = 0;
    for (const auto& [team_id, _] : worlds_) {
        if (team_id >= 0) ++playable_worlds;
    }
    if (!game_over_ && playable_worlds >= 2) {
        Set<i32> surviving_alliances;
        Map<i32, i32> representative_team;
        for (const auto& [team_id, world] : worlds_) {
            if (team_id < 0) continue;
            if (!world.check_victory()) continue;
            const i32 alliance_id = world.alliance_id();
            surviving_alliances.insert(alliance_id);
            auto [it, inserted] = representative_team.emplace(alliance_id, team_id);
            if (!inserted) it->second = std::min(it->second, team_id);
        }
        if (surviving_alliances.size() <= 1) {
            game_over_ = true;
            winner_team_ = surviving_alliances.empty()
                ? -1
                : representative_team.at(*surviving_alliances.begin());
        }
    }
}

void Engine::run_for(f32 seconds, f32 dt) {
    if (seconds <= 0.0f || dt <= 0.0f) return;
    f64 elapsed = 0.0;
    const f64 duration = seconds;
    while (elapsed < duration) {
        const f32 step = static_cast<f32>(std::min<f64>(dt, duration - elapsed));
        tick(step);
        elapsed += step;
    }
}

EngineState Engine::get_state() const {
    EngineState s{};
    s.global_tick = global_tick_;
    s.game_time = game_time_;
    s.speed_mult = speed_mult_;
    s.income_mult = income_mult_;
    s.paused = paused_;
    s.game_over = game_over_;
    s.winner_team = winner_team_;
    for (auto& [tid, w] : worlds_) {
        s.teams[tid] = w.get_state();
    }
    return s;
}

World* Engine::get_world(i32 team_id) {
    auto it = worlds_.find(team_id);
    return it != worlds_.end() ? &it->second : nullptr;
}

const World* Engine::get_world(i32 team_id) const {
    auto it = worlds_.find(team_id);
    return it != worlds_.end() ? &it->second : nullptr;
}

void Engine::set_paused(bool p) {
    paused_ = p;
    for (auto& [tid, w] : worlds_) {
        w.set_paused(p);
    }
}

void Engine::set_fog_enabled(bool enabled) {
    set_fog_mode(enabled ? FogMode::LOS : FogMode::NONE);
}

void Engine::set_fog_mode(FogMode mode) {
    fog_mode_ = mode;
    for (auto& [tid, w] : worlds_) w.set_fog_mode(mode);
}

void Engine::set_unit_cap(i32 cap) {
    unit_cap_ = cap;
    for (auto& [tid, w] : worlds_) {
        w.set_unit_cap(cap);
    }
}

void Engine::set_team_alliance(i32 team_id, i32 alliance_id) {
    player_alliances_[team_id] = alliance_id;
    World* world = get_world(team_id);
    if (world) world->set_alliance_id(alliance_id);
}

i32 Engine::team_alliance(i32 team_id) const {
    const World* world = get_world(team_id);
    if (world) return world->alliance_id();
    const auto alliance = player_alliances_.find(team_id);
    return alliance == player_alliances_.end() ? team_id : alliance->second;
}

bool Engine::are_allied(i32 team_a, i32 team_b) const {
    const World* a = get_world(team_a);
    const World* b = get_world(team_b);
    if (a && b) return a->is_allied_with(*b);
    if ((!a && player_alliances_.count(team_a) == 0) ||
        (!b && player_alliances_.count(team_b) == 0)) return false;
    if (team_a == -1 || team_b == -1) return team_a == team_b;
    return team_alliance(team_a) == team_alliance(team_b);
}

void Engine::execute_command(i32 team_id, const GameCommand& serialized_command) {
    GameCommand cmd = serialized_command;
    World* w = get_world(team_id);
    if (!w) return;
    const auto can_control_owner = [&](i32 owner) {
        const i32 player = cmd.control_player_team_id;
        return player < 0 || owner == player ||
            (owner >= 0 && owner < 16 && are_allied(player, owner) &&
             (cmd.shared_control_mask & (u16(1) << owner)) != 0);
    };
    if (cmd.native_selection && !cmd.group_uids.empty()) {
        // e.i()/e.k() remove stale, dead and uncontrolled selections before
        // clearing queues and committing ab. Number only the survivors so
        // rejecting the final serialized slot cannot strand a formation.
        Vec<u32> controlled;
        for (u32 selected_uid : cmd.group_uids) {
            for (const auto& [owner, world] : worlds_) {
                const Unit* unit = world.get_unit(selected_uid);
                if (!unit) continue;
                if (unit->is_alive && can_control_owner(owner) &&
                    std::find(controlled.begin(), controlled.end(), selected_uid) == controlled.end()) {
                    controlled.push_back(selected_uid);
                }
                break;
            }
        }
        const auto selected = std::find(controlled.begin(), controlled.end(), cmd.unit_uid);
        if (selected == controlled.end()) return;
        cmd.group_index = static_cast<u8>(std::min<size_t>(
            255, static_cast<size_t>(selected - controlled.begin())));
        cmd.group_size = static_cast<u8>(std::min<size_t>(255, controlled.size()));
        cmd.group_uids = std::move(controlled);
    }
    if (cmd.native_selection) {
        for (auto& [owner, world] : worlds_) {
            if (!world.get_unit(cmd.unit_uid)) continue;
            if (!can_control_owner(owner)) return;
            w = &world;
            team_id = owner;
            break;
        }
    }

    w->set_raw_commands(raw_commands_);

    // gameFramework.k.i builds its structure layer from the global am list.
    // Refresh before an order can synchronously request a path.
    sync_building_pathing_();

    if (cmd.group_index == 0) {
        Map<u32, bool> prediction_snapshot;
        for (const auto& [_, world] : worlds_) {
            for (const auto& [uid, unit] : world.units_) {
                prediction_snapshot[uid] =
                    unit.movement_prediction_enabled;
            }
        }
        for (auto& [_, world] : worlds_) {
            world.handoff_command_prediction_snapshot_ =
                prediction_snapshot;
            world.handoff_command_snapshot_tick_ = world.tick_ + 1;
        }
    }

    if (cmd.action == GameCommand::LAUNCH_NUKE) {
        Unit* launcher = w->get_unit(cmd.unit_uid);
        if (!launcher || !launcher->is_alive) return;
        i32 target_team_id = -1;
        if (cmd.target_uid != 0) {
            for (auto& [other_team, other_world] : worlds_) {
                if (other_team == team_id || are_allied(team_id, other_team)) continue;
                Unit* target = other_world.get_unit(cmd.target_uid);
                if (target && target->is_alive) {
                    target_team_id = other_team;
                    break;
                }
            }
        }
        if (target_team_id < 0) {
            for (auto& [other_team, other_world] : worlds_) {
                if (other_team != team_id && !are_allied(team_id, other_team)) {
                    target_team_id = other_team;
                    break;
                }
            }
        }
        w->launch_nuke(*launcher, cmd.target_x, cmd.target_y, target_team_id);
        return;
    }

    Vec<World*> other_worlds;
    other_worlds.reserve(worlds_.size());
    for (auto& [other_team, other_world] : worlds_) {
        if (other_team != team_id) other_worlds.push_back(&other_world);
    }
    bool mixed_owner_selection = false;
    bool final_native_selection = false;
    if (cmd.native_selection && !cmd.group_uids.empty()) {
        auto local_first = std::find_if(
            cmd.group_uids.begin(), cmd.group_uids.end(),
            [&](u32 uid) { return w->get_unit(uid) != nullptr; });
        mixed_owner_selection = std::any_of(
            cmd.group_uids.begin(), cmd.group_uids.end(),
            [&](u32 uid) { return w->get_unit(uid) == nullptr; });
        if (mixed_owner_selection) {
            if (cmd.group_index == 0 &&
                cmd.action <= GameCommand::SET_PASSIVE_TARGET &&
                !cmd.queued && !cmd.clear_non_build_repair && !cmd.clear_orders &&
                cmd.group_uids.size() > 1) {
                // e.k() clears non-leaders across the whole selection before
                // clearing leaders. A per-owner pass changes the old group's
                // new leaders before the other owner's queues have cleared.
                Set<u32> cleared_uids;
                for (bool leader_pass : {false, true}) {
                    for (u32 selected_uid : cmd.group_uids) {
                        for (auto& [_, owner] : worlds_) {
                            Unit* selected = owner.get_unit(selected_uid);
                            if (!selected) continue;
                            if (selected->is_alive &&
                                selected->formation_is_leader == leader_pass) {
                                cleared_uids.insert(selected_uid);
                                owner.rebuild_formation_after_leader_removed_(
                                    *selected, &cleared_uids);
                                selected->formation_leader_uid = 0;
                            }
                            break;
                        }
                    }
                }
            }
            w->command_formation_local_start_ =
                local_first != cmd.group_uids.end() &&
                *local_first == cmd.unit_uid;
            w->command_formation_commit_deferred_ = true;
            w->command_formation_clear_prepared_ = true;
            final_native_selection = cmd.group_uids.back() == cmd.unit_uid;
        }
    }
    w->execute_command(cmd, other_worlds);
    w->command_formation_local_start_ = false;
    w->command_formation_commit_deferred_ = false;
    w->command_formation_clear_prepared_ = false;
    if (mixed_owner_selection && final_native_selection) {
        // e.k() assigns all accepted waypoints before ab.c(). Do not leave
        // an earlier owner's members uncommitted when the final selection
        // belongs to a different shared-control player.
        for (auto& [owner, world] : worlds_) {
            if (!world.pending_formation_uids_.empty()) {
                Vec<World*> peers;
                for (auto& [peer, other] : worlds_) {
                    if (peer != owner) peers.push_back(&other);
                }
                // ab.c() chooses leaders and assigns slots from all accepted
                // current waypoints, including same-layer shared owners.
                world.configure_command_formation_(cmd, peers, 0, true);
                break;
            }
        }
        for (auto& [owner, world] : worlds_) {
            world.pending_formation_uids_.clear();
            world.command_path_snapshot_active_ = false;
            world.command_path_prediction_snapshot_.clear();
            world.command_pending_path_uids_.clear();
        }
    }
}

}
