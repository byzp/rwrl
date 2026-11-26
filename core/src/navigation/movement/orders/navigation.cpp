#include "world_internal.h"
#include "combat/targeting_helpers.h"

#include <cstdio>
#include <cstdlib>
#include <tuple>

namespace rwrl {

bool World::update_unit_order_navigation_(
        Unit& unit, f32 delta_frames, const Vec<World*>& other_worlds) {
    bool activated_order_this_update = false;
    bool patrol_passive_target_stopped_this_update = false;
    bool patrol_passive_target_in_range_this_update = false;
    const bool patrol_logical_target_was_active =
        unit.patrol_logical_target_active;
    const bool guard_replacement_from_patrol =
        unit.guard_replacement_from_patrol;
    unit.guard_replacement_from_patrol = false;
    const bool guard_replacement_refresh_next_frame =
        unit.guard_replacement_refresh_next_frame;
    unit.guard_replacement_refresh_next_frame = false;
    constexpr bool order_phase = true;
    // ad.a starts false for each y.k() dispatch. Consuming an already-full
    // target must not carry the previous turn's h into a later repair scan.
    unit.service_aim_ready = false;
    unit.service_aim_target_uid = 0;
    unit.path_request_low_priority = false;
    // y.k() snapshots k before clearing it. The frame-start proposal is the
    // previous visible object's k; the older frame snapshot can stay true
    // for one update after the patrol has stopped.
    const bool patrol_dispatch_was_moving =
        unit.frame_start_has_move_target;
    if (unit.formation_is_leader &&
        unit.formation_path_activation_pending) {
        unit.formation_path_activation_pending = false;
    }
    unit.attack_move_combat_target_valid = false;
    unit.attack_move_private_navigation_handoff = false;
    unit.attack_move_direct_chase_proposal = false;
    if (unit.order_queue.empty() && unit.direct_attack_chase_this_update) {
        // y.k() clears k before the passive combat update publishes a new
        // j/k/l/m proposal. Retain av/aw while an in-range target holds it.
        if (unit.using_path && !unit.path_target_override_active) {
            unit.path_target_override_active = true;
            unit.path_target_override_x = unit.target_x;
            unit.path_target_override_y = unit.target_y;
        }
        unit.has_move_target = false;
        unit.arrived = true;
        unit.target_x = 0.0f;
        unit.target_y = 0.0f;
    }
    unit.direct_attack_chase_this_update = false;
    unit.guard_path_created_without_node = false;
    // y.k() clears j before c(GUARD) selects this turn's R or q(2).
    // A proposal retained through the prior combat pass must be selected
    // again; otherwise an expired q(2) keeps chasing its old attacker.
    unit.guard_movement_target_hold = false;
    unit.guard_movement_target_valid = false;
    unit.guard_movement_chases_main_target = false;
    // ad.a() resets the temporary formation-projection gate at the
    // start of every y.k(float) object update.
    unit.formation_projection_allowed = true;
    // y.k(float) publishes bR through bQ before clearing an expired bR.
    // The current collision pass therefore still excludes the transport on
    // the expiry turn. This timer advances only on the passenger's active
    // object turn, including an unload earlier in the same update.
    unit.collision_approach_target_uid = unit.collision_ignore_uid;
    if (unit.collision_ignore_uid != 0) {
        unit.collision_ignore_frames = approach_value(
            unit.collision_ignore_frames, 0.0f, combat_delta_frames_);
        if (unit.collision_ignore_frames == 0.0f) {
            unit.collision_ignore_uid = 0;
        }
    }
    if (unit.clear_exposed_move_next_frame) {
        unit.clear_exposed_move_next_frame = false;
        if (unit.order_queue.empty()) {
            unit.has_move_target = false;
            unit.arrived = true;
            unit.target_x = 0.0f;
            unit.target_y = 0.0f;
        }
    }
    if (unit.activate_order_next_frame) {
        unit.activate_order_next_frame = false;
        activated_order_this_update = true;
        while (!unit.order_queue.empty()) {
            const UnitOrder& next = unit.order_queue.front();
            const bool projects_current_waypoint = next.type == WeaponType::BUILD ||
                (next.type == WeaponType::MOVE &&
                 unit.formation_projection_allowed &&
                 !unit.path_request_pending);
            const Unit* waypoint_leader = projects_current_waypoint &&
                unit.formation_leader_uid != 0
                ? formation_unit_(unit.formation_leader_uid) : nullptr;
            if (waypoint_leader && waypoint_leader->is_active()) {
                const UnitOrder* lead = waypoint_leader->order_queue.empty()
                    ? nullptr : &waypoint_leader->order_queue.front();
                if ((!lead && next.type == WeaponType::MOVE) ||
                    (lead && lead->type == next.type && lead->unit_type == next.unit_type &&
                    lead->build_tech_level == next.build_tech_level &&
                    lead->target_uid == next.target_uid &&
                    lead->target_team_id == next.target_team_id &&
                    std::abs(lead->x - next.x) <= 1.0f &&
                    std::abs(lead->y - next.y) <= 1.0f)) {
                    // ax() or construction completion can expose a follower's
                    // waypoint before its own turn. y.k() publishes BUILD or
                    // MOVE through a matching live leader without first
                    // activating a private path and its waiting frame. The
                    // shared command cache is consulted only by that private
                    // path branch, after the native formation gate.
                    unit.has_move_target = true;
                    unit.arrived = false;
                    unit.target_x = next.x;
                    unit.target_y = next.y;
                    // The common controller compares waypoints only when
                    // both members still have one. An idle leader remains a
                    // valid MOVE projection after completing on its turn.
                    if (next.type == WeaponType::MOVE) {
                        unit.movement_activation_delay_frames = 0.0f;
                    }
                    break;
                }
            }
            if (next.type == WeaponType::MOVE &&
                unit.spawn_delay_frames == 0.0f &&
                !unit.path_request_pending &&
                dist2(unit.x, unit.y, next.x, next.y) >= 49.0f) {
                const bool had_physical_path = unit.using_path;
                // A result adopted after ay() remains av on the next turn.
                // The queued MOVE refreshes against that captured chain.
                move_unit_to_(unit, next.x, next.y, 0, true, had_physical_path);
                const bool replay_projection =
                    (next.prepath_from_replay ||
                     (next.replay_command_group_index == 0 &&
                      next.replay_command_has_path_cache)) &&
                    waypoint_leader && waypoint_leader->is_active() &&
                    unit.formation_projection_allowed &&
                    (!next.replay_prepath_command_had_active_order ||
                     next.replay_command_group_index == 0);
                unit.movement_activation_delay_frames =
                    replay_projection || had_physical_path ? 0.0f : 1.0f;
                if (unit.movement_activation_delay_frames > 0.0f) {
                    unit.movement_delay_uses_acceleration =
                        unit.reversing && unit.move_speed_factor < 0.0f;
                }
                break;
            }
            if (next.type == WeaponType::REPAIR ||
                next.type == WeaponType::RECLAIM) {
                // y.k() selects one au, then i() owns its rejection and
                // completion. A newly exposed service target may already
                // be dead; do not erase it here and dispatch its successor
                // during the same object turn.
                activate_order_(unit, next, other_worlds);
                break;
            }
            if (activate_order_(unit, next, other_worlds)) {
                break;
            }
            unit.order_queue.erase(unit.order_queue.begin());
        }
    }
    if (!unit.order_queue.empty() &&
        (unit.order_queue.front().type == WeaponType::REPAIR ||
         unit.order_queue.front().type == WeaponType::RECLAIM)) {
        const UnitOrder& order = unit.order_queue.front();
        // y.i() dispatches the current au every turn. BUILD can replace its
        // payload with REPAIR without calling the command activation helper;
        // that waypoint must work with or without an old construction task.
        unit.weapon_type = order.type;
        unit.target_uid = order.target_uid;
        unit.target_team_id = order.target_team_id;
        const bool repair_order = order.type == WeaponType::REPAIR;
        World* target_world = order_target_world_(
            order.target_team_id, other_worlds);
        Unit* target = target_world ? target_world->get_unit(order.target_uid) : nullptr;
        if (!repair_order && target && target->team_id != unit.team_id &&
            target->unit_type != "crystalResource" && !singleplayer_mode_) {
            // y.i() permits allied non-resource reclaim only when l.P()
            // reports single-player. Network play and replay playback both
            // reject it on dispatch, leaving queued orders intact until then.
            finish_service_order(unit);
            complete_current_order_(unit, other_worlds, false, true);
            unit.order_phase_tick = tick_;
            unit.has_move_target = false;
            unit.arrived = true;
            return false;
        }
        if ((!target || !target->is_alive || target->parent_uid != 0) &&
            (repair_order || !target || target->unit_type != "crystalResource")) {
            // i() calls ax() for an invalid service target. Resource reclaim
            // retains its separate nearest-resource retargeting branch.
            // Advance matching followers before ay(), then leave the newly
            // exposed waypoint for this unit's next object turn.
            complete_formation_followers_(unit, other_worlds);
            finish_service_order(unit);
            complete_current_order_(unit, other_worlds, false, true);
            unit.order_phase_tick = tick_;
            unit.has_move_target = false;
            unit.arrived = true;
            unit.movement_prediction_enabled = false;
            return false;
        }
        if (repair_order && target && target->is_alive && target->parent_uid == 0 &&
            can_repair_target(unit, *target) &&
            order.service_chase_range_extra == 0.0f &&
            !service_target_in_range_(unit, *target)) {
            // y.i() drops au.k==0 before publishing a travelling proposal.
            // No fresh solver request may survive this automatic waypoint.
            finish_service_order(unit);
            complete_current_order_(unit, other_worlds);
            unit.order_phase_tick = tick_;
            unit.has_move_target = false;
            unit.arrived = true;
            unit.movement_prediction_enabled = false;
            unit.activate_order_next_frame = !unit.order_queue.empty();
            return false;
        }
        if (repair_order && target && target->is_alive && target->parent_uid == 0 &&
            can_repair_target(unit, *target) && target->is_complete() &&
            target->hp >= target->max_hp) {
            // y.i() calls ax(), rather than ay(), when its repair target was
            // already full at dispatch. Propagate to followers before this
            // object's service/build phase can consume the waypoint late.
            complete_formation_followers_(unit, other_worlds);
            finish_service_order(unit);
            complete_current_order_(unit, other_worlds);
            // This handler consumed the waypoint selected at the start of
            // y.k(). The physical pass must coast without dispatching the
            // newly exposed waypoint again during this same object turn.
            unit.order_phase_tick = tick_;
            unit.has_move_target = false;
            unit.arrived = true;
            unit.movement_prediction_enabled = false;
            unit.activate_order_next_frame = !unit.order_queue.empty();
            return false;
        }
    }
    if (unit.order_queue.empty() ||
        unit.order_queue.front().type != WeaponType::ATTACK) {
        unit.attack_final_node_coast_pending = false;
    }
    if (!unit.order_queue.empty() &&
        unit.order_queue.front().max_time_frames > 0.0f &&
        unit.active_order_elapsed_frames >
            unit.order_queue.front().max_time_frames) {
        complete_current_order_(unit, other_worlds);
    }

    if (!unit.order_queue.empty() &&
        (unit.is_building || unit.ignore_move_orders)) {
        const WeaponType type = unit.order_queue.front().type;
        if (type == WeaponType::MOVE || type == WeaponType::ATTACK_MOVE ||
            type == WeaponType::PATROL) {
            // y's MOVE-family dispatch checks aR(), then drops a waypoint
            // that the current definition ignores. A deployment conversion
            // runs after dispatch, so this happens on its next object turn.
            complete_current_order_(unit, other_worlds);
            return false;
        }
    }

    if (!unit.order_queue.empty()) {
        const UnitOrder& active = unit.order_queue.front();
        if (active.type == WeaponType::MOVE ||
            active.type == WeaponType::ATTACK_MOVE ||
            active.type == WeaponType::PATROL) {
            // y.a(float,au,ad,boolean) tests the ordered point before the
            // combat approach and av/aw consumption, including coast turns
            // with no active path. This order pass receives zero dt in the
            // split engine, so Y uses the original object's update delta.
            const f32 order_distance_sq =
                dist2(unit.x, unit.y, active.x, active.y);
            if (order_distance_sq < 1681.0f) {
                unit.order_point_dwell_frames +=
                    delta_frames > 0.0f ? delta_frames : combat_delta_frames_;
            }
            if (active.type != WeaponType::PATROL) {
                f32 tolerance = 7.0f;
                if (unit.order_point_dwell_frames > 240.0f) tolerance = 16.0f;
                if (unit.order_point_dwell_frames > 340.0f) tolerance = 36.0f;
                bool keep_attack_move = false;
                if (active.type == WeaponType::ATTACK_MOVE &&
                    unit.target_uid != 0) {
                    World* target_world = order_target_world_(
                        unit.target_team_id, other_worlds);
                    const Unit* target = target_world
                        ? target_world->get_unit(unit.target_uid) : nullptr;
                    const f32 target_range = engagement_range_(unit);
                    keep_attack_move = target && target->is_alive &&
                        target->parent_uid == 0 && target_world != this &&
                        is_enemy_with(*target_world) &&
                        unit.behavior != Behavior::HOLD_FIRE &&
                        unit.behavior != Behavior::RETURN_FIRE &&
                        can_target_(unit, *target) &&
                        dist2(unit.x, unit.y, target->x, target->y) <
                            target_range * target_range;
                }
                if (!keep_attack_move &&
                    order_distance_sq < tolerance * tolerance) {
                    complete_formation_followers_(unit, other_worlds);
                    complete_current_order_(unit, other_worlds, false, true);
                    unit.order_phase_tick = tick_;
                    unit.movement_prediction_enabled = false;
                    return false;
                }
            }
            if (active.type == WeaponType::MOVE) {
                // The MOVE dispatcher publishes k even when a replacement
                // retained an old aU and its previous proposal was stopped.
                // The common controller can then adopt that released route
                // or project the new leader without requesting another path.
                unit.has_move_target = true;
                unit.arrived = false;
                // MOVE publishes y.n=0 even when a linked follower uses
                // projection or an older aU is still pending. The pending
                // solver retains the n captured at submission. An arrival
                // that completed the waypoint above does not publish n.
                unit.path_request_target_size = 0;
            }
        }
    }

    if (unit.order_queue.size() > 1 &&
        unit.order_queue.front().type == WeaponType::PATROL) {
        const UnitOrder& patrol = unit.order_queue.front();
        const bool recent_patrol_collision =
            unit.collision_partner_tick == tick_ ||
            unit.collision_partner_tick == tick_ - 1;
        const f32 completion_radius =
            (!patrol_dispatch_was_moving || recent_patrol_collision)
                ? 70.0f : 30.0f;
        if (dist2(unit.x, unit.y, patrol.x, patrol.y) <
            completion_radius * completion_radius) {
            // y.a(delta,au,ad,z) sets au=null after rotating this patrol.
            // Its target helper therefore cannot submit a chase request
            // for a waypoint that already completed on this object turn.
            complete_current_order_(unit, other_worlds, true, true);
            unit.order_phase_tick = tick_;
            unit.movement_prediction_enabled = false;
            return false;
        }
    }

    // A guarded target can die earlier in this same creation-ordered
    // frame. JAR clears the GUARD waypoint before y.ay() checks the
    // finished-order predicate, so consume it during this order pass
    // instead of leaving the queue visible for one extra tick.
    if (!unit.order_queue.empty() &&
        unit.order_queue.front().type == WeaponType::GUARD &&
        unit.guard_target_uid != 0) {
        World* guarded_world = order_target_world_(
            unit.guard_target_team_id, other_worlds);
        Unit* guarded = guarded_world
            ? guarded_world->get_unit(unit.guard_target_uid) : nullptr;
        if (!guarded || !guarded->is_alive) {
            unit.guard_target_uid = 0;
            unit.guard_target_team_id = -1;
        }
    }
    const bool dispatched_attack = !unit.order_queue.empty() &&
        unit.order_queue.front().type == WeaponType::ATTACK;
    advance_finished_order_(unit, other_worlds);
    unit.order_phase_tick = tick_;
    if (dispatched_attack && unit.activate_order_next_frame) {
        // The native dispatcher does not re-enter g() for the newly exposed
        // queued ATTACK. Passive combat and inertia still update this turn.
        unit.movement_prediction_enabled = false;
        return false;
    }
    const bool dispatched_transport = !unit.order_queue.empty() &&
        (unit.order_queue.front().type == WeaponType::LOAD_INTO ||
         unit.order_queue.front().type == WeaponType::LOAD_UP);
    update_transport_order_(unit, other_worlds);
    if (dispatched_transport && unit.activate_order_next_frame) {
        // h()/e() finish against the captured load waypoint. The new head
        // is visible after ay(), but cannot publish or request MOVE this turn.
        unit.movement_prediction_enabled = false;
        return false;
    }
    if (!unit.is_active()) return false;
    if (!unit.order_queue.empty()) {
        const WeaponType active_type = unit.order_queue.front().type;
        if (active_type == WeaponType::BUILD &&
            unit.weapon_type != WeaponType::BUILD) {
            // A matching formation leader can publish BUILD without calling
            // activate_order_. The positional construction handler still
            // owns this turn, independently of the unit's combat main target.
            if (unit.service_combat_target_uid == 0 &&
                has_weapon(unit.weapon_type)) {
                unit.service_combat_target_uid = unit.target_uid;
                unit.service_combat_target_team_id = unit.target_team_id;
            }
            unit.target_uid = 0;
            unit.target_team_id = -1;
            unit.weapon_type = WeaponType::BUILD;
        }
        Unit* formation_leader = unit.formation_leader_uid != 0
            ? formation_unit_(unit.formation_leader_uid) : nullptr;
        const bool build_follower_repair_handoff =
            active_type == WeaponType::BUILD &&
            unit.target_uid == 0 &&
            formation_leader && formation_leader->is_active() &&
            !formation_leader->order_queue.empty() &&
            (formation_leader->order_queue.front().type == WeaponType::REPAIR ||
             formation_leader->order_queue.front().type == WeaponType::RECLAIM) &&
            formation_leader->order_queue.front().target_uid != 0;
        bool build_target_matches_waypoint = false;
        if (build_follower_repair_handoff) {
            const UnitOrder& build_order = unit.order_queue.front();
            Unit* shared_target = get_unit(
                formation_leader->order_queue.front().target_uid);
            const f32 build_range = service_range_to_(unit, build_order.unit_type);
            if (shared_target && shared_target->is_alive &&
                shared_target->is_building &&
                shared_target->spawn_tick == tick_ &&
                shared_target->unit_type == build_order.unit_type &&
                dist2(unit.x, unit.y, build_order.x, build_order.y) <=
                    build_range * build_range) {
                const f32 match_radius = std::max(
                    1.0f, shared_target->radius + unit.radius);
                build_target_matches_waypoint =
                    dist2(shared_target->x, shared_target->y,
                          build_order.x, build_order.y) <=
                    match_radius * match_radius;
            }
        }
        // y.c(GUARD/FOLLOW) unconditionally clears ad.d after it
        // accepts the live-target waypoint. Keep the formation link
        // for speed/reverse handling, but steer along this unit's
        // own path for the remainder of the object update.
        if ((active_type == WeaponType::GUARD &&
             unit.guard_target_uid != 0) ||
            (active_type == WeaponType::FOLLOW &&
             unit.follow_target_uid != 0) ||
            build_follower_repair_handoff) {
            unit.formation_projection_allowed = false;
        }
        if (active_type == WeaponType::BUILD && formation_leader) {
            // y.f(BUILD) keeps ad.d clear unless the leader still has an
            // au.b()-equal BUILD. Completing the leader's repair does not
            // reopen projection or discard the follower's private path.
            const UnitOrder& build = unit.order_queue.front();
            const UnitOrder* lead = formation_leader->order_queue.empty()
                ? nullptr : &formation_leader->order_queue.front();
            if (!lead || lead->type != WeaponType::BUILD ||
                lead->unit_type != build.unit_type ||
                lead->build_tech_level != build.build_tech_level ||
                lead->target_uid != build.target_uid ||
                std::abs(lead->x - build.x) > 1.0f ||
                std::abs(lead->y - build.y) > 1.0f) {
                unit.formation_projection_allowed = false;
            }
        }
        if (active_type == WeaponType::BUILD && unit.is_mobile()) {
            const UnitOrder& build = unit.order_queue.front();
            const f32 range = service_range_to_(unit, build.unit_type);
            const bool activation_requested_path =
                unit.build_path_request_awaits_dispatch;
            unit.build_path_request_awaits_dispatch = false;
            const bool travelling =
                dist2(unit.x, unit.y, build.x, build.y) > range * range;
            if (travelling) {
                if (unit.using_path && !unit.path_target_override_active) {
                    unit.path_target_override_active = true;
                    unit.path_target_override_x = unit.target_x;
                    unit.path_target_override_y = unit.target_y;
                }
                // y.f() publishes the travelling proposal before formation
                // projection, including a follower leaving nano range while
                // its matching leader is still placing the structure.
                unit.has_move_target = true;
                unit.arrived = false;
                unit.target_x = build.x;
                unit.target_y = build.y;
            } else {
                // y.f() leaves k false within placement range, before the
                // generic controller can retry an exhausted truncated path.
                // Preserve av/aw while the later build phase aims or waits.
                unit.has_move_target = false;
                unit.arrived = true;
                unit.movement_prediction_enabled = false;
                unit.single_node_path_counter = 0;
            }
            if (!activation_requested_path && travelling &&
                unit.repath_cooldown_frames > 90.0f) {
                // y.f() caps s before the generic controller can request
                // another path, including coast turns after aU was lost
                // during loading. A movement delay is independent of s.
                unit.repath_cooldown_frames = 90.0f;
            }
            if (range > 58.0f &&
                dist2(unit.x, unit.y, build.x, build.y) > range * range) {
                // y.f() publishes n before the common controller chooses
                // formation projection, a line path or a grid request. LOAD
                // later inherits this allowance even if BUILD never requested
                // a private path. Short-range BUILD retains the previous n.
                unit.path_request_target_size =
                    service_path_target_size_(unit, build.unit_type);
            }
        }
        // y.i(REPAIR/RECLAIM) is the one service handler that does not
        // clear ad.d outright. It clears it only when the leader is
        // already within u(target)+80 of the service point, when the
        // leader has no active waypoint, or when the leader's waypoint
        // is not au.b()-equal to this one. A group ordered onto a single
        // distant building therefore keeps projecting off its leader and
        // only the leader ever asks the path engine for a route, which is
        // why the followers' y.o/y.p still name a much older request.
        if ((active_type == WeaponType::REPAIR ||
             active_type == WeaponType::RECLAIM) &&
            unit.target_uid != 0) {
            const UnitOrder& service = unit.order_queue.front();
            World* service_world = order_target_world_(
                unit.target_team_id, other_worlds);
            Unit* service_target = service_world
                ? service_world->get_unit(unit.target_uid) : nullptr;
            if (unit.is_mobile() && service_target &&
                service_target->is_alive && service_target->parent_uid == 0 &&
                !service_target_in_range_(unit, *service_target)) {
                // y.i() updates the live allowance on every travelling turn,
                // including projection and an older pending request. Only a
                // new request may replace its captured solver allowance.
                unit.path_request_target_size =
                    service_path_target_size_(unit, service_target->unit_type);
            }
            if (service_target && service_target->is_alive &&
                service_target->parent_uid == 0 &&
                service_target_in_range_(unit, *service_target)) {
                // y.i() leaves k clear before the generic path helper when
                // nano range is reached. The split order pass must stop
                // before it can consume a nearby retained av/aw node.
                unit.has_move_target = false;
                unit.arrived = true;
                unit.movement_prediction_enabled = false;
                // k(float) clears q whenever dispatch leaves k false,
                // before a solved aU can increment it again in b(l).
                unit.single_node_path_counter = 0;
            }
            if (service_target && service_target->is_alive &&
                !service_target_in_range_(unit, *service_target) &&
                unit.repath_cooldown_frames > 90.0f) {
                // y.i() caps the preceding request before y.a() can submit
                // a new one. A request made later in this turn keeps 500.
                unit.repath_cooldown_frames = 90.0f;
            }
            // au.g()/au.h() read the live target's position whenever the
            // waypoint still has one, and the stored point otherwise.
            const f32 point_x = service_target ? service_target->x : service.x;
            const f32 point_y = service_target ? service_target->y : service.y;
            bool projects = formation_leader != nullptr;
            if (projects) {
                const f32 reach = service_range_to_(
                    unit, service_target ? service_target->unit_type : Str{}) + 80.0f;
                projects = dist2(formation_leader->x, formation_leader->y,
                                 point_x, point_y) >= reach * reach;
            }
            if (projects) {
                if (formation_leader->order_queue.empty()) {
                    projects = false;
                } else {
                    // au.b(): same waypoint type, same produced type, same
                    // target object, and the same point to within one unit.
                    const UnitOrder& lead =
                        formation_leader->order_queue.front();
                    projects =
                        lead.type == service.type &&
                        lead.unit_type == service.unit_type &&
                        lead.build_tech_level == service.build_tech_level &&
                        lead.target_uid == service.target_uid &&
                        lead.target_team_id == service.target_team_id &&
                        std::abs(lead.x - service.x) <= 1.0f &&
                        std::abs(lead.y - service.y) <= 1.0f;
                }
            }
            if (!projects) {
                unit.formation_projection_allowed = false;
            }
            if (unit.is_mobile() && service_target &&
                service_target->is_alive && service_target->parent_uid == 0 &&
                !service_target_in_range_(unit, *service_target)) {
                // y.i() republishes the travelling proposal before the
                // common controller can consume a retained nearby node.
                begin_service_chase_(unit, *service_target);
            }
        }
        if (active_type == WeaponType::PATROL) {
            unit.patrol_logical_target_active = false;
            unit.patrol_logical_target_x = unit.order_queue.front().x;
            unit.patrol_logical_target_y = unit.order_queue.front().y;
            // The av.j dispatch calls y.aB() — which detaches the unit from
            // its formation outright rather than merely suppressing ad.d for
            // the update — but only on the branch it takes while still
            // travelling, i.e. when the squared distance to the waypoint is
            // at or beyond the arrival tolerance. A unit already sitting on
            // its patrol point keeps ae/ak/al and its leader link, which is
            // what the next group command's leader preference reads. The
            // previous k or a current/previous collision widens this to the
            // same 80/70 used by the patrol dispatch; otherwise it is 30.
            const UnitOrder& patrol = unit.order_queue.front();
            const bool recent_patrol_collision =
                unit.collision_partner_tick == tick_ ||
                unit.collision_partner_tick == tick_ - 1;
            const bool formation_failure_holds_patrol =
                unit.formation_leader_uid != 0 &&
                !unit.has_move_target && !unit.using_path &&
                unit.formation_path_failure_frames >= 300.0f;
            const f32 travel_radius =
                (formation_failure_holds_patrol ||
                 !patrol_dispatch_was_moving || recent_patrol_collision)
                    ? (unit.order_queue.size() == 1 ? 80.0f : 70.0f)
                    : 30.0f;
            // y.k clears k before this dispatch. The in-radius patrol
            // branch leaves it false unless R or q(3) publishes a new
            // proposal later; an old aU does not preserve the prior k.
            unit.has_move_target = false;
            unit.arrived = true;
            if (dist2(unit.x, unit.y, patrol.x, patrol.y) >=
                    travel_radius * travel_radius) {
                unit.path_request_low_priority = true;
                // The ordered-point proposal publishes n=0 before the
                // optional R chase. A pending worker keeps its separately
                // captured allowance even when this turn stops in range.
                unit.path_request_target_size = 0;
                unit.formation_leader_uid = 0;
                unit.formation_is_leader = false;
                unit.formation_path_activation_pending = false;
                unit.formation_offset_x = 0.0f;
                unit.formation_offset_y = 0.0f;
                unit.formation_path_fallback_frames = 0.0f;
                // MOVE-family dispatch publishes its waypoint before the
                // target helper. A blocked weapon arc leaves this proposal
                // intact; only i(R) can clear k inside follow range.
                if (unit.using_path && !unit.path_target_override_active) {
                    unit.path_target_override_active = true;
                    unit.path_target_override_x = unit.target_x;
                    unit.path_target_override_y = unit.target_y;
                }
                unit.target_x = patrol.x;
                unit.target_y = patrol.y;
                unit.has_move_target = true;
                unit.arrived = false;
            }

            // Passive combat does not replace a PATROL waypoint's path. Once
            // a weapon slot has acquired the main target and that target is
            // outside the weapon follow range, y.c() republishes the live
            // target through y.l/y.m while av[0] keeps steering the old
            // patrol node. Capture that node before exposing the target so
            // the physical pass remains on the existing path.
            Unit* patrol_target = nullptr;
            World* patrol_target_world = nullptr;
            // y.a(R,false) validates the same squared engagement range in
            // order dispatch and in the later passive combat update.
            const f32 patrol_engagement_range = engagement_range_(unit);
            if (unit.target_uid != 0 && unit.target_team_id >= 0 &&
                unit.behavior != Behavior::HOLD_FIRE &&
                unit.behavior != Behavior::RETURN_FIRE &&
                has_weapon(unit.weapon_type)) {
                patrol_target_world = order_target_world_(
                    unit.target_team_id, other_worlds);
                patrol_target = patrol_target_world
                    ? patrol_target_world->get_unit(unit.target_uid) : nullptr;
                if (!patrol_target || !patrol_target->is_alive ||
                    patrol_target->parent_uid != 0 ||
                    patrol_target_world == this ||
                    !is_enemy_with(*patrol_target_world) ||
                    !can_target_(unit, *patrol_target) ||
                    dist2(unit.x, unit.y, patrol_target->x, patrol_target->y) >=
                        patrol_engagement_range * patrol_engagement_range) {
                    patrol_target = nullptr;
                    patrol_target_world = nullptr;
                }
            }
            if (!patrol_target &&
                dist2(unit.x, unit.y, patrol.x, patrol.y) >=
                    travel_radius * travel_radius) {
                // y.a() publishes the patrol point before checking R. Keep
                // the retained av[0] for physical steering when a live
                // target is rejected, including this turn's range boundary.
                if (unit.using_path && !unit.path_target_override_active) {
                    unit.path_target_override_active = true;
                    unit.path_target_override_x = unit.target_x;
                    unit.path_target_override_y = unit.target_y;
                }
                unit.target_x = patrol.x;
                unit.target_y = patrol.y;
                unit.has_move_target = true;
                unit.arrived = false;
                unit.path_request_target_size = 0;
            }
            if (patrol_target) {
                const f32 target_distance_sq = dist2(
                    unit.x, unit.y, patrol_target->x, patrol_target->y);
                if (unit.formation_leader_uid != 0 && formation_leader) {
                    const f32 leader_target_distance_sq = dist2(
                        formation_leader->x, formation_leader->y,
                        patrol_target->x, patrol_target->y);
                    // y.a(delta,R,ad,true) disables projection near either
                    // participant and when the leader shares this target.
                    if ((target_distance_sq < 490000.0f &&
                         (target_distance_sq < 48400.0f ||
                          leader_target_distance_sq < 48400.0f ||
                          (unit.attack_range_adds_radii &&
                           leader_target_distance_sq < 270400.0f))) ||
                        formation_leader->target_uid == patrol_target->uid) {
                        unit.formation_projection_allowed = false;
                    }
                    if (unit.formation_projection_allowed) {
                        unit.formation_projection_disabled_frames = 0.0f;
                    } else {
                        unit.formation_projection_disabled_frames +=
                            combat_delta_frames_;
                    }
                } else {
                    // y.a() uses the no-formation sentinel while a passive
                    // patrol chase owns the active path. This value survives
                    // a later formation command and controls its ATTACK
                    // handoff age.
                    unit.formation_projection_disabled_frames = 500.0f;
                }
                const f32 follow_range = attack_follow_stop_range(unit, *patrol_target);
                // y.a(delta, R, ad, true) refreshes y.S to 10 for a patrol
                // target while it remains inside the weapon's o(target)
                // range. The combat pass subtracts one frame afterward,
                // leaving the native visible value at 9.
                f32 patrol_attack_range = unit.weapon_range;
                if (unit.attack_range_adds_radii) {
                    patrol_attack_range += unit.radius + patrol_target->radius;
                }
                if (target_distance_sq <
                    patrol_attack_range * patrol_attack_range) {
                    unit.target_scan_timer_frames = 10.0f;
                }
                const auto weapon_eligibility =
                    attack_target_weapon_eligibility(*this, unit, *patrol_target);
                const bool needs_approach = !weapon_eligibility.allowed_now &&
                    weapon_eligibility.allowed_without_range;
                if (unit.bomber_attack_movement ||
                    target_distance_sq >= follow_range * follow_range ||
                    needs_approach) {
                    if (!activated_order_this_update &&
                        unit.movement_activation_delay_frames <= 0.0f &&
                        unit.repath_cooldown_frames > 90.0f) {
                        // y.a(delta,R,ad,true) caps s only when it publishes
                        // a chase. An in-range target clears k while keeping
                        // the retained path and its full request cooldown.
                        unit.repath_cooldown_frames = 90.0f;
                    }
                    const auto [attack_x, attack_y] = bomber_attack_target(
                        unit, target_distance_sq, patrol_target->x, patrol_target->y);
                    unit.patrol_logical_target_active = true;
                    unit.patrol_logical_target_x = attack_x;
                    unit.patrol_logical_target_y = attack_y;
                    unit.path_request_target_size =
                        attack_path_target_size(unit, patrol_target);
                    if (!unit.formation_projection_allowed &&
                        unit.formation_projection_disabled_frames < 120.0f &&
                        target_distance_sq < 810000.0f) {
                        // The native short formation handoff publishes j and
                        // s=0.1 even with no av node. Keep that direct proposal
                        // alive without submitting a private path yet.
                        unit.repath_cooldown_frames = 0.1f;
                        unit.repath_cooldown_order_start = -1.0f;
                        if (!unit.using_path) {
                            unit.direct_attack_chase_this_update = true;
                            unit.has_deferred_navigation_target = true;
                            unit.deferred_navigation_target_x = attack_x;
                            unit.deferred_navigation_target_y = attack_y;
                            unit.target_x = attack_x;
                            unit.target_y = attack_y;
                            unit.has_move_target = true;
                            unit.arrived = false;
                            unit.movement_activation_delay_frames = 0.0f;
                        }
                    } else if (!unit.formation_projection_allowed &&
                               unit.formation_projection_disabled_frames >= 120.0f &&
                               unit.repath_cooldown_frames <= 0.1f) {
                        // s was counted down before the native handler. Once
                        // ai reaches 120, its old 0.1 no longer gets rearmed.
                        unit.repath_cooldown_frames = 0.0f;
                    }
                    if (unit.bomber_attack_movement) {
                        if (unit.using_path && !unit.path_target_override_active) {
                            unit.path_target_override_active = true;
                            unit.path_target_override_x = unit.target_x;
                            unit.path_target_override_y = unit.target_y;
                        }
                        unit.target_x = attack_x;
                        unit.target_y = attack_y;
                        unit.has_move_target = true;
                        unit.arrived = false;
                    }
                    // y.c() uses y.o(8) when a path has at least nine
                    // remaining nodes, otherwise the current destination.
                    // C++ stores av[1..] in waypoints and keeps av[0] in
                    // target_x/y, so y.o(8) is waypoints[current + 7].
                    // The 300-unit test is per axis, rather than radial.
                    if (unit.formation_leader_uid != 0 &&
                        unit.formation_projection_allowed &&
                        unit.using_path && !unit.path_request_pending &&
                        unit.repath_cooldown_frames > 30.0f &&
                        !unit.waypoints.empty()) {
                        const std::size_t lookahead = std::min(
                            static_cast<std::size_t>(
                                std::max(0, unit.current_waypoint + 7)),
                            unit.waypoints.size() - 1);
                        const auto& path_probe = unit.waypoints[lookahead];
                        const bool target_drifted =
                            std::abs(unit.last_request_x - path_probe.first) >
                                300.0f ||
                            std::abs(unit.last_request_y - path_probe.second) >
                                300.0f;
                        if (target_drifted) {
                            unit.repath_cooldown_frames = 30.0f;
                        }
                    }
                    // y.a(lVar,f,au,ad) also compares the retained request
                    // point (y.o/y.p) with the logical combat target before
                    // submitting a replacement.  Its r field is reset to
                    // 150 by y.k(), so a target that drifts over 150 units
                    // on either axis brings s down to 30 even when the
                    // current patrol node itself has not drifted.  This is
                    // the handoff that makes a nearly exhausted one-node
                    // patrol path request on the same tick as the JAR.
                    // Only ct() / aV() enable the m() - 1 test. All active
                    // orders also apply y.r's default 150-unit drift limit.
                    const bool uses_m_drift_threshold =
                        unit.move_type == MoveType::AIR ||
                        unit.attack_range_adds_radii;
                    const f32 logical_target_drift_threshold =
                        uses_m_drift_threshold
                            ? std::min(150.0f,
                                       std::max(0.0f, unit.weapon_range - 1.0f))
                            : 150.0f;
                    const bool logical_target_drifted =
                        std::abs(unit.last_request_x - attack_x) >
                            logical_target_drift_threshold ||
                        std::abs(unit.last_request_y - attack_y) >
                            logical_target_drift_threshold;
                    const bool logical_order_target_drifted =
                        std::abs(unit.last_request_x - attack_x) > 150.0f ||
                        std::abs(unit.last_request_y - attack_y) > 150.0f;
                    if (!unit.path_request_pending &&
                        logical_order_target_drifted &&
                        unit.repath_cooldown_frames > 30.0f) {
                        unit.repath_cooldown_frames = 30.0f;
                    }
                    // y.o(8) can keep a patrol chase live without replacing
                    // its current path. Once that request's cooldown reaches
                    // zero, y.c() submits a preserve-active-path refresh in
                    // the order dispatch before y.k() consumes the frame's
                    // cooldown delta. Treat a value that will reach zero in
                    // the physical pass as already due here.
                    if (unit.using_path && !unit.path_request_pending &&
                        logical_target_drifted &&
                        (unit.repath_cooldown_frames == 0.0f ||
                         (unit.repath_cooldown_order_start > 0.0f &&
                          unit.repath_cooldown_frames <=
                              std::max(delta_frames, combat_delta_frames_)))) {
                        // y.k() decrements s before the PATROL handler. The
                        // JAR keeps av/aw attached to the unit while this
                        // refresh is queued, even when aw==1: y.v still
                        // records the original path generation's node count.
                        // A clear-line request can still replace the path
                        // immediately inside move_unit_to_().
                        move_unit_to_(
                            unit, attack_x, attack_y,
                            attack_path_target_size(unit, patrol_target),
                            true, true);
                        // A clear-line preserve request installs its new
                        // first node immediately. The old patrol override is
                        // only valid while an asynchronous grid request is
                        // pending; otherwise it would steer one extra frame
                        // toward the replaced path's former node.
                        if (!unit.path_request_pending) {
                            unit.path_target_override_active = false;
                        }
                    }
                    const bool patrol_proposal_steers_without_path =
                        target_distance_sq < 810000.0f &&
                        (unit.move_type == MoveType::AIR ||
                         unit.attack_range_adds_radii ||
                         (!unit.formation_projection_allowed &&
                          unit.formation_projection_disabled_frames < 120.0f));
                    const bool patrol_repath_due =
                        unit.repath_cooldown_frames == 0.0f ||
                        (unit.repath_cooldown_order_start > 0.0f &&
                         unit.repath_cooldown_frames <=
                             std::max(delta_frames, combat_delta_frames_));
                    if (unit.using_path) {
                        if (!unit.path_target_override_active) {
                            unit.path_target_override_active = true;
                            unit.path_target_override_x = unit.target_x;
                            unit.path_target_override_y = unit.target_y;
                        }
                        unit.target_x = attack_x;
                        unit.target_y = attack_y;
                        unit.has_move_target = true;
                        unit.arrived = false;
                    } else if (!unit.path_request_pending &&
                               patrol_repath_due) {
                        // A replacement PATROL clears av/aw but retains the
                        // passive main target. y.c() immediately submits a
                        // target-sized chase request in that case, rather
                        // than letting the generic AIR branch steer to the
                        // patrol point. AIR adopts its one-node path on this
                        // same object update, so it must not be delayed.
                        move_unit_to_async_(
                            unit, attack_x, attack_y,
                            attack_path_target_size(unit, patrol_target));
                        unit.path_incomplete = false;
                        unit.path_request_target_size =
                            attack_path_target_size(unit, patrol_target);
                        unit.repath_cooldown_frames = 500.0f;
                        unit.movement_activation_delay_frames =
                            patrol_proposal_steers_without_path ? 0.0f : 1.0f;
                        unit.movement_delay_keeps_prediction = false;
                        unit.repath_cooldown_order_start = -1.0f;
                        if (unit.using_path) {
                            unit.path_target_override_active = true;
                            unit.path_target_override_x = unit.target_x;
                            unit.path_target_override_y = unit.target_y;
                        }
                        // a(l,m,n,...) installs av/aw without replacing the
                        // live combat proposal already published by y.c().
                        unit.target_x = attack_x;
                        unit.target_y = attack_y;
                        unit.has_move_target = true;
                    } else if (!unit.path_request_pending) {
                        // With no af, the handler still publishes l/m. Its
                        // j proposal selects whether this turn can steer
                        // while s counts down, independently of the old node.
                        unit.target_x = attack_x;
                        unit.target_y = attack_y;
                        unit.has_move_target = true;
                        unit.arrived = !patrol_proposal_steers_without_path;
                        if (patrol_proposal_steers_without_path) {
                            unit.movement_activation_delay_frames = 0.0f;
                            unit.movement_delay_keeps_prediction = false;
                        }
                    } else {
                        // y.a(delta,R,ad,true) publishes k/l/m before the
                        // generic controller waits for aU. A stopped patrol
                        // point cannot erase that live target proposal.
                        unit.target_x = attack_x;
                        unit.target_y = attack_y;
                        unit.has_move_target = true;
                        unit.arrived = false;
                    }
                } else if (unit.has_move_target && weapon_eligibility.allowed_now) {
                    // Once the passive target is back inside follow range,
                    // y.c() drops only k/l/m. The patrol av/aw chain remains
                    // available for the next patrol leg or a later chase.
                    unit.has_move_target = false;
                    unit.arrived = true;
                    unit.path_incomplete = false;
                    unit.path_request_target_size = 0;
                    patrol_passive_target_stopped_this_update = true;
                }
                if (!unit.bomber_attack_movement &&
                    target_distance_sq < follow_range * follow_range &&
                    weapon_eligibility.allowed_now) {
                    patrol_passive_target_in_range_this_update = true;
                }
            }
            if (unit.formation_leader_uid != 0 && formation_leader &&
                formation_leader->target_uid != 0) {
                // PATROL clears ad.d after the target helper whenever its
                // linked leader retains any R, including a different enemy.
                unit.formation_projection_allowed = false;
            }
        }
        if (!unit.order_queue.empty() &&
            unit.order_queue.front().type == WeaponType::PATROL &&
            !patrol_passive_target_stopped_this_update &&
            unit.using_path && !unit.path_request_pending &&
            !unit.has_move_target && !unit.path_incomplete &&
            unit.target_uid == 0) {
            // y.a(...) republishes the ordered patrol point when a stopped
            // unit is still outside its patrol tolerance, but it does not
            // discard an existing av/aw chain. Keep the current path node as
            // the physical target while exposing the ordered point as k/l/m.
            const UnitOrder& patrol = unit.order_queue.front();
            const bool recent_patrol_collision =
                unit.collision_partner_tick == tick_ ||
                unit.collision_partner_tick == tick_ - 1;
            const f32 republish_radius =
                (!patrol_dispatch_was_moving || recent_patrol_collision)
                    ? (unit.order_queue.size() == 1 ? 80.0f : 70.0f)
                    : 30.0f;
            if (dist2(unit.x, unit.y, patrol.x, patrol.y) >=
                republish_radius * republish_radius) {
                if (!unit.path_target_override_active) {
                    unit.path_target_override_active = true;
                    if (unit.current_waypoint >= 0 &&
                        unit.current_waypoint <
                            static_cast<i32>(unit.waypoints.size())) {
                        const auto& physical =
                            unit.waypoints[unit.current_waypoint];
                        unit.path_target_override_x = physical.first;
                        unit.path_target_override_y = physical.second;
                    } else {
                        unit.path_target_override_x = unit.target_x;
                        unit.path_target_override_y = unit.target_y;
                    }
                }
                unit.target_x = patrol.x;
                unit.target_y = patrol.y;
                unit.has_move_target = true;
                unit.arrived = false;
            }
        }
        const f32 build_range = service_range_to_(
            unit, unit.order_queue.front().unit_type);
        const bool build_private_path =
            active_type == WeaponType::BUILD &&
            unit.spawn_delay_frames == 0.0f &&
            (!formation_leader || !unit.formation_projection_allowed) &&
            dist2(unit.x, unit.y, unit.order_queue.front().x,
                  unit.order_queue.front().y) >
                build_range * build_range;
        if (active_type == WeaponType::BUILD &&
            unit.single_node_path_counter > 3 &&
            dist2(unit.x, unit.y, unit.order_queue.front().x,
                  unit.order_queue.front().y) > build_range * build_range) {
            // y.f() gives up on the next BUILD dispatch after the fourth
            // one-node result. That result still turns the body when adopted.
            complete_current_order_(unit, other_worlds, false, true);
            unit.movement_prediction_enabled = false;
            return false;
        }
        if (build_private_path &&
            !unit.using_path && !unit.path_request_pending) {
            // The generic am.cl gate steers directly while factory launch
            // delay is positive. Only after it clears can BUILD wait for a
            // private node or request a path.
            // y.f() publishes k/l/m even when the preceding update was in
            // construction range. Without af, generic navigation coasts
            // while s counts down; it does not bypass the request cooldown.
            const UnitOrder& build = unit.order_queue.front();
            unit.has_move_target = true;
            unit.arrived = false;
            unit.target_x = build.x;
            unit.target_y = build.y;
            unit.movement_activation_delay_frames = 1.0f;
            unit.movement_delay_keeps_prediction = false;
        }
        if (build_private_path &&
            !build_target_matches_waypoint &&
            unit.has_move_target &&
            !unit.using_path && !unit.path_request_pending &&
            unit.repath_cooldown_frames == 0.0f) {
            // y's generic controller requests the placement route whenever
            // BUILD has no physical node and cannot project off its leader.
            // This also covers a former follower after the leader link is
            // cleared. The requesting update coasts on its old heading.
            const UnitOrder& build_order = unit.order_queue.front();
            move_unit_to_async_(unit, build_order.x, build_order.y,
                                service_path_target_size_(unit, build_order.unit_type));
            unit.movement_activation_delay_frames = 1.0f;
            unit.path_request_prediction_suppressed = true;
            unit.movement_prediction_enabled = false;
            // The request's 500-frame charge is visible for this frame; the
            // next object update applies the normal 90-frame follower cap.
            unit.repath_cooldown_order_start = -1.0f;
        }
    }
    if (!unit.order_queue.empty()) {
        unit.active_order_elapsed_frames += delta_frames;
    }
    const bool formation_patrol_failure_hold =
        !unit.order_queue.empty() &&
        unit.order_queue.front().type == WeaponType::PATROL &&
        unit.formation_leader_uid != 0 &&
        !unit.has_move_target && !unit.using_path &&
        unit.formation_path_failure_frames >= 300.0f;
    const auto single_patrol_holds_position = [&]() {
        if (unit.order_queue.size() != 1) {
            return false;
        }
        const UnitOrder& active = unit.order_queue.front();
        if (active.type != WeaponType::PATROL) return false;
        // A formation follower whose private fallback has exhausted its
        // failure budget keeps the patrol proposal dormant until the
        // formation helper can re-establish a slot. Rebuilding the patrol
        // point here would detach the follower one order phase too early.
        if (formation_patrol_failure_hold) {
            return true;
        }
        // PATROL's ordered-point hold does not inspect the path's u flag.
        // A truncated or retained chain can stay exposed while k is false.
        const bool recent_patrol_collision =
            unit.collision_partner_tick == tick_ ||
            unit.collision_partner_tick == tick_ - 1;
        const f32 hold_radius =
            (!patrol_dispatch_was_moving || recent_patrol_collision)
                ? 80.0f : 30.0f;
        return dist2(unit.x, unit.y, active.x, active.y) <
            hold_radius * hold_radius;
    };
    Unit* recent_order_attacker = nullptr;
    World* recent_order_attacker_world = nullptr;
    u32 recent_attacker_uid = 0;
    bool guard_recent_attacker_from_self = false;
    if (unit.guard_target_uid != 0) {
        World* guarded_world = order_target_world_(
            unit.guard_target_team_id, other_worlds);
        Unit* guarded = guarded_world
            ? guarded_world->get_unit(unit.guard_target_uid) : nullptr;
        if (guarded && guarded->is_alive && guarded->was_attacked &&
            guarded->last_attacker_uid != 0 &&
            guarded->last_damage_age_ms >= 0.0f &&
            guarded->last_damage_age_ms < 2000.0f) {
            recent_attacker_uid = guarded->last_attacker_uid;
        }
    } else if (unit.has_patrol && unit.target_uid == 0 &&
               unit.was_attacked && unit.last_attacker_uid != 0 &&
               unit.last_damage_age_ms >= 0.0f &&
               unit.last_damage_age_ms < 3000.0f) {
        recent_attacker_uid = unit.last_attacker_uid;
    }
    if (recent_attacker_uid != 0) {
        if (unit.uid == recent_attacker_uid) {
            recent_order_attacker = &unit;
            recent_order_attacker_world = this;
        } else {
            recent_order_attacker = get_unit(recent_attacker_uid);
            if (recent_order_attacker) {
                recent_order_attacker_world = this;
            }
            if (!recent_order_attacker) {
                for (World* other : other_worlds) {
                    if (!other) continue;
                    recent_order_attacker = other->get_unit(recent_attacker_uid);
                    if (recent_order_attacker) {
                        recent_order_attacker_world = other;
                        break;
                    }
                }
            }
        }
        // y.c(GUARD/PATROL) passes q(2)'s candidate through
        // b(candidate, true), which rejects allies and targets this
        // unit cannot attack before the candidate can override the
        // current movement proposal.
        if (recent_order_attacker &&
            (!recent_order_attacker->is_alive ||
             recent_order_attacker->parent_uid != 0 ||
             !recent_order_attacker_world ||
             recent_order_attacker_world == this ||
             !is_enemy_with(*recent_order_attacker_world) ||
             !can_target_(unit, *recent_order_attacker))) {
            recent_order_attacker = nullptr;
        }
    }
    // y.c() falls back from the guarded unit's q(2) result to this
    // unit's own q(2) result when the first candidate is absent or
    // incompatible. The JAR q(float) helper only checks its cached
    // attacker age, so do not require was_attacked here.
    if (!recent_order_attacker && unit.guard_target_uid != 0 &&
        unit.last_attacker_uid != 0 &&
        unit.last_damage_age_ms >= 0.0f &&
        unit.last_damage_age_ms < 2000.0f) {
        guard_recent_attacker_from_self = true;
        recent_attacker_uid = unit.last_attacker_uid;
        if (unit.uid == recent_attacker_uid) {
            recent_order_attacker = &unit;
            recent_order_attacker_world = this;
        } else {
            recent_order_attacker = get_unit(recent_attacker_uid);
            if (recent_order_attacker) {
                recent_order_attacker_world = this;
            }
            if (!recent_order_attacker) {
                for (World* other : other_worlds) {
                    if (!other) continue;
                    recent_order_attacker =
                        other->get_unit(recent_attacker_uid);
                    if (recent_order_attacker) {
                        recent_order_attacker_world = other;
                        break;
                    }
                }
            }
        }
        if (recent_order_attacker &&
            (!recent_order_attacker->is_alive ||
             recent_order_attacker->parent_uid != 0 ||
             !recent_order_attacker_world ||
             recent_order_attacker_world == this ||
             !is_enemy_with(*recent_order_attacker_world) ||
             !can_target_(unit, *recent_order_attacker))) {
            recent_order_attacker = nullptr;
        }
    }

    const bool recent_order_chase_active =
        recent_order_attacker != nullptr && unit.is_mobile() &&
        unit.behavior != Behavior::HOLD_FIRE &&
        unit.behavior != Behavior::RETURN_FIRE;
    bool guard_recent_order_candidate =
        recent_order_chase_active && unit.guard_target_uid != 0;
    if (recent_order_chase_active &&
        !guard_recent_order_candidate) {
        const bool waypoint_patrol_chase =
            unit.has_patrol && !unit.order_queue.empty() &&
            unit.order_queue.front().type == WeaponType::PATROL;
        f32 planned_x = unit.target_x;
        f32 planned_y = unit.target_y;
        if (waypoint_patrol_chase) {
            // q(3)'s patrol branch only caps s at 90. The generic helper
            // applies r=150 to its previous request point, independently of
            // the patrol point that dispatch just published through l/m.
            planned_x = unit.last_request_x;
            planned_y = unit.last_request_y;
        } else if (!unit.waypoints.empty()) {
            planned_x = unit.waypoints.back().first;
            planned_y = unit.waypoints.back().second;
        }
        if (unit.repath_cooldown_frames > 90.0f) {
            // k(float) decrements s before q(3) applies this cap. A
            // two-frame update can already bring 91 below the cap.
            const f32 unapplied_patrol_delta = waypoint_patrol_chase &&
                unit.repath_cooldown_frames == unit.repath_cooldown_order_start
                    ? combat_delta_frames_ : 0.0f;
            unit.repath_cooldown_frames = std::min(
                90.0f, std::max(0.0f, unit.repath_cooldown_frames -
                                       unapplied_patrol_delta));
        }
        const f32 drift_limit = waypoint_patrol_chase ? 150.0f : 18.0f;
        const bool chase_drifted =
            std::abs(planned_x - recent_order_attacker->x) > drift_limit ||
            std::abs(planned_y - recent_order_attacker->y) > drift_limit;
        // q(3) caps a patrol's s at 90 while aU is pending. The generic
        // r=150 drift check only runs once that older worker is adopted.
        if (chase_drifted && unit.repath_cooldown_frames > 30.0f &&
            (!waypoint_patrol_chase || !unit.path_request_pending)) {
            unit.repath_cooldown_frames = 30.0f;
        }
        const bool patrol_recent_chase_path =
            unit.has_patrol && !unit.order_queue.empty() &&
            unit.order_queue.front().type == WeaponType::PATROL &&
            unit.target_uid == 0 && unit.using_path &&
            !unit.path_request_pending &&
            (unit.formation_leader_uid == 0 ||
             !unit.formation_projection_allowed);
        if (patrol_recent_chase_path) {
            const f32 refresh_drift_limit =
                unit.move_type == MoveType::AIR || unit.attack_range_adds_radii
                    ? std::min(150.0f,
                               std::max(0.0f, unit.weapon_range - 1.0f))
                    : 150.0f;
            const bool request_target_drifted =
                std::abs(unit.last_request_x - recent_order_attacker->x) >
                    refresh_drift_limit ||
                std::abs(unit.last_request_y - recent_order_attacker->y) >
                    refresh_drift_limit;
            const bool patrol_refresh_due =
                unit.repath_cooldown_frames == 0.0f ||
                (unit.repath_cooldown_order_start > 0.0f &&
                 unit.repath_cooldown_frames <=
                     std::max(delta_frames, combat_delta_frames_));
            if (patrol_refresh_due && request_target_drifted) {
                // q(3) replaces l/m and supplies j without discarding av.
                // With a captured node, s==0 only requests a replacement
                // after the generic r/ct()/aV() drift gates accept it.
                move_unit_to_(
                    unit, recent_order_attacker->x,
                    recent_order_attacker->y, 0,
                    true, true);
                if (unit.using_path && !unit.path_request_pending) {
                    unit.path_target_override_active = true;
                    unit.path_target_override_x = unit.target_x;
                    unit.path_target_override_y = unit.target_y;
                }
                unit.path_incomplete = false;
                unit.path_request_target_size = 0;
                unit.repath_cooldown_frames = 500.0f;
                unit.movement_activation_delay_frames = 0.0f;
                unit.repath_cooldown_order_start = -1.0f;
            }
        }
        const bool missing_patrol_path_due = waypoint_patrol_chase &&
            !unit.using_path && !unit.path_request_pending &&
            // q(3) publishes j/l/m without choosing the private controller.
            // A valid ad and adVar.d still select the formation helper first.
            (unit.formation_leader_uid == 0 ||
             !unit.formation_projection_allowed) &&
            (unit.repath_cooldown_frames == 0.0f ||
             (unit.repath_cooldown_order_start > 0.0f &&
              unit.repath_cooldown_frames <= combat_delta_frames_));
        if (missing_patrol_path_due ||
            (!waypoint_patrol_chase &&
             (!unit.recent_order_chase || !unit.has_move_target ||
              chase_drifted) && unit.repath_cooldown_frames == 0.0f)) {
            unit.using_path = false;
            unit.waypoints.clear();
            unit.current_waypoint = 0;
            if (waypoint_patrol_chase) {
                // The common helper captures aE() before requesting. q(3)
                // supplies j even for ground units, so a new grid route is
                // queued while this turn still steers at the attacker.
                move_unit_to_async_(
                    unit, recent_order_attacker->x, recent_order_attacker->y, 0);
                if (unit.using_path && !unit.path_request_pending) {
                    // The common helper sampled aE() before this request.
                    // Its new av is visible, while q(3)'s j still selects
                    // the attacker for this turn's steering and recovery.
                    unit.has_deferred_navigation_target = true;
                    unit.deferred_navigation_target_x = recent_order_attacker->x;
                    unit.deferred_navigation_target_y = recent_order_attacker->y;
                }
                unit.repath_cooldown_order_start = -1.0f;
                unit.movement_activation_delay_frames = 0.0f;
                unit.movement_delay_keeps_prediction = false;
            } else {
                move_unit_to(unit, recent_order_attacker->x, recent_order_attacker->y);
            }
        }
        if (waypoint_patrol_chase) {
            // y.a(PATROL) publishes q(3)'s attacker before the generic path
            // helper, including the first request without an existing av
            // node. Retain the physical node separately so the later combat
            // scan cannot restore the waypoint point or cap the new request.
            if (unit.using_path && !unit.path_target_override_active) {
                unit.path_target_override_active = true;
                unit.path_target_override_x = unit.target_x;
                unit.path_target_override_y = unit.target_y;
            }
            unit.patrol_logical_target_active = true;
            // q(3) supplies a live j proposal after the ordered-point
            // dispatch. A coast marker left by an earlier idle patrol turn
            // cannot suppress this turn's formation or direct helper.
            unit.movement_activation_delay_frames = 0.0f;
            unit.movement_delay_keeps_prediction = false;
            unit.patrol_logical_target_x = recent_order_attacker->x;
            unit.patrol_logical_target_y = recent_order_attacker->y;
            unit.target_x = recent_order_attacker->x;
            unit.target_y = recent_order_attacker->y;
            unit.has_move_target = true;
            unit.arrived = false;
        }
        unit.recent_order_chase = true;
    } else if (unit.recent_order_chase) {
        // q(2/3) only stops replacing the order's movement proposal.
        // The adopted av/aw chain remains active and the normal
        // GUARD/PATROL handler below resumes from that path.
        unit.recent_order_chase = false;
    }
    if (patrol_logical_target_was_active &&
        !unit.patrol_logical_target_active &&
        unit.has_move_target && !unit.arrived &&
        unit.using_path && !unit.path_request_pending &&
        unit.repath_cooldown_frames > 30.0f &&
        unit.weapon_range <= 130.0f &&
        !unit.order_queue.empty() &&
        unit.order_queue.front().type == WeaponType::PATROL) {
        // Apply the generic drift gate after q(3) has had its chance to
        // replace the patrol proposal. Only a return to the waypoint point
        // compares that point with the retained request's r=150 window.
        const UnitOrder& patrol_order = unit.order_queue.front();
        const bool patrol_point_drifted =
            std::abs(unit.last_request_x - patrol_order.x) > 150.0f ||
            std::abs(unit.last_request_y - patrol_order.y) > 150.0f;
        if (patrol_point_drifted) {
            unit.repath_cooldown_frames = 30.0f;
        }
    }

    const UnitOrder* explicit_attack_order =
        !unit.order_queue.empty() &&
        unit.order_queue.front().type == WeaponType::ATTACK
            ? &unit.order_queue.front() : nullptr;
    if (explicit_attack_order && unit.weapon_type == WeaponType::ATTACK &&
        explicit_attack_order->target_uid != 0 &&
        explicit_attack_order->target_team_id >= 0) {
        World* attacked_world = order_target_world_(
            explicit_attack_order->target_team_id, other_worlds);
        Unit* attacked = attacked_world
            ? attacked_world->waypoint_target_(
                  explicit_attack_order->target_uid, unit.bomber_attack_movement)
            : nullptr;
        const bool retained_bomber_retreat =
            unit.bomber_attack_movement && unit.bomber_retreating;
        if (!attacked || ((!attacked->is_alive || attacked_world == this) &&
                          !retained_bomber_retreat)) {
            unit.has_move_target = false;
            unit.arrived = true;
            unit.using_path = false;
            unit.waypoints.clear();
            unit.current_waypoint = 0;
        } else {
            unit.attack_order_target_x = attacked->x;
            unit.attack_order_target_y = attacked->y;
            f32 main_attack_range = unit.weapon_range;
            if (unit.attack_range_adds_radii) {
                main_attack_range += unit.radius + attacked->radius;
            }
            const f32 distance_sq = dist2(
                unit.x, unit.y, attacked->x, attacked->y);
            // y.a(delta,target,ad,false): a linked ATTACK follower
            // stops projecting its leader's path when either member
            // is close to the explicit target. Units whose range adds
            // radii use the wider leader threshold; sharing the same
            // transient main target also disables projection.
            if (unit.formation_leader_uid != 0 &&
                distance_sq < 490000.0f) {
                Unit* leader = formation_unit_(unit.formation_leader_uid);
                if (leader) {
                    const f32 leader_distance_sq = dist2(
                        leader->x, leader->y,
                        attacked->x, attacked->y);
                    const bool leader_has_attack_target =
                        leader->target_uid == attacked->uid &&
                        leader->target_team_id ==
                            attacked_world->team_id_;
                    if (distance_sq < 48400.0f ||
                        leader_distance_sq < 48400.0f ||
                        (unit.attack_range_adds_radii &&
                         leader_distance_sq < 270400.0f) ||
                        leader_has_attack_target) {
                        unit.formation_projection_allowed = false;
                    }
                }
            }
            if (unit.formation_leader_uid == 0) {
                unit.formation_projection_disabled_frames = 500.0f;
            } else if (unit.formation_projection_allowed) {
                unit.formation_projection_disabled_frames = 0.0f;
            } else {
                // The zero-time order pass still advances y.ai by the
                // complete native update delta, including two-frame replay
                // updates. The physical pass must not advance it again.
                unit.formation_projection_disabled_frames += combat_delta_frames_;
            }
            // y.a(delta, target, ad, false) refreshes S to 10 while
            // the explicit ATTACK target is also R and is inside
            // o(target). y.b(engine, delta) decrements it later in
            // the same object update, leaving 9 in the snapshot.
            if (distance_sq < main_attack_range * main_attack_range) {
                const bool can_attack_order_target =
                    attacked->parent_uid == 0 &&
                    can_target_(unit, *attacked);
                // y.a() checks the old R pointer before y.b() validates it.
                // Cargo loading refreshes S once, then the combat pass clears
                // R; later turns let S count down without adopting that cargo.
                const bool is_current_main_target =
                    unit.target_uid == attacked->uid &&
                    unit.target_team_id == attacked_world->team_id_;
                if (is_current_main_target || can_attack_order_target) {
                    if (!is_current_main_target) {
                        unit.target_uid = attacked->uid;
                        unit.target_team_id = attacked_world->team_id_;
                        // y.a(delta,target,ad,false) calls M(-1) when
                        // the explicit ATTACK target replaces R.
                        refresh_all_weapon_aim_offsets(
                            unit, attacked, tick_);
                    }
                    unit.target_scan_timer_frames = 10.0f;
                }
            }
            // y.a(delta,target,ad,false) has no mobility test: a building
            // with an explicit ATTACK waypoint publishes k/l/m and asks for
            // a path exactly like a tank does. It simply never moves, and
            // y.a(FFIZZ) answers from the standing-on-blocked-tile branch.
            const bool factory_exit_motion_hold =
                unit.spawn_delay_frames > 0.0f;
            if (factory_exit_motion_hold) {
                // The launch delay suppresses only path creation. y.a() still
                // refreshes l/m from the live ATTACK target, so the physical
                // pass can keep turning toward that target while stationary.
                unit.target_x = attacked->x;
                unit.target_y = attacked->y;
                unit.has_move_target = true;
                unit.arrived = false;
                if (distance_sq >= attack_follow_stop_range(unit, *attacked) *
                                   attack_follow_stop_range(unit, *attacked) ||
                    attack_target_needs_approach(*this, unit, *attacked)) {
                    unit.path_request_target_size =
                        attack_path_target_size(unit, attacked);
                }
            }
            const bool pending_preserved_attack_coast =
                !activated_order_this_update &&
                unit.path_request_prediction_suppressed &&
                unit.path_request_pending &&
                unit.path_request_preserves_active_path &&
                !unit.using_path;
            // ATTACK dispatch publishes n before generic movement chooses
            // its physical node or coast. The split handoff delay suppresses
            // steering, not this live allowance; the worker keeps its own
            // immutable path_request_solver_target_size.
            const f32 follow_range = attack_follow_stop_range(unit, *attacked);
            if (unit.bomber_attack_movement ||
                distance_sq >= follow_range * follow_range ||
                attack_target_needs_approach(*this, unit, *attacked)) {
                unit.path_request_target_size =
                    attack_path_target_size(unit, attacked);
            }
            // g(ATTACK) still publishes l/m and caps the prior s while the
            // physical helper has no af and only coasts. The split movement
            // delay must not suppress this continuing waypoint dispatch.
            if ((unit.movement_activation_delay_frames <= 0.0f ||
                 pending_preserved_attack_coast) &&
                !factory_exit_motion_hold) {
            // y.a(delta, target, ad, false) uses the same E()-gated
            // stopping radius as attack-move and patrol.
            if (unit.bomber_attack_movement ||
                distance_sq >= follow_range * follow_range ||
                attack_target_needs_approach(*this, unit, *attacked)) {
                const auto [attack_x, attack_y] = bomber_attack_target(
                    unit, distance_sq, attacked->x, attacked->y);
                unit.attack_order_target_x = attack_x;
                unit.attack_order_target_y = attack_y;
                // y.a(delta,target,ad,false) publishes q(target) in n even
                // while s prevents a new request. Pending solvers keep the
                // target size captured when their own request was submitted.
                unit.path_request_target_size =
                    attack_path_target_size(unit, attacked);
                if (unit.bomber_attack_movement) {
                    // y.a(delta,target,ad,false) publishes l/m on every
                    // bomber turn, independently of the retained av[0].
                    if (unit.using_path && !unit.path_target_override_active) {
                        unit.path_target_override_active = true;
                        unit.path_target_override_x = unit.target_x;
                        unit.path_target_override_y = unit.target_y;
                    }
                    unit.target_x = attack_x;
                    unit.target_y = attack_y;
                }
                if (!activated_order_this_update &&
                    unit.repath_cooldown_frames > 90.0f) {
                    unit.repath_cooldown_frames = 90.0f;
                }
                if (!unit.formation_projection_allowed &&
                    unit.formation_projection_disabled_frames <
                        120.0f &&
                    distance_sq < 810000.0f) {
                    // y.a(delta,target,ad,false) sets s=0.1 and j=true
                    // throughout this handoff, including when av[0] exists.
                    // A retained node still steers the physical pass; an
                    // empty chain uses the live l/m proposal without repath.
                    unit.repath_cooldown_frames = 0.1f;
                    unit.repath_cooldown_order_start = -1.0f;
                    if (unit.using_path && unit.has_move_target) {
                        if (!unit.path_target_override_active) {
                            unit.path_target_override_active = true;
                            unit.path_target_override_x = unit.target_x;
                            unit.path_target_override_y = unit.target_y;
                        }
                    } else {
                        unit.direct_attack_chase_this_update = true;
                        unit.has_deferred_navigation_target = true;
                        unit.deferred_navigation_target_x = attack_x;
                        unit.deferred_navigation_target_y = attack_y;
                        unit.target_x = attack_x;
                        unit.target_y = attack_y;
                    }
                } else if (!unit.formation_projection_allowed &&
                           unit.formation_projection_disabled_frames >=
                               120.0f &&
                           unit.repath_cooldown_frames <= 0.1f) {
                    // JAR counts the previous frame's 0.1 down before
                    // running this handler. At ai==120 the refresh
                    // stops, so the generic controller observes zero
                    // and submits the private path in this update.
                    unit.repath_cooldown_frames = 0.0f;
                }
                // A target-size path deliberately ends short of its
                // requested point. The generic path controller measures
                // target drift from y.o/y.p, the retained request
                // coordinates, rather than from that final navigation node.
                // The m()-1 test applies to units whose y controller enables
                // it; an active order also adds y.r, which y.k() resets to
                // 150 each frame.
                const bool uses_m_drift_threshold =
                    unit.move_type == MoveType::AIR ||
                    unit.attack_range_adds_radii;
                const f32 attack_m_drift_threshold =
                    std::max(0.0f, unit.weapon_range - 1.0f);
                const bool attack_m_drifted =
                    uses_m_drift_threshold &&
                    (std::abs(unit.last_request_x - attack_x) >
                         attack_m_drift_threshold ||
                     std::abs(unit.last_request_y - attack_y) >
                         attack_m_drift_threshold);
                const bool attack_order_drifted =
                    explicit_attack_order != nullptr &&
                    (std::abs(unit.last_request_x - attack_x) > 150.0f ||
                     std::abs(unit.last_request_y - attack_y) > 150.0f);
                const bool attack_drifted =
                    attack_m_drifted || attack_order_drifted;
                // y's physical controller chooses the formation helper
                // before testing drift from the live ATTACK proposal. Its
                // fallback route has an independent request point and timer.
                const bool projects_attack_leader =
                    unit.formation_leader_uid != 0 &&
                    unit.formation_projection_allowed;
                // A linked follower can retain y.k (the logical
                // target proposal) after ad.d stops leader-path
                // projection. The generic controller tests aE(),
                // not y.k: without a private path node it must submit
                // a request even though has_move_target is still true.
                const bool linked_private_path_missing =
                    unit.formation_leader_uid != 0 &&
                    !unit.formation_projection_allowed &&
                    unit.formation_projection_disabled_frames >=
                        120.0f &&
                    !unit.using_path &&
                    !unit.path_request_pending;
                if (!projects_attack_leader && !unit.path_request_pending &&
                    attack_order_drifted &&
                    unit.repath_cooldown_frames > 30.0f) {
                    unit.repath_cooldown_frames = 30.0f;
                }
                const bool attack_path_missing =
                    !unit.using_path && !unit.path_request_pending;
                // y.k() has already counted s down before ATTACK dispatch.
                // A positive AIR cooldown still keeps the old physical node;
                // anticipating another delta would refresh one object early.
                const bool attack_repath_due =
                    unit.repath_cooldown_frames == 0.0f;
                if (!projects_attack_leader &&
                    (attack_path_missing || attack_drifted ||
                     linked_private_path_missing) &&
                    attack_repath_due && !unit.path_request_pending &&
                    !unit.formation_rebuild_path_pending) {
                    // y.a(l,m,n,...) can expose the live ATTACK
                    // target without replacing an existing AIR av/aw
                    // chain. Keep av[0] as the physical steering node
                    // while y.l/y.m follow the target. The old native
                    // branch cleared the chain and incorrectly turned
                    // toward the new logical point on this frame.
                    const bool retain_air_path =
                        unit.move_type == MoveType::AIR &&
                        unit.using_path &&
                        !unit.path_request_pending &&
                        !attack_drifted;
                    if (retain_air_path) {
                        if (!unit.path_target_override_active) {
                            unit.path_target_override_active = true;
                            unit.path_target_override_x = unit.target_x;
                            unit.path_target_override_y = unit.target_y;
                        }
                        unit.target_x = attack_x;
                        unit.target_y = attack_y;
                        unit.has_move_target = true;
                        unit.arrived = false;
                    } else if (unit.move_type != MoveType::AIR &&
                               unit.using_path &&
                               !unit.path_request_pending &&
                               attack_drifted) {
                        // Ground ATTACK refreshes leave av/aw in place while
                        // the replacement grid request is pending. Keep the
                        // live target visible without an idle frame.
                        const f32 physical_target_x = unit.path_target_override_active
                            ? unit.path_target_override_x : unit.target_x;
                        const f32 physical_target_y = unit.path_target_override_active
                            ? unit.path_target_override_y : unit.target_y;
                        move_unit_to_(
                            unit, attack_x, attack_y,
                            attack_path_target_size(unit, attacked), true,
                            true);
                        if (unit.path_request_pending && unit.using_path) {
                            unit.path_target_override_active = true;
                            unit.path_target_override_x = physical_target_x;
                            unit.path_target_override_y = physical_target_y;
                            unit.target_x = attack_x;
                            unit.target_y = attack_y;
                        }
                        unit.has_move_target = true;
                        unit.arrived = false;
                    } else {
                        const bool had_active_path = unit.using_path;
                        unit.path_target_override_active = false;
                        unit.using_path = false;
                        unit.waypoints.clear();
                        unit.current_waypoint = 0;
                        // y.a(l,m,n,...) charges s before dispatching
                        // the movement request. AIR's ct() path is
                        // installed synchronously, but it uses the
                        // same cooldown.
                        unit.repath_cooldown_frames = 500.0f;
                        move_unit_to_async_(
                            unit, attack_x, attack_y,
                            attack_path_target_size(unit, attacked));
                        if (!had_active_path &&
                            unit.move_type == MoveType::AIR &&
                            distance_sq < 810000.0f) {
                            // aE() was sampled before the synchronous AIR
                            // request. With no old node, j supplies the raw
                            // l/m proposal for this turn, even when the new
                            // av node was clamped to the map boundary.
                            unit.direct_attack_chase_this_update = true;
                            unit.has_deferred_navigation_target = true;
                            unit.deferred_navigation_target_x = attack_x;
                            unit.deferred_navigation_target_y = attack_y;
                        }
                        // A ground chase request is visible in the
                        // same tick but coasts until its path is
                        // usable. AIR's ct() branch returns a live
                        // node immediately.
                        const bool formation_projection_only =
                            unit.formation_leader_uid != 0 &&
                            unit.has_move_target && !unit.arrived &&
                            !unit.using_path &&
                            !unit.path_request_pending &&
                            unit.repath_cooldown_frames == 0.0f;
                        unit.movement_activation_delay_frames =
                            // A newly exposed AIR av cannot supply the af
                            // sampled before the request. Inside 900 units
                            // j supplies l/m; outside it this turn coasts.
                            had_active_path ||
                                    (unit.move_type == MoveType::AIR &&
                                     distance_sq < 810000.0f) ||
                                    formation_projection_only
                                ? 0.0f : combat_delta_frames_;
                    }
                }
                if (unit.path_target_override_active &&
                    unit.using_path && !unit.path_request_pending) {
                    // Keep y.l/y.m synchronized with the live target
                    // on every later ATTACK object update while the
                    // retained av node remains active.
                    unit.target_x = attack_x;
                    unit.target_y = attack_y;
                    unit.has_move_target = true;
                    unit.arrived = false;
                }
                // The ATTACK proposal asserts y.k as soon as the
                // target leaves follow range, even while s prevents
                // a new path request. A retained av/aw chain becomes
                // active again immediately and remains the physical
                // steering target until the cooldown permits repath.
                unit.has_move_target = true;
                const bool direct_attack_proposal =
                    (unit.move_type == MoveType::AIR ||
                     unit.attack_range_adds_radii) &&
                    distance_sq < 810000.0f;
                if (!unit.using_path &&
                    !direct_attack_proposal &&
                    !unit.direct_attack_chase_this_update &&
                    (unit.formation_leader_uid == 0 ||
                     !unit.formation_projection_allowed)) {
                    // k/l/m can remain asserted while s forbids a request
                    // or an old aU is still pending after g()/aH() retargets.
                    // Without af or j the physical helper only coasts; a
                    // result adopted later this update clears this delay.
                    unit.movement_activation_delay_frames = 1.0f;
                    unit.movement_delay_keeps_prediction = false;
                }
                if (!unit.using_path &&
                    (direct_attack_proposal || unit.path_request_pending)) {
                    // The ATTACK helper publishes current l/m while aU is
                    // pending too. Without j the physical helper still coasts.
                    unit.target_x = attack_x;
                    unit.target_y = attack_y;
                }
                const bool attack_cooldown_coast =
                    unit.attack_final_node_coast_pending &&
                    !unit.using_path && !unit.path_request_pending &&
                    unit.repath_cooldown_frames > 0.0f &&
                    // ad.d selects the leader's slot before the generic
                    // af/j coast check; the exhausted private path cannot
                    // mark that still-usable formation proposal as arrived.
                    !direct_attack_proposal && !projects_attack_leader &&
                    !unit.direct_attack_chase_this_update;
                unit.arrived = attack_cooldown_coast;
                if (unit.using_path || unit.path_request_pending ||
                    unit.repath_cooldown_frames == 0.0f) {
                    unit.attack_final_node_coast_pending = false;
                }
            } else if (unit.has_move_target) {
                // The ATTACK helper leaves y.av/aw intact when the
                // target enters follow range. It only stops asserting
                // y.k, allowing the existing path chain to be reused.
                if (unit.using_path &&
                    !unit.path_target_override_active) {
                    // Keep the exposed l/m proposal independent from the
                    // physical av[0] node. When the target leaves follow
                    // range again, JAR resumes this node without rebuilding
                    // the retained path.
                    unit.path_target_override_active = true;
                    unit.path_target_override_x = unit.target_x;
                    unit.path_target_override_y = unit.target_y;
                }
                unit.has_move_target = false;
                unit.arrived = true;
                unit.target_x = 0.0f;
                unit.target_y = 0.0f;
            }
            }
        }
    }

    if (activated_order_this_update &&
        unit.attack_order_had_active_air_path &&
        unit.formation_leader_uid != 0 &&
        unit.move_type == MoveType::AIR && unit.using_path &&
        !unit.path_target_override_active) {
        // A replacement ATTACK path is exposed at the live target,
        // but JAR keeps that node as the physical proposal while the
        // target itself moves on later frames.
        unit.path_target_override_active = true;
        unit.path_target_override_x = unit.target_x;
        unit.path_target_override_y = unit.target_y;
    }
    if (activated_order_this_update) {
        unit.attack_order_had_active_air_path = false;
        unit.attack_order_had_main_target = false;
    }

    const bool active_attack_move =
        !unit.order_queue.empty() &&
        unit.order_queue.front().type == WeaponType::ATTACK_MOVE &&
        unit.weapon_type == WeaponType::ATTACK_MOVE;
    if (active_attack_move) {
        // MOVE-family dispatch publishes n=0 before the combat helper can
        // replace it with q(R). An in-range hold keeps this zero even while
        // the previous av/aw chain and its captured solver radius survive.
        unit.path_request_target_size = 0;
    }
    bool attack_move_main_target_valid = false;
    bool attack_move_retains_outer_proposal = false;
    Unit* attack_move_leader =
        active_attack_move && unit.formation_leader_uid != 0
            ? formation_unit_(unit.formation_leader_uid) : nullptr;
    if (active_attack_move && unit.target_uid != 0 &&
        unit.target_team_id >= 0) {
        const f32 main_target_range = engagement_range_(unit);
        World* attacked_world = order_target_world_(
            unit.target_team_id, other_worlds);
        Unit* attacked = attacked_world
            ? attacked_world->get_unit(unit.target_uid) : nullptr;
        if (attacked && attacked->is_alive &&
            attacked->parent_uid == 0 && attacked_world != this &&
            is_enemy_with(*attacked_world) &&
            unit.behavior != Behavior::HOLD_FIRE &&
            unit.behavior != Behavior::RETURN_FIRE &&
            can_target_(unit, *attacked) &&
            dist2(unit.x, unit.y, attacked->x, attacked->y) <
                main_target_range * main_target_range) {
            attack_move_main_target_valid = true;
            const f32 distance_sq = dist2(
                unit.x, unit.y, attacked->x, attacked->y);

            // attackMove shares y.a(delta,target,ad,true) with the
            // ATTACK handler. Its formation projection is disabled
            // when either member approaches the combat target, or the
            // leader is already tracking that same target.
            Unit* leader = attack_move_leader;
            if (leader) {
                if (distance_sq < 490000.0f) {
                    const f32 leader_distance_sq = dist2(
                        leader->x, leader->y,
                        attacked->x, attacked->y);
                    if (distance_sq < 48400.0f ||
                        leader_distance_sq < 48400.0f ||
                        (unit.attack_range_adds_radii &&
                         leader_distance_sq < 270400.0f)) {
                        unit.formation_projection_allowed = false;
                    }
                }
                if (leader->target_uid == attacked->uid &&
                    leader->target_team_id == attacked_world->team_id_) {
                    unit.formation_projection_allowed = false;
                }
            }
            if (!leader) {
                unit.formation_projection_disabled_frames = 500.0f;
            } else if (unit.formation_projection_allowed) {
                unit.formation_projection_disabled_frames = 0.0f;
            } else {
                unit.formation_projection_disabled_frames += combat_delta_frames_;
            }
            // y.a(delta,R,ad,true) observes ad.d before the outer MOVE-family
            // handler closes it for any leader target. A different leader
            // target must not turn this helper's ai=0 into a 0.1 cooldown.
            const bool target_helper_projects = unit.formation_projection_allowed;
            if (leader && leader->target_uid != 0) {
                unit.formation_projection_allowed = false;
            }

            // y.a(delta,target,ad,true) refreshes the passive main
            // target scan timer while ATTACK_MOVE's live target is
            // inside the unit's primary attack range. The combat
            // pass subtracts one frame afterward, leaving S=9 in
            // the visible state. Without this refresh S expires
            // early and the passive scanner can replace R while the
            // ATTACK_MOVE target is still valid.
            f32 main_attack_range = unit.weapon_range;
            if (unit.attack_range_adds_radii) {
                main_attack_range += unit.radius + attacked->radius;
            }
            if (distance_sq < main_attack_range * main_attack_range) {
                unit.target_scan_timer_frames = 10.0f;
            }

            const f32 follow_range = attack_follow_stop_range(unit, *attacked);
            const auto weapon_eligibility =
                attack_target_weapon_eligibility(*this, unit, *attacked);
            const bool needs_approach = !weapon_eligibility.allowed_now &&
                weapon_eligibility.allowed_without_range;
            const bool waiting_for_preserved_attack_move_path =
                unit.path_request_prediction_suppressed &&
                unit.path_request_pending &&
                unit.path_request_preserves_active_path && !unit.using_path;
            if (unit.is_mobile() &&
                (unit.movement_activation_delay_frames <= 0.0f ||
                 waiting_for_preserved_attack_move_path) &&
                (unit.bomber_attack_movement ||
                 distance_sq >= follow_range * follow_range ||
                 needs_approach)) {
                const auto [attack_x, attack_y] = bomber_attack_target(
                    unit, distance_sq, attacked->x, attacked->y);
                // y.a(delta,R,ad,true) still publishes k/l/m/n when aH()
                // cleared the old nodes and aU has not returned. The split
                // movement delay only suppresses steering for this turn.
                unit.attack_move_combat_target_valid = true;
                unit.attack_move_combat_target_x = attack_x;
                unit.attack_move_combat_target_y = attack_y;
                unit.attack_move_direct_chase_proposal =
                    distance_sq < 810000.0f &&
                    (unit.move_type == MoveType::AIR ||
                     unit.attack_range_adds_radii ||
                     (!target_helper_projects &&
                      unit.formation_projection_disabled_frames < 120.0f));
                if (!unit.using_path && unit.path_request_pending) {
                    unit.target_x = attack_x;
                    unit.target_y = attack_y;
                    unit.has_move_target = true;
                    unit.arrived = false;
                }
                unit.path_request_target_size =
                    attack_path_target_size(unit, attacked);
                if (unit.repath_cooldown_frames > 90.0f) {
                    unit.repath_cooldown_frames = 90.0f;
                }

                if (!target_helper_projects &&
                    unit.formation_projection_disabled_frames <
                        120.0f &&
                    distance_sq < 810000.0f) {
                    unit.repath_cooldown_frames = 0.1f;
                    unit.repath_cooldown_order_start = -1.0f;
                    if (!unit.using_path) {
                        // With no captured av node the combat proposal
                        // drives this frame directly. An adopted path
                        // remains the physical target even though
                        // y.l/m expose the live combat target.
                        unit.direct_attack_chase_this_update = true;
                        unit.has_deferred_navigation_target = true;
                        unit.deferred_navigation_target_x = attack_x;
                        unit.deferred_navigation_target_y = attack_y;
                        // y.a(...,true) publishes live k/l/m before the
                        // generic controller checks the pending aU request.
                        unit.target_x = attack_x;
                        unit.target_y = attack_y;
                        unit.has_move_target = true;
                        unit.arrived = false;
                    }
                } else if (!target_helper_projects &&
                           unit.formation_projection_disabled_frames >=
                               120.0f &&
                           unit.repath_cooldown_frames <= 0.1f) {
                    unit.repath_cooldown_frames = 0.0f;
                }

                if (unit.using_path && !unit.path_request_pending &&
                    !unit.formation_projection_allowed &&
                    (unit.formation_projection_disabled_frames < 120.0f ||
                     (unit.formation_leader_uid != 0 &&
                      unit.formation_group_size > 1 &&
                      !unit.formation_preferred &&
                      remaining_navigation_points(unit) <= 3 &&
                      unit.repath_cooldown_frames > 0.0f)) &&
                    distance_sq < 810000.0f) {
                    // ATTACK_MOVE keeps the existing av[0] as the physical
                    // node while publishing the live combat target through
                    // k/l/m during the formation handoff window.
                    if (!unit.path_target_override_active) {
                        unit.path_target_override_active = true;
                        unit.path_target_override_x = unit.target_x;
                        unit.path_target_override_y = unit.target_y;
                    }
                    unit.target_x = attack_x;
                    unit.target_y = attack_y;
                    unit.has_move_target = true;
                    unit.arrived = false;
                }

                const bool uses_m_drift_threshold =
                    unit.move_type == MoveType::AIR ||
                    unit.attack_range_adds_radii;
                const f32 attack_drift_threshold = uses_m_drift_threshold
                    ? std::min(150.0f,
                               std::max(0.0f, unit.weapon_range - 1.0f))
                    : 150.0f;
                const bool attack_drifted =
                    std::abs(unit.last_request_x - attack_x) >
                        attack_drift_threshold ||
                    std::abs(unit.last_request_y - attack_y) >
                        attack_drift_threshold;
                const bool attack_order_drifted =
                    std::abs(unit.last_request_x - attack_x) > 150.0f ||
                    std::abs(unit.last_request_y - attack_y) > 150.0f;
                const bool linked_private_path_missing =
                    unit.formation_leader_uid != 0 &&
                    !unit.formation_projection_allowed &&
                    unit.formation_projection_disabled_frames >=
                        120.0f &&
                    !unit.using_path && !unit.path_request_pending;
                const bool private_chase_path_missing =
                    !unit.using_path && !unit.path_request_pending &&
                    (!leader || !unit.formation_projection_allowed);
                // y.a(engine,delta,order,ad) selects formation projection
                // before its entire private-path drift/request branch. The
                // live combat l/m/n proposal does not bypass a linked ad.d.
                const bool private_attack_navigation =
                    !leader || !unit.formation_projection_allowed;
                if (private_attack_navigation && !unit.path_request_pending &&
                    attack_order_drifted &&
                    unit.repath_cooldown_frames > 30.0f) {
                    unit.repath_cooldown_frames = 30.0f;
                }
                if (private_attack_navigation && !unit.path_request_pending &&
                    (!unit.using_path || attack_drifted ||
                     linked_private_path_missing || private_chase_path_missing) &&
                    (unit.repath_cooldown_frames == 0.0f ||
                     (private_chase_path_missing && unit.path_incomplete &&
                      unit.repath_cooldown_frames < 450.0f))) {
                    // The combat helper has already asserted this frame's
                    // k/l/m. A previous in-range k=false does not discard
                    // av/aw: only a missing chain or destination drift asks
                    // the generic controller to replace the retained path.
                    const bool had_active_path = unit.using_path;
                    if (had_active_path && unit.move_type != MoveType::AIR) {
                        const f32 old_path_x = unit.path_target_override_active
                            ? unit.path_target_override_x : unit.target_x;
                        const f32 old_path_y = unit.path_target_override_active
                            ? unit.path_target_override_y : unit.target_y;
                        move_unit_to_(unit, attack_x, attack_y,
                                      attack_path_target_size(unit, attacked),
                                      true, true);
                        unit.path_target_override_active =
                            unit.path_request_pending;
                        if (unit.path_request_pending) {
                            unit.path_target_override_x = old_path_x;
                            unit.path_target_override_y = old_path_y;
                            unit.target_x = attack_x;
                            unit.target_y = attack_y;
                        }
                    } else {
                        unit.using_path = false;
                        unit.waypoints.clear();
                        unit.current_waypoint = 0;
                        move_unit_to_async_(
                            unit, attack_x, attack_y,
                            attack_path_target_size(unit, attacked));
                    }
                    // y.a(delta,R,ad,true) supplies j for every nearby
                    // AIR chase, including a new one-unit formation. Its
                    // physical pass can turn immediately after requesting.
                    const bool air_chase_proposal =
                        unit.move_type == MoveType::AIR &&
                        distance_sq < 810000.0f;
                    unit.movement_activation_delay_frames =
                        had_active_path || air_chase_proposal
                            ? 0.0f : 1.0f;
                }
                if (unit.using_path && !unit.path_target_override_active) {
                    unit.path_target_override_active = true;
                    unit.path_target_override_x = unit.target_x;
                    unit.path_target_override_y = unit.target_y;
                }
                unit.target_x = attack_x;
                unit.target_y = attack_y;
                unit.has_move_target = true;
                unit.arrived = false;
                const bool direct_attack_proposal =
                    (unit.move_type == MoveType::AIR ||
                     unit.attack_range_adds_radii) &&
                    distance_sq < 810000.0f;
                if (!unit.using_path && !unit.path_request_pending &&
                    !direct_attack_proposal &&
                    !unit.direct_attack_chase_this_update &&
                    (!leader || !unit.formation_projection_allowed)) {
                    unit.movement_activation_delay_frames = 1.0f;
                    unit.movement_delay_keeps_prediction = false;
                }
            } else if (unit.is_mobile() &&
                       (unit.movement_activation_delay_frames <= 0.0f ||
                        waiting_for_preserved_attack_move_path) &&
                       weapon_eligibility.allowed_now) {
                // ATTACK_MOVE passes true to the shared combat helper:
                // once i(target) accepts the current range it clears
                // y.k for this update, while retaining av/aw so the
                // original order path can resume later.
                if (unit.using_path && !unit.path_target_override_active) {
                    unit.path_target_override_active = true;
                    unit.path_target_override_x = unit.target_x;
                    unit.path_target_override_y = unit.target_y;
                }
                unit.has_move_target = false;
                unit.arrived = true;
                // The outer MOVE-family handler published the order point
                // before the helper cleared k, including a pending aU hold.
                unit.target_x = unit.order_queue.front().x;
                unit.target_y = unit.order_queue.front().y;
                unit.movement_prediction_enabled = false;
                unit.has_deferred_navigation_target = false;
            } else if (unit.is_mobile() &&
                       (unit.movement_activation_delay_frames <= 0.0f ||
                        waiting_for_preserved_attack_move_path)) {
                // i(R)==false and j(R)==false reject a new chase without
                // clearing the outer waypoint's k/l/m. A previous weapon
                // hold must resume its retained path on this same turn.
                attack_move_retains_outer_proposal = true;
                if (unit.using_path && !unit.path_target_override_active) {
                    unit.path_target_override_active = true;
                    unit.path_target_override_x = unit.target_x;
                    unit.path_target_override_y = unit.target_y;
                }
                unit.has_move_target = true;
                unit.arrived = false;
                unit.target_x = unit.order_queue.front().x;
                unit.target_y = unit.order_queue.front().y;
            }
        }
    }

    if (active_attack_move && !attack_move_main_target_valid &&
        attack_move_leader && attack_move_leader->target_uid != 0) {
        // Even a rejected follower R leaves the outer handler's ad.d closed
        // while its linked leader retains another main target reference.
        unit.formation_projection_allowed = false;
    }
    if (active_attack_move && !attack_move_main_target_valid) {
        // MOVE-family dispatch publishes the waypoint before testing R with
        // y.a(R,false). A dead, loaded, out-of-range or otherwise rejected R
        // leaves that proposal active; the later combat pass clears R itself.
        // Retain av/aw so an interrupted route resumes its captured node.
        unit.has_move_target = true;
        unit.arrived = false;
        unit.has_deferred_navigation_target = false;
        unit.path_request_target_size = 0;
        if (!unit.using_path && !unit.path_request_pending) {
            unit.path_target_override_active = false;
            unit.target_x = unit.order_queue.front().x;
            unit.target_y = unit.order_queue.front().y;
        }
        if (unit.using_path && !unit.path_request_pending &&
            unit.spawn_delay_frames == 0.0f && unit.is_mobile() &&
            (!attack_move_leader || !unit.formation_projection_allowed)) {
            // y.a(engine,delta,order,ad) checks r against the restored l/m
            // even when the interrupted attack left av/aw attached. A grid
            // refresh retains that chain until b(l) adopts its result; the
            // straight-line branch overwrites the captured node immediately.
            const UnitOrder& active = unit.order_queue.front();
            const bool drifted =
                std::abs(unit.last_request_x - active.x) > 150.0f ||
                std::abs(unit.last_request_y - active.y) > 150.0f;
            if (drifted && unit.repath_cooldown_frames > 30.0f) {
                unit.repath_cooldown_frames = 30.0f;
            }
            // ct()/aV() also compare m()-1 at s==0. This stricter
            // range does not shorten a still-running cooldown.
            const f32 refresh_drift_limit =
                unit.move_type == MoveType::AIR || unit.attack_range_adds_radii
                    ? std::min(150.0f, unit.weapon_range - 1.0f) : 150.0f;
            const bool refresh_drifted =
                std::abs(unit.last_request_x - active.x) > refresh_drift_limit ||
                std::abs(unit.last_request_y - active.y) > refresh_drift_limit;
            if (refresh_drifted && unit.repath_cooldown_frames == 0.0f) {
                move_unit_to_(unit, active.x, active.y, 0, true, true);
            }
        }
    }

    if (unit.has_guard_pos && !activated_order_this_update &&
        !unit.order_queue.empty() &&
        unit.order_queue.front().type == WeaponType::GUARD_AT &&
        unit.has_move_target && unit.repath_cooldown_frames > 90.0f) {
        // c(GUARD_AT) caps the existing route before the generic request.
        // A request made on activation keeps its new 500-frame charge.
        unit.repath_cooldown_frames = 90.0f;
    }

    const bool active_follow_waypoint = !unit.order_queue.empty() &&
        unit.order_queue.front().type == WeaponType::FOLLOW &&
        unit.follow_target_uid != 0;
    if (unit.follow_target_uid != 0 && !active_follow_waypoint) {
        World* followed_world = order_target_world_(
            unit.follow_target_team_id, other_worlds);
        Unit* followed = followed_world
            ? followed_world->get_unit(unit.follow_target_uid) : nullptr;
        if (!followed || !followed->is_alive) {
            unit.follow_target_uid = 0;
            unit.follow_target_team_id = -1;
            unit.has_move_target = false;
            unit.arrived = true;
            unit.using_path = false;
            unit.waypoints.clear();
            unit.current_waypoint = 0;
        } else {
            const f32 follow_range = unit.radius + followed->radius + 50.0f;
            if (unit.distance_to(*followed) > follow_range) {
                f32 planned_x = unit.target_x;
                f32 planned_y = unit.target_y;
                if (!unit.waypoints.empty()) {
                    planned_x = unit.waypoints.back().first;
                    planned_y = unit.waypoints.back().second;
                }
                if (!activated_order_this_update &&
                    unit.repath_cooldown_frames > 90.0f) {
                    unit.repath_cooldown_frames = 90.0f;
                }
                const bool follow_drifted =
                    std::abs(planned_x - followed->x) > 18.0f ||
                    std::abs(planned_y - followed->y) > 18.0f;
                if (follow_drifted &&
                    unit.repath_cooldown_frames > 30.0f) {
                    unit.repath_cooldown_frames = 30.0f;
                }
                if ((!unit.has_move_target || follow_drifted) &&
                    unit.repath_cooldown_frames == 0.0f) {
                    unit.using_path = false;
                    unit.waypoints.clear();
                    unit.current_waypoint = 0;
                    move_unit_to(unit, followed->x, followed->y);
                }
            } else if (unit.has_move_target) {
                // The ATTACK helper leaves y.av/aw intact when the
                // target enters follow range. It only stops asserting
                // y.k, allowing the existing path chain to be reused.
                unit.has_move_target = false;
                unit.arrived = true;
            }
        }
    }

    if (unit.guard_target_uid != 0 || active_follow_waypoint) {
        // Native c() shares target dispatch between GUARD and FOLLOW. Their
        // follow allowances differ; only GUARD adds q(2) and repair orders.
        const u32 guarded_uid = active_follow_waypoint
            ? unit.follow_target_uid : unit.guard_target_uid;
        const i32 guarded_team = active_follow_waypoint
            ? unit.follow_target_team_id : unit.guard_target_team_id;
        World* guarded_world = order_target_world_(
            guarded_team, other_worlds);
        Unit* guarded = guarded_world
            ? guarded_world->get_unit(guarded_uid) : nullptr;
        if (!guarded || !guarded->is_alive) {
            if (active_follow_waypoint) {
                unit.follow_target_uid = 0;
                unit.follow_target_team_id = -1;
            } else {
                unit.guard_target_uid = 0;
                unit.guard_target_team_id = -1;
            }
            unit.has_move_target = false;
            unit.arrived = true;
            unit.using_path = false;
            unit.waypoints.clear();
            unit.current_waypoint = 0;
        } else {
            // y.c(GUARD) derives its follow allowance from cK captured at
            // the start of this object update, before the new proposal is
            // written by the movement pass.
            const f32 follow_range = unit.radius + guarded->radius +
                (active_follow_waypoint
                    ? (unit.movement_prediction_enabled ? 30.0f : 50.0f)
                    : (unit.movement_prediction_enabled ? 80.0f : 100.0f));
            const f32 guard_distance_sq = dist2(
                unit.x, unit.y, guarded->x, guarded->y);
            const f32 guard_distance = std::sqrt(guard_distance_sq);
            // y.k(float) resets r to 150 before dispatching the
            // active order. Only c(...)'s initial guarded-unit follow
            // proposal changes it to 18; a later combat/q candidate
            // can replace l/m without replacing that threshold.
            const f32 guard_path_drift_threshold =
                guard_distance > follow_range ? 18.0f : 150.0f;
            unit.guard_path_drift_threshold =
                guard_path_drift_threshold;
            Unit* movement_target =
                guard_distance_sq > follow_range * follow_range
                    ? guarded : nullptr;
            if (!movement_target) {
                unit.guard_combat_chase_outer = false;
            }
            const bool large_replay_guard_path =
                !unit.order_queue.empty() &&
                unit.order_queue.front().type == WeaponType::GUARD &&
                unit.order_queue.front().replay_command_group_size >= 19 &&
                unit.formation_leader_uid != 0 &&
                unit.formation_group_size > 1 &&
                unit.using_path &&
                remaining_navigation_points(unit) >= 17 &&
                guard_distance > 550.0f;
            // The order pass can expose a retained combat chase in
            // y.l/y.m, then auto-combat clears R before the physical
            // pass. JAR leaves that same-frame logical proposal in
            // place; do not replace it with the guarded unit merely
            // because the order target has just been cleared.
            if (!order_phase && unit.guard_movement_target_valid &&
                unit.guard_movement_chases_main_target &&
                unit.target_uid == 0 &&
                unit.guard_movement_target_uid != 0 &&
                unit.guard_movement_target_uid != guarded_uid) {
                World* cached_world = order_target_world_(
                    unit.guard_movement_target_team_id, other_worlds);
                Unit* cached_target = cached_world
                    ? cached_world->get_unit(
                          unit.guard_movement_target_uid)
                    : nullptr;
                if (cached_target && cached_target->is_alive &&
                    cached_target->parent_uid == 0 &&
                    cached_world != this &&
                    is_enemy_with(*cached_world)) {
                    movement_target = cached_target;
                }
            }
            bool main_target_handled = false;
            if (movement_target &&
                unit.movement_activation_delay_frames <= 0.0f &&
                unit.repath_cooldown_frames > 90.0f &&
                unit.repath_cooldown_frames <= unit.repath_cooldown_order_start) {
                unit.repath_cooldown_frames = 90.0f;
            }
            // y.c(delta, waypoint, movement) first proposes following
            // the guarded unit. A retained main target then cancels
            // that proposal inside o(target), or replaces it with a
            // combat chase outside range.
            Unit* main_target = nullptr;
            World* main_target_world = order_target_world_(
                unit.target_team_id, other_worlds);
            const f32 main_engagement_range = engagement_range_(unit);
            if (main_target_world && main_target_world != this &&
                is_enemy_with(*main_target_world) &&
                unit.behavior != Behavior::HOLD_FIRE &&
                unit.behavior != Behavior::RETURN_FIRE) {
                Unit* candidate = main_target_world->get_unit(
                    unit.target_uid);
                if (candidate && candidate->is_alive &&
                    candidate->parent_uid == 0 &&
                    can_target_(unit, *candidate) &&
                    dist2(unit.x, unit.y, candidate->x, candidate->y) <
                        main_engagement_range * main_engagement_range) {
                    main_target = candidate;
                }
            }
            const bool invalid_main_target_pending_scan =
                unit.target_uid != 0 && main_target == nullptr;
            if (invalid_main_target_pending_scan &&
                guarded_uid != 0) {
                World* guarded_world = order_target_world_(
                    guarded_team, other_worlds);
                Unit* guarded = guarded_world
                    ? guarded_world->get_unit(guarded_uid)
                    : nullptr;
                if (guarded && guarded->is_alive &&
                    guarded->parent_uid == 0 &&
                    unit.distance_to(*guarded) >
                        unit.radius + guarded->radius + 100.0f) {
                    // y.c() discards a stale combat proposal before
                    // the passive scan can install its replacement.
                    unit.guard_movement_target_valid = true;
                    unit.guard_movement_chases_main_target = false;
                    unit.guard_movement_target_x = guarded->x;
                    unit.guard_movement_target_y = guarded->y;
                } else {
                    unit.guard_movement_target_valid = false;
                    unit.guard_movement_chases_main_target = false;
                }
            }
            // c() tests b(R,false) before accepting a main-target proposal.
            // An expired or incompatible R leaves the guarded proposal and
            // q(2) fallback intact; the later combat pass clears R itself.
            bool has_active_slot_target = false;
            for (const WeaponSlot& slot : unit.weapon_slots) {
                if (slot.fire_enabled && slot.target_uid != 0) {
                    has_active_slot_target = true;
                    break;
                }
            }
            f32 main_target_range = unit.weapon_range;
            if (main_target && unit.attack_range_adds_radii) {
                main_target_range += unit.radius + main_target->radius;
            }
            const bool main_target_in_range =
                main_target && has_active_slot_target &&
                dist2(unit.x, unit.y,
                      main_target->x, main_target->y) <
                    main_target_range * main_target_range;
            if (main_target) {
                if (guard_distance_sq < 22500.0f) {
                    unit.guard_combat_chase_outer = false;
                }
                // y.c() keeps w=1 for an out-of-range R until the strict
                // 150-unit reset. An in-range R may cancel the return
                // inside 450; with w=0 it may hold position up to 550.
                const bool guard_return_latched =
                    unit.guard_combat_chase_outer;
                const bool must_return_to_guarded =
                    (!main_target_in_range &&
                     (guard_return_latched || guard_distance_sq > 122500.0f)) ||
                    guard_distance_sq > 302500.0f ||
                    (guard_return_latched && guard_distance_sq > 202500.0f);
                if (!must_return_to_guarded) {
                    movement_target = main_target_in_range ? nullptr : main_target;
                    main_target_handled = true;
                    unit.guard_movement_chases_main_target = !main_target_in_range;
                    unit.guard_combat_chase_outer = false;
                } else {
                    unit.guard_combat_chase_outer = true;
                }
            }
            // The guarded unit's q(2) is allowed while returning. The
            // fallback to this unit's q(2) tests w after the R branch,
            // including a return state first entered on this object turn.
            if (guard_recent_attacker_from_self &&
                unit.guard_combat_chase_outer) {
                guard_recent_order_candidate = false;
            }
            unit.recent_order_chase =
                guard_recent_order_candidate && !main_target_handled;
            if (unit.recent_order_chase) {
                // q(2)'s accepted candidate uses n=0 in c(...), just
                // as a retained-main-target chase does.
                movement_target = recent_order_attacker;
                unit.guard_movement_chases_main_target = true;
            }
            if (order_phase && !main_target_handled && !unit.recent_order_chase &&
                !unit.order_queue.empty() &&
                unit.order_queue.front().type == WeaponType::GUARD) {
                // c(GUARD) can prepend repair before generic navigation.
                // Builders may join the guarded orderable's X() target while
                // that unit's retained h marks active service work.
                World* repair_world = guarded_world;
                Unit* repair_target = nullptr;
                if (can_repair_target(unit, *guarded) &&
                    (guarded->hp < guarded->max_hp || !guarded->is_complete())) {
                    repair_target = guarded;
                } else if (unit.is_builder && guarded->service_range_extended &&
                           !guarded->order_queue.empty()) {
                    const UnitOrder& servicing = guarded->order_queue.front();
                    if (servicing.type == WeaponType::REPAIR ||
                        servicing.type == WeaponType::RECLAIM) {
                        repair_world = order_target_world_(servicing.target_team_id, other_worlds);
                        Unit* candidate = repair_world
                            ? repair_world->get_unit(servicing.target_uid) : nullptr;
                        if (candidate && candidate->is_alive &&
                            can_repair_target(unit, *candidate)) {
                            repair_target = candidate;
                        }
                    }
                }
                if (repair_target && repair_world) {
                    // c(GUARD) uses a(am)'s repair capability, including an
                    // enemy building accepted as the guarded waypoint. Team
                    // ownership does not add a gate to this insertion.
                    const u32 main_uid = unit.main_target_uid();
                    const i32 main_team = unit.main_target_team_id();
                    UnitOrder repair;
                    repair.type = WeaponType::REPAIR;
                    repair.x = repair.y = 2.0f;
                    repair.target_uid = repair_target->uid;
                    repair.target_team_id = repair_world->team_id_;
                    repair.automatic = true;
                    if (unit.order_queue.size() >= 29) unit.order_queue.pop_back();
                    unit.order_queue.insert(unit.order_queue.begin(), std::move(repair));
                    unit.service_combat_target_uid = main_uid;
                    unit.service_combat_target_team_id = main_team;
                    // ao() retains R/aU while clearing h, av/aw, s, W/X
                    // and q. k(float) then sees a different ar() and clears
                    // this turn's movement proposal before the controller.
                    unit.service_range_extended = false;
                    unit.active_order_elapsed_frames = 0.0f;
                    unit.order_point_dwell_frames = 0.0f;
                    unit.has_move_target = false;
                    unit.arrived = true;
                    unit.using_path = false;
                    unit.waypoints.clear();
                    unit.current_waypoint = 0;
                    unit.path_target_override_active = false;
                    unit.path_incomplete = false;
                    unit.path_request_preserves_active_path = false;
                    unit.repath_cooldown_frames = 0.0f;
                    unit.waypoint_stall_frames = 0.0f;
                    unit.last_waypoint_stall_frames = 0.0f;
                    unit.single_node_path_counter = 0;
                    unit.has_deferred_navigation_target = false;
                    unit.guard_target_uid = 0;
                    unit.guard_target_team_id = -1;
                    unit.guard_movement_target_valid = false;
                    unit.guard_movement_target_hold = false;
                    unit.guard_movement_chases_main_target = false;
                    unit.activate_order_next_frame = true;
                    movement_target = nullptr;
                }
            }
            if (large_replay_guard_path && !unit.has_move_target &&
                unit.repath_cooldown_frames <= 62.0f && unit.using_path) {
                // A large formation can clear k when its combat target is
                // briefly in range, then y.c() republishes the guarded-path
                // proposal on the next object turn while av/aw remain live.
                movement_target = guarded;
            }
            if (movement_target) {
                unit.guard_movement_target_valid = true;
                unit.guard_movement_target_x = movement_target->x;
                unit.guard_movement_target_y = movement_target->y;
                unit.guard_movement_target_uid = movement_target->uid;
                unit.guard_movement_target_team_id =
                    movement_target == guarded
                        ? guarded_team
                        : (movement_target == recent_order_attacker
                               ? recent_order_attacker_world->team_id_
                               : unit.target_team_id);
                // y.c() evaluates q(2) before its guarded-unit follow
                // proposal reaches y.a(). If that recent attacker is
                // accepted, the initial GUARD activation must replace
                // the just-created guarded-unit path immediately;
                // retaining it would leave av/aw aimed at ad while
                // l/m already expose q(2)'s target. This is especially
                // visible when a GUARD command arrives while the unit
                // is carrying a long MOVE path.
                const bool recent_candidate_path_drifted =
                    activated_order_this_update &&
                    guard_recent_order_candidate && unit.using_path &&
                    !unit.path_request_pending &&
                    (std::abs(unit.last_request_x -
                              movement_target->x) > 18.0f ||
                     std::abs(unit.last_request_y -
                              movement_target->y) > 18.0f);
                if (recent_candidate_path_drifted) {
                    unit.using_path = false;
                    unit.waypoints.clear();
                    unit.current_waypoint = 0;
                    unit.repath_cooldown_frames = 0.0f;
                    // The discarded guarded-unit activation may have
                    // installed the one-frame launch delay used when
                    // GUARD starts with a fresh path. q(2)'s same-pass
                    // replacement is already a live proposal in JAR,
                    // so it must steer on this object update.
                    unit.movement_activation_delay_frames = 0.0f;
                    unit.movement_delay_keeps_prediction = false;
                }
                if (!unit.using_path) {
                    // With no adopted av node, y.c refreshes the raw
                    // k/l/m proposal even while s or aU delays the path.
                    // Only a combat chase sets y.j and steers at that raw
                    // point; a plain follow proposal keeps k while coasting.
                    // An earlier waypoint's av[0] override is no longer a
                    // physical node once that path has been cleared.
                    unit.path_target_override_active = false;
                    unit.target_x = movement_target->x;
                    unit.target_y = movement_target->y;
                    unit.has_move_target = true;
                    unit.arrived = false;
                    if (!unit.guard_movement_chases_main_target) {
                        unit.movement_activation_delay_frames = 1.0f;
                        unit.movement_delay_keeps_prediction = false;
                    }
                }
                if (!unit.has_move_target && unit.using_path) {
                    // Every accepted GUARD proposal reasserts k. This
                    // includes returning to the guarded unit after an
                    // in-range or dead R suppressed movement on the
                    // preceding frame. The retained av chain remains
                    // the physical route.
                    unit.has_move_target = true;
                    unit.arrived = false;
                }
                if (invalid_main_target_pending_scan &&
                    !unit.has_move_target && unit.using_path) {
                    // Every accepted GUARD proposal reasserts k. This
                    // includes returning to the guarded unit after an
                    // in-range or dead R suppressed movement on the
                    // preceding frame. The retained av chain remains
                    // the physical route.
                    unit.has_move_target = true;
                    unit.arrived = false;
                }
                // An adopted path remains active while y.s counts
                // down; subsequent chase turns cap it at 90.
                if (unit.movement_activation_delay_frames <= 0.0f &&
                    unit.repath_cooldown_frames > 90.0f &&
                    unit.repath_cooldown_frames <= unit.repath_cooldown_order_start) {
                    // c(GUARD) caps the previous request before y.a() can
                    // issue a new one. Keep this turn's fresh 500 unchanged.
                    unit.repath_cooldown_frames = 90.0f;
                }
                // JAR runtime refresh (k(float) + a(l,f2,au,ad)):
                // while an adopted path is active, a proposal that
                // drifts more than the current y.r from the last
                // request point (y.o/p) caps y.s at 30; y.s==0 then
                // resubmits
                // y.a(l, m, n). Its grid branch enqueues the k.l
                // request without touching au/aw, so the old path
                // stays active until y.b(l) adopts the solved list.
                // Without an active node, y.s==0 alone resubmits.
                const bool refresh_drifted =
                    std::abs(unit.last_request_x -
                             movement_target->x) >
                        guard_path_drift_threshold ||
                    std::abs(unit.last_request_y -
                             movement_target->y) >
                        guard_path_drift_threshold;
                const f32 range_drift_threshold = unit.weapon_range - 1.0f;
                const bool refresh_range_drifted =
                    (unit.move_type == MoveType::AIR ||
                     unit.attack_range_adds_radii) &&
                    (std::abs(unit.last_request_x - movement_target->x) >
                         range_drift_threshold ||
                     std::abs(unit.last_request_y - movement_target->y) >
                         range_drift_threshold);
                if (!unit.path_request_pending &&
                    unit.has_move_target && refresh_drifted &&
                    unit.repath_cooldown_frames > 30.0f) {
                    unit.repath_cooldown_frames = 30.0f;
                }
                if (unit.repath_cooldown_frames == 0.0f &&
                    !unit.path_request_pending &&
                    (!unit.using_path || refresh_drifted || refresh_range_drifted)) {
                    const bool had_path_before_guard_refresh = unit.using_path;
                    unit.repath_cooldown_frames = 500.0f;
                    if (!unit.has_move_target && !unit.using_path) {
                        // y.c has already exposed its k/l/m movement
                        // proposal before y.a(...) submits the grid
                        // request. With no adopted node to preserve,
                        // that proposal remains visible while aU is
                        // pending.
                        unit.target_x = movement_target->x;
                        unit.target_y = movement_target->y;
                        unit.has_move_target = true;
                        unit.arrived = false;
                    }
                    // y.c's follow proposal hard-codes n=2, while a
                    // retained main target replaces it with n=0. This
                    // intentionally differs from the explicit ATTACK
                    // handler, which uses q(main target).
                    const i32 refresh_target_size =
                        unit.guard_movement_chases_main_target ? 0 : 2;
                    const bool replace_guard_path_immediately =
                        activated_order_this_update &&
                        unit.target_uid != 0 &&
                        unit.target_uid == guarded_uid;
                    const bool replace_guard_path_on_next_frame =
                        guard_replacement_refresh_next_frame;
                    if (replace_guard_path_immediately ||
                        replace_guard_path_on_next_frame ||
                        (activated_order_this_update &&
                         guard_replacement_from_patrol)) {
                        // Replacement clears the old av/aw chain, but a
                        // blocked route still goes through the native grid
                        // worker. Only the direct-path branch can expose new
                        // nodes in this object update.
                        move_unit_to_(unit, movement_target->x,
                                      movement_target->y,
                                      refresh_target_size, true, false);
                    } else {
                        move_unit_to_(unit, movement_target->x,
                                      movement_target->y,
                                      refresh_target_size, true, true);
                    }
                    if (!had_path_before_guard_refresh &&
                        unit.guard_movement_chases_main_target) {
                        // With no captured aE(), y.j makes this update use
                        // the raw combat proposal even if the request has
                        // just installed a new chain or is still solving.
                        unit.has_deferred_navigation_target = true;
                        unit.deferred_navigation_target_x = movement_target->x;
                        unit.deferred_navigation_target_y = movement_target->y;
                        if (!unit.using_path) {
                            unit.target_x = movement_target->x;
                            unit.target_y = movement_target->y;
                        }
                        unit.movement_activation_delay_frames = 0.0f;
                    }
                    const bool fresh_guard_follow_path =
                        !had_path_before_guard_refresh &&
                        !unit.guard_movement_chases_main_target;
                    if (fresh_guard_follow_path) {
                        // y.a() captures aE() before the follow request. A
                        // new av/aw chain is visible now, but this object
                        // turn still has no physical node and only coasts.
                        unit.movement_activation_delay_frames = 1.0f;
                        unit.movement_delay_keeps_prediction = false;
                    }
                    if (unit.move_type == MoveType::AIR) {
                        unit.using_path = true;
                        unit.waypoints.clear();
                        unit.current_waypoint = 0;
                        unit.path_request_target_size =
                            refresh_target_size;
                    }
                    unit.guard_path_created_without_node =
                        !had_path_before_guard_refresh && unit.using_path;
                }
            } else if (!main_target_in_range && unit.has_move_target) {
                // y.c(GUARD/FOLLOW) stops publishing k/l/m once the guarded
                // unit is back inside the follow allowance, but it never
                // touches au/aw: the solved chain stays visible until some
                // later request replaces it. Same rule as the in-range main
                // target branch below.
                unit.has_move_target = false;
                unit.arrived = true;
            }
        }
    }

    // PATROL checks its repair callback before generic navigation.
    // A repair inserted on this turn must clear k/av before the
    // resume branch can create a new aU for the patrol point.
    if (!unit.order_queue.empty() &&
        unit.order_queue.front().type == WeaponType::PATROL &&
        unit.is_builder &&
        static_cast<u64>(tick_) % 10ULL ==
            static_cast<u64>(unit.uid) % 10ULL) {
        Unit* repair_target = nullptr;
        World* repair_world = nullptr;
        f32 repair_distance_sq = 0.0f;
        const f32 scan_range = service_range(unit) + 150.0f;
        const f32 scan_range_sq = scan_range * scan_range;
        const i32 cell_width = std::max(
            1, static_cast<i32>(map_width() * tile_size()) /
                   EngineConstants::SPATIAL_GRID_SIZE);
        const i32 cell_height = std::max(
            1, static_cast<i32>(map_height() * tile_size()) /
                   EngineConstants::SPATIAL_GRID_SIZE);
        const auto cell = [](f32 coordinate, f32 inverse_size) {
            return clamp_i(static_cast<i32>(coordinate * inverse_size),
                           0, EngineConstants::SPATIAL_GRID_SIZE - 1);
        };
        const f32 min_x = unit.x - scan_range;
        const f32 max_x = unit.x + scan_range;
        const f32 min_y = unit.y - scan_range;
        const f32 max_y = unit.y + scan_range;
        const i32 min_cell_x = cell(min_x, 1.0f / static_cast<f32>(cell_width));
        const i32 max_cell_x = cell(max_x, 1.0f / static_cast<f32>(cell_width));
        const i32 min_cell_y = cell(min_y, 1.0f / static_cast<f32>(cell_height));
        const i32 max_cell_y = cell(max_y, 1.0f / static_cast<f32>(cell_height));
        struct RepairCandidate {
            World* world;
            Unit* unit;
        };
        Vec<RepairCandidate> candidates;
        auto scan_world = [&](World& candidate_world) {
            for (auto& [candidate_uid, candidate] :
                 candidate_world.units_) {
                if (candidate_uid == unit.uid ||
                    !candidate.is_alive || candidate.parent_uid != 0 ||
                    (candidate.hp >= candidate.max_hp &&
                     candidate.is_complete()) ||
                    !can_repair_target(unit, candidate)) {
                    continue;
                }
                // r.a()/cc.a() query the previous global geo-index pass.
                // A constructor can already be in am.bE while its resident
                // cell is still -1; moved residents also need both filters.
                if (candidate.collision_spatial_cell_x < min_cell_x ||
                    candidate.collision_spatial_cell_x > max_cell_x ||
                    candidate.collision_spatial_cell_y < min_cell_y ||
                    candidate.collision_spatial_cell_y > max_cell_y ||
                    candidate.x < min_x || candidate.x > max_x ||
                    candidate.y < min_y || candidate.y > max_y) {
                    continue;
                }
                candidates.push_back({&candidate_world, &candidate});
            }
        };
        scan_world(*this);
        for (World* other : other_worlds) {
            if (other && other != this && is_allied_with(*other)) {
                scan_world(*other);
            }
        }
        // This callback scans all teams together: cells in x/y order, then
        // residents in insertion order. Equal distances retain the first.
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
        for (const RepairCandidate& entry : candidates) {
            const f32 candidate_distance_sq = dist2(
                unit.x, unit.y, entry.unit->x, entry.unit->y);
            if (candidate_distance_sq >= scan_range_sq) continue;
            if (!repair_target || candidate_distance_sq < repair_distance_sq) {
                repair_target = entry.unit;
                repair_world = entry.world;
                repair_distance_sq = candidate_distance_sq;
            }
        }

        if (repair_target && repair_world) {
            const u32 main_uid = unit.main_target_uid();
            const i32 main_team = unit.main_target_team_id();
            UnitOrder repair;
            repair.type = WeaponType::REPAIR;
            repair.x = 2.0f;
            repair.y = 2.0f;
            repair.service_chase_range_extra = 200.0f;
            repair.target_uid = repair_target->uid;
            repair.target_team_id = repair_world->team_id_;
            if (unit.order_queue.size() >= 29) {
                unit.order_queue.pop_back();
            }
            unit.order_queue.insert(
                unit.order_queue.begin(), std::move(repair));
            // r.a()/y.ao() keep R when PATROL prepends repair. The new
            // service head changes its storage before this turn's combat.
            unit.restore_main_target(main_uid, main_team);
            unit.active_order_elapsed_frames = 0.0f;
            unit.order_point_dwell_frames = 0.0f;
            unit.has_move_target = false;
            unit.arrived = true;
            unit.using_path = false;
            unit.waypoints.clear();
            unit.current_waypoint = 0;
            unit.path_incomplete = false;
            unit.repath_cooldown_frames = 0.0f;
            unit.waypoint_stall_frames = 0.0f;
            unit.last_waypoint_stall_frames = 0.0f;
            unit.single_node_path_counter = 0;
            unit.has_patrol = false;
            unit.patrol_points.clear();
            unit.patrol_index = 0;
            unit.activate_order_next_frame = true;
        }
    }

    if (unit.has_patrol && !recent_order_chase_active &&
        !patrol_passive_target_in_range_this_update &&
        unit.arrived && !unit.has_move_target &&
        !unit.using_path &&
        !formation_patrol_failure_hold &&
        !single_patrol_holds_position()) {
        if (!unit.order_queue.empty() &&
            unit.order_queue.front().type == WeaponType::PATROL) {
            // A waypoint-backed patrol resumes its current au point. The
            // scene helper's implicit return leg is independent of that queue.
            const UnitOrder& patrol = unit.order_queue.front();
            if (!unit.path_request_pending &&
                (unit.repath_cooldown_frames == 0.0f ||
                 (unit.path_incomplete && unit.repath_cooldown_frames < 450.0f))) {
                move_unit_to_async_(unit, patrol.x, patrol.y, 0);
            } else {
                // y.c() resumes k/l/m after losing its main target. The
                // common controller retains aU and s until that request can
                // be adopted; publishing the waypoint cannot restart it.
                unit.has_move_target = true;
                unit.arrived = false;
                unit.target_x = patrol.x;
                unit.target_y = patrol.y;
            }
            unit.movement_activation_delay_frames = 1.0f;
        } else if (!unit.patrol_points.empty()) {
            auto& pt = unit.patrol_points[unit.patrol_index];
            // y.a() submits the patrol leg through the same delayed grid
            // request every other waypoint activation uses: the proposal is
            // exposed now, the nodes arrive with the solver result, and this
            // object update still coasts without steering.
            move_unit_to_async_(unit, pt.first, pt.second, 0);
            unit.movement_activation_delay_frames = 1.0f;
            unit.patrol_index = (unit.patrol_index + 1) % static_cast<i32>(unit.patrol_points.size());
        }
    }

    // Factory products move toward the raw exit point while cl is
    // positive. On the first frame at cl==0, y.a() requests a real path.
    // Direct/escape nodes are visible immediately, while grid results wait
    // for the path-engine handoff; physical movement coasts this frame.
    if (unit.factory_exit_path_pending &&
        unit.spawn_delay_frames == 0.0f) {
        if (!unit.order_queue.empty()) {
            const UnitOrder& exit_order = unit.order_queue.front();
            if (exit_order.type == WeaponType::MOVE &&
                (exit_order.factory_exit ||
                 unit.formation_leader_uid == 0)) {
                unit.using_path = false;
                unit.waypoints.clear();
                unit.current_waypoint = 0;
                move_unit_to_async_(unit, exit_order.x, exit_order.y);
                unit.movement_activation_delay_frames = 1.0f;
            }
        }
        unit.factory_exit_path_pending = false;
    }

    // MOVE/PATROL still count s down in the physical pass. Combat and service
    // orders have already done so before dispatch; do not anticipate twice.
    const f32 unapplied_cooldown_delta_frames =
        !unit.order_queue.empty() &&
        (unit.order_queue.front().type == WeaponType::MOVE ||
         unit.order_queue.front().type == WeaponType::PATROL)
            ? combat_delta_frames_ : 0.0f;
    const Unit* retry_leader = unit.formation_leader_uid != 0
        ? formation_unit_(unit.formation_leader_uid) : nullptr;
    const bool formation_controls_retry =
        retry_leader && retry_leader->is_active() &&
        unit.formation_projection_allowed && !unit.order_queue.empty() &&
        (retry_leader->order_queue.empty() ||
         (retry_leader->order_queue.front().type == unit.order_queue.front().type &&
          retry_leader->order_queue.front().target_uid == unit.order_queue.front().target_uid &&
          retry_leader->order_queue.front().x == unit.order_queue.front().x &&
          retry_leader->order_queue.front().y == unit.order_queue.front().y));
    if (unit.path_retry_next_frame &&
        (unit.using_path || unit.path_request_pending || formation_controls_retry)) {
        // y.a(engine,...) selects the formation helper before its generic
        // retry branch. Recovery aH() clears av/aw and s but leaves that
        // choice intact: fallback requests still target the leader at s=700.
        // A completed adoption also supersedes this older recovery hint.
        unit.path_retry_next_frame = false;
    }
    if (unit.path_retry_next_frame && unit.path_incomplete &&
        unit.repath_cooldown_frames - unapplied_cooldown_delta_frames >= 450.0f) {
        // y.k() decrements s before the generic controller tests s < 450.
        // An exhausted truncated path retains k/l/m while waiting, but its
        // absent af cannot steer or contribute movement prediction.
        unit.movement_activation_delay_frames = 1.0f;
        unit.movement_delay_keeps_prediction = false;
    } else if (unit.path_retry_next_frame &&
               (unit.path_incomplete ||
                unit.repath_cooldown_frames -
                    unapplied_cooldown_delta_frames <= 0.0f)) {
        // Wall recovery cleared av/aw in the preceding physical pass. The
        // current handler can publish a new combat proposal and reset s to
        // 0.1 before generic navigation; that cooldown still gates the retry.
        unit.path_retry_next_frame = false;
        if (!unit.order_queue.empty() && unit.has_move_target &&
            !unit.path_request_pending) {
            const UnitOrder& retry_order = unit.order_queue.front();
            f32 retry_x = retry_order.x;
            f32 retry_y = retry_order.y;
            i32 retry_target_size = 0;
            const bool service_retry =
                retry_order.type == WeaponType::REPAIR ||
                retry_order.type == WeaponType::RECLAIM;
            const bool attack_retry = retry_order.type == WeaponType::ATTACK;
            if ((service_retry || attack_retry) && retry_order.target_uid != 0) {
                World* target_world = order_target_world_(
                    retry_order.target_team_id, other_worlds);
                Unit* target = target_world
                    ? target_world->get_unit(retry_order.target_uid) : nullptr;
                if (target) {
                    retry_x = target->x;
                    retry_y = target->y;
                    retry_target_size = service_retry
                        ? service_path_target_size_(unit, target->unit_type)
                        : attack_path_target_size(unit, target);
                }
            } else if (retry_order.type == WeaponType::ATTACK_MOVE &&
                       unit.attack_move_combat_target_valid) {
                // The generic controller requests the current k/l/m/n,
                // which combat can replace after MOVE-family dispatch.
                retry_x = unit.attack_move_combat_target_x;
                retry_y = unit.attack_move_combat_target_y;
                retry_target_size = unit.path_request_target_size;
            }
            if (service_retry) {
                // A truncated service path is handed back to the path engine
                // asynchronously.  The request remains visible while its
                // cached result is consumed on a later object turn.
                move_unit_to_async_(unit, retry_x, retry_y,
                                    retry_target_size);
                unit.movement_delay_uses_acceleration =
                    unit.reversing && unit.move_speed_factor < 0.0f;
            } else {
                // y's generic controller submits a grid retry to k.l; the
                // resulting av/aw nodes remain hidden until the request is
                // adopted on a later object turn. Direct-line/escape retries
                // still complete immediately inside move_unit_to_async_.
                move_unit_to_async_(unit, retry_x, retry_y, retry_target_size);
            }
            // The replacement path is visible immediately, but this
            // object update still coasts on the previous vector.
            unit.movement_activation_delay_frames = 1.0f;
            unit.movement_delay_keeps_prediction = service_retry;
        }
    }

    // The generic y.a(engine,...) controller drops an inactive leader before
    // choosing formation projection or a private path. Doing this only in
    // the physical pass would let a newly ordered follower steer at the old
    // slot and postpone its own request by one object update.
    if (unit.formation_leader_uid != 0) {
        const Unit* leader = formation_unit_(unit.formation_leader_uid);
        if (!leader || !leader->is_active()) {
            unit.formation_leader_uid = 0;
            unit.attack_move_private_navigation_handoff =
                active_attack_move && unit.attack_move_combat_target_valid;
        } else if (unit.formation_projection_allowed &&
                   !unit.order_queue.empty() && !leader->order_queue.empty()) {
            // y.a(engine,...) applies au.b() before choosing the private
            // controller. A live leader can have switched to an automatic
            // repair while its followers still retain MOVE and their ad link.
            const UnitOrder& order = unit.order_queue.front();
            const UnitOrder& lead = leader->order_queue.front();
            if (lead.type != order.type || lead.unit_type != order.unit_type ||
                lead.build_tech_level != order.build_tech_level ||
                lead.target_uid != order.target_uid ||
                std::abs(lead.x - order.x) > 1.0f ||
                std::abs(lead.y - order.y) > 1.0f) {
                unit.formation_projection_allowed = false;
                unit.attack_move_private_navigation_handoff =
                    active_attack_move && unit.attack_move_combat_target_valid;
            }
        }
    }

    // y 2552+: exhausted paths re-request toward the order point,
    // truncated ones at s < 450 and complete ones once s reaches
    // zero (unreachable orders stay active and retry). Air MOVE
    // activation first exposes y.k/l/m with no av node; the generic
    // controller creates that one-node path on the next object turn.
    const bool air_path_missing =
        unit.move_type == MoveType::AIR &&
        // A live ATTACK_MOVE helper has already published l/m/n and j.
        // Its private request is resolved after leader validation, using
        // that combat point rather than installing the outer order point.
        !unit.attack_move_private_navigation_handoff &&
        unit.spawn_delay_frames == 0.0f &&
        unit.has_move_target && !unit.using_path &&
        !unit.path_request_pending &&
        !unit.factory_exit_path_pending &&
        (unit.formation_leader_uid == 0 ||
         !unit.formation_projection_allowed);
    const bool linked_private_path_missing =
        unit.formation_leader_uid != 0 &&
        !unit.formation_projection_allowed &&
        unit.spawn_delay_frames == 0.0f &&
        ((!unit.order_queue.empty() &&
          unit.order_queue.front().type == WeaponType::MOVE) ||
         unit.target_uid == 0 ||
         (active_attack_move &&
          (!attack_move_main_target_valid ||
           attack_move_retains_outer_proposal))) &&
        unit.has_move_target &&
        !unit.using_path && !unit.path_request_pending;
    const bool formation_rebuild_path_missing =
        unit.formation_rebuild_path_pending &&
        unit.spawn_delay_frames == 0.0f &&
        unit.formation_is_leader && unit.has_move_target &&
        !unit.using_path && !unit.path_request_pending;
    const bool ordinary_path_missing =
        unit.formation_leader_uid == 0 && unit.has_move_target &&
        !unit.using_path && !unit.path_request_pending &&
        !unit.factory_exit_path_pending && unit.spawn_delay_frames == 0.0f &&
        !unit.order_queue.empty() &&
        (unit.order_queue.front().type == WeaponType::MOVE ||
         (unit.order_queue.front().type == WeaponType::ATTACK_MOVE &&
          (!attack_move_main_target_valid ||
           attack_move_retains_outer_proposal)) ||
         (unit.order_queue.front().type == WeaponType::PATROL &&
          !unit.patrol_logical_target_active && !recent_order_chase_active &&
          !single_patrol_holds_position()));
    if (!unit.order_queue.empty() &&
        !unit.direct_attack_chase_this_update &&
        (ordinary_path_missing ||
         (linked_private_path_missing &&
          (unit.order_queue.front().type == WeaponType::MOVE ||
           unit.order_queue.front().type == WeaponType::ATTACK_MOVE ||
           unit.order_queue.front().type == WeaponType::PATROL)))) {
        // The generic controller found no af before deciding whether s
        // permits a replacement. A follower whose projection is suppressed
        // also waits for a private node. Keep k/l/m visible and coast either
        // way, applying the normal r=150 drift gate even without av/aw.
        const UnitOrder& active = unit.order_queue.front();
        if ((std::abs(unit.last_request_x - active.x) > 150.0f ||
             std::abs(unit.last_request_y - active.y) > 150.0f) &&
            unit.repath_cooldown_frames > 30.0f) {
            unit.repath_cooldown_frames = 30.0f;
        }
        unit.movement_activation_delay_frames = combat_delta_frames_;
        unit.movement_delay_keeps_prediction = false;
    }
    const bool attack_move_holds_for_target =
        !unit.order_queue.empty() &&
        unit.order_queue.front().type == WeaponType::ATTACK_MOVE &&
        attack_move_main_target_valid &&
        !unit.attack_move_combat_target_valid;
    const bool replay_replacement_attack_hold =
        unit.formation_is_leader && !unit.order_queue.empty() &&
        unit.order_queue.front().type == WeaponType::ATTACK_MOVE &&
        unit.order_queue.front().replay_command_group_size >= 19 &&
        unit.order_queue.front().replay_command_has_handoff_cache &&
        !unit.has_move_target;
    if ((!unit.has_move_target || air_path_missing ||
         linked_private_path_missing ||
         formation_rebuild_path_missing || ordinary_path_missing) &&
        !patrol_passive_target_stopped_this_update &&
        !patrol_passive_target_in_range_this_update &&
        !formation_patrol_failure_hold &&
        !attack_move_holds_for_target &&
        !replay_replacement_attack_hold &&
        !unit.order_queue.empty() &&
        unit.is_mobile() &&
        ((unit.path_incomplete &&
          unit.repath_cooldown_frames -
              (ordinary_path_missing ? unapplied_cooldown_delta_frames : 0.0f) < 450.0f) ||
         unit.repath_cooldown_frames <=
             (ordinary_path_missing ? unapplied_cooldown_delta_frames : 0.0f))) {
        UnitOrder& active = unit.order_queue.front();
        if (active.type == WeaponType::MOVE ||
            active.type == WeaponType::ATTACK_MOVE ||
            (active.type == WeaponType::PATROL &&
             !single_patrol_holds_position())) {
            const bool synchronous_path_rebuild_without_active_path =
                !unit.using_path && !unit.path_request_pending;
            unit.path_incomplete = false;
            if (formation_rebuild_path_missing) {
                move_unit_to_async_(unit, active.x, active.y);
            } else if (synchronous_path_rebuild_without_active_path &&
                       (ordinary_path_missing ||
                        (active_attack_move && !attack_move_main_target_valid &&
                         !linked_private_path_missing))) {
                // y.a() samples the old af before this request. Direct lines
                // expose new nodes immediately, solved caches remain aU until
                // the next object turn, and neither can steer this update.
                move_unit_to_async_(unit, active.x, active.y);
                unit.movement_activation_delay_frames = combat_delta_frames_;
                unit.movement_delay_keeps_prediction = false;
            } else if (linked_private_path_missing) {
                // Positional followers submit this private route through
                // the asynchronous grid worker. The request stays visible
                // while its solved nodes remain hidden until a later object
                // update; a synchronous call would expose the whole chain
                // on the command frame.
                const bool requeue_stale_replay_prepath =
                    active.type == WeaponType::ATTACK_MOVE &&
                    active.replay_command_group_size == 8 &&
                    active.has_prepath &&
                    active.prepath_attach_frame < tick_;
                if (requeue_stale_replay_prepath) {
                    // A follower can retain the shared command prepath after
                    // its first request. JAR queues that route again on a
                    // later private-path refresh instead of exposing the
                    // cached nodes during the refresh frame.
                    active.has_prepath = false;
                }
                move_unit_to_async_(unit, active.x, active.y);
            } else {
                move_unit_to(unit, active.x, active.y);
                if (synchronous_path_rebuild_without_active_path) {
                    // y.k() can expose a newly built av/aw chain after the
                    // object-turn controller already found no active node.
                    // The chain is visible now, but y.a() coasts once before
                    // enabling movement prediction for the new request.
                    unit.movement_activation_delay_frames = 1.0f;
                    unit.movement_delay_keeps_prediction = false;
                }
            }
            if (linked_private_path_missing) {
                // aE() was captured before the private path was
                // created. The nodes are exposed immediately, but
                // this update coasts without steering toward node 0.
                unit.movement_activation_delay_frames = combat_delta_frames_;
                unit.movement_delay_keeps_prediction = false;
            }
            if (formation_rebuild_path_missing) {
                unit.formation_rebuild_path_pending = false;
                unit.movement_activation_delay_frames = 1.0f;
                unit.movement_delay_keeps_prediction = false;
            }
            if (unit.move_type == MoveType::AIR) {
                unit.using_path = true;
                unit.waypoints.clear();
                unit.current_waypoint = 0;
                unit.repath_cooldown_frames = 500.0f;
                if (air_path_missing) {
                    // aE() was sampled before y.a(...) installed the
                    // air node, so this update only coasts.
                    unit.movement_activation_delay_frames = 1.0f;
                    unit.movement_delay_keeps_prediction = false;
                }
            }
            if (active.type == WeaponType::PATROL && unit.using_path) {
                // The generic retry writes av independently of the l/m
                // proposal already published by the PATROL handler.
                unit.path_target_override_active = true;
                unit.path_target_override_x = unit.target_x;
                unit.path_target_override_y = unit.target_y;
                unit.target_x = active.x;
                unit.target_y = active.y;
            }
        }
    }

    if (unit.has_guard_pos && unit.arrived && !unit.has_move_target) {
        f32 d = dist(unit.x, unit.y, unit.guard_x, unit.guard_y);
        if (d > EngineConstants::ARRIVAL_THRESHOLD * 2.0f) {
            if (unit.repath_cooldown_frames > 90.0f) {
                unit.repath_cooldown_frames = 90.0f;
            }
            if (unit.repath_cooldown_frames == 0.0f) {
                move_unit_to(unit, unit.guard_x, unit.guard_y);
            }
        }
    }

    if (unit.has_move_target &&
        !unit.order_queue.empty() &&
        ((unit.order_queue.front().type == WeaponType::GUARD &&
          !unit.guard_combat_chase_outer) ||
         unit.order_queue.front().type == WeaponType::GUARD_AT ||
         unit.order_queue.front().type == WeaponType::FOLLOW) &&
        unit.target_uid != 0 && has_weapon(unit.weapon_type)) {
        Unit* current_main_target = nullptr;
        World* current_main_world = nullptr;
        for (World* candidate_world : other_worlds) {
            if (!candidate_world || candidate_world == this ||
                !is_enemy_with(*candidate_world)) continue;
            Unit* candidate = candidate_world->get_unit(unit.target_uid);
            if (candidate && candidate->is_alive &&
                candidate->parent_uid == 0) {
                current_main_world = candidate_world;
                current_main_target = candidate;
                break;
            }
        }
        bool slot_has_target = false;
        for (const WeaponSlot& slot : unit.weapon_slots) {
            if (slot.fire_enabled && slot.target_uid != 0) {
                slot_has_target = true;
                break;
            }
        }
        if (current_main_target && current_main_world &&
            slot_has_target &&
            can_target_(unit, *current_main_target)) {
            f32 active_weapon_range = unit.weapon_range;
            if (unit.attack_range_adds_radii) {
                active_weapon_range +=
                    unit.radius + current_main_target->radius;
            }
            const UnitOrder& active_guard_order = unit.order_queue.front();
            bool retain_large_replay_guard_path =
                active_guard_order.type == WeaponType::GUARD &&
                active_guard_order.replay_command_group_size >= 19 &&
                unit.formation_leader_uid != 0 &&
                unit.formation_group_size > 1 &&
                unit.using_path &&
                remaining_navigation_points(unit) >= 17;
            if (retain_large_replay_guard_path &&
                unit.guard_target_uid != 0) {
                World* guarded_world = order_target_world_(
                    unit.guard_target_team_id, other_worlds);
                Unit* guarded = guarded_world
                    ? guarded_world->get_unit(unit.guard_target_uid)
                    : nullptr;
                retain_large_replay_guard_path =
                    guarded && guarded->is_alive && guarded->parent_uid == 0 &&
                    unit.distance_to(*guarded) > 500.0f;
            } else {
                retain_large_replay_guard_path = false;
            }
            if (!retain_large_replay_guard_path &&
                dist2(unit.x, unit.y,
                      current_main_target->x,
                      current_main_target->y) <
                active_weapon_range * active_weapon_range) {
                // y.c(GUARD/FOLLOW/GUARD_AT): once R is inside
                // o(target) and at least one weapon slot is actively
                // tracking, k is cleared but av/aw remain intact.
                unit.has_move_target = false;
                unit.arrived = true;
            }
        }
    }
    return true;
}

}  // namespace rwrl
