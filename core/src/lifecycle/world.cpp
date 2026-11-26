#include "world_internal.h"

namespace rwrl {

void World::rebuild_local_buildings_() {
    if (object_cost_sources_.empty()) {
        pathfinder_.rebuild_from_buildings(units_);
    } else {
        pathfinder_.rebuild_from_buildings(object_cost_sources_);
    }
    ++building_revision_;
}

void World::rebuild_pathing_from_worlds_(const Vec<World*>& worlds) {
    Vec<const Map<u32, Unit>*> unit_sets;
    unit_sets.reserve(worlds.size() + 1);
    peer_worlds_.clear();
    peer_worlds_.reserve(worlds.size() + 1);
    bool included_self = false;
    for (World* world : worlds) {
        if (!world) continue;
        unit_sets.push_back(&world->units_);
        peer_worlds_.push_back(world);
        if (world == this) included_self = true;
    }
    if (!included_self) {
        unit_sets.push_back(&units_);
        peer_worlds_.push_back(this);
    }
    pathfinder_.rebuild_from_buildings(unit_sets);
    // Remember the cross-world sources for the lazy object-cost refresh in
    // move_unit_to. Building changes do not touch the k.i.k request gate.
    object_cost_sources_ = unit_sets;
}

void World::flush_unit_list_(const Vec<World*>& worlds) {
    // am.bF() applies both additions and removals queued before this scan.
    // Requests made by the scan itself remain visible until the next flush.
    for (World* world : worlds) {
        if (!world) continue;
        for (auto it = world->units_.begin(); it != world->units_.end();) {
            Unit& unit = it->second;
            if (unit.is_dead && !unit.cleanup_done) world->remove_unit(it->first);
            if (unit.list_removal_tick >= 0 ||
                (unit.is_dead && !unit.death_persistent)) {
                world->retain_collision_unit_(unit);
                it = world->units_.erase(it);
                continue;
            }
            unit.pending_global_commit = false;
            ++it;
        }
    }
}

void World::retain_collision_unit_(Unit& unit) {
    const u32 uid = unit.uid;
    retained_collision_units_.insert_or_assign(uid, std::move(unit));
}

Unit* World::waypoint_target_(u32 uid, bool retain_removed) {
    if (Unit* unit = get_unit(uid)) return unit;
    if (!retain_removed) return nullptr;
    const auto found = retained_collision_units_.find(uid);
    return found == retained_collision_units_.end() ? nullptr : &found->second;
}

void World::queue_wreck_removal_(Unit& unit) {
    // The virtual y.a() runs az()/aI() before am.a()/w.a() queues the
    // global-list removal. Dead cargo may have acquired an unload waypoint
    // since bt(), so cleanup must run again on this observation frame.
    const u32 main_target_uid = unit.main_target_uid();
    const i32 main_target_team_id = unit.main_target_team_id();
    clear_current_order_state_(unit, true);
    unit.order_queue.clear();
    unit.restore_main_target(main_target_uid, main_target_team_id);
    unit.path_incomplete = false;
    rebuild_formation_after_leader_removed_(unit);
    unit.formation_leader_uid = 0;
    unit.native_unit_list_member = false;
    // az()/aI() retain k/l/m, cK and the independent aU request. The object
    // remains visible until the following am.bF()/w.dK() list flush.
    unit.list_removal_tick = tick_;
}

void World::clear_placement_area_(const Unit& placed,
                                  const UnitDef& definition) {
    // y.br() runs once on a building the moment y.a(au,as,int,float,float)
    // accepts its placement. The rectangle is the construction footprint in
    // tiles, scaled to world units, moved onto the object, pulled back by the
    // building-to-footprint offset and then grown by 10 on every side.
    const f32 tile = pathfinder_.tile_size();
    const f32 origin_x =
        placed.x - definition.building_to_footprint_offset_x;
    const f32 origin_y =
        placed.y - definition.building_to_footprint_offset_y;
    const f32 left =
        static_cast<f32>(definition.placement_footprint.left) * tile +
        origin_x - 10.0f;
    const f32 right =
        static_cast<f32>(definition.placement_footprint.right) * tile +
        origin_x + 10.0f;
    const f32 top =
        static_cast<f32>(definition.placement_footprint.top) * tile +
        origin_y - 10.0f;
    const f32 bottom =
        static_cast<f32>(definition.placement_footprint.bottom) * tile +
        origin_y + 10.0f;
    flush_unit_list_(peer_worlds_);
    for (World* world : peer_worlds_) {
        if (!world) continue;
        for (auto& [_, other] : world->units_) {
            if (&other == &placed) continue;
            // am.a(RectF) grows the candidate by its own collision radius and
            // takes a strict overlap on all four edges.
            if (other.x + other.radius <= left) continue;
            if (other.x - other.radius >= right) continue;
            if (other.y + other.radius <= top) continue;
            if (other.y - other.radius >= bottom) continue;
            // y.br() calls al.k() for every tree, including already fallen
            // ones, whose fall method is idempotent. Trees are not y wrecks.
            if (other.unit_type == "tree") {
                world->fell_tree_(other);
                continue;
            }
            if (!other.is_dead || other.list_removal_tick >= 0) continue;
            world->queue_wreck_removal_(other);
        }
    }
}

void World::fell_tree_(Unit& tree, bool damage_death) {
    if (tree.is_dead && !damage_death) return;
    // al.k() retains the base object and its current HP, disables collision,
    // and moves ordinary tree sprites by et/2-3 (30/2-3 = 12).
    tree.is_dead = true;
    tree.is_alive = false;
    tree.collision_enabled = false;
    tree.death_persistent = true;
    tree.death_tick = tick_;
    tree.death_clock_ms = simulation_clock_ms_;
    tree.cleanup_done = true;
    if (tree.tree_type == 1 || tree.tree_type == 2) {
        tree.x += jar_cosine_degrees(tree.rotation) * 12.0f;
        tree.y += jar_sine_degrees(tree.rotation) * 12.0f;
    }
    grid_.remove(tree);
}

bool World::update_unit_lifecycle(f32 delta_frames, u32 unit_uid) {
    Unit* unit = get_unit(unit_uid);
    if (!unit) return false;
    // w.dK() has flushed a queued removal before the next object pass.
    // A direct ci()/non-wreck bv() also leaves the native list immediately;
    // our observation-frame retention must not schedule another dead turn.
    if ((unit->list_removal_tick >= 0 && tick_ > unit->list_removal_tick) ||
        (unit->is_dead && !unit->death_persistent && tick_ > unit->death_tick)) {
        return false;
    }

    // am.a(float) observes lethal HP before bT() gates the orderable update,
    // including attached passengers. The subclass's dead update then runs
    // on this same object turn, after bv() cleanup.
    if (!unit->is_dead && unit->hp <= 0.0f &&
        (unit->custom_unit || unit->unit_type != "tree")) {
        unit->is_dead = true;
        unit->is_alive = false;
        remove_unit(unit_uid);
    }

    // w.a(float) is dispatched per object. Direct reclaim/delete queues a
    // later list removal, so an object whose turn has not run yet still
    // executes its subclass's dead update in this frame. Persistence only
    // determines whether it remains scheduled on following frames.
    if (unit->is_dead) {
        if (!unit->custom_unit &&
            unit->move_type == MoveType::WATER && unit->can_move &&
            unit->unit_type != "amphibiousJet") {
            // h.f.a(delta) accelerates a stock ship's wreck toward -10.
            // It may overshoot during one update, then clamps on the next.
            if (unit->height > -10.0f) {
                unit->crash_fall_velocity += 0.002f * delta_frames;
                unit->height -= unit->crash_fall_velocity * delta_frames;
            } else {
                unit->height = -10.0f;
                unit->crash_impacted = true;
            }
        } else if (unit->custom_unit || ((unit->move_type == MoveType::AIR ||
             unit->move_type == MoveType::HOVER ||
             (!unit->custom_unit && unit->move_type == MoveType::LAND) ||
             unit->unit_type == "amphibiousJet") && unit->can_move)) {
            update_aircraft_wrecks_(delta_frames, unit_uid);
        }
        return false;
    }

    // Temporary map objects (notably fog revealers) decrement their timer in
    // their own am.a(float), after the ordinary superclass update begins.
    if (unit->is_alive && unit->lifetime_frames >= 0.0f) {
        unit->lifetime_frames -= delta_frames;
        if (unit->lifetime_frames < 0.0f) {
            const bool fog_revealer = unit->unit_type == "fogRevealer";
            if (fog_revealer) {
                // u.a(delta) calls ci()/bu(), clamping HP to zero. The
                // world object leaves w.er now, while am.a retains its
                // removal entry through this frame's replay snapshot.
                unit->hp = 0.0f;
                unit->is_dead = true;
                unit->is_alive = false;
            }
            remove_unit(unit_uid);
            return false;
        }
    }
    if (unit->spawn_delay_frames > 0.0f) {
        unit->spawn_delay_frames = std::max(
            0.0f, unit->spawn_delay_frames - delta_frames);
    }
    return unit->is_alive;
}

void World::sweep_wrecks_(const Vec<World*>& worlds) {
    flush_unit_list_(worlds);
    Vec<std::pair<World*, Unit*>> wrecks;
    for (World* world : worlds) {
        if (!world) continue;
        for (auto& [uid, unit] : world->units_) {
            if (!unit.is_dead || !unit.death_persistent ||
                unit.list_removal_tick >= 0 ||
                (unit.unit_type == "tree" && !unit.custom_unit)) continue;
            wrecks.emplace_back(world, &unit);
        }
    }
    if (wrecks.size() <= 70) return;
    std::sort(wrecks.begin(), wrecks.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.second->object_creation_order != rhs.second->object_creation_order) {
            return lhs.second->object_creation_order < rhs.second->object_creation_order;
        }
        return lhs.second->uid < rhs.second->uid;
    });
    std::size_t remaining = wrecks.size();
    for (const auto& [world, unit] : wrecks) {
        if (remaining <= 70) break;
        if (unit->death_clock_ms < world->simulation_clock_ms_ - 30000) {
            world->queue_wreck_removal_(*unit);
            --remaining;
        }
    }
}
World::World(i32 team_id, const UnitRegistry& registry,
             u32* global_next_uid, u64* global_next_object_order,
             i32* global_object_cost_refresh_frames,
             Vec<i8>* global_object_cost_layers,
             Vec<ScorchMark>* global_scorch_marks,
             SP<PathClearanceState> global_clearance_state)
    : team_id_(team_id), alliance_id_(team_id), registry_(registry),
      global_next_uid_(global_next_uid),
      global_next_object_order_(global_next_object_order), grid_(),
      pathfinder_(EngineConstants::MAP_WIDTH, EngineConstants::MAP_HEIGHT, EngineConstants::TILE_SIZE),
      fog_tiles_(EngineConstants::MAP_WIDTH * EngineConstants::MAP_HEIGHT, 10),
      global_object_cost_refresh_frames_(global_object_cost_refresh_frames),
      global_object_cost_layers_(global_object_cost_layers),
      global_clearance_state_(std::move(global_clearance_state)),
      global_scorch_marks_(global_scorch_marks) {}

