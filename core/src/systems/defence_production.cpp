#include "world_internal.h"

namespace rwrl {

void World::update_unit_auto_actions_(
        f32 delta_frames, u32 unit_uid, const Vec<World*>& other_worlds) {
    Unit* unit = get_unit(unit_uid);
    if (!unit || !unit->custom_unit) return;
    bool force_check = unit->auto_trigger_first_update;
    unit->auto_trigger_first_update = false;
    if (!unit->is_active()) return;
    const UnitDef* definition = registry_.get(unit->unit_type);
    if (!definition || definition->auto_triggers.empty()) return;
    // custom.j.b(float,boolean) forces every rate when w expires. A
    // conversion to the same metadata still executes, resetting j and stats.
    if (unit->auto_trigger_cooldown_frames != 0.0f) {
        unit->auto_trigger_cooldown_frames = approach_value(
            unit->auto_trigger_cooldown_frames, 0.0f, delta_frames);
        if (unit->auto_trigger_cooldown_frames != 0.0f) return;
        force_check = true;
    }
    for (i32 interval : {1, 4, 8}) {
        if (interval != 1 && !force_check &&
            (static_cast<u64>(tick_) + unit->uid) % interval != 0) continue;
        for (const CustomAutoTriggerDef& trigger : definition->auto_triggers) {
            if (trigger.check_interval_frames != interval) continue;
            const i32 tx = pathfinder_.unclamped_tile_x(unit->x);
            const i32 ty = pathfinder_.unclamped_tile_y(unit->y);
            const bool outside = tx < 0 || ty < 0 ||
                tx >= pathfinder_.width() || ty >= pathfinder_.height();
            // am.cJ()/k.l.a(int,int): WATER accepts this terrain while
            // OVER_CLIFF rejects it. Buildings and object costs are ignored.
            const bool over_water = outside ||
                (!pathfinder_.is_passable_ignoring_buildings(tx, ty, MoveType::OVER_CLIFF) &&
                 pathfinder_.is_passable_ignoring_buildings(tx, ty, MoveType::WATER));
            if (over_water != trigger.when_over_water) continue;
            CustomActionContext context;
            context.has_position = true;
            context.x = unit->x;
            context.y = unit->y;
            execute_custom_action_(*unit, trigger.action_id, other_worlds,
                                   nullptr, &context);
            const UnitDef* current_definition = registry_.get(unit->unit_type);
            unit->auto_trigger_cooldown_frames = current_definition
                ? current_definition->auto_trigger_cooldown_frames : 0.0f;
            if (!unit->is_alive || current_definition != definition) return;
        }
    }
}

void World::update_unit_action_building_state_(f32 delta_frames, u32 unit_uid) {
    Unit* unit = get_unit(unit_uid);
    if (!unit || !unit->custom_unit || !unit->is_active()) return;
    const CustomActionDef* action = nullptr;
    const auto queue = factory_queues_.find(unit_uid);
    const Str prefix = "__custom_action__:";
    if (queue != factory_queues_.end() && !queue->second.queue.empty()) {
        const Str& item = queue->second.queue.front();
        if (item.compare(0, prefix.size(), prefix) == 0) {
            action = registry_.get_custom_action(
                unit->unit_type, item.substr(prefix.size()));
        }
    }
    if (action && action->has_when_building_rotate_to) {
        const f32 desired = action->when_building_rotate_to;
        const bool reversing = action->when_building_rotate_to_or_backwards &&
            std::abs(shortest_angle_delta(unit->rotation, desired)) > 100.0f;
        set_reversing_with_turret_delay(*unit, reversing);
        const f32 body_delta = turn_toward(
            *unit, desired + (reversing ? 180.0f : 0.0f), delta_frames);
        if (body_delta != 0.0f) {
            for (WeaponSlot& slot : unit->weapon_slots) {
                if (slot.turret_rotate_with_body) {
                    slot.turret_angle = rotate_weapon_angle_with_body(slot.turret_angle, body_delta);
                }
            }
        }
    }
    unit->custom_action_movement_blocked = action && action->when_building_cannot_move;
    if (unit->custom_action_movement_blocked) {
        unit->vel_x = 0.0f;
        unit->vel_y = 0.0f;
        unit->move_speed_factor = 0.0f;
    }
}

void World::update_factory_production(f32 dt, f32 speed_mult) {
    update_unit_factory_production(dt, speed_mult, 0);
}

void World::update_unit_factory_production(
        f32 dt, f32 speed_mult, u32 only_factory_uid,
        const Vec<World*>& other_worlds) {
    Vec<u32> empty_factories;
    // Completed custom effects may delete a source or create another queue.
    // Iterate IDs so those callbacks cannot invalidate the map traversal.
    Vec<u32> factory_uids;
    for (const auto& [fid, _] : factory_queues_) factory_uids.push_back(fid);
    for (u32 fid : factory_uids) {
        if (only_factory_uid != 0 && fid != only_factory_uid) continue;
        auto queue_it = factory_queues_.find(fid);
        if (queue_it == factory_queues_.end()) continue;
        FactoryQueue& fq = queue_it->second;
        Unit* factory = get_unit(fid);
        if (!factory || !factory->is_alive) {
            for (f32 cost : fq.upfront_costs) {
                refund_credits(cost, EngineConstants::CANCEL_REFUND);
            }
            if (factory && fq.resource_progress > 0.0f && !fq.queue.empty()) {
                refund_credits(
                    static_cast<f32>(factory_queue_item_cost_(
                        *factory, fq.queue.front())) * fq.resource_progress,
                    EngineConstants::CANCEL_REFUND);
            }
            empty_factories.push_back(fid);
            continue;
        }

        if (!factory->is_active()) continue;

        if (fq.queue.empty()) continue;

        if (fq.head_changed) {
            fq.progress = fq.item_states.front().progress;
            fq.resource_progress = fq.item_states.front().resource_progress;
            fq.head_changed = false;
        }

        auto erase_front = [&]() {
            fq.queue.erase(fq.queue.begin());
            fq.item_states.erase(fq.item_states.begin());
            fq.head_changed = true;
            if (!fq.upfront_costs.empty()) {
                fq.upfront_costs.erase(fq.upfront_costs.begin());
            }
            if (!fq.action_contexts.empty()) {
                fq.action_contexts.erase(fq.action_contexts.begin());
            }
        };
        auto refund_front = [&]() {
            factory->ammo += fq.item_states.front().ammo_cost;
            if (!fq.upfront_costs.empty()) {
                refund_credits(
                    fq.upfront_costs.front(), EngineConstants::CANCEL_REFUND);
            }
            if (fq.resource_progress > 0.0f) {
                refund_credits(
                    static_cast<f32>(factory_queue_item_cost_(
                        *factory, fq.queue.front())) * fq.resource_progress,
                    EngineConstants::CANCEL_REFUND);
            }
        };

        const Str unit_type = fq.queue.front();
        const Str action_prefix = "__custom_action__:";
        const bool building_action = unit_type.compare(
            0, action_prefix.size(), action_prefix) == 0;
        const Str action_id = building_action
            ? unit_type.substr(action_prefix.size()) : "";
        const CustomActionDef* action = building_action
            ? registry_.get_custom_action(factory->unit_type, action_id)
            : nullptr;
        if (building_action && !action) {
            refund_front();
            erase_front();
            fq.progress = 0.0f;
            fq.resource_progress = 0.0f;
            continue;
        }
        const bool building_nuke = unit_type == "__build_nuke__";
        const bool building_antinuke = unit_type == "__build_antinuke__";
        const Str upgrade_prefix = "__upgrade__:";
        const bool building_upgrade = unit_type.compare(
            0, upgrade_prefix.size(), upgrade_prefix) == 0;
        const bool building_tier_upgrade = unit_type == "__upgrade_tier__";
        const Str upgrade_target = building_upgrade
            ? unit_type.substr(upgrade_prefix.size())
            : (building_tier_upgrade ? unit_type : "");
        i32 upgrade_cost = 0;
        f32 upgrade_speed = 0.0f;
        if ((building_upgrade || building_tier_upgrade) && !resolve_upgrade_action_(
                *factory, upgrade_target, upgrade_cost, upgrade_speed)) {
            refund_front();
            erase_front();
            fq.progress = 0.0f;
            fq.resource_progress = 0.0f;
            continue;
        }

        if (fq.progress == 0.0f && fq.resource_progress == 0.0f &&
            !building_nuke && !building_antinuke &&
            !building_action &&
            !building_upgrade && !building_tier_upgrade &&
            !can_factory_build_(*factory, unit_type)) {
            refund_front();
            erase_front();
            continue;
        }

        fq.build_speed = building_action
            ? (action->build_speed >= 1.0f ? 1000.0f : action->build_speed) :
                         (building_nuke ? 0.0003f :
                         (building_antinuke ? 0.0007f :
                          ((building_upgrade || building_tier_upgrade)
                               ? upgrade_speed : registry_.get_build_speed(unit_type))));
        const f32 factory_speed = building_action &&
            action->ignore_factory_speed_modifiers
                ? 1.0f : factory->nano_factory_speed;
        const f32 candidate = std::min(
            1.0f,
            fq.progress + fq.build_speed * factory_speed *
                EngineConstants::FRAME_RATE * dt * speed_mult);
        fq.progress = candidate;
        fq.remember_head();

        if (fq.progress >= 1.0f) {
            if (building_action) {
                const CustomActionContext context = fq.action_contexts.empty()
                    ? CustomActionContext{} : fq.action_contexts.front();
                // d.k removes the paid item before custom.j executes effects;
                // refundAllQueuedItems must only see the remaining queue.
                erase_front();
                fq.progress = 0.0f;
                fq.resource_progress = 0.0f;
                execute_custom_action_(
                    *factory, action_id, other_worlds, nullptr, &context);
                continue;
            }
            if (building_upgrade || building_tier_upgrade) {
                upgrade_unit_now_(*factory, upgrade_target, false);
                erase_front();
                fq.progress = 0.0f;
                fq.resource_progress = 0.0f;
                continue;
            }
            if (building_nuke || building_antinuke) {
                if (building_nuke && factory->is_nuke && factory->ammo < factory->ammo_max) {
                    factory->ammo++;
                } else if (building_antinuke && factory->is_antinuke && factory->ammo < factory->ammo_max) {
                    factory->ammo++;
                } else {
                    fq.progress = 0.0f;
                    erase_front();
                    continue;
                }
                erase_front();
                fq.progress = 0.0f;
                fq.resource_progress = 0.0f;
                continue;
            }

            if (factory->custom_unit && factory->cargo_capacity > 0) {
                // d.k only applies dA() to unit-producing queue entries.
                // custom.j.dA() holds e at 1 while dI(), the occupied slot
                // weight, is strictly greater than maxTransportingUnits.
                i32 cargo_weight = 0;
                for (u32 cargo_uid : factory->cargo_units) {
                    Unit* cargo = waypoint_target_(cargo_uid, true);
                    if (!cargo) {
                        for (World* peer : peer_worlds_) {
                            if (!peer || peer == this) continue;
                            cargo = peer->waypoint_target_(cargo_uid, true);
                            if (cargo) break;
                        }
                    }
                    if (cargo) cargo_weight += cargo->cargo_weight;
                }
                if (cargo_weight > factory->cargo_capacity) continue;
            }

            // d.k creates products five units below the factory, then custom.j
            // applies the optional exit offsets and queues the move-away
            // point. Keep the generic stock fallback for definitions without
            // custom [core] exit_* metadata.
            Unit* produced = spawn_unit(
                unit_type, factory->x, factory->y + 5.0f, true);
            if (produced) {
                produced->pending_global_commit = true;
                // com.corrodinggames.rts.game.units.d.k.a(am, ...): all
                // factory products start with cl=30. Experimental land
                // factories add 40; slow products receive two additional
                // launch delays based on their prototype z() speed.
                produced->spawn_delay_frames = 30.0f;
                if (factory->unit_type == "experimentalLandFactory") {
                    produced->spawn_delay_frames += 40.0f;
                }
                const f32 product_speed = produced->move_speed_per_frame();
                if (product_speed < 0.75f) {
                    produced->spawn_delay_frames += 30.0f;
                }
                if (product_speed < 0.55f) {
                    produced->spawn_delay_frames += 20.0f;
                }
                produced->factory_exit_path_pending = true;
                const bool custom_exit = factory->has_factory_exit_config;
                const f32 side_nudge = factory->custom_factory_exit_side
                    ? 0.0f : 1.0f;
                // custom.j only applies exit_dirOffset to a building factory
                // when the key is present. Building objects start at -90deg,
                // so the JAR's 90deg default is bodyRotation + 180deg.
                produced->rotation = (
                    custom_exit && factory->is_building &&
                    factory->factory_exit_has_dir_offset)
                    ? factory->rotation + factory->factory_exit_dir_offset + 180.0f
                    : 90.0f;
                for (WeaponSlot& slot : produced->weapon_slots) {
                    // d.k calls y.j(90): set ap.a to 90+B(slot), retaining
                    // ap.b as the idle sweep's constructor origin. Stock
                    // factory products are constructed with ap.b=0, whereas
                    // spawn_unit's public map heading initially uses 90.
                    slot.turret_angle = 90.0f + slot.turret_idle_direction;
                    if (!produced->custom_unit) {
                        slot.turret_previous_angle = 0.0f;
                    }
                }
                const f32 heading_x = jar_cosine_degrees(produced->rotation);
                const f32 heading_y = jar_sine_degrees(produced->rotation);
                const f32 exit_distance = custom_exit
                    ? factory->factory_exit_move_away_amount
                    : factory->radius * (fq.has_rally ? 2.0f : 3.0f);
                f32 exit_x = factory->x + heading_x * exit_distance;
                f32 exit_y = factory->y + heading_y * exit_distance;
                if (!fq.has_rally) {
                    const f32 lateral = custom_exit ? side_nudge : 1.0f;
                    exit_x -= heading_y * lateral;
                    exit_y += heading_x * lateral;
                }
                if (custom_exit) {
                    // j.F(am) relocates the completed product to exit_x/y;
                    // the move-away waypoint remains based on the factory
                    // origin, not this relocated position.
                    produced->x = factory->x + factory->factory_exit_x;
                    produced->y = factory->y + factory->factory_exit_y;
                    if (factory->factory_exit_height_ignore_parent) {
                        produced->height = factory->factory_exit_height_offset;
                    } else {
                        produced->height = factory->height +
                            factory->factory_exit_height_offset;
                    }
                    grid_.update(*produced);
                } else if (!factory->custom_unit) {
                    // d.i.a(j) clears the product's radius past ep + dv()
                    // before its first object turn and y.g() collision pass.
                    // dv() is -100 for stock factories, except d.t's -20.
                    const f32 exit_edge_y = factory->y +
                        (factory->unit_type == "seaFactory" ? -20.0f : -100.0f);
                    if (produced->y - produced->radius < exit_edge_y) {
                        produced->y = exit_edge_y + produced->radius;
                        grid_.update(*produced);
                    }
                }
                produced->using_path = false;
                produced->target_x = exit_x;
                produced->target_y = exit_y;
                produced->has_move_target = true;
                produced->arrived = false;
                produced->waypoints.clear();
                produced->current_waypoint = 0;
                // k.a(...) calls y.d(exitX, exitY), which appends the
                // factory exit as an ordinary MOVE waypoint. Keep the
                // active navigation fields above for the path solver, while
                // also preserving the typed queue observable to commands.
                UnitOrder exit_order;
                exit_order.type = WeaponType::MOVE;
                exit_order.x = exit_x;
                exit_order.y = exit_y;
                exit_order.factory_exit = true;
                produced->order_queue.push_back(exit_order);
                if (fq.has_rally) {
                    const f32 rally_nudge = custom_exit ? side_nudge : 1.0f;
                    produced->waypoints.push_back({
                        fq.rally_x + rally_nudge, fq.rally_y});
                    UnitOrder rally_order;
                    rally_order.type = WeaponType::MOVE;
                    rally_order.x = fq.rally_x + rally_nudge;
                    rally_order.y = fq.rally_y;
                    produced->order_queue.push_back(rally_order);
                }
                if (custom_exit) {
                    if (!factory->is_building) {
                        apply_mobile_factory_exit_(*factory, *produced);
                    }
                    factory->custom_factory_exit_side =
                        !factory->custom_factory_exit_side;
                }
            }
            erase_front();
            fq.progress = 0.0f;
            fq.resource_progress = 0.0f;
        }
    }
    for (u32 fid : empty_factories) factory_queues_.erase(fid);
}

void World::update_antinuke_interception(f32 dt, const Vec<World*>& enemy_worlds) {
    update_antinuke_interception_(dt, enemy_worlds, 0);
}

void World::update_unit_antinuke_interception(
    f32 dt, u32 unit_uid, const Vec<World*>& enemy_worlds) {
    update_antinuke_interception_(dt, enemy_worlds, unit_uid);
}

void World::update_antinuke_interception_(
    f32 dt, const Vec<World*>& enemy_worlds, u32 only_unit_uid) {
    const f32 game_frames = dt * EngineConstants::FRAME_RATE;
    for (auto unit_update_it = only_unit_uid == 0
             ? units_.begin() : units_.find(only_unit_uid);
         unit_update_it != units_.end();
         unit_update_it = only_unit_uid == 0
             ? std::next(unit_update_it) : units_.end()) {
        auto& [uid, unit] = *unit_update_it;
        if (!unit.is_active()) continue;
        if (unit.custom_unit) continue;
        if (!unit.is_antinuke) continue;
        if (unit.ammo <= 0) continue;
        unit.antinuke_scan_timer = std::max(0.0f, unit.antinuke_scan_timer - game_frames);
        if (unit.antinuke_scan_timer > 0.0f) continue;
        unit.antinuke_scan_timer = 15.0f;

        World* selected_world = nullptr;
        u32 selected_pid = 0;
        u64 selected_order = 0;
        for (World* enemy_world : enemy_worlds) {
            if (!enemy_world) continue;
            for (auto& [pid, p] : enemy_world->projectiles_) {
                if (!p.is_nuke) continue;
                if (p.height <= 50.0f) continue;
                // d.c rejects same-team and allied projectile owners. Neutral
                // is neither allied nor hostile, so its nukes are still
                // eligible for interception.
                if (is_allied_with(*enemy_world)) continue;
                if (dist(unit.x, unit.y, p.x, p.y) >= unit.nuke_range) continue;
                // d.c only protects nuclear impacts within 1000 units of the
                // anti-nuke launcher; a distant flight path is ignored.
                if (dist(unit.x, unit.y, p.target_x, p.target_y) >= 1000.0f) continue;

                bool already_intercepted = false;
                auto check_interceptors = [&](const World& world) {
                    for (const auto& [_, interceptor] : world.projectiles_) {
                        if (interceptor.is_antinuke &&
                            interceptor.target_team_id == enemy_world->team_id_ &&
                            interceptor.target_projectile_uid == pid) {
                            return true;
                        }
                    }
                    return false;
                };
                already_intercepted = check_interceptors(*this);
                if (!already_intercepted) {
                    for (World* other_world : enemy_worlds) {
                        if (other_world && check_interceptors(*other_world)) {
                            already_intercepted = true;
                            break;
                        }
                    }
                }
                if (already_intercepted) continue;

                if (p.creation_order >= selected_order) {
                    selected_order = p.creation_order;
                    selected_world = enemy_world;
                    selected_pid = pid;
                }
            }
        }

        if (!selected_world || unit.ammo <= 0) continue;
        const Projectile& nuke = selected_world->projectiles_.at(selected_pid);
        Projectile interceptor{};
        interceptor.uid = global_next_uid_ ? (*global_next_uid_)++ : next_uid_++;
            interceptor.creation_order = allocate_object_order_();
        interceptor.owner_uid = unit.uid;
        interceptor.team_id = team_id_;
        interceptor.x = unit.x;
        interceptor.y = unit.y - 9.0f;
        interceptor.target_x = nuke.x;
        interceptor.target_y = nuke.y;
        interceptor.target_team_id = selected_world->team_id_;
        interceptor.target_projectile_uid = selected_pid;
        interceptor.speed = 0.2f * EngineConstants::FRAME_RATE;
        interceptor.speed_per_frame = 0.2f;
        interceptor.target_speed = 6.5f * EngineConstants::FRAME_RATE;
        interceptor.speed_acceleration = 0.1f * EngineConstants::FRAME_RATE;
        interceptor.lifetime = 99999.0f;
        interceptor.tracking = true;
        interceptor.is_antinuke = true;
        interceptor.laser_defence_excluded = true;
        interceptor.height_guided = true;
        interceptor.launch_height = 80.0f;
        interceptor.cruise_height = 100.0f;
        interceptor.vertical_speed = 2.0f;
        projectiles_[interceptor.uid] = std::move(interceptor);
        unit.ammo--;
    }
}

void World::update_shield_generator() {
    for (auto& [uid, unit] : units_) {
        if (!unit.is_active()) continue;
        if (unit.unit_type != "shieldGenerator") continue;

        auto nearby = grid_.query_circle(unit.x, unit.y, unit.weapon_range);
        for (u32 nid : nearby) {
            Unit* other = get_unit(nid);
            if (!other || !other->is_alive || other->team_id != team_id_) continue;
            if (other->max_shield > 0 && other->shield < other->max_shield) {
                other->shield = std::min(other->max_shield, other->shield + other->max_shield * EngineConstants::SHIELD_REGEN_RATE * 2.0f);
            }
        }
    }
}

} // namespace rwrl
