#include "world_internal.h"

#include <cstdio>
#include <cstdlib>

namespace rwrl {

void World::surrender() {
    if (surrendered_) return;
    surrendered_ = true;
    // n.f() records l.by in its first team update after the command.
    surrender_start_clock_ms_ = -1;
}

void World::update_surrender_() {
    if (!surrendered_) return;
    if (surrender_start_clock_ms_ < 0) {
        surrender_start_clock_ms_ = simulation_clock_ms_;
    }

    // game.n.f() uses l.by and gradually
    // disposes of surrendered units. The command center is always selected;
    // ordinary units use the same deterministic per-object draw as the JAR.
    const i32 elapsed_clock = std::max(
        0, simulation_clock_ms_ - surrender_start_clock_ms_);
    i32 random_threshold = -1;
    i32 selection_limit = 1;
    if (elapsed_clock > 10000) {
        random_threshold = -1;
        selection_limit = 50;
    } else if (elapsed_clock > 6000) {
        random_threshold = 90;
        selection_limit = 20;
    } else if (elapsed_clock > 2000) {
        random_threshold = 98;
        selection_limit = 2;
    }

    Vec<Unit*> candidates;
    for (auto& [_, unit] : units_) {
        // n.f() scans am.bE, including passengers detached only from the
        // spatial index. Their next object update observes the lethal HP.
        if (!unit.is_alive || unit.is_dead || !unit.native_unit_list_member) continue;
        candidates.push_back(&unit);
    }
    std::sort(candidates.begin(), candidates.end(), [](const Unit* lhs, const Unit* rhs) {
        return lhs->object_creation_order < rhs->object_creation_order;
    });

    i32 selected = 0;
    for (Unit* unit : candidates) {
        bool remove = unit->unit_type == "commandCenter";
        if (!remove && random_threshold >= 0) {
            remove = deterministic_unit_random_int(
                         *unit, tick_, 0, 100, 0) > random_threshold;
        } else if (!remove && random_threshold < 0 && elapsed_clock > 10000) {
            remove = true;
        }
        if (!remove) continue;

        // The JAR writes cu=-1 here, after the object pass. Leave the unit
        // alive until the next object turn instead of invoking damage/death
        // cleanup in this team-update phase.
        unit->hp = -1.0f;
        unit->surrender_death = true;
        ++selected;
        if (selected > selection_limit) break;
    }
}
i32 World::count_active_units() const {
    i32 count = 0;
    for (auto& [_, u] : units_) {
        // Transported units are removed from active simulation in this
        // engine, but JAR C(am) only detaches them from the spatial index.
        // They remain alive and continue to consume the team's unit cap.
        if (!u.is_dead && !u.is_building) count++;
    }
    // game.n.T.b also includes queued non-building products. Upgrade, nuke
    // and anti-nuke actions have no produced UnitDef and do not consume cap.
    for (const auto& [_, queue] : factory_queues_) {
        for (const Str& item : queue.queue) {
            const UnitDef* product = registry_.get(item);
            if (product && !product->is_building) ++count;
        }
    }
    return count;
}

void World::step(f32 dt, f32 speed_mult, f32 income_mult, World* enemy_world) {
    Vec<World*> enemies;
    if (enemy_world) enemies.push_back(enemy_world);
    step(dt, speed_mult, income_mult, enemies);
}

void World::step(f32 dt, f32 speed_mult, f32 income_mult, const Vec<World*>& enemy_worlds) {
    begin_step(dt, speed_mult, income_mult, enemy_worlds);
    if (paused_) return;

    Vec<World*> hostile_worlds;
    hostile_worlds.reserve(enemy_worlds.size());
    for (World* other : enemy_worlds) {
        if (other && other != this && is_enemy_with(*other)) {
            hostile_worlds.push_back(other);
        }
    }
    const f32 game_dt = dt * speed_mult;
    auto update_unit = [&](u32 uid) {
        Unit* current = get_unit(uid);
        if (!current) return;
        if (!update_unit_lifecycle(
                game_dt * EngineConstants::FRAME_RATE, uid)) return;
        const UnitDef* custom_energy_definition = current->custom_unit
            ? registry_.get(current->unit_type) : nullptr;
        if (!current->custom_unit) update_unit_shields(game_dt, uid);
        if (!current->custom_unit && current->unit_type != "laserDefence") {
            update_unit_energy(game_dt, uid);
        }
        update_unit_ammo(dt, speed_mult, uid);
        const bool stock_transport = !current->custom_unit &&
            (current->unit_type == "hovercraft" || current->unit_type == "dropship");
        const bool stock_post_movement_height = stock_transport ||
            (!current->custom_unit &&
             (current->unit_type == "gunShip" ||
              current->move_type == MoveType::HOVER ||
              (current->move_type == MoveType::LAND && current->can_move)));
        if (!current->custom_unit && !stock_post_movement_height) {
            update_unit_flying_states(dt, speed_mult, uid);
        }
        update_unit_runtime_precombat(game_dt, uid);
        update_unit_movement(0.0f, uid, enemy_worlds);
        publish_single_attack_move_proposal_(uid, enemy_worlds);
        update_unit_builds(dt, speed_mult, uid);
        update_unit_command_behaviors(
            game_dt * EngineConstants::FRAME_RATE, uid, enemy_worlds);
        if (!hostile_worlds.empty()) {
            update_unit_combat(uid, enemy_worlds);
        }
        update_unit_cooldowns(game_dt, uid);
        update_unit_movement(game_dt, uid, enemy_worlds);
        apply_unit_collision_impulse(uid);
        // Stock submersibles finish their vertical update after movement.
        update_unit_submersible_states(dt, speed_mult, uid);
        if (current->custom_unit && current->is_active()) {
            update_legs_(*current, game_dt * EngineConstants::FRAME_RATE);
        }
        if (current->custom_unit) {
            update_unit_auto_repair(
                game_dt * EngineConstants::FRAME_RATE, uid, enemy_worlds);
            update_unit_laser_defence(game_dt, uid, enemy_worlds);
            update_unit_energy(game_dt, uid, custom_energy_definition);
            if (!current->is_alive) return;
            update_unit_shields(game_dt, uid);
        }
        const bool custom_production = current->custom_unit;
        if (custom_production) {
            update_unit_passive_health(game_dt, uid);
            update_unit_factory_production(dt, speed_mult, uid, enemy_worlds);
        }
        update_unit_income(dt, speed_mult, income_mult, uid);
        if (custom_production) {
            update_unit_transport_unloading(dt, speed_mult, uid);
            update_unit_flying_states(dt, speed_mult, uid);
        }
        update_unit_action_building_state_(
            game_dt * EngineConstants::FRAME_RATE, uid);
        update_unit_auto_actions_(
            game_dt * EngineConstants::FRAME_RATE, uid, enemy_worlds);
        if (!custom_production) {
            update_unit_passive_health(game_dt, uid);
            update_repair_bay(
                game_dt * EngineConstants::FRAME_RATE, uid, enemy_worlds);
        }
        update_dynamic_map_units(
            game_dt * EngineConstants::FRAME_RATE, enemy_worlds, uid);
        if (!custom_production) {
            update_unit_factory_production(dt, speed_mult, uid, enemy_worlds);
        }
        if (!custom_production) {
            if (current->unit_type == "laserDefence") update_unit_energy(game_dt, uid);
            update_unit_laser_defence(game_dt, uid, enemy_worlds);
        }
        update_unit_antinuke_interception(game_dt, uid, enemy_worlds);
        if (!custom_production) update_unit_transport_unloading(dt, speed_mult, uid);
        // Recheck the native bT() gate after LOAD_INTO's attachment.
        if (stock_post_movement_height) {
            update_unit_flying_states(dt, speed_mult, uid);
        }
        update_unit_service_decay(
            game_dt * EngineConstants::FRAME_RATE, uid);
    };

    enum class ObjectKind : u8 { UNIT, PROJECTILE };
    struct ObjectUpdate {
        u64 creation_order;
        u32 uid;
        ObjectKind kind;
    };
    Set<u64> known_object_orders;
    auto collect_new_objects = [&]() {
        Vec<ObjectUpdate> objects;
        for (const auto& [uid, unit] : units_) {
            if (known_object_orders.insert(
                    unit.object_creation_order).second) {
                objects.push_back(
                    {unit.object_creation_order, uid, ObjectKind::UNIT});
            }
        }
        for (const auto& [uid, projectile] : projectiles_) {
            if (known_object_orders.insert(projectile.creation_order).second) {
                objects.push_back(
                    {projectile.creation_order, uid, ObjectKind::PROJECTILE});
            }
        }
        std::sort(objects.begin(), objects.end(),
                  [](const ObjectUpdate& lhs, const ObjectUpdate& rhs) {
                      if (lhs.creation_order != rhs.creation_order) {
                          return lhs.creation_order < rhs.creation_order;
                      }
                      if (lhs.kind != rhs.kind) return lhs.kind < rhs.kind;
                      return lhs.uid < rhs.uid;
                  });
        return objects;
    };
    auto update_object = [&](const ObjectUpdate& object) {
        if (object.kind == ObjectKind::UNIT) {
            update_unit(object.uid);
        } else {
            update_projectile(game_dt, object.uid, enemy_worlds);
        }
    };
    Vec<ObjectUpdate> frame_objects = collect_new_objects();
    for (const ObjectUpdate& object : frame_objects) update_object(object);
    Vec<ObjectUpdate> pending_objects = collect_new_objects();
    for (const ObjectUpdate& object : pending_objects) update_object(object);
    update_collision(game_dt);
    update_shield_generator();
    if (++object_update_sweep_counter_ >= 1000) {
        object_update_sweep_counter_ = 0;
        sweep_wrecks_({this});
    }
    finish_step(dt, speed_mult, enemy_worlds);
}

void World::begin_step(f32 dt, f32 speed_mult, f32 income_mult,
                       const Vec<World*>& enemy_worlds) {
    if (paused_) return;

    rebuild_pathing_from_worlds_(enemy_worlds);
    step_start_prediction_snapshot_.clear();
    if (object_cost_sources_.empty()) {
        for (const auto& [uid, unit] : units_) {
            step_start_prediction_snapshot_[uid] =
                unit.movement_prediction_enabled;
        }
    } else {
        for (const Map<u32, Unit>* source : object_cost_sources_) {
            if (!source) continue;
            for (const auto& [uid, unit] : *source) {
                step_start_prediction_snapshot_[uid] =
                    unit.movement_prediction_enabled;
            }
        }
    }

    tick_++;
    speed_mult_ = speed_mult;
    income_mult_ = income_mult;
    for (auto& [_, unit] : units_) {
        unit.pending_global_commit = false;
        unit.frame_start_height = unit.height;
        // Commands leave native k intact until y.k() snapshots it. C++ can
        // already have activated a replacement, so use the value captured
        // before the first command targeting this object turn.
        unit.frame_start_has_move_target =
            unit.movement_proposal_command_tick == tick_
                ? unit.command_start_has_move_target : unit.has_move_target;
    }
    // game-lib.jar multiplies the frame delta once in the engine loop before
    // dispatching it to units. All time-based unit systems must see this
    // game-time delta, while economy/build helpers retain their explicit
    // speed parameter for pulse accounting.
    const f32 game_dt = dt * speed_mult;
    action_cooldown_tick_remainder_ +=
        game_dt * EngineConstants::ACTION_COOLDOWN_TICKS_PER_SECOND;
    const i32 elapsed_action_ticks =
        static_cast<i32>(action_cooldown_tick_remainder_);
    if (elapsed_action_ticks > 0) {
        action_cooldown_tick_ += elapsed_action_ticks;
        action_cooldown_tick_remainder_ -=
            static_cast<f32>(elapsed_action_ticks);
    }
    const f32 game_frames = game_dt * EngineConstants::FRAME_RATE;
    // game.i.a(float) performs both the multiply and addition as floats,
    // then truncates to int: delta 1 advances by 16, delta 2 by 33.
    const i32 previous_clock_ms = simulation_clock_ms_;
    simulation_clock_ms_ = next_simulation_clock_ms_.value_or(static_cast<i32>(
        static_cast<f32>(simulation_clock_ms_) + game_frames * 16.666666f));
    next_simulation_clock_ms_.reset();
    const f32 elapsed_clock_ms = static_cast<f32>(
        simulation_clock_ms_ - previous_clock_ms);
    step_in_progress_ = true;
    for (auto& [_, unit] : units_) {
        if (unit.formation_tick_pending) {
            unit.formation_tick = simulation_clock_ms_;
            unit.formation_tick_pending = false;
        }
    }
    for (auto& [_, unit] : units_) {
        unit.service_active_this_frame = false;
        unit.service_completed_in_build_pass = false;
        unit.weapon_target_acquired_this_frame = false;
        for (WeaponSlot& slot : unit.weapon_slots) {
            slot.warmup_hold_this_frame = false;
        }
    }
    for (auto& [_, unit] : units_) {
        if (unit.last_damage_age_ms >= 0.0f) {
            // am.q(seconds) compares l.by against the bs damage timestamp.
            // Its deadline advances with the actual integer clock, including
            // truncation on half/double frames and mixed replay step rates.
            unit.last_damage_age_ms += elapsed_clock_ms;
        }
    }
    update_visibility_(game_dt * EngineConstants::FRAME_RATE, enemy_worlds);
    combat_delta_frames_ = game_frames;
}

void World::finish_step(f32 dt, f32 speed_mult,
                        const Vec<World*>& enemy_worlds) {
    if (paused_) return;
    const f32 game_dt = dt * speed_mult;
    const f32 game_frames = game_dt * EngineConstants::FRAME_RATE;
    Vec<World*> hostile_worlds;
    hostile_worlds.reserve(enemy_worlds.size());
    for (World* other : enemy_worlds) {
        if (other && other != this && is_enemy_with(*other)) {
            hostile_worlds.push_back(other);
        }
    }

    // A formation leader can be removed after its former follower has already
    // taken its object turn. JAR's aD()/aU handoff submits the new leader's
    // ATTACK path at the end of that frame, so do the delayed request after
    // collision and before the visible state is sampled.
    for (auto& [_, unit] : units_) {
        if (!unit.formation_rebuild_path_pending ||
            !unit.formation_is_leader || unit.path_request_pending ||
            unit.using_path || !unit.has_move_target ||
            unit.order_queue.empty() ||
            unit.order_queue.front().type != WeaponType::ATTACK) {
            continue;
        }
        const UnitOrder& order = unit.order_queue.front();
        World* target_world = order_target_world_(
            order.target_team_id, enemy_worlds);
        Unit* target = target_world
            ? target_world->get_unit(order.target_uid) : nullptr;
        if (!target || !target->is_alive || target->parent_uid != 0 ||
            target_world == this || !is_enemy_with(*target_world)) {
            unit.formation_rebuild_path_pending = false;
            continue;
        }
        if (unit.repath_cooldown_frames == 0.1f &&
            unit.formation_projection_disabled_frames < 120.0f) {
            // y.a(delta,target,ad,false) has already selected j/k/l/m for a
            // short direct approach. The later aD()/ab.c() promotion keeps
            // that proposal and s=0.1; it cannot submit another path during
            // this frame. The next object turn applies the ordinary refresh.
            unit.formation_rebuild_path_pending = false;
            continue;
        }
        // JAR resumes the global object pass in creation order after aD(). At
        // this leader's aU(), earlier objects expose this frame's cK while
        // later objects still expose the value captured at frame start.
        if (unit.formation_group_size > 1) {
            handoff_command_prediction_snapshot_ =
                step_start_prediction_snapshot_;
            const auto capture_already_visited =
                [&](const Map<u32, Unit>& source) {
                    for (const auto& [uid, candidate] : source) {
                        if (candidate.object_creation_order <
                            unit.object_creation_order) {
                            bool prediction =
                                candidate.movement_prediction_enabled;
                            if (!candidate.order_queue.empty()) {
                                const UnitOrder& active =
                                    candidate.order_queue.front();
                                if (active.type == WeaponType::GUARD &&
                                    active.target_uid != 0) {
                                    bool target_alive = false;
                                    if (object_cost_sources_.empty()) {
                                        const auto found =
                                            units_.find(active.target_uid);
                                        target_alive =
                                            found != units_.end() &&
                                            found->second.is_alive &&
                                            found->second.hp > 0.0f &&
                                            found->second.parent_uid == 0;
                                    } else {
                                        for (const Map<u32, Unit>* targets :
                                             object_cost_sources_) {
                                            if (!targets) continue;
                                            const auto found = targets->find(
                                                active.target_uid);
                                            if (found != targets->end() &&
                                                found->second.is_alive &&
                                                found->second.hp > 0.0f &&
                                                found->second.parent_uid == 0) {
                                                target_alive = true;
                                                break;
                                            }
                                        }
                                    }
                                    if (!target_alive) prediction = false;
                                }
                            }
                            handoff_command_prediction_snapshot_[uid] =
                                prediction;
                        }
                    }
                };
            if (object_cost_sources_.empty()) {
                capture_already_visited(units_);
            } else {
                for (const Map<u32, Unit>* source : object_cost_sources_) {
                    if (source) capture_already_visited(*source);
                }
            }
            handoff_command_prediction_snapshot_[unit.uid] = false;
            handoff_command_snapshot_tick_ = tick_;
        }
        unit.force_grid_path_request = true;
        handoff_path_request_active_ = true;
        move_unit_to_async_(
            unit, target->x, target->y, attack_path_target_size(unit, target));
        handoff_path_request_active_ = false;
        // This handoff is submitted after the former follower's object turn.
        // Its path-engine countdown has already spent that frame in JAR, so
        // compensate the generic deferred-request budget by one frame.
        unit.path_request_delay_frames = std::max(
            0.0f, unit.path_request_delay_frames - 1.0f);
        unit.path_request_prediction_suppressed = true;
        unit.movement_prediction_enabled = false;
        unit.formation_rebuild_path_pending = false;
    }

    ai_timer_ += game_dt * 1000.0f;
    if (!disable_ai_ && ai_timer_ >= EngineConstants::AI_UPDATE_MS) {
        for (World* enemy_world : hostile_worlds) {
            if (enemy_world) update_behaviors(*enemy_world);
        }
        update_builder_behavior();
        ai_timer_ = 0.0f;
    }

    // game.n.f() runs after the global object and collision passes. This is
    // deliberately before ordinary dead-object cleanup so cu=-1 remains
    // observable for the same frame as the native JAR.
    update_surrender_();

    for (auto it = units_.begin(); it != units_.end(); ) {
        if (it->second.is_dead && !it->second.cleanup_done) {
            remove_unit(it->first);
        }
        if (it->second.is_dead) {
            it->second.death_age_frames += game_dt * EngineConstants::FRAME_RATE;
        }
        // Non-wreck JAR objects have already left the global object list in
        // bv(). Keep one observation frame for safe Python references, then
        // erase them. Wreck-producing units stay addressable like JAR units
        // until something queues them through am.a(), which utility.o also
        // applies one frame late.
        if ((it->second.is_dead && !it->second.death_persistent &&
             tick_ > it->second.death_tick) ||
            (it->second.list_removal_tick >= 0 &&
             tick_ > it->second.list_removal_tick)) {
            retain_collision_unit_(it->second);
            it = units_.erase(it);
        } else {
            ++it;
        }
    }

    for (auto& [_, unit] : units_) {
        unit.position_history.push_back({tick_, unit.x, unit.y,
                                          unit.rotation});
        if (unit.position_history.size() > 12) {
            unit.position_history.erase(unit.position_history.begin());
        }
    }

    step_in_progress_ = false;
}

WorldState World::get_state() const {
    WorldState s{};
    s.team_id = team_id_;
    s.tick = tick_;
    s.credits = static_cast<f32>(credits_);
    s.total_income_rate = total_income_rate_;
    s.income_per_sec = total_income_rate_ * (EngineConstants::FRAME_RATE / EngineConstants::ECONOMY_WINDOW) *
                       speed_mult_ * ai_income_multiplier() * income_mult_;
    s.total_income = total_income_;
    s.total_spent = total_spent_;
    s.active_units = 0; s.total_units = 0; s.buildings = 0;
    for (auto& [_, u] : units_) {
        if (u.pending_global_commit) continue;
        s.total_units++;
        if (!u.is_dead) s.active_units++;
        if (!u.is_dead && u.is_building) s.buildings++;
    }
    s.pending_builds = static_cast<i32>(pending_builds_.size());
    s.projectiles = static_cast<i32>(projectiles_.size());
    s.kills = kills_;
    s.losses = losses_;
    s.units_built = units_built_;
    s.buildings_completed = buildings_completed_;
    s.unit_cap = unit_cap();
    i32 fq_size = 0;
    for (auto& [_, fq] : factory_queues_) fq_size += static_cast<i32>(fq.queue.size());
    s.factory_queue_size = fq_size;
    return s;
}


} // namespace rwrl