bool World::try_create_scorch_mark_(f32 x, f32 y, bool nuclear) {
    auto& marks = global_scorch_marks_ ? *global_scorch_marks_ : scorch_marks_;
    i32 nearby_count = 0;
    const f32 nearby_limit = nuclear ? 45.0f : 25.0f;
    for (const auto& mark : marks) {
        if (mark.nuclear != nuclear) continue;
        const f32 dx = std::abs(mark.x - x);
        const f32 dy = std::abs(mark.y - y);
        if (dx < 5.0f && dy < 5.0f) return false;
        if (dx < nearby_limit && dy < nearby_limit && ++nearby_count >= 3) {
            return false;
        }
    }
    marks.push_back({x, y, nuclear});
    return true;
}

void World::apply_map(const MapData& map) {
    pathfinder_.reset(map.width, map.height, map.tile_size);
    const std::size_t expected =
        static_cast<std::size_t>(map.width) * static_cast<std::size_t>(map.height);
    for (std::size_t index = 0; index < expected; ++index) {
        const i32 tx = static_cast<i32>(index % static_cast<std::size_t>(map.width));
        const i32 ty = static_cast<i32>(index / static_cast<std::size_t>(map.width));
        if (index < map.terrain_set.size() && map.terrain_set[index]) {
            pathfinder_.set_terrain(
                tx, ty, index < map.terrain.size() ? map.terrain[index] : TERRAIN_NONE);
        }
        pathfinder_.set_tile_metadata(
            tx, ty,
            index < map.resource_pool.size() && map.resource_pool[index],
            index < map.block_buildings.size() && map.block_buildings[index],
            index < map.path_cost.size() ? map.path_cost[index] : 0);
    }
    map_loaded_ = true;
    fog_tiles_.assign(expected, 10);
    fog_refresh_frames_ = fog_mode_ == FogMode::LOS ? 150.0f : 0.0f;
    rebuild_local_buildings_();
    pathfinder_.initialize_clearance_layers(true);
    pathfinder_.share_clearance_state(global_clearance_state_);
    if (fog_enabled()) {
        for (const auto& [_, unit] : units_) reveal_visibility_(unit);
    }
}

