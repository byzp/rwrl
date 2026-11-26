#include "world_internal.h"

#include <cstdio>
#include <cstdlib>

namespace rwrl {

void World::execute_command(const GameCommand& cmd,
                            const Vec<World*>& other_worlds) {
    Unit* u = get_unit(cmd.unit_uid);
    if (cmd.group_index == 0 || command_formation_local_start_) {
        active_command_formation_group_id_ = next_formation_group_id_++;
        command_path_prediction_snapshot_.clear();
        command_pending_path_uids_.clear();
        const auto capture_predictions = [&](const Map<u32, Unit>& source) {
            for (const auto& [uid, unit] : source) {
                command_path_prediction_snapshot_[uid] =
                    unit.movement_prediction_enabled;
                if (unit.path_request_pending) {
                    command_pending_path_uids_.insert(uid);
                }
            }
        };
        if (object_cost_sources_.empty()) {
            capture_predictions(units_);
        } else {
            for (const Map<u32, Unit>* source : object_cost_sources_) {
                if (source) capture_predictions(*source);
            }
        }
        handoff_command_prediction_snapshot_ =
            command_path_prediction_snapshot_;
        handoff_command_snapshot_tick_ = tick_ + 1;
        command_path_snapshot_active_ = true;
        if (!command_formation_clear_prepared_ &&
            cmd.action <= GameCommand::SET_PASSIVE_TARGET &&
            !cmd.queued && !cmd.clear_non_build_repair && !cmd.clear_orders &&
            cmd.group_uids.size() > 1) {
            // e.k() clears non-leaders first, then leaders, before appending
            // any replacement waypoint. az()/aD() can regroup the remaining
            // old waypoints and change ae during either pass. Keep those
            // links and leader preferences while the per-unit API replaces
            // the actual queues below; already-cleared queues cannot join
            // an intervening regroup.
            Set<u32> cleared_uids;
            for (bool leader_pass : {false, true}) {
                for (u32 selected_uid : cmd.group_uids) {
                    Unit* selected = get_unit(selected_uid);
                    if (!selected || !selected->is_alive ||
                        selected->formation_is_leader != leader_pass) {
                        continue;
                    }
                    cleared_uids.insert(selected_uid);
                    rebuild_formation_after_leader_removed_(
                        *selected, &cleared_uids);
                    selected->formation_leader_uid = 0;
                }
            }
        }
    }
    if (!u) {
        if (cmd.action <= GameCommand::SET_PASSIVE_TARGET) {
            collect_command_formation_(cmd, other_worlds);
        }
        return;
    }
    if (cmd.action == GameCommand::TRIGGER_ACTION && cmd.direct_action) {
        if (u->unit_type == "hovercraft" || u->unit_type == "dropship" ||
            (u->custom_unit && u->cargo_capacity > 0)) {
            // The default transport buttons operate independently of au.
            // custom.j also distinguishes alternate/cancel invocations.
            if (cmd.action_id == "109") {
                if (u->custom_unit && !u->transport_add_unload_option) return;
                if (u->custom_unit && cmd.action_alt) cancel_unload_all(*u);
                else begin_unload_all(*u);
                return;
            }
            if (cmd.action_id == "110") {
                if (u->custom_unit && !u->transport_add_unload_option) return;
                if (!u->custom_unit || !cmd.action_alt) cancel_unload_all(*u);
                return;
            }
        }
        const CustomActionDef* action =
            registry_.get_custom_action(u->unit_type, cmd.action_id);
        if (action && action->queueable) {
            if (cmd.action_alt) {
                if (action->can_player_cancel) {
                    cancel_factory_queue(u->uid,
                        "__custom_action__:" + action->id);
                }
            } else {
                CustomActionContext context;
                context.has_position = cmd.has_action_position;
                context.x = cmd.target_x;
                context.y = cmd.target_y;
                context.target_uid = cmd.target_uid;
                World* target_world = find_order_target_world_(
                    cmd.target_uid, other_worlds);
                if (target_world) context.target_team_id = target_world->team_id_;
                queue_custom_action_(*u, *action, context);
            }
            return;
        }
    }
    if (cmd.action == GameCommand::TRIGGER_ACTION && cmd.direct_action) {
        // Stock factories use 110; laser defence and supply depots use 102.
        // These buttons operate on production while the building is incomplete.
        const bool factory_button = cmd.action_id == "110" &&
            (u->unit_type == "landFactory" ||
             u->unit_type == "airFactory" || u->unit_type == "seaFactory");
        const bool defence_button = !u->custom_unit && cmd.action_id == "102" &&
            (u->unit_type == "laserDefence" || u->unit_type == "supplyDepot");
        if (factory_button || defence_button) {
            if (cmd.action_alt) {
                // Native cancellation refunds the paid queue item in full.
                cancel_factory_queue(u->uid, "__upgrade_tier__");
            } else {
                queue_upgrade_(*u, "");
            }
            return;
        }
    }
    // e.i() retains live selections regardless of am.cm. Waypoints are
    // stored immediately, while y.k() waits for a complete object turn.
    const bool incomplete_unit_command =
        u->is_alive && u->parent_uid == 0 &&
        (cmd.action <= GameCommand::SET_PASSIVE_TARGET ||
         (u->is_building && (cmd.action == GameCommand::SET_RALLY ||
                             cmd.action == GameCommand::UPGRADE)) ||
         (cmd.action == GameCommand::STOP && cmd.clear_orders));
    const bool production_button =
        cmd.action == GameCommand::BUILD && cmd.direct_action;
    const bool incomplete_custom_production =
        production_button && u->custom_unit && u->is_alive &&
        u->parent_uid == 0;
    // e.i() excludes dead selections, but keeps units inside a transport.
    // Their waypoints and formation links change now; y.k() cannot dispatch
    // them until the passenger has an active object turn again.
    const bool passenger_waypoint_command =
        u->is_alive && u->parent_uid != 0 &&
        (cmd.action <= GameCommand::SET_PASSIVE_TARGET ||
         (cmd.action == GameCommand::STOP && cmd.clear_orders));
    if (!u->is_active() && !incomplete_unit_command &&
        !incomplete_custom_production &&
        !passenger_waypoint_command) {
        // Native e.i() removes dead selection entries before e.k() commits
        // the formation. The per-unit API must still finish that commit when
        // the last serialized selection entry is skipped.
        if (cmd.action <= GameCommand::SET_PASSIVE_TARGET) {
            collect_command_formation_(cmd, other_worlds);
        }
        return;
    }

    if (u->movement_proposal_command_tick != tick_ + 1) {
        u->command_start_has_move_target = u->has_move_target;
        u->movement_proposal_command_tick = tick_ + 1;
    }

    const bool replacing_active_movement =
        !cmd.queued && u->has_move_target && !u->arrived;
    const bool replacing_active_guard =
        replacing_active_movement &&
        !u->order_queue.empty() &&
        u->order_queue.front().type == WeaponType::GUARD;
    const bool guard_replaces_active_movement =
        !cmd.queued && cmd.action == GameCommand::GUARD &&
        u->has_move_target && !u->arrived;

    if (cmd.clear_orders) {
        clear_current_order_state_(*u,
            passenger_waypoint_command || !u->is_complete());
        u->order_queue.clear();
        // e.o calls y.az(): detach both sides of the old formation, while
        // leaving an outstanding aU result available to the next object turn.
        rebuild_formation_after_leader_removed_(*u);
        u->formation_leader_uid = 0;
        if (cmd.action == GameCommand::STOP) return;
    }

    if (cmd.action == GameCommand::TRIGGER_ACTION && cmd.direct_action &&
        (u->unit_type == "attackSubmarine" || u->unit_type == "amphibiousJet") &&
        (cmd.action_id == "151" || cmd.action_id == "152")) {
        // h.e.a(s,boolean) and b.c.a(s,boolean) only change the requested
        // surface mode. These direct buttons do not submit an au waypoint,
        // clear av/aw or reset W/s, even when the mode is already selected.
        set_submerged(*u, cmd.action_id == "152");
        return;
    }

    if (cmd.action <= GameCommand::SET_PASSIVE_TARGET &&
        !(cmd.action == GameCommand::BUILD &&
          (u->is_building || production_button))) {
        const bool replay_activation_snapshot =
            cmd.replay_prepaths_provided && command_path_snapshot_active_;
        if (replay_activation_snapshot && !cmd.group_uids.empty()) {
            // The public API submits one command per selected unit. The
            // serialized replay cache can make the formation leader's path
            // visible while earlier members are still being queued, so keep
            // the historical projection for requests that use that cache.
            // Very large replay selections are still being visited by the
            // native object loop when the first cached request rebuilds the
            // layer. Do not project their future representatives early; the
            // current unit is marked below when its own request is submitted.
            const bool skip_large_replay_projection = cmd.group_size >= 20;
            size_t projection_end = cmd.group_index;
            if (!skip_large_replay_projection) {
                projection_end = std::min<size_t>(
                    cmd.group_index, cmd.group_uids.size() - 1);
                for (const ReplayPrepath& prepath : cmd.replay_prepaths) {
                    if (prepath.move_type != u->move_type) continue;
                    const auto representative = std::find(
                        cmd.group_uids.begin(), cmd.group_uids.end(),
                        prepath.unit_uid);
                    if (representative == cmd.group_uids.end()) continue;
                    const size_t representative_index = static_cast<size_t>(
                        representative - cmd.group_uids.begin());
                    if (representative_index >= cmd.group_index) {
                        projection_end = representative_index;
                        break;
                    }
                }
            }
            for (size_t index = cmd.group_index;
                 index <= projection_end && index < cmd.group_uids.size();
                 ++index) {
                command_path_prediction_snapshot_[cmd.group_uids[index]] = true;
            }
        }
        const bool prior_prediction =
            replay_activation_snapshot
                ? command_path_prediction_snapshot_[u->uid]
                : u->movement_prediction_enabled;
        const bool linked_formation_prediction =
            prior_prediction && u->formation_leader_uid != 0;
        if (replay_activation_snapshot) {
            // e.k() visits selected units sequentially. y.a() sets cK=true
            // before probing either the direct line or the grid path, and it
            // remains true while later members of the same command are
            // submitted. Future members retain the cK value captured before
            // the group command until their own request is visited.
            command_path_prediction_snapshot_[u->uid] = true;
        }
        command_path_request_active_ = true;
        const bool idle_prediction_before_command =
            u->movement_prediction_enabled;
        const bool submitted = submit_order_(*u, cmd, other_worlds);
        command_path_request_active_ = false;
        if (submitted && u->idle_prediction_command_tick != tick_ + 1) {
            u->idle_prediction_before_command = idle_prediction_before_command;
            u->idle_prediction_command_tick = tick_ + 1;
        }
        if (submitted && !cmd.queued) {
            // Native replay replaces the active GUARD before the unit's next
            // object turn. y.a() still uses the replacement's logical target
            // for that turn's physical tolerance, once only.
            u->guard_logical_tolerance_pending =
                guard_replaces_active_movement;
            if (cmd.action == GameCommand::GUARD) {
                const bool prior_movement_prediction =
                    u->movement_prediction_enabled;
                bool tracks_guard_target = false;
                bool tracks_retained_main_target = false;
                for (const WeaponSlot& slot : u->weapon_slots) {
                    if (slot.target_uid == cmd.target_uid) {
                        tracks_guard_target = true;
                    }
                    if (u->target_uid != 0 &&
                        slot.target_uid == u->target_uid) {
                        tracks_retained_main_target = true;
                    }
                }
                World* recent_guard_world = find_order_target_world_(
                    cmd.target_uid, other_worlds);
                const Unit* recent_guard_target = recent_guard_world
                    ? recent_guard_world->get_unit(cmd.target_uid) : nullptr;
                const bool guard_target_is_recent_attacker =
                    u->was_attacked &&
                    u->last_attacker_uid == cmd.target_uid &&
                    u->last_damage_age_ms >= 0.0f &&
                    u->last_damage_age_ms < 2000.0f &&
                    recent_guard_target && recent_guard_world != this &&
                    is_enemy_with(*recent_guard_world) &&
                    can_target_(*u, *recent_guard_target);
                const bool keep_formation_target_motion =
                    !replacing_active_guard && tracks_guard_target &&
                    u->move_type == MoveType::AIR &&
                    u->target_uid == cmd.target_uid &&
                    (prior_movement_prediction ||
                     u->formation_leader_uid != 0);
                const bool keep_retained_small_formation_motion =
                    !replacing_active_guard &&
                    tracks_retained_main_target &&
                    cmd.group_size < 19 &&
                    replacing_active_movement &&
                    u->formation_group_size > 1 &&
                    prior_movement_prediction;
                if (replacing_active_movement &&
                    tracks_retained_main_target &&
                    cmd.group_size >= 19) {
                    u->guard_replacement_refresh_next_frame = true;
                }
                if (replacing_active_guard && u->target_uid != 0 &&
                        (tracks_guard_target || tracks_retained_main_target) ||
                    guard_target_is_recent_attacker ||
                    keep_formation_target_motion ||
                    keep_retained_small_formation_motion) {
                    u->guard_replacement_keeps_movement = true;
                }
            }
        }
        if (replay_activation_snapshot) {
            command_path_prediction_snapshot_[u->uid] =
                linked_formation_prediction;
            // An in-range positional builder does not call y.a() and keeps
            // cK=false. Its prior formation link must not turn it into a
            // moving object in a later member's path-cost refresh.
            if (cmd.action == GameCommand::BUILD &&
                (!u->has_move_target || u->arrived)) {
                command_path_prediction_snapshot_[u->uid] = false;
            }
        }
        if (submitted && replacing_active_guard &&
            cmd.action == GameCommand::MOVE &&
            u->movement_activation_delay_frames > 0.0f &&
            u->using_path) {
            // Native replay commands execute after this unit's object turn,
            // so only the command tick skips physical steering. The Python
            // driver submits before Engine::tick(); defer that one pass and
            // let the following tick steer along the replacement path.
            u->movement_activation_delay_frames = 1.0f;
            u->movement_delay_keeps_prediction = true;
        }
        collect_command_formation_(cmd, other_worlds);
        return;
    }

    switch (cmd.action) {
    case GameCommand::BUILD: {
        if (cmd.action_alt) {
            cancel_factory_queue(u->uid, cmd.unit_type);
        } else if (can_factory_build_(*u, cmd.unit_type)) {
            queue_unit(u->uid, cmd.unit_type);
        }
        break;
    }
    case GameCommand::UNLOAD_ALL:
        begin_unload_all(*u);
        break;
    case GameCommand::CANCEL_UNLOAD:
        cancel_unload_all(*u);
        break;
    case GameCommand::STOP:
        stop_unit(*u);
        break;
    case GameCommand::UPGRADE:
        queue_upgrade_(*u, cmd.unit_type);
        break;
    case GameCommand::SUBMERGE:
        set_submerged(*u, true);
        break;
    case GameCommand::SURFACE:
        set_submerged(*u, false);
        break;
    case GameCommand::CANCEL_QUEUE:
        cancel_factory_queue(u->uid, cmd.unit_type);
        break;
    case GameCommand::SET_RALLY:
        if (u->is_building) set_factory_rally(u->uid, cmd.target_x, cmd.target_y);
        break;
    case GameCommand::BUILD_NUKE:
        queue_nuke(u->uid);
        break;
    case GameCommand::BUILD_ANTINUKE:
        queue_antinuke(u->uid);
        break;
    case GameCommand::LAUNCH_NUKE:
        launch_nuke(*u, cmd.target_x, cmd.target_y);
        break;
    }
}

} // namespace rwrl
