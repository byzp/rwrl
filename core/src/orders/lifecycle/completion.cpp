#include "world_internal.h"

#include <cstdio>
#include <cstdlib>

namespace rwrl {

void World::complete_formation_followers_(Unit& leader,
                                         const Vec<World*>& other_worlds) {
    // y.ax() invokes aC() before ay(): nearby followers with an au.b()-equal
    // second waypoint advance now. Their own object turn activates that
    // waypoint, so creation order determines whether it runs in this frame.
    const UnitOrder* next = leader.order_queue.size() > 1
        ? &leader.order_queue[1] : nullptr;
    Vec<World*> worlds = other_worlds;
    worlds.insert(worlds.begin(), this);
    for (World* world : worlds) {
        if (!world) continue;
        for (auto& [uid, follower] : world->units_) {
            if (follower.formation_leader_uid != leader.uid) continue;
            const UnitOrder* follower_next = follower.order_queue.size() > 1
                ? &follower.order_queue[1] : nullptr;
            if (!next && !follower_next) continue;
            const bool matching_next = next && follower_next &&
                next->type == follower_next->type &&
                next->unit_type == follower_next->unit_type &&
                next->target_uid == follower_next->target_uid &&
                next->target_team_id == follower_next->target_team_id &&
                std::abs(next->x - follower_next->x) <= 1.0f &&
                std::abs(next->y - follower_next->y) <= 1.0f;
            if (!matching_next ||
                dist2(leader.x, leader.y, follower.x, follower.y) >= 108900.0f) {
                // y.a(null) drops only the link, preserving slot metadata.
                follower.formation_leader_uid = 0;
                continue;
            }
            const bool exposed_move = follower.has_move_target;
            const bool arrived = follower.arrived;
            f32 exposed_x = follower.target_x;
            f32 exposed_y = follower.target_y;
            const UnitOrder& current = follower.order_queue.front();
            if (exposed_move) {
                if (current.type == WeaponType::LOAD_INTO ||
                    current.type == WeaponType::LOAD_UP) {
                    exposed_x = follower.transport_order_target_x;
                    exposed_y = follower.transport_order_target_y;
                } else if (current.target_uid != 0) {
                    World* target_world = world->order_target_world_(
                        current.target_team_id, worlds);
                    const Unit* target = target_world
                        ? target_world->get_unit(current.target_uid) : nullptr;
                    if (target) {
                        exposed_x = target->x;
                        exposed_y = target->y;
                    }
                } else {
                    exposed_x = current.x;
                    exposed_y = current.y;
                }
            }
            const u32 main_target = follower.main_target_uid();
            const i32 main_target_team = follower.main_target_team_id();
            // aC() calls ay() with a matching queued waypoint still behind
            // the current one. That branch preserves y.e/c/d across the
            // handoff, including followers whose object turn already ended.
            world->clear_current_order_state_(follower, false, true);
            follower.order_queue.erase(follower.order_queue.begin());
            follower.restore_main_target(main_target, main_target_team);
            // ay()/aH() clear the path, but leave k/l/m and cK from an
            // already-finished object turn visible until its next update.
            follower.has_move_target = exposed_move;
            follower.arrived = arrived;
            follower.target_x = exposed_x;
            follower.target_y = exposed_y;
            follower.activate_order_next_frame = true;
        }
    }
}

void World::complete_current_order_(Unit& unit,
                                    const Vec<World*>& other_worlds,
                                    bool cycle_to_back,
                                    bool defer_next_order_activation) {
    if (unit.order_queue.empty()) return;
    if (cycle_to_back && unit.order_queue.size() > 1) {
        // PATROL dispatch appends a copy of its current au, then calls ax().
        // aC() advances nearby followers whose second waypoint matches
        // before ay() exposes this leader's next loop point. A follower
        // whose object turn comes later can dispatch that point this frame.
        complete_formation_followers_(unit, other_worlds);
    }
    UnitOrder completed = unit.order_queue.front();
    const bool preserve_main_target =
        unit.has_armed_service_order() ||
        completed.type == WeaponType::BUILD ||
        completed.type == WeaponType::REPAIR ||
        completed.type == WeaponType::RECLAIM ||
        completed.type == WeaponType::MOVE ||
        completed.type == WeaponType::ATTACK_MOVE ||
        completed.type == WeaponType::PATROL ||
        completed.type == WeaponType::GUARD ||
        completed.type == WeaponType::GUARD_AT ||
        completed.type == WeaponType::LOAD_INTO ||
        completed.type == WeaponType::LOAD_UP;
    const u32 saved_main_target_uid = preserve_main_target
        ? unit.main_target_uid() : 0;
    const i32 saved_main_target_team_id = preserve_main_target
        ? unit.main_target_team_id() : -1;
    // y.ay(): finishing an order resets the stall-recovery counters.
    unit.waypoint_stall_frames = 0.0f;
    unit.last_waypoint_stall_frames = 0.0f;
    unit.attack_move_final_node_coast_pending = false;
    // ay() clears e/c/d only when the current waypoint is the last one.
    // A queued continuation keeps the formation's independent timers.
    clear_current_order_state_(unit, false, unit.order_queue.size() > 1);
    unit.order_queue.erase(unit.order_queue.begin());
    if (cycle_to_back) unit.order_queue.push_back(std::move(completed));
    if (preserve_main_target) {
        // R survives ay(). Select its storage after exposing the next
        // waypoint, which can switch between armed service and combat.
        unit.restore_main_target(saved_main_target_uid, saved_main_target_team_id);
    }

    // ay() retains this unit's ad link even when its next waypoint differs
    // from the leader's. The common controller suppresses projection while
    // the retained link still affects speed and reverse-driving decisions.

    if (defer_next_order_activation && !unit.order_queue.empty()) {
        // MOVE's arrival dispatch calls ax() and clears its selected au.
        // The next waypoint is visible, but y.k() cannot dispatch it until
        // the next object turn. In particular, do not submit its path or
        // run its BUILD handler during this finishing turn.
        unit.activate_order_next_frame = true;
        return;
    }

    // d.k's synthetic exit MOVE is removed during y.aJ(), but y.ay() does
    // not activate the queued rally MOVE until this unit's next object turn.
    // That activation goes through the delayed k.l path-request queue.
    if (completed.factory_exit && !unit.order_queue.empty()) {
        unit.activate_order_next_frame = true;
        return;
    }

    // y.i()/y.g() expose the next service waypoint after ay(), but the
    // replacement is not activated until the unit's next object turn. That
    // also applies to a linked repairer exposing a positional BUILD: its
    // retained ad link cannot publish the new waypoint during the old i().
    // is also observable when a reclaim target disappears while another reclaim
    // remains queued: activating it here would submit its chase one frame
    // before the native engine.
    if (!unit.order_queue.empty() &&
        (completed.type == WeaponType::REPAIR ||
         completed.type == WeaponType::RECLAIM ||
         completed.type == WeaponType::LOAD_INTO ||
         completed.type == WeaponType::LOAD_UP)) {
        unit.activate_order_next_frame = true;
        return;
    }

    while (!unit.order_queue.empty() &&
           !activate_order_(unit, unit.order_queue.front(), other_worlds)) {
        unit.order_queue.erase(unit.order_queue.begin());
    }
}

void World::advance_finished_order_(Unit& unit,
                                    const Vec<World*>& other_worlds) {
    if (unit.order_queue.empty()) return;
    const WeaponType type = unit.order_queue.front().type;
    if (type == WeaponType::TRIGGER_ACTION ||
        type == WeaponType::TRIGGER_WHEN_IN_RANGE ||
        type == WeaponType::SET_PASSIVE_TARGET) {
        if (type == WeaponType::TRIGGER_ACTION ||
            type == WeaponType::TRIGGER_WHEN_IN_RANGE) {
            const Str action_id = unit.order_queue.front().action_id;
            bool active_waypoint_consumed = false;
            execute_custom_action_(
                unit, action_id, other_worlds, &active_waypoint_consumed);
            // deleteSelf runs before later effects such as convertTo. Its
            // death cleanup clears the queue even though the action did not
            // use clearActiveWaypoint; do not erase the former head again.
            if (active_waypoint_consumed || unit.order_queue.empty()) return;
        }
        // y.ay() consumes these one-shot waypoints without clearing y.R.
        // Preserve the passive target while making the next queued order
        // current.
        unit.order_queue.erase(unit.order_queue.begin());
        unit.active_order_elapsed_frames = 0.0f;
        unit.order_point_dwell_frames = 0.0f;
        while (!unit.order_queue.empty() &&
               !activate_order_(unit, unit.order_queue.front(), other_worlds)) {
            unit.order_queue.erase(unit.order_queue.begin());
        }
        return;
    }
    bool finished = false;
    switch (type) {
    case WeaponType::ATTACK: {
        UnitOrder& attack_order = unit.order_queue.front();
        World* target_world = order_target_world_(
            attack_order.target_team_id, other_worlds);
        Unit* target = target_world
            ? target_world->waypoint_target_(
                  attack_order.target_uid, unit.bomber_attack_movement)
            : nullptr;
        // y.g() keeps a live enemy waypoint when its target enters cargo.
        // The common combat pass clears R. g() retargets only a dead/missing
        // object or one owned by this player; another allied player's unit
        // still remains the explicit waypoint target.
        finished = !target || !target->is_alive ||
                   target_world == this;
        const bool bomber = unit.bomber_attack_movement;
        if (finished && (bomber ? !unit.bomber_target_search_done
                                : unit.order_queue.size() == 1)) {
            // Stock y.g(ATTACK) does not finish a lone explicit ATTACK when
            // its target dies. It clears the dead R pointer, immediately runs
            // the two-pass passive callback with y.b(true), and mutates the
            // existing au waypoint when another enemy is available. This
            // bypasses y.S, so the replacement can happen while the ordinary
            // passive scan timer is still positive.
            World* replacement_world = unit.target_uid != 0
                ? order_target_world_(unit.target_team_id, other_worlds)
                : nullptr;
            Unit* replacement = replacement_world
                ? replacement_world->get_unit(unit.target_uid) : nullptr;
            // g() clears R only when it is dead. A scan with no candidates
            // leaves a live reference in place, even outside b(true); the
            // later combat pass may clear R after au has adopted it.
            if (!replacement || !replacement->is_alive) {
                replacement = nullptr;
                replacement_world = nullptr;
                unit.target_uid = 0;
                unit.target_team_id = -1;
            }
            if (unit.behavior != Behavior::HOLD_FIRE &&
                unit.behavior != Behavior::RETURN_FIRE) {
                World* scanned_world = nullptr;
                Unit* scanned = select_passive_target_(
                    unit, other_worlds,
                    engagement_range_(unit, true) + (bomber ? 200.0f : 0.0f),
                    scanned_world);
                if (scanned) {
                    replacement = scanned;
                    replacement_world = scanned_world;
                }
            }
            if (replacement && replacement_world) {
                attack_order.target_uid = replacement->uid;
                attack_order.target_team_id = replacement_world->team_id_;
                unit.target_uid = replacement->uid;
                unit.target_team_id = replacement_world->team_id_;

                // y.aB() detaches both leaders and followers from the old
                // formation while preserving af/ah and the historical
                // heading/distance fields.
                unit.formation_leader_uid = 0;
                unit.formation_is_leader = false;
                unit.formation_path_activation_pending = false;
                unit.formation_offset_x = 0.0f;
                unit.formation_offset_y = 0.0f;
                unit.formation_path_fallback_frames = 0.0f;

                // y.aH() drops the adopted av/aw chain but leaves aU alive.
                // No node selected for the old target survives this g()
                // dispatch. The retained request can still be adopted by
                // the generic controller later in the same object update.
                unit.has_deferred_navigation_target = false;
                unit.path_target_override_active = false;
                unit.path_request_preserves_active_path = false;
                unit.path_retry_next_frame = false;
                unit.using_path = false;
                unit.waypoints.clear();
                unit.current_waypoint = 0;
                unit.path_incomplete = false;
                unit.repath_cooldown_frames = 0.0f;
                unit.waypoint_stall_frames = 0.0f;
                unit.last_waypoint_stall_frames = 0.0f;
                unit.single_node_path_counter = 0;
                finished = false;
            } else if (bomber) {
                // g() runs a bomber's expanded search once, including with
                // queued waypoints. A failed search starts the existing
                // recharge retreat toward the old object's position.
                unit.bomber_target_search_done = true;
                unit.bomber_retreating = true;
            }
        }
        if (finished && bomber && target && unit.bomber_retreating) {
            // A null au.h still completes immediately. A dead or same-owner
            // object remains usable by the bomber controller until ar clears.
            finished = false;
        }
        // y.g() accepts the ATTACK waypoint, resolves its target, then
        // finishes it when both aR() and l() are false. Unarmed mobile
        // builders can still follow an ATTACK; unarmed buildings cannot.
        const bool accepts_movement_orders = unit.custom_unit
            ? !unit.ignore_move_orders : unit.can_move;
        if (!accepts_movement_orders && !unit.can_attack) finished = true;
        break;
    }
    case WeaponType::REPAIR:
    case WeaponType::RECLAIM:
    case WeaponType::TOUCH_TARGET:
        finished = unit.target_uid == 0;
        break;
    case WeaponType::LOAD_INTO:
    case WeaponType::LOAD_UP:
        // The transport handler completes these waypoints from au.h's
        // validity and loading distance, independently of combat target R.
        break;
    case WeaponType::BUILD:
        // A positional build waypoint has no target UID until the builder
        // reaches the site and creates the incomplete structure.
        finished = false;
        break;
    case WeaponType::GUARD:
        finished = unit.guard_target_uid == 0;
        break;
    case WeaponType::FOLLOW:
        finished = unit.follow_target_uid == 0;
        break;
    default:
        break;
    }
    if (finished) {
        // g(ATTACK) clears its selected au after ay(). The queued waypoint
        // becomes visible now, but its target helper runs on the next turn.
        complete_current_order_(unit, other_worlds, false,
                                type == WeaponType::ATTACK);
        if (!unit.order_queue.empty() &&
            !unit.activate_order_next_frame) {
            // A target order that becomes invalid at the start of y.k(float)
            // advances into the queued order within the same object update.
            // activate_order_'s one-pass delay compensates for external
            // commands submitted before Engine::tick(), but must not suppress
            // steering for this internal queue transition.
            unit.movement_activation_delay_frames = 0.0f;
            unit.movement_delay_keeps_prediction = false;
        }
    }
}

void World::stop_unit(Unit& u) {
    u.bomber_attack_heading = -999.0f;
    u.bomber_retreating = false;
    u.bomber_target_search_done = false;
    u.has_move_target = false;
    u.arrived = true;
    u.waypoint_stall_frames = 0.0f;
    u.last_waypoint_stall_frames = 0.0f;
    u.single_node_path_counter = 0;
    u.guard_combat_chase_outer = false;
    u.formation_path_fallback_frames = 0.0f;
    u.formation_path_failure_frames = 0.0f;
    u.path_incomplete = false;
    u.path_request_pending = false;
    u.path_request_uses_prepath = false;
    u.path_request_prepath.clear();
    u.path_request_prepath_from_replay = false;
    u.path_request_preserves_active_path = false;
    u.path_request_cost_snapshot.reset();
    u.path_request_delay_frames = 0.0f;
    u.activate_order_next_frame = false;
    u.target_uid = 0;
    u.target_team_id = -1;
    u.unarmed_main_target_uid = 0;
    u.unarmed_main_target_team_id = -1;
    clear_navigation_orders(u);
    u.using_path = false;
    u.waypoints.clear();
    u.current_waypoint = 0;
    u.order_queue.clear();
    if (u.weapon_type == WeaponType::REPAIR ||
        u.weapon_type == WeaponType::RECLAIM) {
        finish_service_order(u);
    } else if (u.weapon_type == WeaponType::LOAD_INTO ||
               u.weapon_type == WeaponType::LOAD_UP) {
        u.weapon_type = WeaponType::MOVE;
        restore_default_weapon_type(registry_, u);
    }
}

} // namespace rwrl