Unit* World::spawn_unit(const Str& name, f32 x, f32 y, bool start_built) {
    const UnitDef* def = registry_.get(name);
    if (!def) return nullptr;
    return spawn_unit_from_def_(*def, name, x, y, start_built);
}

Unit* World::spawn_map_unit(const Str& map_name, f32 x, f32 y) {
    const UnitDef* def = registry_.get_map_unit(map_name);
    if (!def) return nullptr;
    return spawn_unit_from_def_(*def, def->name, x, y, true);
}

Unit* World::spawn_unit_from_def_(const UnitDef& definition, const Str& name,
                                 f32 x, f32 y, bool start_built) {
    const UnitDef* def = &definition;
    Unit u{};
    u.uid = global_next_uid_ ? (*global_next_uid_)++ : next_uid_++;
    u.object_creation_order = allocate_object_order_();
    u.unit_type = name;
    u.team_id = team_id_;
    u.custom_unit = registry_.is_custom(name);
    u.x = x; u.y = y;
    u.radius = def->radius;
    u.mass = def->mass;
    u.soft_collision_on_all = def->soft_collision_on_all;
    u.armour = def->armour;
    u.armour_min_damage = def->armour_min_damage;
    u.hp = def->hp; u.max_hp = def->hp;
    u.shield = def->start_shield_at_zero ? 0.0f : def->shield;
    u.max_shield = def->shield;
    u.shield_regen_rate = def->shield_regen_rate;
    u.shield_regen_per_frame = def->shield_regen_per_frame;
    u.damage = def->damage;
    u.shoot_delay_multiplier = def->shoot_delay_multiplier;
    u.shoot_damage_multiplier = def->shoot_damage_multiplier;
    u.weapon_range = def->range;
    u.attack_range_adds_radii = def->attack_range_adds_radii;
    u.attack_follow_e_flag = def->attack_follow_e_flag;
    u.attack_follow_weapon_n = def->attack_follow_weapon_n;
    u.bomber_attack_movement = def->bomber_attack_movement;
    u.disable_passive_targeting = def->disable_passive_targeting;
    u.turret_multi_targeting = def->turret_multi_targeting;
    u.move_speed = def->speed > 0 ? def->speed * EngineConstants::TICK_RATE : 0.0f;
    u.turn_speed = def->turn_speed;
    u.turn_acceleration = def->turn_acceleration;
    u.move_acceleration = def->move_acceleration;
    u.move_deceleration = def->move_deceleration;
    u.strafes = def->strafes;
    u.move_ignores_body = def->move_ignores_body;
    u.joins_group_formations = def->joins_group_formations;
    u.reverse_speed_multiplier = def->reverse_speed_multiplier;
    u.move_type = def->move_type;
    u.weapon_type = def->weapon_type;
    u.slow_death_fall = def->slow_death_fall;
    u.explode_on_death_ground_collision =
        def->explode_on_death_ground_collision;
    u.hide_scorch_mark = def->hide_scorch_mark;
    u.death_effect_uid_count = def->death_effect_uid_count;
    u.has_wreck_image = def->has_wreck_image;
    u.default_ground_death_effect = def->default_ground_death_effect;
    u.fire_turret_on_death = def->fire_turret_on_death;
    u.nuke_on_death = def->nuke_on_death;
    u.nuke_on_death_range = def->nuke_on_death_range;
    u.nuke_on_death_damage = def->nuke_on_death_damage;
    u.nuke_on_death_disable_when_no_nuke = def->nuke_on_death_disable_when_no_nuke;
    u.leg_definitions = def->legs;
    u.leg_rotation_slot = def->leg_rotation_slot;
    u.leg_death_scorch = def->leg_death_scorch;
    u.behavior = def->behavior;
    u.is_building = def->is_building;
    u.collision_enabled = !u.is_building;
    u.collision_footprint = def->collision_footprint;
    u.placement_footprint = def->placement_footprint;
    u.building_to_footprint_offset_x = def->building_to_footprint_offset_x;
    u.building_to_footprint_offset_y = def->building_to_footprint_offset_y;
    u.has_factory_exit_config = def->has_factory_exit_config;
    u.factory_exit_x = def->factory_exit_x;
    u.factory_exit_y = def->factory_exit_y;
    u.factory_exit_move_away_amount = def->factory_exit_move_away_amount;
    u.factory_exit_has_dir_offset = def->factory_exit_has_dir_offset;
    u.factory_exit_dir_offset = def->factory_exit_dir_offset;
    u.factory_exit_height_offset = def->factory_exit_height_offset;
    u.factory_exit_height_ignore_parent = def->factory_exit_height_ignore_parent;
    u.factory_keep_built_units = def->factory_keep_built_units;
    u.rotation = (u.is_building || name == "tree" || name == "spreadingFire")
        ? -90.0f : 0.0f;
    u.income_rate = def->income;
    u.income_timer = 0.0f;
    u.income_interval = def->income_interval;
    u.cost = def->cost;
    u.reclaim_cost = static_cast<f32>(def->cost);
    u.build_time = def->build_time;
    u.cooldown_max = def->cooldown_max;
    u.splash_radius = def->splash_radius;
    u.splash_damage = def->splash_damage;
    u.projectile_speed = def->projectile_speed >= 0.0f
        ? def->projectile_speed : EngineConstants::PROJECTILE_SPEED;
    u.projectile_lifetime = def->projectile_lifetime >= 0.0f
        ? def->projectile_lifetime : EngineConstants::PROJECTILE_LIFETIME;
    u.projectile_tracking = def->projectile_tracking;
    u.projectile_hitscan = def->projectile_hitscan;
    u.can_move = def->can_move;
    u.is_builder = def->is_builder;
    u.can_attack = def->can_attack;
    u.ignore_move_orders = def->ignore_move_orders;
    u.airborne = def->airborne;
    u.submerged = false;
    // The stock attack submarine starts with its dive state requested; the
    // JAR transitions below the waterline on its first simulation updates.
    u.submerge_requested = name == "attackSubmarine" ? true : false;
    u.height = 0.0f;
    // helicopter.o is its rotor lift state; the stock constructor starts it
    // at 0.14 and does not begin raising eq until it exceeds 0.4.
    u.vertical_speed = name == "helicopter" ? 0.14f : 0.0f;
    u.flight_phase = 0.0f;
    u.spawn_tick = tick_;
    u.spawn_clock_ms = simulation_clock_ms_;
    u.sight_range = def->sight_range;
    u.nano_range = def->nano_range;
    u.nano_turret_index = def->nano_turret_index;
    u.nano_build_speed = def->nano_build_speed;
    u.self_regen_per_frame = def->self_regen_rate;
    u.self_regen_rate = u.self_regen_per_frame * EngineConstants::FRAME_RATE;
    u.target_height = def->target_height;
    u.nano_factory_speed = def->nano_factory_speed;
    u.target_height_drift = def->target_height_drift;
    u.height_change_rate = def->height_change_rate;
    u.falling_acceleration = def->falling_acceleration;
    u.land_on_ground = def->land_on_ground;
    u.land_on_ground_only_idle = def->land_on_ground_only_idle;
    u.base_nano_range = def->base_nano_range;
    u.can_repair = def->can_repair;
    u.can_repair_any = def->can_repair_any;
    u.auto_repair = def->auto_repair;
    u.nano_repair_speed = def->nano_repair_speed;
    u.nano_reclaim_speed = def->nano_reclaim_speed;
    u.resource_reclaim_multiplier = def->resource_reclaim_multiplier;
    u.nano_unbuild_speed = def->nano_unbuild_speed;
    u.can_reclaim = def->can_reclaim;
    u.can_repair_buildings = def->can_repair_buildings;
    u.can_reclaim_resources = def->can_reclaim_resources;
    u.can_be_repaired = def->can_be_repaired;
    u.ammo = def->initial_ammo >= 0 ? def->initial_ammo : 0;
    u.ammo_max = def->ammo_max;
    u.ammo_reload_timer = def->ammo_reload_ticks;
    u.targets_air = def->targets_air;
    u.targets_ground = def->targets_ground;
    u.ground_attack_disabled_while_flying = def->ground_attack_disabled_while_flying;
    u.targets_underwater = def->targets_underwater;
    u.targets_not_touching_water = def->targets_not_touching_water;
    // custom.j constructs its turret state while the body still has the
    // constructor heading (cg=0).  Building metadata changes cg to -90 only
    // afterwards, so a custom building's slots do not start at the body's
    // final rotation.  The host loader supplies the INI idle direction as
    // the equivalent B(slot) offset.
    const f32 weapon_initial_rotation = u.custom_unit ? 0.0f : u.rotation;
    for (const WeaponDef& weapon : def->weapons) {
        WeaponSlot slot{};
        slot.damage = weapon.damage;
        slot.splash_damage = weapon.splash_damage;
        slot.splash_radius = weapon.splash_radius;
        slot.projectile_speed = weapon.projectile_speed;
        slot.projectile_speed_spread = weapon.projectile_speed_spread;
        slot.projectile_wobble_amplitude = weapon.projectile_wobble_amplitude;
        slot.projectile_wobble_frequency_frames = weapon.projectile_wobble_frequency_frames;
        slot.projectile_target_speed = weapon.projectile_target_speed;
        slot.projectile_acceleration = weapon.projectile_acceleration;
        slot.projectile_lifetime = weapon.projectile_lifetime;
        slot.splash_expand_time = weapon.splash_expand_time;
        slot.splash_friendly_fire = weapon.splash_friendly_fire;
        slot.projectile_retarget_on_death = weapon.projectile_retarget_on_death;
        slot.projectile_retarget_range = weapon.projectile_retarget_range;
        slot.projectile_retarget_lead = weapon.projectile_retarget_lead;
        slot.projectile_deflection_power = weapon.projectile_deflection_power;
        slot.projectile_tags = weapon.projectile_tags;
        slot.projectile_should_reveal_fog = weapon.projectile_should_reveal_fog;
        slot.cooldown_max = weapon.cooldown_max;
        slot.energy_usage = weapon.energy_usage;
        slot.laser_defence_enabled = weapon.laser_defence_enabled;
        slot.laser_defence_energy_usage = weapon.laser_defence_energy_usage;
        slot.intercept_projectile_tags = weapon.intercept_projectile_tags;
        slot.intercept_ground_target_distance = weapon.intercept_ground_target_distance;
        slot.intercept_projectile_distance = weapon.intercept_projectile_distance;
        slot.intercept_projectile_height = weapon.intercept_projectile_height;
        slot.intercept_ammo_usage = weapon.intercept_ammo_usage;
        slot.intercept_required_flags = weapon.intercept_required_flags;
        slot.intercept_missing_flags = weapon.intercept_missing_flags;
        slot.limiting_range = weapon.limiting_range;
        slot.limiting_min_range = weapon.limiting_min_range;
        slot.limiting_angle = weapon.limiting_angle;
        slot.fire_delay_frames = weapon.fire_delay_frames > 0.0f
            ? weapon.fire_delay_frames
            : static_cast<f32>(weapon.cooldown_max);
        slot.recoil_offset = weapon.recoil_offset;
        slot.recoil_out_frames = weapon.recoil_out_frames;
        slot.recoil_return_frames = weapon.recoil_return_frames;
        slot.cooldown_ticks = 0;
        slot.tracking = weapon.tracking;
        slot.fixed_target = weapon.fixed_target;
        slot.fixed_target_include_height = weapon.fixed_target_include_height;
        slot.disable_lead_targeting = weapon.disable_lead_targeting;
        slot.lead_targeting_speed = weapon.lead_targeting_speed;
        slot.ground_target_height_offset = weapon.ground_target_height_offset;
        slot.ground_target_spread = weapon.ground_target_spread;
        slot.hitscan = weapon.hitscan;
        slot.persist_after_hit = weapon.persist_after_hit;
        slot.instant_reuse_last = weapon.instant_reuse_last;
        slot.instant_reuse_last_change_turret_aim = weapon.instant_reuse_last_change_turret_aim;
        slot.instant_reuse_last_keep_area_damage_list = weapon.instant_reuse_last_keep_area_damage_list;
        slot.move_with_parent = weapon.move_with_parent;
        slot.sweep_offset = weapon.sweep_offset;
        slot.sweep_offset_from_target_radius = weapon.sweep_offset_from_target_radius;
        slot.projectile_flame_weapon = weapon.projectile_flame_weapon;
        slot.laser_defence_excluded = weapon.laser_defence_excluded;
        slot.splash_same_layer = weapon.splash_same_layer;
        slot.targets_air = weapon.targets_air;
        slot.targets_ground = weapon.targets_ground;
        slot.targets_underwater = weapon.targets_underwater;
        slot.targets_not_touching_water = weapon.targets_not_touching_water;
        slot.shield_mult = weapon.shield_mult;
        slot.shield_absorb = weapon.shield_absorb;
        slot.hp_mult = weapon.hp_mult;
        slot.building_damage_mult = weapon.building_damage_mult;
        slot.air_damage_mult = weapon.air_damage_mult;
        slot.experimental_damage_mult = weapon.experimental_damage_mult;
        slot.warmup_frames = weapon.warmup_frames;
        slot.warmup_no_reset = weapon.warmup_no_reset;
        slot.warmup_call_down_rate = weapon.warmup_call_down_rate;
        slot.warmup_shoot_delay_transfer = weapon.warmup_shoot_delay_transfer;
        slot.fire_enabled = weapon.fire_enabled;
        slot.linked_slot = weapon.linked_slot;
        slot.delay_link_slot = weapon.delay_link_slot;
        slot.turret_angle = weapon_initial_rotation +
            weapon.turret_idle_direction;
        slot.turret_previous_angle = slot.turret_angle;
        slot.turret_turn_speed = weapon.turret_turn_speed;
        slot.turret_turn_acceleration = weapon.turret_turn_acceleration;
        slot.turret_turn_deceleration = weapon.turret_turn_deceleration;
        slot.turret_resets_to_idle = weapon.turret_resets_to_idle;
        slot.turret_idle_direction = weapon.turret_idle_direction;
        slot.turret_idle_direction_reversing =
            weapon.turret_idle_direction_reversing;
        slot.turret_idle_spin = weapon.turret_idle_spin;
        slot.turret_idle_sweep_angle = weapon.turret_idle_sweep_angle;
        slot.turret_idle_sweep_delay = weapon.turret_idle_sweep_delay;
        slot.turret_idle_sweep_speed = weapon.turret_idle_sweep_speed;
        slot.turret_idle_sweep_random_delay =
            weapon.turret_idle_sweep_random_delay;
        slot.turret_idle_sweep_random_angle =
            weapon.turret_idle_sweep_random_angle;
        slot.turret_idle_sweep_requires_stationary =
            weapon.turret_idle_sweep_requires_stationary;
        slot.aim_tolerance = weapon.aim_tolerance;
        slot.aim_offset_spread = weapon.aim_offset_spread;
        slot.muzzle_offset = weapon.muzzle_offset;
        slot.muzzle_side = weapon.muzzle_side;
        slot.muzzle_side_odd = weapon.muzzle_side_odd;
        slot.mount_forward = weapon.mount_forward;
        slot.mount_side = weapon.mount_side;
        slot.attached_to_slot = weapon.attached_to_slot;
        slot.projectile_uses_muzzle = weapon.projectile_uses_muzzle;
        slot.height_guided = weapon.height_guided;
        slot.nuke_weapon = weapon.nuke_weapon;
        slot.launch_height = weapon.launch_height;
        slot.cruise_height = weapon.cruise_height;
        slot.vertical_speed = weapon.vertical_speed;
        slot.projectile_unguided_speed_height = weapon.projectile_unguided_speed_height;
        slot.projectile_unguided_speed_x = weapon.projectile_unguided_speed_x;
        slot.projectile_unguided_speed_y = weapon.projectile_unguided_speed_y;
        slot.projectile_gravity = weapon.projectile_gravity;
        slot.projectile_true_gravity = weapon.projectile_true_gravity;
        slot.fixed_turret = weapon.fixed_turret;
        slot.turret_rotate_with_body = weapon.turret_rotate_with_body;
        u.weapon_slots.push_back(slot);
    }
    u.upgrade_to = def->upgrade_to;
    u.cargo_capacity = def->cargo_capacity;
    u.transport_unload_condition = def->transport_unload_condition;
    u.transport_add_unload_option = def->transport_add_unload_option;
    u.transport_unload_delay = def->transport_unload_delay;
    u.hovercraft_unload_style = def->hovercraft_unload_style;
    u.cargo_weight = def->cargo_weight;
    u.path_min_clearance = def->path_min_clearance;
    u.transportable = def->transportable;
    u.unloading_all = false;
    u.unload_timer_frames = 0.0f;
    u.auto_repair_timer_frames = 0.0f;
    configure_service_warmup(u);
    u.lifetime_frames = name == "fogRevealer" ? 60.0f : -1.0f;
    if (name == "damagingBorder" || name == "zoneMarker") {
        u.border_current_radius = 2000.0f;
        u.border_target_radius = 2000.0f;
        u.border_damage_multiplier = 1.0f;
        u.border_initialized = true;
        u.zone_marker = name == "zoneMarker";
    }
    if (name == "spreadingFire") {
        u.fire_intensity = 0.05f;
        u.fire_growth = 0.0005f;
        u.fire_max_intensity = 1.0f;
        u.fire_spread_threshold = 0.3f;
        u.fire_spread_timer = 0.0f;
        u.fire_animation_timer = 0.0f;
        u.fire_initialized = false;
        u.fire_spread_exhausted = false;
    }
    u.is_nuke = def->is_nuke;
    u.is_antinuke = def->is_antinuke;
    u.nuke_range = def->nuke_range;
    u.max_energy = u.custom_unit || def->max_energy > 0.0f
        ? def->max_energy
        : (def->energy_per_shot > 0 ? 100.0f : 0.0f);
    u.energy = u.custom_unit || def->max_energy > 0.0f
        ? def->initial_energy : 0.0f;
    u.energy_per_shot = def->energy_per_shot;
    u.energy_regen_rate = def->max_energy > 0.0f
        ? def->energy_regen_rate
        : (def->energy_per_shot > 0 ? 1.0f : 0.0f);
    u.energy_regen_per_frame = def->energy_regen_per_frame;
    u.energy_recharge_regen_per_frame = def->energy_recharge_regen_per_frame;
    u.energy_needs_full_recharge = def->energy_needs_full_recharge;
    u.die_on_zero_energy = def->die_on_zero_energy;
    u.shield_mult = def->shield_mult;
    u.shield_absorb = def->shield_absorb;
    u.hp_mult = def->hp_mult;
    u.air_damage_mult = def->air_damage_mult;
    u.experimental_damage_mult = def->experimental_damage_mult;
    u.is_experimental = def->is_experimental;
    u.building_damage_mult = def->building_damage_mult;
    u.is_melee = def->is_melee;
    u.is_beam = def->is_beam;
    u.chain_count = def->chain_count;
    u.chain_range = def->chain_range;
    u.is_flame = def->is_flame;
    u.flame_expand_rate = def->flame_expand_rate;
    u.can_build = def->can_build;
    u.built_from = def->built_from;
    u.tech_level = def->tech_level;

    if (def->is_building) {
        u.build_progress = (start_built || name == "commandCenter") ? 1.0f : 0.0f;
    } else {
        u.build_progress = 1.0f;
    }
    u.build_progress_secondary = u.build_progress;

    if (def->range > 0 && (def->damage > 0 || def->splash_damage > 0) &&
        def->can_attack && !u.is_nuke && !u.is_antinuke) {
        u.weapon_type = u.is_mobile() ? WeaponType::ATTACK : WeaponType::GUARD;
    }

    if (u.build_progress >= 1.0f && u.income_rate > 0) {
        total_income_rate_ += u.income_rate;
    }

    initialize_legs_(u);
    u32 uid = u.uid;
    units_[uid] = std::move(u);
    grid_.add(units_[uid]);
    units_built_++;

    if (units_[uid].is_building) {
        rebuild_local_buildings_();
    }
    if (fog_enabled()) reveal_visibility_(units_[uid]);

    return &units_[uid];
}

