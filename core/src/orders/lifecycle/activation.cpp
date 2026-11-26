#include "world_internal.h"

#include <cstdio>
#include <cstdlib>

namespace rwrl {
World* World::find_order_target_world_(
    u32 target_uid, const Vec<World*>& other_worlds) {
    if (target_uid == 0) return nullptr;
    if (get_unit(target_uid)) return this;
    for (World* other : other_worlds) {
        if (other && other->get_unit(target_uid)) return other;
    }
    return nullptr;
}

void World::clear_current_order_state_(Unit& unit,
                                       bool preserve_movement_proposal,
                                       bool preserve_formation_timers) {
    unit.service_range_extended = false;
    unit.active_order_elapsed_frames = 0.0f;
    // y.ay() clears V, Y and W together.
    unit.order_point_dwell_frames = 0.0f;
    unit.bomber_attack_heading = -999.0f;
    unit.bomber_retreating = false;
    unit.bomber_target_search_done = false;
    unit.movement_activation_delay_frames = 0.0f;
    unit.build_path_request_awaits_dispatch = false;
    if (!preserve_formation_timers) {
        unit.formation_idle_leader_frames = 0.0f;
        unit.formation_path_fallback_frames = 0.0f;
        unit.formation_path_failure_frames = 0.0f;
    }
    unit.movement_activation_delay_frames = 0.0f;
    unit.movement_delay_keeps_prediction = false;
    unit.movement_delay_uses_acceleration = false;
    unit.service_final_waypoint_coast = false;
    unit.attack_move_final_node_coast_pending = false;
    unit.patrol_final_node_coast_pending = false;
    unit.path_request_prediction_suppressed = false;
    unit.attack_order_had_active_air_path = false;
    unit.attack_order_had_main_target = false;
    unit.path_retry_next_frame = false;
    unit.activate_order_next_frame = false;
    unit.guard_replacement_keeps_movement = false;
    unit.guard_replacement_from_patrol = false;
    unit.guard_replacement_refresh_next_frame = false;
    unit.formation_path_activation_pending = false;
    unit.formation_rebuild_handoff_pending = false;
    unit.single_node_path_counter = 0;
    if (!preserve_movement_proposal) {
        unit.has_move_target = false;
        unit.arrived = true;
    }
    // Command-time az()/aH() leave k/l/m and cK untouched. A passenger has
    // no subsequent navigation turn to replace that movement proposal.
    unit.has_deferred_navigation_target = false;
    // y.aH() clears the adopted av/aw chain but deliberately leaves y.aU
    // alive. A solver request can therefore finish after its originating
    // waypoint was replaced and is adopted by the next object update.
    // Once the active chain is gone the request no longer has a path to
    // preserve, so keep the request itself but use ordinary pending motion.
    unit.path_request_preserves_active_path = false;
    unit.attack_order_target_x = 0.0f;
    unit.attack_order_target_y = 0.0f;
    unit.transport_order_target_x = 0.0f;
    unit.transport_order_target_y = 0.0f;
    unit.attack_move_combat_target_valid = false;
    unit.patrol_logical_target_active = false;
    unit.patrol_logical_target_x = 0.0f;
    unit.patrol_logical_target_y = 0.0f;
    unit.guard_movement_target_uid = 0;
    unit.guard_movement_target_team_id = -1;
    // y.ay()/az() clear w when the active waypoint is completed or replaced.
    // The former GUARD return state cannot latch a later combat chase.
    unit.guard_combat_chase_outer = false;
    // y.ay() → aH() when the current order is dropped or completed: s=0,
    // W=0, X=0, aw=0. The repath cooldown must not survive an order
    // replacement, or an in-range ATTACK that replaces a MOVE order keeps a
    // stale s and cannot request the chase path once the target exits range.
    unit.repath_cooldown_frames = 0.0f;
    unit.waypoint_stall_frames = 0.0f;
    unit.last_waypoint_stall_frames = 0.0f;
    unit.target_uid = 0;
    unit.target_team_id = -1;
    unit.unarmed_main_target_uid = 0;
    unit.unarmed_main_target_team_id = -1;
    unit.service_combat_target_uid = 0;
    unit.service_combat_target_team_id = -1;
    unit.using_path = false;
    unit.waypoints.clear();
    unit.current_waypoint = 0;
    clear_navigation_orders(unit);
    restore_default_weapon_type(registry_, unit);
}

bool World::activate_order_(Unit& unit, const UnitOrder& order,
                            const Vec<World*>& other_worlds) {
    const u32 main_target = unit.service_combat_target_uid != 0
        ? unit.service_combat_target_uid : unit.main_target_uid();
    const i32 main_target_team = unit.service_combat_target_uid != 0
        ? unit.service_combat_target_team_id : unit.main_target_team_id();
    const bool guard_replacement_keeps_movement =
        order.type == WeaponType::GUARD &&
        unit.guard_replacement_keeps_movement;
    unit.guard_replacement_keeps_movement = false;
    World* target_world = order.target_uid != 0
        ? order_target_world_(order.target_team_id, other_worlds) : nullptr;
    Unit* target = target_world ? target_world->get_unit(order.target_uid) : nullptr;

    switch (order.type) {
    case WeaponType::MOVE:
        if (unit.path_request_pending) {
            if (unit.path_request_uses_prepath &&
                unit.path_request_prepath_submission_tick == tick_ &&
                unit.order_phase_tick != tick_ &&
                !order.replay_prepath_cache.empty()) {
                // Multiple replay commands can replace a MOVE before the
                // unit has dispatched that turn. Reselect that unexecuted
                // waypoint's cache; after an object update, aU already owns
                // its old result and survives the replacement instead.
                move_unit_to_async_(unit, order.x, order.y, 0);
                unit.movement_activation_delay_frames = 1.0f;
                unit.movement_delay_uses_acceleration =
                    unit.reversing && unit.move_speed_factor < 0.0f;
                return true;
            }
            // Native replay commands can replace the waypoint after this
            // unit's object turn. y.az()/aH() exposes the new l/m proposal
            // without cancelling the older aU request; y.b(engine) adopts
            // that request at the start of the following object update.
            unit.target_x = order.x;
            unit.target_y = order.y;
            unit.has_move_target = true;
            unit.arrived = false;
            unit.using_path = false;
            unit.waypoints.clear();
            unit.current_waypoint = 0;
            unit.movement_activation_delay_frames = 1.0f;
            unit.movement_delay_uses_acceleration =
                unit.reversing && unit.move_speed_factor < 0.0f;
            return true;
        }
        if (unit.spawn_delay_frames > 0.0f) {
            // With am.cl still positive, y.a(...,ad,...) exposes the new
            // order point directly through ad.e/f. It does not request or
            // adopt a path until cl reaches zero.
            unit.target_x = order.x;
            unit.target_y = order.y;
            unit.has_move_target = true;
            unit.arrived = false;
            unit.using_path = false;
            unit.waypoints.clear();
            unit.current_waypoint = 0;
            return true;
        }
        // y.a() submits a grid request for a newly activated MOVE and leaves
        // the path empty until the delayed solver result is adopted. The
        // line/prepath branches inside move_unit_to_ remain immediate.
        if (dist2(unit.x, unit.y, order.x, order.y) < 49.0f) {
            // The ordered-point arrival dispatcher precedes generic path
            // creation. Let it finish this near waypoint without starting
            // a worker that could later outlive the completed order.
            return true;
        }
        move_unit_to_async_(unit, order.x, order.y, 0);
        unit.movement_activation_delay_frames = 1.0f;
        unit.movement_delay_uses_acceleration =
            unit.reversing && unit.move_speed_factor < 0.0f;
        return true;
    case WeaponType::ATTACK: {
        if (!target || !target->is_alive || target->parent_uid != 0 || target_world == this) {
            // Keep the accepted waypoint for y.g()'s invalid-target branch.
            // It may adopt the retained R target before deciding to finish;
            // cargo targets also remain available to the order handler.
            unit.weapon_type = WeaponType::ATTACK;
            return true;
        }
        if (unit.bomber_attack_movement) {
            // g(ATTACK) adjusts the bomber's flee destination before the
            // generic controller requests a route. Requesting the target's
            // center here would leave a different av on the first turn.
            unit.weapon_type = WeaponType::ATTACK;
            return true;
        }
        // The typed ATTACK waypoint and y.R's passive/main combat target are
        // independent. Activating a new attack order must not replace an
        // existing valid main target that the weapon slots are tracking.
        const u32 saved_main_target_uid = unit.target_uid;
        const i32 saved_main_target_team_id = unit.target_team_id;

        // y.g(float, au, ad) resolves the formation projection gate before
        // the common y.a(...) path request.  In particular, a linked ATTACK
        // follower near the target must submit its private path on this same
        // object turn; allowing attack_unit() to request first makes the path
        // helper take the projection-only branch and loses that request.
        if (unit.formation_leader_uid != 0) {
            Unit* leader = formation_unit_(unit.formation_leader_uid);
            const f32 follower_distance_sq = dist2(
                unit.x, unit.y, target->x, target->y);
            bool leader_has_replay_path = false;
            if (leader && !unit.order_queue.empty()) {
                const UnitOrder& active = unit.order_queue.front();
                for (const UnitOrderPrepath& cached :
                     active.replay_prepath_cache) {
                    // A two-node cache is the JAR's short final hop. It does
                    // not trigger the follower's private ATTACK route; the
                    // full handoff path that needs the gate has at least one
                    // intermediate node.
                    if (cached.path.size() <= 2 ||
                        cached.move_type != unit.move_type ||
                        cached.frame + 180 < tick_ ||
                        std::abs(cached.target_x - target->x) > 10.0f ||
                        std::abs(cached.target_y - target->y) > 10.0f ||
                        dist2(leader->x, leader->y,
                              cached.start_x, cached.start_y) >= 3600.0f) {
                        continue;
                    }
                    leader_has_replay_path = true;
                    break;
                }
            }
            if (leader && leader_has_replay_path &&
                unit.formation_projection_disabled_frames >= 120.0f &&
                follower_distance_sq < 490000.0f) {
                const f32 leader_distance_sq = dist2(
                    leader->x, leader->y, target->x, target->y);
                const bool leader_has_attack_target =
                    leader->target_uid == target->uid &&
                    leader->target_team_id == target_world->team_id_;
                if (follower_distance_sq < 48400.0f ||
                    leader_distance_sq < 48400.0f ||
                    (unit.attack_range_adds_radii &&
                     leader_distance_sq < 270400.0f) ||
                    leader_has_attack_target) {
                    unit.formation_projection_allowed = false;
                }
            }
        }
        attack_unit(unit, *target);
        if (saved_main_target_uid != 0) {
            unit.target_uid = saved_main_target_uid;
            unit.target_team_id = saved_main_target_team_id;
        } else {
            // attack_unit() also serves the direct World API and assigns R.
            // Native waypoint activation adopts a new R only inside o(target)
            // after aq.a() accepts its layer. A rejected target must leave R
            // null so the later order pass does not spuriously refresh S.
            unit.target_uid = 0;
            unit.target_team_id = -1;
            f32 main_attack_range = unit.weapon_range;
            if (unit.attack_range_adds_radii) {
                main_attack_range += unit.radius + target->radius;
            }
            if (dist2(unit.x, unit.y, target->x, target->y) <
                    main_attack_range * main_attack_range &&
                can_target_(unit, *target)) {
                unit.target_uid = target->uid;
                unit.target_team_id = target_world->team_id_;
                // y.a(delta,target,ad,false) adopts the in-range explicit
                // ATTACK target as R and calls M(-1) once. attack_unit() has
                // already assigned R, so the later order update cannot
                // observe that transition.
                const i32 target_adoption_frame = tick_ +
                    (command_path_request_active_ ? 1 : 0);
                refresh_all_weapon_aim_offsets(
                    unit, target, target_adoption_frame);
            }
        }
        const bool formation_projection_only =
            unit.formation_leader_uid != 0 &&
            unit.formation_projection_allowed &&
            unit.has_move_target && !unit.arrived &&
            !unit.using_path &&
            unit.repath_cooldown_frames == 0.0f;
        // y.a(delta,target,ad,false) raises y.j -- the fallback that lets
        // ad.b steer straight from the l/m proposal while aE() still has no
        // node -- only when the target is inside 900 units (810000 squared)
        // and ct()/aV() holds, which for the stock hierarchy means a flying
        // unit. Beyond that range ad.b stays clear and the physical pass
        // coasts for one frame exactly as a ground chase does.
        const bool proposal_steers_immediately =
            // The generic controller uses current l/m directly while cl is
            // positive, including a ground factory product replacing MOVE
            // with ATTACK before its exit path starts.
            unit.spawn_delay_frames > 0.0f ||
            (unit.move_type == MoveType::AIR &&
             dist2(unit.x, unit.y, target->x, target->y) < 810000.0f);
        unit.movement_activation_delay_frames =
            proposal_steers_immediately || formation_projection_only
                ? 0.0f : 1.0f;
        // A replay ATTACK that replaces an active path preserves the
        // reversing coast rate through its activation frame. Ordinary
        // asynchronous ATTACK requests retain the braking branch.
        const bool replay_attack_replaces_active_path_in_reverse =
            order.replay_prepath_command_had_active_order &&
            !order.replay_prepath_cache.empty() && unit.reversing &&
            unit.move_speed_factor < 0.0f;
        unit.movement_delay_uses_acceleration =
            unit.movement_activation_delay_frames > 0.0f &&
            unit.move_speed_factor < 0.0f &&
            (unit.path_request_uses_prepath ||
             replay_attack_replaces_active_path_in_reverse);
        return true;
    }
    case WeaponType::BUILD: {
        if (!registry_.can_build_unit(unit.unit_type, order.unit_type,
                                       order.build_tech_level)) return false;
        // A custom unit's construction menu is independent of its current
        // attack/repair state. Select the BUILD handler for this waypoint.
        unit.weapon_type = WeaponType::BUILD;
        unit.service_combat_target_uid = main_target;
        unit.service_combat_target_team_id = main_target ? main_target_team : -1;
        unit.target_uid = 0;
        unit.target_team_id = -1;
        // y.c() dispatches an already-in-range positional BUILD directly to
        // the builder handler. That handler turns toward the placement point
        // and suspends movement; submitting the generic path here exposes
        // av/aw one object update too early.
        if (dist(unit.x, unit.y, order.x, order.y) <=
            service_range_to_(unit, order.unit_type)) {
            return true;
        }
        // e.f/aA() can append BUILD without clearing an idle av chain or
        // its s gate. y.f() publishes the placement proposal, then the
        // generic controller decides whether that retained state needs a
        // request. Activation must not replace it with a fresh coast path.
        if (unit.spawn_delay_frames > 0.0f ||
            unit.using_path || unit.path_request_pending ||
            unit.repath_cooldown_frames > 0.0f) {
            return true;
        }
        move_unit_to_async_(unit, order.x, order.y,
                            service_path_target_size_(unit, order.unit_type));
        unit.movement_activation_delay_frames = 1.0f;
        unit.build_path_request_awaits_dispatch = true;
        return true;
    }
    case WeaponType::REPAIR:
        if (!target || !target->is_alive || target->parent_uid != 0 || target->uid == unit.uid ||
            !can_repair_target(unit, *target)) return false;
        unit.service_combat_target_uid = main_target;
        unit.service_combat_target_team_id = main_target ? main_target_team : -1;
        unit.target_uid = target->uid;
        unit.target_team_id = target_world->team_id_;
        if (unit.path_request_pending) {
            // Native replay commands replace the waypoint after this unit's
            // object turn. y.az()/aH() re-exposes the service proposal
            // without cancelling the older aU request, exactly as the MOVE
            // branch above does for a positional waypoint.
            unit.target_x = target->x;
            unit.target_y = target->y;
            unit.has_move_target = true;
            unit.arrived = false;
        }
        if (!target->is_complete() && target_world == this && unit.can_reclaim) {
            unit.weapon_type = WeaponType::BUILD;
            bool tracked_build = false;
            for (BuildTask& task : pending_builds_) {
                if (task.target_uid == target->uid) {
                    task.requires_builder = true;
                    tracked_build = true;
                    break;
                }
            }
            if (!tracked_build) unit.weapon_type = WeaponType::REPAIR;
        } else {
            unit.weapon_type = WeaponType::REPAIR;
        }
        return true;
    case WeaponType::LOAD_INTO:
        // au.h owns the transport reference; y.R remains an independent
        // combat target while an armed passenger approaches the transport.
        // Target validity and ownership belong to the load dispatcher after
        // formation setup, including an unresolved au.h.
        unit.weapon_type = WeaponType::LOAD_INTO;
        return true;
    case WeaponType::UNLOAD_AT:
        // Stock y.k(float) has no dispatch branch for av.unloadAt. The
        // waypoint remains current and does not invoke the transport's UI
        // unload action.
        return true;
    case WeaponType::RECLAIM:
        if (!target || !target->is_alive || target->parent_uid != 0 || target->uid == unit.uid ||
            !can_reclaim_target(unit, *target)) return false;
        if (target->unit_type != "crystalResource" && target_world != this &&
            !is_allied_with(*target_world)) return false;
        unit.service_combat_target_uid = main_target;
        unit.service_combat_target_team_id = main_target ? main_target_team : -1;
        unit.target_uid = target->uid;
        unit.target_team_id = target_world->team_id_;
        unit.weapon_type = WeaponType::RECLAIM;
        return true;
    case WeaponType::ATTACK_MOVE:
        if (unit.path_request_pending) {
            // y.az()/aH() replace the waypoint and clear av/aw, but retain
            // aU for the generic controller's next b(l). Publish the new
            // logical point without replacing the old request or countdown
            // with this command's cache, regardless of formation size.
            unit.target_x = order.x;
            unit.target_y = order.y;
            unit.has_move_target = true;
            unit.arrived = false;
            unit.weapon_type = WeaponType::ATTACK_MOVE;
            // y.k() still dispatches the combat proposal while the older
            // aU waits. The physical request handler supplies the coast;
            // an activation delay here would suppress live l/m for a turn.
            unit.movement_activation_delay_frames = 0.0f;
            return true;
        }
        // y.a(FFIZZ) probes the direct line before looking at ab.g. A
        // serialized command cache does not force a grid request, including
        // when a large formation replaces an active waypoint.
        if (unit.target_uid != 0 && unit.target_team_id >= 0) {
            World* main_target_world = order_target_world_(
                unit.target_team_id, other_worlds);
            Unit* main_target = main_target_world
                ? main_target_world->get_unit(unit.target_uid) : nullptr;
            if (main_target && main_target->is_alive &&
                main_target->parent_uid == 0) {
                // The MOVE-family dispatch calls y.a(delta,R,ad,true)
                // before generic pathing. Inside follow range it stops;
                // outside it requests the combat target on this object turn.
                // Leave that choice to the order pass so the command point
                // cannot install a path or a one-frame activation delay first.
                unit.weapon_type = WeaponType::ATTACK_MOVE;
                unit.has_move_target = false;
                unit.arrived = true;
                unit.using_path = false;
                unit.waypoints.clear();
                unit.current_waypoint = 0;
                unit.movement_activation_delay_frames = 0.0f;
                return true;
            }
        }
        unit.weapon_type = WeaponType::ATTACK_MOVE;
        if (unit.spawn_delay_frames > 0.0f) {
            // The generic controller's am.cl gate publishes the new l/m
            // directly as ad.e/f. Factory launch delay suppresses path
            // creation while retaining this update's steering and speed.
            unit.target_x = order.x;
            unit.target_y = order.y;
            unit.has_move_target = true;
            unit.arrived = false;
            unit.using_path = false;
            unit.waypoints.clear();
            unit.current_waypoint = 0;
            return true;
        }
        if (dist2(unit.x, unit.y, order.x, order.y) < 49.0f) {
            // A live retained R was handled above. With no such target,
            // attackMove completes inside 7 before requesting any route.
            return true;
        }
        move_unit_to_async_(unit, order.x, order.y, 0);
        // A linked follower's short cache can reach activation on its object
        // turn. Its new formation already supplies the physical proposal;
        // the private request must not suspend that same-turn projection.
        unit.movement_activation_delay_frames =
            !command_path_request_active_ &&
            unit.formation_leader_uid != 0 &&
            unit.formation_projection_allowed ? 0.0f : 1.0f;
        return true;
    case WeaponType::LOAD_UP:
        if (!target || target_world != this || unit.cargo_capacity <= 0 ||
            !target->transportable || target->team_id != unit.team_id) return false;
        unit.weapon_type = WeaponType::LOAD_UP;
        return true;
    case WeaponType::PATROL:
        patrol_unit_to(unit, order.x, order.y);
        return true;
    case WeaponType::GUARD:
        if (!target || !target->is_alive) return false;
        {
        bool tracks_guard_target = false;
        bool tracks_retained_main_target = false;
        for (const WeaponSlot& slot : unit.weapon_slots) {
            if (unit.target_uid != 0 && slot.target_uid == target->uid) {
                tracks_guard_target = true;
            }
            if (unit.target_uid != 0 &&
                slot.target_uid == unit.target_uid) {
                tracks_retained_main_target = true;
            }
        }
        const bool prior_movement_prediction =
            unit.movement_prediction_enabled;
        guard_unit(unit, *target_world, *target);
        // A replacement GUARD with no route can process immediately. Once
        // guard_unit has produced a path, the command tick only exposes it:
        // steering starts on the following object update unless a weapon
        // slot and a retained main chase kept the movement controller live
        // across the replacement. Cached slot targets alone cannot supply
        // the native j proposal when R is absent.
        unit.movement_activation_delay_frames =
            !guard_replacement_keeps_movement &&
            ((!prior_movement_prediction) ||
             ((!tracks_guard_target &&
               (unit.using_path ||
                (unit.strafes
                     ? (unit.vel_x != 0.0f || unit.vel_y != 0.0f)
                     : unit.move_speed_factor != 0.0f))) ||
             (unit.formation_group_size > 1 &&
              (unit.strafes
                   ? (unit.vel_x != 0.0f || unit.vel_y != 0.0f)
                   : unit.move_speed_factor != 0.0f))))
                ? 1.0f : 0.0f;
        return true;
        }
    case WeaponType::GUARD_AT:
        guard_unit_at(unit, order.x, order.y);
        // c(GUARD_AT) requests after capturing aE(); a new direct path
        // cannot steer until the following object update.
        unit.movement_activation_delay_frames = 1.0f;
        return true;
    case WeaponType::TOUCH_TARGET:
        if (!target || !target->is_alive || target->parent_uid != 0 || target_world == this ||
            !is_enemy_with(*target_world)) return false;
        touch_target(unit, *target);
        return true;
    case WeaponType::FOLLOW:
        if (!target || !target->is_alive) return false;
        follow_unit(unit, *target_world, *target);
        unit.movement_activation_delay_frames = 1.0f;
        return true;
    case WeaponType::TRIGGER_ACTION:
    case WeaponType::TRIGGER_WHEN_IN_RANGE:
        // Canonical units have no custom action payload for these protocol
        // waypoints. Stock y.k(float) consumes a missing action id without
        // mutating the unit.
        return true;
    case WeaponType::SET_PASSIVE_TARGET:
        if (target) {
            unit.restore_main_target(target->uid, target_world->team_id_);
            unit.target_refresh_timer_frames = std::min(
                unit.target_refresh_timer_frames, 5.0f);
        }
        return true;
    default:
        return false;
    }
}

} // namespace rwrl
