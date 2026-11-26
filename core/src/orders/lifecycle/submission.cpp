#include "world_internal.h"

#include <cstdio>
#include <cstdlib>

namespace rwrl {

bool World::submit_order_(Unit& unit, const GameCommand& cmd,
                          const Vec<World*>& other_worlds) {
    if (cmd.action > GameCommand::SET_PASSIVE_TARGET) return false;

    UnitOrder order;
    order.type = static_cast<WeaponType>(cmd.action);
    order.x = cmd.target_x;
    order.y = cmd.target_y;
    order.target_uid = cmd.target_uid;
    order.unit_type = cmd.unit_type;
    order.build_tech_level = cmd.build_tech_level;
    order.action_id = cmd.action_id;
    order.formation_group_id = active_command_formation_group_id_;
    order.relaxed_formation = cmd.relaxed_formation;
    order.formation_controller_relaxed_formation = cmd.relaxed_formation;
    order.prefer_clearance = cmd.group_size > 1;
    order.replay_command_group_size = cmd.group_size;
    order.replay_command_group_index = cmd.group_index;
    order.replay_command_has_path_cache =
        cmd.has_replay_prepath || !cmd.replay_prepaths.empty();
    const bool command_had_active_order = !unit.order_queue.empty();
    const bool formation_rebuild_reverse_coast_candidate =
        !cmd.queued && order.type == WeaponType::MOVE &&
        cmd.group_size > 1 &&
        cmd.replay_prepaths_provided && !cmd.replay_prepaths.empty() &&
        (unit.reversing || unit.move_speed_factor < 0.0f);
    const bool guard_replaces_patrol =
        !cmd.queued && order.type == WeaponType::GUARD && unit.has_patrol;

    if ((order.type == WeaponType::TRIGGER_ACTION ||
         order.type == WeaponType::TRIGGER_WHEN_IN_RANGE) &&
        registry_.get_custom_action(unit.unit_type, order.action_id) &&
        custom_action_cooldown_remaining_(unit, order.action_id) > 0) {
        return false;
    }

    World* target_world = nullptr;
    Unit* target = nullptr;
    if (cmd.target_uid != 0) {
        target_world = find_order_target_world_(cmd.target_uid, other_worlds);
        target = target_world ? target_world->get_unit(cmd.target_uid) : nullptr;
        if (target_world) order.target_team_id = target_world->team_id_;
    }

    // JAR e.k() mutates the old queue before validating/appending the new
    // waypoint. aA() also increments its index after a removal, so an entry
    // shifted into the just-removed slot is deliberately skipped.
    if (cmd.clear_non_build_repair) {
        for (size_t index = 0; index < unit.order_queue.size(); ++index) {
            const WeaponType type = unit.order_queue[index].type;
            if (type == WeaponType::BUILD || type == WeaponType::REPAIR) continue;
            if (index == 0) {
                // aA()/n(0)/ay() retain R while pruning the active waypoint.
                // Restore it in the storage used by the shifted queue entry
                // before that entry's activation reads the combat target.
                const u32 saved_main_target_uid = unit.main_target_uid();
                const i32 saved_main_target_team_id = unit.main_target_team_id();
                clear_current_order_state_(unit, !unit.is_active());
                unit.order_queue.erase(unit.order_queue.begin());
                unit.restore_main_target(
                    saved_main_target_uid, saved_main_target_team_id);
                if (!unit.order_queue.empty()) {
                    // y.ay() activates the shifted entry but leaves it in
                    // place even when its target has since become invalid.
                    if (!unit.is_active()) {
                        unit.activate_order_next_frame = true;
                    } else {
                        activate_order_(unit, unit.order_queue.front(), other_worlds);
                    }
                }
            } else {
                unit.order_queue.erase(unit.order_queue.begin() + index);
            }
        }
    } else if (!cmd.queued) {
        // y.az() replaces the waypoint queue without clearing y.R. Only the
        // command's explicit clear-main-target flag does that (handled by the
        // clear_orders branch above).
        const bool preserve_main_target = unit.main_target_uid() != 0;
        const bool attack_had_active_air_path =
            order.type == WeaponType::ATTACK &&
            unit.move_type == MoveType::AIR && unit.using_path &&
            unit.has_move_target && !unit.arrived;
        const u32 saved_main_target_uid = preserve_main_target
            ? unit.main_target_uid() : 0;
        const i32 saved_main_target_team_id = preserve_main_target
            ? unit.main_target_team_id() : -1;
        clear_current_order_state_(unit, !unit.is_active());
        unit.attack_order_had_active_air_path = attack_had_active_air_path;
        unit.attack_order_had_main_target =
            order.type == WeaponType::ATTACK &&
            unit.move_type == MoveType::AIR &&
            saved_main_target_uid != 0;
        unit.order_queue.clear();
        if (saved_main_target_uid != 0) {
            unit.restore_main_target(saved_main_target_uid,
                                     saved_main_target_team_id);
        }
    } else if (cmd.merge_duplicate && !unit.order_queue.empty()) {
        const UnitOrder& tail = unit.order_queue.back();
        const bool tail_is_point = tail.type == WeaponType::MOVE ||
                                   tail.type == WeaponType::ATTACK_MOVE;
        const bool new_is_point = order.type == WeaponType::MOVE ||
                                  order.type == WeaponType::ATTACK_MOVE;
        if (tail_is_point && new_is_point &&
            std::abs(order.x - tail.x) <= 3.0f &&
            std::abs(order.y - tail.y) <= 3.0f) {
            if (unit.order_queue.size() == 1) {
                clear_current_order_state_(unit, !unit.is_active());
                unit.order_queue.clear();
            } else {
                unit.order_queue.pop_back();
            }
        }
    }

    if (cmd.target_uid != 0) {
        if (order.type == WeaponType::SET_PASSIVE_TARGET) {
            if (!target) {
                order.target_uid = 0;
                order.target_team_id = -1;
            }
        } else if (order.type == WeaponType::ATTACK) {
            // e.k()/y.a(au,boolean) accept ATTACK before y.g() resolves
            // a missing, dead or same-team target on the object's turn.
            // au.c() replaces an unresolved saved object ID with null.
            if (!target) {
                order.target_uid = 0;
                order.target_team_id = -1;
            }
        } else if (order.type == WeaponType::LOAD_INTO) {
            // au.c() resolves a vanished object to null. e.k() still queues
            // the LOAD waypoint and configures its formation before h()
            // consumes an absent, dead or unavailable carrier.
            if (!target) {
                order.target_uid = 0;
                order.target_team_id = -1;
            }
        } else if (order.type == WeaponType::GUARD ||
                   order.type == WeaponType::FOLLOW) {
            // y.a(au) accepts the target reference before c() checks its
            // lifetime on the object's turn. Formation must still commit
            // when a dead or vanished target makes that waypoint finish.
            if (!target) {
                order.target_uid = 0;
                order.target_team_id = -1;
            }
        } else if (!target || !target->is_alive ||
                   (target->parent_uid != 0 &&
                    order.type != WeaponType::GUARD &&
                    order.type != WeaponType::FOLLOW)) {
            return false;
        }

        switch (order.type) {
        case WeaponType::TOUCH_TARGET:
            if (target_world == this || !is_enemy_with(*target_world)) return false;
            break;
        case WeaponType::GUARD:
        case WeaponType::FOLLOW:
            break;
        case WeaponType::REPAIR:
            if (target->uid == unit.uid || !can_repair_target(unit, *target)) return false;
            break;
        case WeaponType::RECLAIM:
            if (target->uid == unit.uid || !can_reclaim_target(unit, *target) ||
                (target->unit_type != "crystalResource" && target_world != this &&
                 !is_allied_with(*target_world))) return false;
            break;
        case WeaponType::LOAD_INTO:
            // e.k() accepts this waypoint before h() checks d(cargo,false).
            // A foreign carrier still participates in command formation
            // setup; its ownership rejects loading on the object's turn.
            break;
        case WeaponType::LOAD_UP:
            if (target_world != this || unit.cargo_capacity <= 0 ||
                !target->transportable || target->team_id != unit.team_id) return false;
            break;
        default:
            break;
        }
    } else if (order.type == WeaponType::REPAIR ||
               order.type == WeaponType::RECLAIM ||
               order.type == WeaponType::LOAD_UP ||
               order.type == WeaponType::TOUCH_TARGET) {
        return false;
    }

    // Construction availability belongs to the type's menu, independently
    // of its current combat or repair handler.
    if (order.type == WeaponType::BUILD &&
        !registry_.can_build_unit(unit.unit_type, order.unit_type,
                                  order.build_tech_level)) return false;

    if (unit.order_queue.size() >= 29) return false;

    if (formation_rebuild_reverse_coast_candidate) {
        // The command clears the old path before formation setup visits the
        // selected followers. Keep the state as a candidate until the
        // formation pass confirms that this is a leader-removal rebuild.
        unit.formation_rebuild_reverse_coast_candidate = true;
    }

    // gameFramework.e.b(): MOVE/ATTACK_MOVE/ATTACK commands pre-solve a
    // grid path with prefer-clearance weighting from the unit's position
    // at command time; y.a() adopts it via the per-order cache for up to
    // 180 frames. Attack orders on buildings carry the footprint target
    // size like y.q(target).
    const bool prepath_eligible =
        (order.type == WeaponType::MOVE ||
         order.type == WeaponType::ATTACK_MOVE ||
         order.type == WeaponType::ATTACK) &&
        unit.can_move &&
        (unit.move_type == MoveType::LAND ||
         unit.move_type == MoveType::WATER ||
         unit.move_type == MoveType::HOVER ||
         unit.move_type == MoveType::OVER_CLIFF ||
         unit.move_type == MoveType::OVER_CLIFF_WATER);
    if (prepath_eligible && cmd.replay_prepaths_provided) {
        order.replay_prepath_command_had_active_order =
            command_had_active_order;
        const auto append_replay_prepath = [&](const ReplayPrepath& source) {
            if (!source.has_path) return;
            UnitOrderPrepath cached;
            cached.unit_uid = source.unit_uid;
            cached.start_x = source.start_x;
            cached.start_y = source.start_y;
            cached.target_x = source.target_x;
            cached.target_y = source.target_y;
            cached.frame = source.frame;
            cached.move_type = source.move_type;
            cached.path.reserve(source.tiles.size());
            for (const auto& [tile_x, tile_y] : source.tiles) {
                cached.path.push_back({
                    pathfinder_.world_x(tile_x),
                    pathfinder_.world_y(tile_y)});
            }
            if (!cached.path.empty() &&
                pathfinder_.unclamped_tile_x(source.target_x) ==
                    source.tiles.back().first &&
                pathfinder_.unclamped_tile_y(source.target_y) ==
                    source.tiles.back().second) {
                cached.path.back() = {source.target_x, source.target_y};
            }
            order.replay_prepath_cache.push_back(std::move(cached));
        };
        for (const ReplayPrepath& source : cmd.replay_prepaths) {
            if (source.has_path && source.tiles.size() > 4) {
                order.replay_command_has_long_prepath = true;
            }
            if (source.has_path && source.tiles.size() == 9) {
                order.replay_command_has_handoff_cache = true;
            }
            append_replay_prepath(source);
        }
        // Retain compatibility with callers that still provide one entry
        // through the original scalar fields.
        if (cmd.replay_prepaths.empty() && cmd.has_replay_prepath) {
            ReplayPrepath source;
            source.unit_uid = unit.uid;
            source.start_x = unit.x;
            source.start_y = unit.y;
            source.target_x = cmd.replay_prepath_target_x;
            source.target_y = cmd.replay_prepath_target_y;
            source.frame = cmd.replay_prepath_frame;
            source.move_type = cmd.replay_prepath_move_type;
            source.has_path = true;
            source.tiles = cmd.replay_prepath_tiles;
            order.replay_command_has_long_prepath = source.tiles.size() > 4;
            order.replay_command_has_handoff_cache = source.tiles.size() == 9;
            append_replay_prepath(source);
        }
    } else if (prepath_eligible) {
        f32 prepath_goal_x = order.x;
        f32 prepath_goal_y = order.y;
        f32 prepath_start_x = unit.x;
        f32 prepath_start_y = unit.y;
        f32 prepath_rotation = unit.rotation;
        i32 prepath_target_size = 0;
        if (order.type == WeaponType::ATTACK && target != nullptr) {
            prepath_goal_x = target->x;
            prepath_goal_y = target->y;
            prepath_target_size = attack_path_target_size(unit, target);
            if (!raw_commands_) {
                const i32 historical_tick = tick_ - 6;
                for (const PositionSnapshot& snapshot : unit.position_history) {
                    if (snapshot.tick != historical_tick) continue;
                    prepath_start_x = snapshot.x;
                    prepath_start_y = snapshot.y;
                    prepath_rotation = snapshot.rotation;
                    break;
                }
                for (const PositionSnapshot& snapshot : target->position_history) {
                    if (snapshot.tick != historical_tick) continue;
                    prepath_goal_x = snapshot.x;
                    prepath_goal_y = snapshot.y;
                    break;
                }
            }
        }
        // k.l.a(i, false): the asynchronous pre-path request refreshes the
        // object layer on the 50-frame gate and backdates the stamp by 40.
        const i32 prepath_layer =
            Pathfinder::ground_layer_index(unit.move_type);
        if (prepath_layer >= 0 &&
            object_cost_refresh_frame_[prepath_layer] + 50 < tick_) {
            const i32 old_refresh_frame = object_cost_refresh_frame_[prepath_layer];
            object_cost_refresh_frame_[prepath_layer] = tick_ - 40;
            const Map<u32, bool>* prediction_snapshot =
                command_path_snapshot_active_
                    ? &command_path_prediction_snapshot_ : nullptr;
            if (object_cost_sources_.empty()) {
                pathfinder_.rebuild_object_costs(
                    unit.move_type, {&units_}, prediction_snapshot);
            } else {
                pathfinder_.rebuild_object_costs(
                    unit.move_type, object_cost_sources_,
                    prediction_snapshot);
            }
        }
        // k.k.p: the prepath always uses prefer_clearance=true per JAR.
        order.prepath = pathfinder_.find_path(
            prepath_start_x, prepath_start_y, prepath_goal_x, prepath_goal_y,
            unit.move_type, unit.radius, prepath_rotation,
            unit.reverse_speed_multiplier > 0.95f || unit.reversing,
            true, prepath_target_size, unit.path_min_clearance);
        order.prepath_frame = tick_;
        order.prepath_attach_frame = tick_;
        order.prepath_target_x = prepath_goal_x;
        order.prepath_target_y = prepath_goal_y;
        order.prepath_move_type = unit.move_type;
        order.has_prepath = true;
    }

    const bool activate_now = unit.order_queue.empty();
    if (activate_now && guard_replaces_patrol) {
        unit.guard_replacement_from_patrol = true;
    }
    const u32 activation_main_target_uid = activate_now ? unit.main_target_uid() : 0;
    const i32 activation_main_target_team_id = activate_now ? unit.main_target_team_id() : -1;
    const bool activation_main_target_is_live = [&] {
        if (activation_main_target_uid == 0) return false;
        World* owner = order_target_world_(activation_main_target_team_id, other_worlds);
        const Unit* main = owner ? owner->get_unit(activation_main_target_uid) : nullptr;
        return main && main->is_alive && main->parent_uid == 0;
    }();
    unit.order_queue.push_back(std::move(order));
    if (activate_now) {
        // Adding an armed service waypoint changes where R is stored.
        // Preserve it before activate_order_ or a deferred object dispatch
        // reads the newly exposed queue head.
        unit.restore_main_target(activation_main_target_uid,
                                 activation_main_target_team_id);
    }
    if (activate_now &&
        unit.order_queue.front().type == WeaponType::ATTACK_MOVE) {
        // Native dispatch and passive range read the current waypoint type.
        // A newly linked follower can skip activate_order_ while retaining R.
        unit.weapon_type = WeaponType::ATTACK_MOVE;
    }
    if (activate_now && unit.order_queue.front().type == WeaponType::BUILD) {
        // e.k() changes the waypoint while retaining R, even when a linked
        // BUILD will skip private activation. Store R separately from the
        // construction target before the next object's dispatch can run.
        unit.service_combat_target_uid = activation_main_target_uid;
        unit.service_combat_target_team_id = activation_main_target_team_id;
        unit.target_uid = 0;
        unit.target_team_id = -1;
        unit.weapon_type = WeaponType::BUILD;
    }

    if (activate_now && !raw_commands_ && !unit.is_active()) {
        // e.k() stores waypoints while transported or incomplete. Do not
        // create aU or publish k/l/m before the first active object update.
        unit.activate_order_next_frame = true;
    } else if (raw_commands_ &&
        (unit.order_queue.front().type == WeaponType::MOVE ||
         unit.order_queue.front().type == WeaponType::ATTACK_MOVE)) {
        const UnitOrder& front = unit.order_queue.front();
        unit.has_move_target = true;
        unit.target_x = front.x;
        unit.target_y = front.y;
        unit.arrived = false;
        unit.using_path = false;
        unit.waypoints.clear();
        unit.current_waypoint = 0;
    } else if (activate_now &&
               unit.order_queue.front().type == WeaponType::BUILD &&
               dist(unit.x, unit.y,
                    unit.order_queue.front().x,
                    unit.order_queue.front().y) <=
                   service_range_to_(unit, unit.order_queue.front().unit_type)) {
        // y.c() reaches an already-in-range positional BUILD before the
        // generic navigation controller can submit aU. Leave the waypoint
        // queued for this object's build phase without creating a path
        // request that could be adopted after movement has already stopped.
        unit.weapon_type = WeaponType::BUILD;
    } else if (activate_now && !raw_commands_ &&
               cmd.replay_prepaths_provided &&
               unit.order_queue.front().type == WeaponType::BUILD) {
        // e.k() queues BUILD without requesting a path. Independent commands
        // can arrive in a different order from the builders' object turns;
        // only the first live turn may refresh the shared stationary costs.
        unit.activate_order_next_frame = true;
    } else if (activate_now && !raw_commands_ &&
               cmd.replay_prepaths_provided && cmd.group_size <= 1 &&
               unit.order_queue.front().type == WeaponType::MOVE &&
               unit.spawn_delay_frames <= 0.0f) {
        // e.k() queues a solo MOVE before y.k() requests its path on the
        // unit's object turn. Earlier objects, including LOAD requesters,
        // must update cK before this request can refresh shared costs.
        unit.activate_order_next_frame = true;
    } else if (activate_now && !raw_commands_ &&
               cmd.defer_waypoint_activation) {
        // e.k() mutates every command's queue, then y.k() dispatches the
        // surviving waypoint on the unit's object turn. An intermediate
        // waypoint replaced in the same frame must not submit aU, change
        // y.o/y.p, or refresh the object-cost layer.
        unit.activate_order_next_frame = true;
    } else if (activate_now && !raw_commands_ &&
               cmd.replay_prepaths_provided && cmd.group_size > 1 &&
               unit.order_queue.front().type == WeaponType::MOVE &&
               !unit.path_request_pending && unit.spawn_delay_frames <= 0.0f) {
        // e.k() replaces every selected waypoint before ab.c() assigns the
        // new leader. A moving member can become a follower too: its old
        // MOVE must not make it refresh object costs while the group is
        // still being submitted. Dispatch the surviving leader's request
        // on its object turn, after preceding followers have updated cK.
        unit.activate_order_next_frame = true;
    } else if (activate_now && !raw_commands_ &&
               cmd.replay_prepaths_provided &&
               unit.order_queue.front().type == WeaponType::ATTACK_MOVE &&
               !activation_main_target_is_live && !unit.path_request_pending) {
        // e.k() leaves an unavailable R for y.k() to inspect on the object's
        // turn. A stale reference must not activate a command-time path or
        // refresh stationary costs before preceding objects have updated cK.
        // The new formation's leader requests after command setup finishes.
        // Followers can discard the activation flag when they are linked;
        // the ATTACK_MOVE role still governs their passive search range.
        // This also applies after an ATTACK whose main target was cleared:
        // charging s during the command would spend one extra frame on it.
        unit.weapon_type = WeaponType::ATTACK_MOVE;
        unit.activate_order_next_frame = true;
    } else if (activate_now && !raw_commands_ &&
               cmd.replay_prepaths_provided && !command_had_active_order &&
               (cmd.group_index > 0 || cmd.group_size > 1) &&
               (order.type == WeaponType::MOVE ||
                order.type == WeaponType::ATTACK_MOVE)) {
        // Native replay e.k() only queues a waypoint. The path probe runs
        // later on the unit's object turn, after the command assigns its new
        // formation. Even the first selected unit can become a follower and
        // must not refresh object costs before the new leader requests a path.
        unit.activate_order_next_frame = true;
    } else if (activate_now &&
               (order.type == WeaponType::ATTACK ||
                order.type == WeaponType::GUARD ||
                order.type == WeaponType::FOLLOW) &&
               !raw_commands_) {
        // Native commands become part of the global object update before the
        // selected unit's own turn. The attack handler therefore resolves the
        // target's latest position there, while the order is already visible.
        unit.activate_order_next_frame = true;
    } else if (activate_now &&
               !activate_order_(unit, unit.order_queue.front(), other_worlds)) {
        unit.order_queue.clear();
        clear_current_order_state_(unit, !unit.is_active());
        return false;
    }
    return true;
}

} // namespace rwrl