Unit* World::get_unit(u32 uid) {
    auto it = units_.find(uid);
    return it != units_.end() ? &it->second : nullptr;
}

const Unit* World::get_unit(u32 uid) const {
    auto it = units_.find(uid);
    return it != units_.end() ? &it->second : nullptr;
}

Unit* World::formation_unit_(u32 uid) {
    World* owner = find_order_target_world_(uid, peer_worlds_);
    return owner ? owner->get_unit(uid) : nullptr;
}

void World::rebuild_formation_after_leader_removed_(
        Unit& leader, const Set<u32>* excluded_uids) {
    const Vec<World*> owners = peer_worlds_.empty()
        ? Vec<World*>{this} : peer_worlds_;
    Vec<Unit*> direct_followers;
    for (World* owner : owners) {
        for (auto& [_, unit] : owner->units_) {
            if (unit.uid != leader.uid &&
                unit.formation_leader_uid == leader.uid) {
                direct_followers.push_back(&unit);
            }
        }
    }
    if (direct_followers.empty()) return;

    std::stable_sort(
        direct_followers.begin(), direct_followers.end(),
        [](const Unit* lhs, const Unit* rhs) {
            return lhs->object_creation_order < rhs->object_creation_order;
        });

    // y.aD() clears every direct link, then uses the last follower visited in
    // global creation order to recover the shared au.i/ab controller.
    Unit* controller_source = nullptr;
    for (Unit* follower : direct_followers) {
        follower->formation_leader_uid = 0;
        if (!excluded_uids ||
            excluded_uids->count(follower->uid) == 0) {
            controller_source = follower;
        }
    }
    if (!controller_source || controller_source->order_queue.empty()) return;

    const UnitOrder& source_order = controller_source->order_queue.front();
    const u64 group_id = source_order.formation_group_id;
    if (group_id == 0) return;
    const u64 controller_id = controller_source->formation_controller_id;

    Vec<Unit*> members;
    for (World* owner : owners) {
        for (auto& [_, unit] : owner->units_) {
            // ab.a() uses I()/bg() and the current waypoint's controller.
            // am.bT() only gates updates; transported members still regroup.
            if (unit.uid == leader.uid || !unit.is_alive || !unit.is_mobile() ||
                (excluded_uids && excluded_uids->count(unit.uid) != 0) ||
                unit.order_queue.empty() ||
                unit.order_queue.front().formation_group_id != group_id ||
                (controller_id != 0 &&
                 unit.formation_controller_id != controller_id)) {
                continue;
            }
            members.push_back(&unit);
        }
    }
    if (members.empty()) return;
    std::stable_sort(
        members.begin(), members.end(),
        [](const Unit* lhs, const Unit* rhs) {
            return lhs->object_creation_order < rhs->object_creation_order;
        });
    GameCommand regroup;
    regroup.action = static_cast<GameCommand::Action>(source_order.type);
    regroup.target_x = source_order.x;
    regroup.target_y = source_order.y;
    regroup.unit_type = source_order.unit_type;
    regroup.build_tech_level = source_order.build_tech_level;
    regroup.action_id = source_order.action_id;
    regroup.relaxed_formation =
        source_order.formation_controller_relaxed_formation;
    regroup.group_size = static_cast<u8>(
        std::min<std::size_t>(255, members.size()));

    if (source_order.target_uid != 0) {
        const Unit* target = get_unit(source_order.target_uid);
        if (!target) {
            for (const Map<u32, Unit>* source : object_cost_sources_) {
                if (!source) continue;
                auto found = source->find(source_order.target_uid);
                if (found != source->end()) {
                    target = &found->second;
                    break;
                }
            }
        }
        if (target) {
            regroup.target_x = target->x;
            regroup.target_y = target->y;
        }
    }

    Vec<u32> saved_pending_formation_uids = pending_formation_uids_;
    const bool saved_formation_rebuild_active = formation_rebuild_active_;
    pending_formation_uids_.clear();
    pending_formation_uids_.reserve(members.size());
    for (const Unit* member : members) {
        pending_formation_uids_.push_back(member->uid);
    }
    formation_rebuild_active_ = true;
    configure_command_formation_(regroup, owners, controller_id);
    formation_rebuild_active_ = saved_formation_rebuild_active;
    pending_formation_uids_ = std::move(saved_pending_formation_uids);

}

void World::remove_unit(u32 uid) {
    Unit* u = get_unit(uid);
    if (!u) return;

    if (u->cleanup_done) return;

    retired_beam_projectiles_.erase(u->active_beam_projectile_uid);

    // A reclaim/direct delete enters here while the object is still alive and
    // is exposed as a -1 HP sentinel for one frame. Damage deaths arrive with
    // is_dead already set and remain observable as zero-HP wrecks for custom
    // buildings. Preserve that distinction before the common cleanup marks
    // every removed object dead below.
    const bool was_already_dead = u->is_dead;

    if (was_already_dead && !u->custom_unit && u->unit_type == "tree") {
        // am.bv() removes the unit from bE before al.e()/k() moves the
        // trunk. Placement/collision falls invoke k() without that removal.
        fell_tree_(*u, true);
        u->native_unit_list_member = false;
        return;
    }

    // These shipped units use the default ab.b ground death effect.
    // y.a(ab.b, true) creates two gameFramework.d.f effect objects and one
    // game.l scorch mark when the unit is not over liquid. All three inherit
    // w and therefore consume the same global UID and creation-order counters
    // as gameplay objects, even though the native engine does not otherwise
    // simulate these visual-only objects.
    if (u->is_dead && u->death_effect_uid_count > 0) {
        for (i32 i = 0; i < u->death_effect_uid_count; ++i) {
            if (global_next_uid_) {
                ++(*global_next_uid_);
            } else {
                ++next_uid_;
            }
            (void)allocate_object_order_();
        }
    }

    if ((u->is_dead || u->surrender_death) && u->custom_unit &&
        u->build_progress >= 1.0f && u->fire_turret_on_death >= 0 &&
        u->fire_turret_on_death < static_cast<i32>(u->weapon_slots.size())) {
        // custom.j.e() calls the projectile factory directly, bypassing
        // canShoot, targeting, energy and reload checks. This happens before
        // the ordinary death effects consume their shared object IDs.
        Unit point{};
        point.x = u->x;
        point.y = u->y;
        point.height = 0.0f;
        create_weapon_projectile_(
            *u, u->weapon_slots[u->fire_turret_on_death], point, true);
    }

    if ((u->is_dead || u->surrender_death) && u->custom_unit &&
        u->build_progress >= 1.0f && u->nuke_on_death &&
        !(u->nuke_on_death_disable_when_no_nuke &&
          !singleplayer_mode_ && !nukes_enabled_)) {
        // custom.j.e() uses q.a's two-argument projectile factory, which
        // does not sample I or advance bC. Only aH, Z and Y are overridden.
        // Normal object scheduling also preserves a nuke born while the
        // pending-projectile pass is running until the next native turn.
        Projectile p{};
        p.uid = global_next_uid_ ? (*global_next_uid_)++ : next_uid_++;
        p.creation_order = allocate_object_order_();
        p.owner_uid = u->uid;
        p.team_id = team_id_;
        p.x = p.target_x = u->x;
        p.y = p.target_y = u->y;
        p.damage = 300.0f;
        p.splash_radius = u->nuke_on_death_range;
        p.splash_damage = u->nuke_on_death_damage;
        p.speed_per_frame = 0.1f;
        p.speed = p.speed_per_frame * EngineConstants::FRAME_RATE;
        p.target_speed = 2.7f * EngineConstants::FRAME_RATE;
        p.lifetime = 99999.0f;
        p.is_nuke = p.fixed_target = p.impact_at_target = true;
        p.splash_friendly_fire = true;
        p.should_reveal_fog = true;
        p.expanding_splash = true;
        p.splash_expand_remaining_frames = p.splash_expand_duration_frames = 75.0f;
        p.launch_height = 80.0f;
        p.cruise_height = 100.0f;
        p.vertical_speed = 1.1f;
        p.laser_defence_excluded = true;
        projectiles_.emplace(p.uid, std::move(p));
    }

    // The stock command center uses d.e's two-part death animation rather
    // than the configured unit effect metadata. Its native bv() path creates
    // five visual game objects (four effects and one scorch mark) for damage
    // and surrender deaths. They consume the shared object UID/order stream.
    // Direct reclaim/delete does not enter this native death animation.
    if (u->is_dead && u->unit_type == "commandCenter") {
        for (i32 i = 0; i < 4; ++i) {
            if (global_next_uid_) {
                ++(*global_next_uid_);
            } else {
                ++next_uid_;
            }
            (void)allocate_object_order_();
        }
        if (!u->hide_scorch_mark && try_create_scorch_mark_(u->x, u->y)) {
            if (global_next_uid_) {
                ++(*global_next_uid_);
            } else {
                ++next_uid_;
            }
            (void)allocate_object_order_();
        }
    }

    // custom.j.e() settles its existing feet with a zero-time h update and
    // creates a scorch mark at each visible exploding leg before the body
    // smoke/scorch bundle. Each point has its own liquid and density checks.
    if ((u->is_dead || u->surrender_death) && u->custom_unit &&
        u->build_progress >= 1.0f && u->leg_death_scorch) {
        update_legs_(*u, 0.0f);
        for (std::size_t i = 0; i < u->legs.size(); ++i) {
            const LegDef& definition = u->leg_definitions[i];
            if (!definition.explode_on_death || definition.hidden ||
                u->hide_scorch_mark) continue;
            const f32 x = u->x + u->legs[i].x;
            const f32 y = u->y + u->legs[i].y;
            const i32 tx = pathfinder_.tile_x(x);
            const i32 ty = pathfinder_.tile_y(y);
            if (pathfinder_.has_terrain(tx, ty) &&
                (pathfinder_.terrain(tx, ty) &
                 (TERRAIN_WATER | TERRAIN_LAVA)) != 0) continue;
            if (!try_create_scorch_mark_(x, y)) continue;
            if (global_next_uid_) ++(*global_next_uid_);
            else ++next_uid_;
            (void)allocate_object_order_();
        }
    }

    // Completed custom ground units use y.a(ab,true)'s two smoke objects
    // and scorch mark whether or not custom.j.e() keeps a wreck image.
    // Incomplete custom units use ab.verysmall; airborne/liquid deaths skip smoke.
    // Explicit effectOnDeath allocations above remain independent of this
    // default branch. Stock ground units retain their native death behavior.
    const bool custom_ground_effect =
        u->custom_unit && u->default_ground_death_effect &&
        u->build_progress >= 1.0f && !u->airborne;
    const bool stock_ground_effect = !u->custom_unit &&
        (u->unit_type == "builder" || u->unit_type == "tank" ||
         u->unit_type == "heavyTank" ||
         u->unit_type == "c_tank" || u->unit_type == "scout" ||
         u->unit_type == "hoverTank" || u->unit_type == "hovercraft" ||
         u->unit_type == "heavyHoverTank" ||
         u->unit_type == "experimentalHoverTank") &&
        u->death_effect_uid_count == 0;
    // d.d.e() dispatches L() only after completion. Air/sea factories use
    // ab.large and repair bays use ab.normal; experimental factories use
    // ab.verylarge. Each creates the shared
    // smoke/scorch objects when cK() finds no liquid at the unit's position.
    const bool stock_factory_ground_effect = !u->custom_unit &&
        u->build_progress >= 1.0f &&
        (u->unit_type == "airFactory" || u->unit_type == "seaFactory" ||
         u->unit_type == "repairbay" ||
         u->unit_type == "experimentalLandFactory");
    if ((u->is_dead || u->surrender_death) &&
        (custom_ground_effect || stock_ground_effect ||
         stock_factory_ground_effect)) {
        const i32 tile_x = pathfinder_.tile_x(u->x);
        const i32 tile_y = pathfinder_.tile_y(u->y);
        bool over_liquid = false;
        if (pathfinder_.has_terrain(tile_x, tile_y)) {
            const u8 terrain = pathfinder_.terrain(tile_x, tile_y);
            // am.cK()/utility.y.d() reads water and lava, independently of
            // the bridge flag used by movement. Bridges still emit smoke.
            over_liquid = (terrain & (TERRAIN_WATER | TERRAIN_LAVA)) != 0;
        }
        if (!over_liquid) {
            for (i32 i = 0; i < 2; ++i) {
                if (global_next_uid_) {
                    ++(*global_next_uid_);
                } else {
                    ++next_uid_;
                }
                (void)allocate_object_order_();
            }
            if (!u->hide_scorch_mark && try_create_scorch_mark_(u->x, u->y)) {
                if (global_next_uid_) {
                    ++(*global_next_uid_);
                } else {
                    ++next_uid_;
                }
                (void)allocate_object_order_();
            }
        }
    }

    if (u->build_progress >= 1.0f && u->income_rate > 0) {
        total_income_rate_ = std::max(0.0f, total_income_rate_ - u->income_rate);
    }

    grid_.remove(*u);

    // custom.j.e() keeps B and every cargo cN when slowDeathFall (fi) is
    // enabled. Its dead update invokes ds() at the first grounded turn.
    // Incomplete units and direct removal enter a(), which releases now.
    const bool cargo_waits_for_impact = u->custom_unit && u->slow_death_fall &&
        u->build_progress >= 1.0f && (was_already_dead || u->surrender_death);
    if (!cargo_waits_for_impact) {
        release_transport_cargo_on_death_(*u);
    }

    for (auto& [_, other] : units_) {
        if (other.target_uid == uid &&
            (other.target_team_id < 0 || other.target_team_id == team_id_)) {
            other.target_uid = 0;
            other.target_team_id = -1;
        }
        if (other.guard_target_uid == uid &&
            other.guard_target_team_id == team_id_) {
            other.guard_target_uid = 0;
            other.guard_target_team_id = -1;
        }
        if (other.follow_target_uid == uid &&
            other.follow_target_team_id == team_id_) {
            other.follow_target_uid = 0;
            other.follow_target_team_id = -1;
        }
        if (other.parent_uid == uid && !cargo_waits_for_impact) other.parent_uid = 0;
    }

    for (auto& [fid, fq] : factory_queues_) {
        if (fid != uid) continue;
        for (const FactoryQueueItemState& state : fq.item_states) u->ammo += state.ammo_cost;
        for (f32 cost : fq.upfront_costs) {
            refund_credits(cost, EngineConstants::CANCEL_REFUND);
        }
        if (fq.resource_progress > 0.0f && !fq.queue.empty()) {
            refund_credits(
                static_cast<f32>(factory_queue_item_cost_(*u, fq.queue.front())) *
                    fq.resource_progress,
                EngineConstants::CANCEL_REFUND);
        }
    }
    factory_queues_.erase(uid);

    // am.bv() removes the unit from active gameplay and drops its own
    // command state before the corpse remains observable for cleanup. y.az()
    // also asks the shared ab controller to regroup surviving followers when
    // this unit was their formation leader.
    rebuild_formation_after_leader_removed_(*u);
    // y.az() calls a(null) after aD(): even a follower with no dependants
    // drops its own ad link before cached collision pairs run later this tick.
    u->formation_leader_uid = 0;
    // y.az() invokes aH(), which clears the adopted path but leaves the
    // per-frame movement proposal k/l/m untouched on the corpse.
    const bool death_move_target = u->has_move_target;
    const bool death_arrived = u->arrived;
    f32 death_target_x = u->target_x;
    f32 death_target_y = u->target_y;
    // A waypoint queued while cN suspends y.k() has not replaced l/m yet.
    // bt()/az() leaves that retained proposal on the released cargo's wreck.
    if (death_move_target && !u->order_queue.empty() &&
        !u->clear_exposed_move_next_frame && !u->activate_order_next_frame) {
        const UnitOrder& order = u->order_queue.front();
        const Unit* order_target = nullptr;
        if (order.target_uid != 0) {
            order_target = get_unit(order.target_uid);
            if (!order_target) {
                for (const Map<u32, Unit>* source : object_cost_sources_) {
                    if (!source) continue;
                    const auto found = source->find(order.target_uid);
                    if (found != source->end()) {
                        order_target = &found->second;
                        break;
                    }
                }
            }
        }
        if (order.type == WeaponType::GUARD &&
            u->guard_movement_target_valid) {
            death_target_x = u->guard_movement_target_x;
            death_target_y = u->guard_movement_target_y;
        } else if (order.type == WeaponType::ATTACK_MOVE &&
                   u->attack_move_combat_target_valid) {
            death_target_x = u->attack_move_combat_target_x;
            death_target_y = u->attack_move_combat_target_y;
        } else if (order.type == WeaponType::PATROL &&
                   u->patrol_logical_target_active) {
            death_target_x = u->patrol_logical_target_x;
            death_target_y = u->patrol_logical_target_y;
        } else if (order.type == WeaponType::ATTACK) {
            death_target_x = u->attack_order_target_x;
            death_target_y = u->attack_order_target_y;
        } else if (order.type == WeaponType::LOAD_INTO ||
                   order.type == WeaponType::LOAD_UP) {
            death_target_x = u->transport_order_target_x;
            death_target_y = u->transport_order_target_y;
        } else if (order_target) {
            death_target_x = order_target->x;
            death_target_y = order_target->y;
        } else if (order.type == WeaponType::MOVE ||
                   order.type == WeaponType::ATTACK_MOVE ||
                   order.type == WeaponType::BUILD ||
                   order.type == WeaponType::PATROL ||
                   order.type == WeaponType::GUARD_AT) {
            death_target_x = order.x;
            death_target_y = order.y;
        }
    }
    clear_current_order_state_(*u);
    u->has_move_target = death_move_target;
    u->arrived = death_arrived;
    u->target_x = death_target_x;
    u->target_y = death_target_y;
    u->order_queue.clear();
    for (WeaponSlot& slot : u->weapon_slots) {
        slot.target_uid = 0;
        slot.target_team_id = -1;
    }
    u->is_dead = true;
    u->is_alive = false;
    u->death_tick = tick_;
    u->death_clock_ms = simulation_clock_ms_;
    u->death_age_frames = 0.0f;
    // Damage and surrender enter bv(), where e() decides whether the wreck
    // remains addressable. Direct delete/reclaim calls instead remove the
    // object without invoking that persistence branch.
    u->death_persistent =
        jar_death_persists(*u) && (was_already_dead || u->surrender_death);
    // e.j keeps dK across am.bv(): a ground unit can die while its live
    // height update is already falling. Aircraft and ships own other fields.
    u->crash_fall_velocity = !u->custom_unit && u->can_move &&
        (u->move_type == MoveType::LAND || u->move_type == MoveType::HOVER)
            ? u->vertical_speed : 0.0f;
    u->crash_impacted = false;
    // The JAR caches cK() on the first dead update at or below height zero,
    // after any horizontal slowDeathFall movement. Do not sample the death
    // position here; a sliding wreck can cross a terrain boundary first.
    u->crash_over_water = false;
    u->cleanup_done = true;
    u->native_unit_list_member = false;
    // am.bu() clamps positive HP only. Surrender and released cargo enter
    // bv() with cu=-1 and retain it on the dead object; damage uses zero.
    if (u->hp > 0.0f) u->hp = 0.0f;
    // JAR slowDeathFall units retain cc/cd so n(float) keeps sliding while
    // the custom wreck fall is applied on subsequent object turns. Stock
    // hover units also retain cc/cd: their e.j death update only changes
    // height, but does not clear the inertial velocity fields.
    if (!u->slow_death_fall &&
        (u->move_type != MoveType::HOVER || u->custom_unit)) {
        u->vel_x = 0.0f;
        u->vel_y = 0.0f;
    }

    if (u->is_building) {
        rebuild_local_buildings_();
    }

}

} // namespace rwrl
