#include "world_internal.h"
#include "combat/targeting_helpers.h"

namespace rwrl {
namespace {

void advance_path_request_countdown(Unit& unit, f32 delta_frames) {
    const bool ordinary_solver = !unit.path_request_uses_prepath &&
        !unit.path_request_needs_worker_handoff;
    if (ordinary_solver) {
        // Ordinary budgets encode k.t + one object-turn handoff. k.l.d()
        // checks t <= 0 before subtracting this frame's delta, so crossing
        // zero must retain that handoff even when delta changes or overshoots.
        unit.path_request_delay_frames =
            unit.path_request_delay_frames <= 1.0f
                ? 0.0f
                : std::max(1.0f,
                           unit.path_request_delay_frames - delta_frames);
        if (unit.path_request_delay_frames == 0.0f) {
            // The final object-turn marker corresponds to k.l releasing u,
            // even if the owner's current formation does not need a handoff.
            // A replacement command must see that already-released result.
            unit.path_request_solver_ready = true;
        }
    } else {
        unit.path_request_delay_frames = std::max(
            0.0f, unit.path_request_delay_frames - delta_frames);
    }
}

bool formation_leader_is_stalled(const Unit& unit, i32 path_points) {
    if (!unit.formation_is_leader || unit.formation_group_size <= 1 ||
        path_points < 2) {
        return false;
    }
    const f32 limit = unit.move_speed_per_frame() > 0.5f
        ? 150.0f : 300.0f;
    return unit.waypoint_stall_frames > limit ||
           unit.last_waypoint_stall_frames > limit;
}

struct FormationTimerResetAfterMovement {
    Unit& unit;
    bool physical_pass;
    ~FormationTimerResetAfterMovement() {
        if (physical_pass && unit.formation_timers_clear_after_movement) {
            // Slot projection calls aH() before the generic controller
            // samples aw. Only its retained final private node clears c/d.
            if (unit.movement_navigation_points == 1) {
                unit.formation_path_fallback_frames = 0.0f;
                unit.formation_path_failure_frames = 0.0f;
            }
            unit.formation_timers_clear_after_movement = false;
        }
    }
};

} // namespace

void World::update_unit_movement(
        f32 dt, u32 only_unit_uid, const Vec<World*>& other_worlds) {
    const f32 delta_frames = dt * EngineConstants::FRAME_RATE;
    for (auto unit_update_it = only_unit_uid == 0
             ? units_.begin() : units_.find(only_unit_uid);
         unit_update_it != units_.end();
         unit_update_it = only_unit_uid == 0
             ? std::next(unit_update_it) : units_.end()) {
        auto& [uid, unit] = *unit_update_it;
        // y.a(float) checks bT() once before entering k(float). Cargo loaded
        // during that same order update still finishes its physical pass;
        // cargo attached before its object turn skips the whole update.
        const bool loaded_during_own_order = delta_frames > 0.0f &&
            unit.parent_uid != 0 && unit.is_active_for_update(tick_);
        if (!unit.is_active() && !loaded_during_own_order) {
            // k.l advances queued requests independently of the owner's
            // bT() gate. Keep a passenger's result pending for y.b(l), while
            // allowing its worker countdown to finish during transport.
            if (delta_frames > 0.0f && unit.is_alive &&
                unit.parent_uid != 0 && unit.path_request_pending) {
                advance_path_request_countdown(unit, delta_frames);
            }
            continue;
        }
        if (!unit.custom_unit && unit.unit_type == "builder") {
            // e.b.z()/A() read am.cK() for each movement or service turn.
            const u8 terrain = pathfinder_.terrain(
                pathfinder_.tile_x(unit.x), pathfinder_.tile_y(unit.y));
            const bool over_liquid =
                (terrain & (TERRAIN_WATER | TERRAIN_LAVA)) != 0;
            unit.move_speed = (over_liquid ? 0.6f : 0.8f) *
                              EngineConstants::FRAME_RATE;
            unit.turn_speed = over_liquid ? 1.7f : 3.8f;
        } else if (unit.unit_type == "hovercraft") {
            // Stock i.z()/A() read cK() at the current position. Crossing
            // onto liquid changes both speed and turn rate on the next turn.
            const u8 terrain = pathfinder_.terrain(
                pathfinder_.tile_x(unit.x), pathfinder_.tile_y(unit.y));
            const bool over_liquid =
                (terrain & (TERRAIN_WATER | TERRAIN_LAVA)) != 0;
            unit.move_speed = (over_liquid ? 1.2f : 0.9f) *
                              EngineConstants::FRAME_RATE;
            unit.turn_speed = over_liquid ? 1.8f : 1.4f;
        } else if (unit.unit_type == "amphibiousJet") {
            // b.c.A() reads Q() at the time of each body turn.
            unit.turn_speed = unit.height < -1.0f ? 1.5f : 3.8f;
            // b.c.h() selects water as soon as Dive is requested, including
            // the descent before Q() becomes true.
            unit.move_type = !unit.submerge_requested && unit.height >= 0.0f
                ? MoveType::AIR : MoveType::WATER;
        }
        const bool order_phase = unit.order_phase_tick != tick_;
        FormationTimerResetAfterMovement formation_timer_reset{
            unit, !order_phase};
        if (order_phase) {
            unit.formation_fallback_countdown_active =
                unit.move_speed_factor != 0.0f;
        } else if (unit.order_queue.empty()) {
            // y.a(engine,delta,waypoint,ad) clears ad.d when ar()==null.
            // An orderless passive chase can retain ad, but its private
            // path must survive instead of entering the formation helper.
            unit.formation_projection_allowed = false;
        }
        const bool formation_rebuild_reverse_coast =
            !order_phase && unit.formation_rebuild_reverse_coast_pending;
        if (!order_phase) {
            unit.formation_rebuild_reverse_coast_pending = false;
        }
        if (!order_phase && unit.attack_move_final_node_coast_pending &&
            !unit.attack_move_combat_target_valid &&
            unit.waypoint_consumed_tick != tick_) {
            bool publish_attack_move_coast =
                unit.formation_is_leader &&
                unit.formation_group_size > 1 &&
                !unit.formation_preferred && !unit.using_path &&
                !unit.path_request_pending &&
                unit.repath_cooldown_frames > 0.0f &&
                !unit.order_queue.empty() &&
                unit.order_queue.front().type == WeaponType::ATTACK_MOVE &&
                unit.target_uid != 0;
            World* target_world = nullptr;
            Unit* target = nullptr;
            if (publish_attack_move_coast) {
                target_world = order_target_world_(
                    unit.target_team_id, other_worlds);
                target = target_world
                    ? target_world->get_unit(unit.target_uid) : nullptr;
                publish_attack_move_coast =
                    target && target->is_alive && target->parent_uid == 0 &&
                    target_world != this && is_enemy_with(*target_world) &&
                    can_target_(unit, *target);
            }
            if (publish_attack_move_coast) {
                const f32 follow_range = attack_follow_stop_range(unit, *target);
                publish_attack_move_coast = unit.bomber_attack_movement || dist2(
                    unit.x, unit.y, target->x, target->y) >=
                    follow_range * follow_range ||
                    attack_target_needs_approach(*this, unit, *target);
            }
            if (publish_attack_move_coast) {
                // The final-node coast must not participate in the preceding
                // command or combat decisions. Publish it only when this
                // object's physical pass begins, after those phases finish.
                unit.attack_move_combat_target_valid = true;
                unit.attack_move_combat_target_x = target->x;
                unit.attack_move_combat_target_y = target->y;
                unit.path_request_target_size =
                    attack_path_target_size(unit, target);
                unit.repath_cooldown_frames = std::min(
                    unit.repath_cooldown_frames, 90.0f);
                unit.repath_cooldown_order_start = -1.0f;
                if (!unit.path_target_override_active) {
                    unit.path_target_override_active = true;
                    unit.path_target_override_x = unit.target_x;
                    unit.path_target_override_y = unit.target_y;
                }
                unit.target_x = target->x;
                unit.target_y = target->y;
                unit.has_move_target = true;
                unit.arrived = false;
            } else {
                unit.attack_move_final_node_coast_pending = false;
            }
        }
        const bool move_request_was_active =
            unit.has_move_target && !unit.arrived;
        if (order_phase) {
            unit.formation_stall_clock_refresh_pending = false;
            unit.formation_fallback_consumption_pending = false;
            // This flag is consumed by the physical pass in the same object
            // update. Do not carry it across a skipped/aborted order phase.
            unit.patrol_soft_stop_coast = false;
        }
        // A GUARD replacement is submitted before the native object's next
        // turn. Its logical target is needed by that turn's physical pass,
        // then the one-pass exception is consumed regardless of early exits.
        const bool consume_guard_logical_tolerance =
            !order_phase && unit.guard_logical_tolerance_pending;
        if (consume_guard_logical_tolerance) {
            unit.guard_logical_tolerance_pending = false;
        }
        bool adopted_path_without_prior_node = false;
        bool adopted_order_final_node = false;
        bool order_path_refreshed = false;
        bool formation_fallback_expired = false;
        const auto integrate_external_displacement = [&] {
            if (delta_frames <= 0.0f) return false;
            bool displaced = false;
            if (!unit.strafes && unit.is_mobile() &&
                (unit.vel_x != 0.0f || unit.vel_y != 0.0f)) {
                // y.a(float) adds a non-strafing unit's cc/cd before
                // damping them, independently of its current cf proposal.
                unit.x += unit.vel_x * delta_frames;
                unit.y += unit.vel_y * delta_frames;
                const f32 speed = unit.move_speed_per_frame();
                if (unit.vel_x * unit.vel_x + unit.vel_y * unit.vel_y >
                    speed * speed) {
                    unit.vel_x = static_cast<f32>(
                        static_cast<double>(unit.vel_x) -
                        static_cast<double>(unit.vel_x) * 0.05 *
                            static_cast<double>(delta_frames));
                    unit.vel_y = static_cast<f32>(
                        static_cast<double>(unit.vel_y) -
                        static_cast<double>(unit.vel_y) * 0.05 *
                            static_cast<double>(delta_frames));
                }
                const f32 damping = 0.5f * speed * delta_frames;
                unit.vel_x = approach_value(unit.vel_x, 0.0f, damping);
                unit.vel_y = approach_value(unit.vel_y, 0.0f, damping);
                unit.collision_moved = true;
                displaced = true;
            }
            if (unit.collision_impulse_x != 0.0f ||
                unit.collision_impulse_y != 0.0f) {
                apply_unit_collision_impulse(uid);
                displaced = true;
            }
            return displaced;
        };
        const bool replay_move_cache_refresh_pending =
            order_phase && unit.formation_rebuild_path_pending &&
            !unit.using_path && !unit.path_request_pending &&
            !unit.order_queue.empty() &&
            unit.order_queue.front().type == WeaponType::MOVE &&
            unit.order_queue.front().has_prepath &&
            unit.order_queue.front().prepath_from_replay &&
            unit.order_queue.front().prepath_attach_frame < tick_ &&
            !unit.order_queue.front().replay_prepath_cache.empty();
        if (order_phase && unit.service_final_waypoint_coast) {
            unit.service_final_waypoint_coast = false;
            unit.has_move_target = false;
            unit.arrived = true;
            unit.has_deferred_navigation_target = false;
            unit.target_x = 0.0f;
            unit.target_y = 0.0f;
        }
        if (order_phase && unit.patrol_final_node_coast_pending &&
            !unit.using_path &&
            (unit.repath_cooldown_frames == 0.0f ||
             (!unit.patrol_logical_target_active &&
              unit.move_type != MoveType::AIR &&
              !unit.attack_range_adds_radii))) {
            // The next object turn still exposes k/l/m, but its physical
            // helper has no af or j for a ground patrol without a passive
            // target. Coast through positive s as well as its expiry turn.
            unit.movement_activation_delay_frames = 1.0f;
            unit.movement_delay_keeps_prediction = false;
        }
        bool attack_move_final_node_coast_active = false;
        if (order_phase && unit.attack_move_final_node_coast_pending) {
            bool keep_attack_move_coast =
                unit.formation_is_leader &&
                unit.formation_group_size > 1 &&
                !unit.formation_preferred &&
                !unit.using_path && !unit.path_request_pending &&
                // y.k() spends this turn's delta before testing the coast.
                unit.repath_cooldown_frames > combat_delta_frames_ &&
                !unit.order_queue.empty() &&
                unit.order_queue.front().type == WeaponType::ATTACK_MOVE &&
                unit.target_uid != 0;
            if (keep_attack_move_coast) {
                World* target_world = order_target_world_(
                    unit.target_team_id, other_worlds);
                Unit* target = target_world
                    ? target_world->get_unit(unit.target_uid) : nullptr;
                keep_attack_move_coast =
                    target && target->is_alive && target->parent_uid == 0 &&
                    target_world != this && is_enemy_with(*target_world) &&
                    can_target_(unit, *target);
                if (keep_attack_move_coast) {
                    const f32 follow_range = attack_follow_stop_range(unit, *target);
                    keep_attack_move_coast = unit.bomber_attack_movement || dist2(
                        unit.x, unit.y, target->x, target->y) >=
                        follow_range * follow_range ||
                        attack_target_needs_approach(*this, unit, *target);
                }
            }
            if (keep_attack_move_coast) {
                // The live target remains visible, but this object turn only
                // coasts on the previous heading while s counts down.
                unit.movement_activation_delay_frames = 1.0f;
                unit.movement_delay_keeps_prediction = false;
                unit.movement_prediction_enabled = false;
                attack_move_final_node_coast_active = true;
            } else {
                unit.attack_move_final_node_coast_pending = false;
            }
        }
        const bool active_guard_order =
            unit.guard_target_uid != 0 && !unit.order_queue.empty() &&
            unit.order_queue.front().type == WeaponType::GUARD;
        if (order_phase) {
            unit.repath_cooldown_order_start =
                unit.repath_cooldown_frames;
        }
        const bool patrol_recent_chase_proposal =
            !unit.order_queue.empty() &&
            unit.order_queue.front().type == WeaponType::PATROL &&
            unit.recent_order_chase && unit.patrol_logical_target_active;
        if (order_phase && unit.path_request_prediction_suppressed &&
            unit.path_request_pending &&
            unit.path_request_preserves_active_path && !unit.using_path &&
            !patrol_recent_chase_proposal) {
            // Once a preserved path is exhausted, JAR keeps the
            // replacement request visible but coasts and decelerates until
            // its solver result is adopted. Re-arm the one-frame movement
            // delay at each object turn while that request is still pending.
            unit.movement_activation_delay_frames = 1.0f;
            unit.movement_delay_keeps_prediction = false;
        }
        // y.k(float) counts s down before passive combat, including objects
        // with no waypoint. Its zero crossing can submit an orderless chase
        // request in this turn; waiting for the physical pass delays that
        // request and its eventual path adoption by one object update.
        const bool early_cooldown_order = unit.order_queue.empty() ||
            active_guard_order ||
            (!unit.order_queue.empty() &&
             (unit.order_queue.front().type == WeaponType::MOVE ||
              unit.order_queue.front().type == WeaponType::ATTACK ||
              unit.order_queue.front().type == WeaponType::GUARD ||
              unit.order_queue.front().type == WeaponType::FOLLOW ||
              unit.order_queue.front().type == WeaponType::ATTACK_MOVE ||
              unit.order_queue.front().type == WeaponType::BUILD ||
              unit.order_queue.front().type == WeaponType::REPAIR ||
              unit.order_queue.front().type == WeaponType::RECLAIM ||
              unit.order_queue.front().type == WeaponType::LOAD_INTO ||
              unit.order_queue.front().type == WeaponType::LOAD_UP));
        if (order_phase && early_cooldown_order &&
            unit.repath_cooldown_frames != 0.0f) {
            // y.k() spends s before MOVE can submit its private request.
            // A cooldown reaching zero must request on this object turn.
            unit.repath_cooldown_frames = std::max(
                0.0f,
                unit.repath_cooldown_frames - combat_delta_frames_);
        } else if (!order_phase &&
                   (!early_cooldown_order || unit.order_queue.empty()) &&
                   unit.repath_cooldown_frames != 0.0f) {
            // y.s counts toward zero once per frame (f.a(s, f2)).
            // A positional waypoint may have completed in the order pass.
            // Its now-orderless physical pass still applies that decrement
            // if no earlier handler changed the frame-start cooldown.
            if (unit.repath_cooldown_frames ==
                unit.repath_cooldown_order_start) {
                unit.repath_cooldown_frames = std::max(
                    0.0f, unit.repath_cooldown_frames - delta_frames);
            }
        }
        if (!order_phase && unit.formation_fallback_countdown_active &&
            unit.formation_path_fallback_frames != 0.0f) {
            // y.k() counts c down only while am.cf (the signed forward-speed
            // factor) is nonzero. The formation helper can refresh it to 90
            // later in this same object update.
            const f32 fallback_before =
                unit.formation_path_fallback_frames;
            unit.formation_path_fallback_frames = std::max(
                0.0f, fallback_before - delta_frames);
            formation_fallback_expired =
                fallback_before > 0.0f &&
                unit.formation_path_fallback_frames == 0.0f;
        }
        const bool had_path_before_order_dispatch = unit.using_path;
        if (order_phase &&
            !update_unit_order_navigation_(
                unit, delta_frames, other_worlds)) {
            continue;
        }
        if (order_phase && unit.path_request_prediction_suppressed &&
            unit.path_request_pending &&
            unit.path_request_preserves_active_path && !unit.using_path &&
            !unit.order_queue.empty() &&
            unit.order_queue.front().type == WeaponType::ATTACK_MOVE &&
            unit.attack_move_direct_chase_proposal) {
            // The old af-less request re-armed a coast before dispatch.
            // y.a(delta,R,ad,true) can now publish j=true, which supplies
            // ad.b from live l/m even while the same aU is still pending.
            unit.movement_activation_delay_frames = 0.0f;
        }
        if (order_phase && attack_move_final_node_coast_active) {
            World* target_world = order_target_world_(
                unit.target_team_id, other_worlds);
            Unit* target = target_world
                ? target_world->get_unit(unit.target_uid) : nullptr;
            if (target && target->is_alive && target->parent_uid == 0 &&
                target_world != this && is_enemy_with(*target_world) &&
                can_target_(unit, *target)) {
                // The MOVE-family order handler restores the queued point
                // while the delayed chase is suppressed. Re-publish the live
                // target after that handler so l/m match the native object.
                unit.target_x = target->x;
                unit.target_y = target->y;
                unit.has_move_target = true;
                unit.arrived = false;
            } else {
                unit.attack_move_final_node_coast_pending = false;
            }
        }
        if (order_phase) {
            unit.formation_stall_clock_refresh_pending =
                // aG() precedes a new private-path request. A chain created
                // from an empty af cannot refresh an until the next turn;
                // b(l) adoption samples its new chain in the physical pass.
                had_path_before_order_dispatch &&
                unit.has_move_target && !unit.arrived &&
                formation_leader_is_stalled(
                    unit, unit.using_path ? remaining_navigation_points(unit) : 0);
        }
        if (!order_phase && !unit.order_queue.empty() &&
            !unit.activate_order_next_frame) {
            // y.k() charged V before dispatch. ay() resets it when service
            // finishes, so the newly exposed waypoint stays at zero until
            // its own object turn instead of aging in this physical pass.
            unit.active_order_elapsed_frames += delta_frames;
        }

        // y.a(...,ad,...) stores am.cK from the movement request selected by
        // the order update. It remains true while turning or launch-delayed,
        // and becomes false while merely coasting after the request ends.
        const bool large_attack_move_projection_coast =
            order_phase && unit.path_request_pending &&
            !unit.direct_attack_chase_this_update &&
            !unit.path_request_uses_prepath && !unit.using_path &&
            !unit.order_queue.empty() &&
            unit.order_queue.front().type == WeaponType::ATTACK_MOVE &&
            unit.order_queue.front().replay_command_group_size >= 19 &&
            unit.order_queue.front().replay_command_has_long_prepath &&
            !unit.order_queue.front().replay_command_has_handoff_cache &&
            unit.formation_leader_uid != 0 &&
            !unit.formation_projection_allowed;
        const bool replay_handoff_request_ready =
            !unit.order_queue.empty() &&
            unit.order_queue.front().type == WeaponType::ATTACK_MOVE &&
            unit.order_queue.front().replay_command_group_size >= 19 &&
            unit.order_queue.front().replay_command_has_handoff_cache &&
            // n on the live proposal cannot enable an older n=0 worker's
            // handoff before its result has been released by k.l.
            unit.path_request_pending && unit.path_request_solver_target_size > 0 &&
            !unit.using_path &&
            unit.path_request_delay_frames <= delta_frames;
        unit.movement_prediction_enabled =
            unit.has_move_target && !unit.arrived && unit.is_mobile() &&
            !(unit.path_request_prediction_suppressed &&
              unit.path_request_pending && !unit.using_path &&
              !unit.has_deferred_navigation_target &&
              !unit.direct_attack_chase_this_update &&
              !patrol_recent_chase_proposal &&
              !replay_handoff_request_ready) &&
            !large_attack_move_projection_coast;
        if (large_attack_move_projection_coast) {
            unit.movement_activation_delay_frames = 1.0f;
            unit.movement_delay_keeps_prediction = false;
        }

        if (!order_phase && delta_frames > 0.0f && unit.path_request_pending) {
            const bool cache_selected_this_turn =
                unit.path_request_uses_prepath &&
                unit.path_request_prepath_submission_tick == tick_;
            const bool active_attack_order =
                !unit.order_queue.empty() &&
                unit.order_queue.front().type == WeaponType::ATTACK;
            const bool replaced_by_move_order =
                !unit.order_queue.empty() &&
                unit.order_queue.front().type == WeaponType::MOVE &&
                !unit.path_request_preserves_active_path;
            const bool same_pass_attack_request =
                // n belongs to the dispatched proposal, while the worker
                // retains the allowance captured when aU was submitted.
                // Replacing MOVE with ATTACK cannot turn its old n=0
                // request into a target-size worker handoff. MOVE itself
                // clears n, disabling the ATTACK handoff even when its
                // retained worker was submitted with a nonzero allowance.
                (active_attack_order &&
                  unit.path_request_solver_target_size > 0 &&
                  !unit.path_request_prefer_clearance &&
                  unit.formation_leader_uid != 0 &&
                  !unit.formation_projection_allowed) ||
                (replaced_by_move_order && unit.path_request_target_size > 0);
            // An ordinary budget ends with one object-turn marker. A half
            // frame releases that marker on this pass too.
            const f32 same_pass_request_budget =
                !unit.path_request_uses_prepath &&
                        !unit.path_request_needs_worker_handoff
                    ? std::max(1.0f, delta_frames) : delta_frames;
            if (!cache_selected_this_turn && unit.path_request_delay_frames <=
                (same_pass_attack_request ? same_pass_request_budget : 0.0f)) {
                const bool worker_completes_this_pass =
                    unit.path_request_delay_frames > 0.0f &&
                    !unit.path_request_uses_prepath;
                // The countdown reaches zero during this physical pass. The
                // JAR path worker can expose the completed result to the
                // same object update, so do not wait for the next pass.
                unit.path_request_delay_frames = 0.0f;
                const bool had_path_node_before_adoption =
                    unit.using_path && unit.has_move_target && !unit.arrived;
                const bool had_move_request_before_adoption =
                    unit.has_move_target && !unit.arrived;
                const f32 logical_target_x_before_adoption = unit.target_x;
                const f32 logical_target_y_before_adoption = unit.target_y;
                Vec<std::pair<f32, f32>> path;
                const bool adopt_prepath =
                    unit.path_request_uses_prepath;
                const bool adopted_replay_prepath =
                    adopt_prepath && unit.path_request_prepath_from_replay;
                // k.k captures n when submitted. A replacement waypoint's
                // selection and live proposal cannot rewrite that worker.
                const i32 solver_target_size = unit.path_request_solver_target_size;
                if (adopt_prepath) {
                    path = unit.path_request_prepath;
                } else {
                    path = pathfinder_.find_path(
                        unit.path_request_start_x,
                        unit.path_request_start_y,
                        unit.path_request_goal_x,
                        unit.path_request_goal_y,
                        unit.move_type,
                        unit.radius,
                        unit.path_request_rotation,
                        unit.path_request_flexible_reverse,
                        unit.path_request_prefer_clearance,
                        solver_target_size,
                        unit.path_min_clearance,
                        unit.path_request_cost_snapshot.get());
                }
                const bool attack_solver_handoff_candidate =
                    unit.path_request_needs_worker_handoff &&
                    !unit.path_request_uses_prepath &&
                    unit.path_request_solver_target_size > 0 &&
                    !unit.path_request_preserves_active_path &&
                    !unit.order_queue.empty() &&
                    unit.order_queue.front().type == WeaponType::ATTACK &&
                    unit.path_request_prefer_clearance &&
                    !unit.formation_preferred &&
                    unit.formation_group_size > 1;
                const bool expose_same_pass_single_node =
                    same_pass_attack_request && path.size() == 1;
                const bool active_build_order =
                    !unit.order_queue.empty() &&
                    unit.order_queue.front().type == WeaponType::BUILD;
                const bool active_service_order =
                    !unit.order_queue.empty() &&
                    (unit.order_queue.front().type == WeaponType::REPAIR ||
                     unit.order_queue.front().type == WeaponType::RECLAIM);
                const bool consume_single_final_node_now =
                    (expose_same_pass_single_node ||
                     attack_solver_handoff_candidate ||
                     ((active_attack_order || active_build_order ||
                       active_service_order) &&
                      had_move_request_before_adoption &&
                      unit.is_mobile())) &&
                    path.size() == 1 &&
                    dist2(unit.x, unit.y, path.front().first,
                          path.front().second) < 16.0f;
                const bool defer_attack_solver_handoff =
                    (same_pass_attack_request &&
                     (!expose_same_pass_single_node ||
                      worker_completes_this_pass)) ||
                    (attack_solver_handoff_candidate &&
                     !consume_single_final_node_now);
                if (defer_attack_solver_handoff &&
                    !unit.path_request_solver_ready) {
                    // k.l.b(float) releases a newly completed worker result
                    // after the object loop. Even a one-node result waits
                    // for the next y.b(l), then can be consumed immediately
                    // inside its final-node radius. The old av node remains
                    // this turn's movement input while that result waits.
                    unit.path_request_solver_ready = true;
                    const bool has_direct_chase_proposal =
                        unit.direct_attack_chase_this_update &&
                        unit.has_deferred_navigation_target;
                    if (!had_path_node_before_adoption &&
                        !has_direct_chase_proposal) {
                        // The newly released result cannot provide af yet,
                        // but a live j proposal still steers while it waits.
                        unit.movement_activation_delay_frames = 1.0f;
                        unit.movement_delay_keeps_prediction = false;
                        if (!unit.order_queue.empty() &&
                            unit.order_queue.front().type == WeaponType::ATTACK &&
                            !unit.path_request_uses_prepath &&
                            !unit.using_path && unit.move_speed_factor < 0.0f) {
                            // A normal ATTACK request keeps the reverse coast's
                            // acceleration branch through the solver handoff
                            // frame, just as it does while its countdown runs.
                            unit.movement_delay_uses_acceleration = true;
                        }
                        unit.movement_prediction_enabled = false;
                    }
                } else {
                    unit.path_request_solver_ready = false;
                    unit.path_request_needs_worker_handoff = false;
                    unit.path_request_pending = false;
                    unit.path_request_uses_prepath = false;
                    unit.path_request_prepath.clear();
                    unit.path_request_prepath_from_replay = false;
                    unit.path_request_preserves_active_path = false;
                    unit.path_request_cost_snapshot.reset();
                    consume_solver_path_(unit, path);
                    adopted_order_final_node = path.size() == 1 &&
                        !unit.order_queue.empty() &&
                        (unit.order_queue.front().type == WeaponType::ATTACK ||
                         unit.order_queue.front().type == WeaponType::MOVE ||
                         unit.order_queue.front().type == WeaponType::ATTACK_MOVE ||
                         unit.order_queue.front().type == WeaponType::PATROL ||
                         unit.order_queue.front().type == WeaponType::GUARD ||
                         unit.order_queue.front().type == WeaponType::FOLLOW ||
                         unit.order_queue.front().type == WeaponType::LOAD_INTO ||
                         unit.order_queue.front().type == WeaponType::LOAD_UP ||
                         active_build_order || active_service_order);
                    // A new solver result supersedes a logical-pass deferred
                    // target. Specialized first-node pop branches below
                    // re-install their own physical target when needed.
                    if (!path.empty()) {
                        unit.has_deferred_navigation_target = false;
                    }
                    if (unit.patrol_final_node_coast_pending &&
                        unit.using_path && !unit.path_request_pending) {
                        unit.patrol_final_node_coast_pending = false;
                    }
                // y.b(l) adopts the solved av/aw chain before the movement
                // controller captures aw. Some native adoption branches pop
                // the first node early and preserve it as a deferred target,
                // so capture the pre-pop count here.
                unit.movement_navigation_points =
                    consume_single_final_node_now
                        ? 1
                        : (unit.using_path
                            ? remaining_navigation_points(unit) : 0);
                if (!had_move_request_before_adoption && unit.using_path) {
                    // y.b(l) replaces av/aw but does not rewrite y.k. An
                    // order handler that suppressed movement this update
                    // keeps k=false while retaining the newly adopted chain.
                    unit.has_move_target = false;
                    unit.arrived = true;
                    if (!unit.order_queue.empty() &&
                        unit.order_queue.front().type == WeaponType::ATTACK_MOVE) {
                        // b(l) only replaces av/aw. Keep the held waypoint's
                        // l/m independent of the new first physical node.
                        unit.path_target_override_active = true;
                        unit.path_target_override_x = unit.target_x;
                        unit.path_target_override_y = unit.target_y;
                        unit.target_x = logical_target_x_before_adoption;
                        unit.target_y = logical_target_y_before_adoption;
                    }
                } else if (had_move_request_before_adoption &&
                           unit.using_path) {
                    // A replacement waypoint can leave the old aU alive while
                    // installing a one-frame activation delay. Once y.b(l)
                    // adopts that request, its first node is usable in this
                    // same object update and the delay no longer suppresses
                    // steering.
                    unit.movement_activation_delay_frames = 0.0f;
                }
                // b(l) replaces av before aG() checks the stalled leader.
                // Capture the new chain before the physical node pops below.
                unit.formation_stall_clock_refresh_pending =
                    had_move_request_before_adoption &&
                    formation_leader_is_stalled(unit, unit.movement_navigation_points);
                adopted_path_without_prior_node =
                    !had_path_node_before_adoption;
                // y.b(l) installs a ready GUARD result before the controller
                // captures af. A valid k proposal can use that node on this
                // turn, independently of the command's selection size.
                // JAR consumes a completed aU request at the start of the
                // generic movement controller, after the order handler has
                // produced k/l/m but before it checks path drift. Native
                // movement is split into order and physical passes, so repeat
                // that check here for the active waypoint after adoption.
                if (active_service_order && unit.has_move_target &&
                    !unit.arrived &&
                    (unit.formation_leader_uid == 0 ||
                     !unit.formation_projection_allowed)) {
                    World* target_world = order_target_world_(
                        unit.target_team_id, other_worlds);
                    Unit* target = target_world
                        ? target_world->get_unit(unit.target_uid) : nullptr;
                    if (target && target->is_alive && target->parent_uid == 0 &&
                        !service_target_in_range_(unit, *target)) {
                        // y.b(l) can adopt a request left by a replaced MOVE
                        // before y.i()'s r=n drift gate runs. A service refresh
                        // can start on this same object turn while retaining
                        // the newly adopted physical chain.
                        const i32 allowance = unit.path_request_target_size;
                        const bool drifted =
                            std::abs(unit.last_request_x - target->x) > allowance ||
                            std::abs(unit.last_request_y - target->y) > allowance;
                        if (drifted && unit.repath_cooldown_frames > 30.0f) {
                            unit.repath_cooldown_frames = 30.0f;
                        }
                        if (drifted && unit.repath_cooldown_frames == 0.0f) {
                            begin_service_chase_(unit, *target);
                            if (unit.path_request_pending) {
                                advance_path_request_countdown(unit, delta_frames);
                            }
                        }
                    }
                }
                if (!unit.order_queue.empty() &&
                    (unit.order_queue.front().type == WeaponType::LOAD_INTO ||
                     unit.order_queue.front().type == WeaponType::LOAD_UP) &&
                    unit.has_move_target && !unit.arrived &&
                    (unit.formation_leader_uid == 0 ||
                     !unit.formation_projection_allowed)) {
                    // LOAD publishes the live transport position before
                    // b(l) adopts this result. Its r=18 drift gate starts
                    // affecting s in this adoption turn, not while aU waits.
                    const f32 goal_x = unit.transport_order_target_x;
                    const f32 goal_y = unit.transport_order_target_y;
                    const bool drifted =
                        std::abs(unit.last_request_x - goal_x) > 18.0f ||
                        std::abs(unit.last_request_y - goal_y) > 18.0f;
                    if (drifted && unit.repath_cooldown_frames > 30.0f) {
                        unit.repath_cooldown_frames = 30.0f;
                    }
                    const bool load_drifted =
                        std::abs(unit.last_request_x - goal_x) > 12.0f ||
                        std::abs(unit.last_request_y - goal_y) > 12.0f;
                    if (unit.repath_cooldown_frames == 0.0f &&
                        (!unit.using_path || load_drifted)) {
                        move_unit_to_(unit, goal_x, goal_y,
                                      unit.path_request_target_size, true,
                                      unit.using_path);
                        // The old request was adopted above, so this new
                        // request cannot reach the ordinary countdown below.
                        // k.l still spends the submission turn's solver frame.
                        if (unit.path_request_pending) {
                            advance_path_request_countdown(unit, delta_frames);
                        }
                    }
                }
                if (!unit.order_queue.empty() &&
                    (unit.order_queue.front().type == WeaponType::GUARD ||
                     unit.order_queue.front().type == WeaponType::FOLLOW) &&
                    unit.guard_movement_target_valid) {
                    const bool refresh_drifted =
                        std::abs(unit.last_request_x -
                                 unit.guard_movement_target_x) >
                            unit.guard_path_drift_threshold ||
                        std::abs(unit.last_request_y -
                                 unit.guard_movement_target_y) >
                            unit.guard_path_drift_threshold;
                    if (unit.using_path && refresh_drifted &&
                        unit.repath_cooldown_frames > 30.0f) {
                        unit.repath_cooldown_frames = 30.0f;
                    }
                    if ((!unit.using_path || refresh_drifted) &&
                        unit.repath_cooldown_frames == 0.0f) {
                        const i32 refresh_target_size =
                            unit.guard_movement_chases_main_target ? 0 : 2;
                        move_unit_to_(unit,
                                      unit.guard_movement_target_x,
                                      unit.guard_movement_target_y,
                                      refresh_target_size, true, true);
                        // This adoption branch bypasses the ordinary worker
                        // countdown. Spend the new request's submission turn
                        // before returning to the physical controller.
                        if (unit.path_request_pending) {
                            advance_path_request_countdown(unit, delta_frames);
                        }
                    }
                }
                if (!unit.order_queue.empty() &&
                    unit.order_queue.front().type == WeaponType::MOVE &&
                    unit.has_move_target && !unit.arrived &&
                    (unit.formation_leader_uid == 0 ||
                     !unit.formation_projection_allowed)) {
                    // y.b(l) adopts the old aU before dispatch chooses the
                    // matching leader's formation helper. That helper owns
                    // projection and fallback requests; the generic MOVE
                    // drift refresh runs only outside that branch.
                    const UnitOrder& active = unit.order_queue.front();
                    const bool refresh_drifted =
                        std::abs(unit.last_request_x - active.x) > 150.0f ||
                        std::abs(unit.last_request_y - active.y) > 150.0f;
                    if (unit.using_path && refresh_drifted &&
                        unit.formation_path_fallback_frames == 0.0f &&
                        unit.repath_cooldown_frames > 30.0f) {
                        unit.repath_cooldown_frames = 30.0f;
                    }
                    if ((!unit.using_path || refresh_drifted) &&
                        unit.repath_cooldown_frames == 0.0f) {
                        move_unit_to_(unit, active.x, active.y, 0, true,
                                      unit.using_path);
                        // The adoption branch has already bypassed the
                        // ordinary pending-request countdown below. Spend
                        // this refresh's submission turn in k.l as well.
                        if (unit.path_request_pending) {
                            advance_path_request_countdown(unit, delta_frames);
                        }
                    }
                    }
                if (active_attack_order && unit.using_path &&
                    !unit.path_request_pending && unit.has_move_target &&
                    !unit.arrived &&
                    (unit.formation_leader_uid == 0 ||
                     !unit.formation_projection_allowed)) {
                    // g() can retarget and call aH() while leaving aU alive.
                    // The generic controller adopts that result first, then
                    // checks o/p against the new l/m proposal. A clear-line
                    // refresh overwrites the captured av node in this turn.
                    const f32 goal_x = unit.attack_order_target_x;
                    const f32 goal_y = unit.attack_order_target_y;
                    const f32 movement_drift =
                        std::max(0.0f, unit.weapon_range - 1.0f);
                    const bool range_drifted =
                        (unit.move_type == MoveType::AIR ||
                         unit.attack_range_adds_radii) &&
                        (std::abs(unit.last_request_x - goal_x) > movement_drift ||
                         std::abs(unit.last_request_y - goal_y) > movement_drift);
                    const bool order_drifted =
                        std::abs(unit.last_request_x - goal_x) > 150.0f ||
                        std::abs(unit.last_request_y - goal_y) > 150.0f;
                    if (order_drifted && unit.repath_cooldown_frames > 30.0f) {
                        unit.repath_cooldown_frames = 30.0f;
                    }
                    if ((range_drifted || order_drifted) &&
                        unit.repath_cooldown_frames == 0.0f) {
                        move_unit_to_(unit, goal_x, goal_y,
                                      unit.path_request_target_size, true, true);
                        // b(l) already adopted the previous request above.
                        // The replacement still spends this object turn in
                        // k.l, even though it bypasses the pending branch.
                        if (unit.path_request_pending) {
                            advance_path_request_countdown(unit, delta_frames);
                        }
                        unit.movement_navigation_points = unit.using_path
                            ? remaining_navigation_points(unit) : 0;
                    }
                }
                const bool adopted_patrol_chase =
                    !unit.order_queue.empty() &&
                    unit.order_queue.front().type == WeaponType::PATROL &&
                    unit.patrol_logical_target_active;
                if (!unit.order_queue.empty() &&
                    (unit.order_queue.front().type == WeaponType::ATTACK_MOVE ||
                     adopted_patrol_chase) &&
                    unit.has_move_target && !unit.arrived &&
                    !unit.path_request_pending &&
                    (unit.formation_leader_uid == 0 ||
                     !unit.formation_projection_allowed)) {
                    // b(l) releases aU before generic navigation compares
                    // o/p with this turn's l/m. The order handler could not
                    // refresh while that old request was still pending.
                    const UnitOrder& active = unit.order_queue.front();
                    const f32 goal_x = adopted_patrol_chase
                        ? unit.patrol_logical_target_x
                        : (unit.attack_move_combat_target_valid
                            ? unit.attack_move_combat_target_x : active.x);
                    const f32 goal_y = adopted_patrol_chase
                        ? unit.patrol_logical_target_y
                        : (unit.attack_move_combat_target_valid
                            ? unit.attack_move_combat_target_y : active.y);
                    const f32 movement_drift = unit.weapon_range - 1.0f;
                    const bool range_drifted =
                        (unit.move_type == MoveType::AIR ||
                         unit.attack_range_adds_radii) &&
                        (std::abs(unit.last_request_x - goal_x) > movement_drift ||
                         std::abs(unit.last_request_y - goal_y) > movement_drift);
                    const bool order_drifted =
                        std::abs(unit.last_request_x - goal_x) > 150.0f ||
                        std::abs(unit.last_request_y - goal_y) > 150.0f;
                    if (order_drifted && unit.repath_cooldown_frames > 30.0f) {
                        unit.repath_cooldown_frames = 30.0f;
                    }
                    if ((!unit.using_path || range_drifted || order_drifted) &&
                        unit.repath_cooldown_frames == 0.0f) {
                        const i32 target_size =
                            adopted_patrol_chase || unit.attack_move_combat_target_valid
                            ? unit.path_request_target_size : 0;
                        move_unit_to_(unit, goal_x, goal_y, target_size,
                                      true, unit.using_path);
                        if (unit.path_request_pending) {
                            // This adoption branch bypasses the ordinary
                            // countdown below, but k.l still spends the
                            // submission turn on the replacement request.
                            advance_path_request_countdown(unit, delta_frames);
                        } else {
                            // A direct request rewrites the captured av[0]
                            // immediately, including its physical target.
                            unit.has_deferred_navigation_target = false;
                        }
                        unit.movement_navigation_points = unit.using_path
                            ? remaining_navigation_points(unit) : 0;
                    }
                }
                // A preserving refresh can publish new l/m after b(l)
                // installed av[0]. Node consumption still uses that
                // captured av point, which now lives in the override.
                const f32 adopted_node_x = unit.path_target_override_active
                    ? unit.path_target_override_x : unit.target_x;
                const f32 adopted_node_y = unit.path_target_override_active
                    ? unit.path_target_override_y : unit.target_y;
                // A drift refresh can rewrite the captured av[0] in place.
                // Sample Z against that point after every adoption refresh;
                // a pending grid refresh retains it in the override instead.
                bool adopted_node_covered_by_collision = false;
                const bool suppress_collision_path_pop =
                    unit.path_request_prediction_suppressed &&
                    active_attack_order;
                if (!suppress_collision_path_pop &&
                    unit.collision_partner_uid != 0 &&
                    (unit.collision_partner_tick == tick_ ||
                     unit.collision_partner_tick == tick_ - 1)) {
                    World* partner_world =
                        unit.collision_partner_team_id == team_id_
                            ? this
                            : order_target_world_(
                                  unit.collision_partner_team_id, other_worlds);
                    Unit* partner = partner_world
                        ? partner_world->waypoint_target_(
                              unit.collision_partner_uid, true)
                        : nullptr;
                    if (partner) {
                        const f32 overlap_radius = partner->radius + 2.0f;
                        adopted_node_covered_by_collision =
                            dist2(partner->x, partner->y,
                                  adopted_node_x, adopted_node_y) <
                            overlap_radius * overlap_radius;
                    }
                }
                if (consume_single_final_node_now && unit.using_path &&
                    remaining_navigation_points(unit) == 1 &&
                    dist2(unit.x, unit.y, adopted_node_x, adopted_node_y) <
                        16.0f) {
                    // b(l) installs a one-node av before the generic drift
                    // gate. Keep it through that gate so a changed l/m can
                    // submit its next request on this same object turn.
                    // Only aJ then consumes the captured physical node.
                    unit.last_waypoint_stall_frames = unit.waypoint_stall_frames;
                    unit.waypoint_stall_frames = 0.0f;
                    unit.waypoint_consumed_tick = tick_;
                    unit.has_deferred_navigation_target = true;
                    unit.deferred_navigation_target_x = adopted_node_x;
                    unit.deferred_navigation_target_y = adopted_node_y;
                    unit.using_path = false;
                    unit.waypoints.clear();
                    unit.current_waypoint = 0;
                    unit.movement_activation_delay_frames = 0.0f;
                    unit.attack_final_node_coast_pending =
                        !unit.order_queue.empty() &&
                        unit.order_queue.front().type == WeaponType::ATTACK;
                    unit.service_final_waypoint_coast = active_service_order;
                }
                const bool formation_owns_adopted_node =
                    !adopt_prepath && unit.formation_leader_uid != 0 &&
                    unit.formation_projection_allowed &&
                    unit.has_move_target && !unit.arrived;
                bool patrol_refreshes_adopted_node = false;
                if (!unit.order_queue.empty() &&
                    unit.order_queue.front().type == WeaponType::PATROL &&
                    !unit.patrol_logical_target_active && unit.using_path &&
                    unit.has_move_target && !unit.arrived &&
                    !unit.path_request_pending &&
                    unit.spawn_delay_frames == 0.0f &&
                    unit.repath_cooldown_frames == 0.0f &&
                    (unit.formation_leader_uid == 0 ||
                     !unit.formation_projection_allowed)) {
                    const UnitOrder& active = unit.order_queue.front();
                    const f32 drift_x = std::abs(unit.last_request_x - active.x);
                    const f32 drift_y = std::abs(unit.last_request_y - active.y);
                    patrol_refreshes_adopted_node =
                        drift_x > 150.0f || drift_y > 150.0f ||
                        ((unit.move_type == MoveType::AIR ||
                          unit.attack_range_adds_radii) &&
                         (drift_x > unit.weapon_range - 1.0f ||
                          drift_y > unit.weapon_range - 1.0f));
                }
                // Ordinary PATROL refreshes below, after this adoption block.
                // Its captured af can be overwritten before the native aJ();
                // keep W/X and let that refreshed point own the later pop.
                if (formation_owns_adopted_node) {
                    // b(l) installs av before the formation helper. That
                    // helper can overwrite av[0] with a direct fallback;
                    // aJ() must test that point afterward, including Z's
                    // collision radius, rather than popping the old node now.
                    unit.formation_fallback_consumption_pending = true;
                }
                if (adopted_replay_prepath && !patrol_refreshes_adopted_node &&
                    unit.using_path &&
                    unit.has_move_target && path.size() > 1 &&
                    unit.current_waypoint <
                        static_cast<i32>(unit.waypoints.size()) &&
                    (dist2(unit.x, unit.y,
                           adopted_node_x, adopted_node_y) < 256.0f ||
                     adopted_node_covered_by_collision)) {
                    // Replay packets already contain the JAR solver's list,
                    // with the cache-generation start tile removed. The
                    // first serialized node is therefore the live movement
                    // target; aJ consumes it after this update when it was
                    // already within 16 units.
                    unit.last_waypoint_stall_frames =
                        unit.waypoint_stall_frames;
                    unit.waypoint_stall_frames = 0.0f;
                    unit.waypoint_consumed_tick = tick_;
                    unit.has_deferred_navigation_target = true;
                    unit.deferred_navigation_target_x = adopted_node_x;
                    unit.deferred_navigation_target_y = adopted_node_y;
                    const auto& adopted_next =
                        unit.waypoints[unit.current_waypoint];
                    unit.target_x = adopted_next.first;
                    unit.target_y = adopted_next.second;
                    unit.current_waypoint++;
                    unit.path_target_override_active = false;
                    } else if (adopt_prepath && !adopted_replay_prepath &&
                           path.size() > 1) {
                    // k.o keeps the request's start tile in its solved list.
                    // y uses that node for this update's movement calculation,
                    // then aJ pops it at the end when it is within 16 units.
                    // Pathfinder::find_path omits the start tile, so preserve
                    // it only as this frame's physical target while exposing
                    // the already-popped path state.
                    const f32 start_node_x = pathfinder_.world_x(
                        pathfinder_.tile_x(unit.path_request_start_x));
                    const f32 start_node_y = pathfinder_.world_y(
                        pathfinder_.tile_y(unit.path_request_start_y));
                    const f32 start_distance_sq = dist2(
                        unit.x, unit.y, start_node_x, start_node_y);
                    const f32 second_distance_sq = dist2(
                        unit.x, unit.y, path.front().first, path.front().second);
                    // The transferred path can already have consumed its
                    // synthesized start tile when the unit is within 6. Fast
                    // units additionally skip it when the next node is within
                    // 19, matching y's two-node look-ahead optimization.
                    const bool pre_popped = start_distance_sq < 36.0f ||
                        (unit.move_speed_per_frame() > 5.0f &&
                         second_distance_sq < 361.0f);
                    if (!pre_popped && start_distance_sq < 256.0f) {
                        unit.has_deferred_navigation_target = true;
                        unit.deferred_navigation_target_x = start_node_x;
                        unit.deferred_navigation_target_y = start_node_y;
                    }
                }
                if (!adopt_prepath && !formation_owns_adopted_node &&
                    !patrol_refreshes_adopted_node && unit.using_path &&
                    unit.has_move_target &&
                    unit.current_waypoint <
                        static_cast<i32>(unit.waypoints.size()) &&
                    (dist2(unit.x, unit.y,
                           adopted_node_x, adopted_node_y) < 256.0f ||
                     adopted_node_covered_by_collision)) {
                    // JAR physics (y.a(f2,af,ad,au)) turns and moves toward
                    // the just-adopted first node, then aJ() pops it within
                    // the same object update when it was already within 16
                    // units before the move. This applies to ordinary and
                    // path-preserving grid requests. Command prepaths use
                    // their synthesized start-node handling above. Archive
                    // the node as the deferred physical target so this update
                    // still turns toward it while exposing the popped path.
                    unit.last_waypoint_stall_frames =
                        unit.waypoint_stall_frames;
                    unit.waypoint_stall_frames = 0.0f;
                    unit.waypoint_consumed_tick = tick_;
                    unit.has_deferred_navigation_target = true;
                    unit.deferred_navigation_target_x = adopted_node_x;
                    unit.deferred_navigation_target_y = adopted_node_y;
                    const auto& adopted_next =
                        unit.waypoints[unit.current_waypoint];
                    unit.target_x = adopted_next.first;
                    unit.target_y = adopted_next.second;
                    unit.current_waypoint++;
                    unit.path_target_override_active = false;
                }
                if (!adopt_prepath && !formation_owns_adopted_node && path.size() == 1 && unit.using_path &&
                    unit.has_move_target && !unit.order_queue.empty() &&
                    (unit.order_queue.front().type == WeaponType::LOAD_INTO ||
                     unit.order_queue.front().type == WeaponType::LOAD_UP) &&
                    unit.current_waypoint >=
                        static_cast<i32>(unit.waypoints.size()) &&
                    (dist2(unit.x, unit.y, unit.target_x, unit.target_y) < 16.0f ||
                     adopted_node_covered_by_collision)) {
                    // LOAD can adopt and consume a one-node solver result in
                    // the same physical pass. aJ() retains the captured node's
                    // turn/speed calculation and the handler's live proposal.
                    unit.last_waypoint_stall_frames = unit.waypoint_stall_frames;
                    unit.waypoint_stall_frames = 0.0f;
                    unit.waypoint_consumed_tick = tick_;
                    unit.has_deferred_navigation_target = true;
                    unit.deferred_navigation_target_x = unit.target_x;
                    unit.deferred_navigation_target_y = unit.target_y;
                    unit.target_x = unit.transport_order_target_x;
                    unit.target_y = unit.transport_order_target_y;
                    unit.using_path = false;
                    unit.waypoints.clear();
                    unit.current_waypoint = 0;
                    unit.formation_path_fallback_frames = 0.0f;
                    unit.formation_path_failure_frames = 0.0f;
                }
                }
            } else {
                advance_path_request_countdown(unit, delta_frames);
                // The request has no active av node yet. cK is false and the
                // unit only coasts on its previous heading this frame.
                // Preserve-mode requests keep an active av chain, so the
                // unit simply keeps following the adopted path.
                const bool pending_guard_combat_proposal =
                    unit.guard_movement_chases_main_target &&
                    unit.guard_movement_target_valid && !unit.using_path;
                const bool pending_direct_attack_chase =
                    unit.direct_attack_chase_this_update && !unit.using_path &&
                    unit.has_deferred_navigation_target;
                const bool pending_patrol_recent_proposal =
                    !unit.order_queue.empty() &&
                    unit.order_queue.front().type == WeaponType::PATROL &&
                    unit.recent_order_chase &&
                    unit.patrol_logical_target_active && !unit.using_path;
                const bool pending_guard_request_without_path =
                    !unit.order_queue.empty() &&
                    unit.order_queue.front().type == WeaponType::GUARD &&
                    unit.path_request_preserves_active_path &&
                    !unit.using_path;
                const bool pending_move_request_without_path =
                    !unit.order_queue.empty() &&
                    unit.order_queue.front().type == WeaponType::MOVE &&
                    !unit.using_path;
                const bool pending_build_request_without_path =
                    !unit.order_queue.empty() &&
                    unit.order_queue.front().type == WeaponType::BUILD &&
                    !unit.using_path;
                const bool pending_attack_move_request_without_proposal =
                    !unit.order_queue.empty() &&
                    unit.order_queue.front().type == WeaponType::ATTACK_MOVE &&
                    !unit.attack_move_direct_chase_proposal &&
                    !unit.using_path && unit.waypoint_consumed_tick != tick_;
                const bool pending_service_request =
                    !unit.order_queue.empty() &&
                    (unit.order_queue.front().type == WeaponType::REPAIR ||
                     unit.order_queue.front().type == WeaponType::RECLAIM);
                const bool pending_reversing_attack_request =
                    !unit.order_queue.empty() &&
                    unit.order_queue.front().type == WeaponType::ATTACK &&
                    !unit.using_path &&
                    !unit.path_request_preserves_active_path &&
                    unit.move_speed_factor < 0.0f;
                if (pending_reversing_attack_request) {
                    // JAR keeps the reverse coast's acceleration branch for
                    // every object pass while an ATTACK path is pending.
                    unit.movement_delay_uses_acceleration = true;
                }
                const bool pending_build_follower_request =
                    unit.path_request_prediction_suppressed &&
                    !unit.order_queue.empty() &&
                    unit.order_queue.front().type == WeaponType::BUILD &&
                    unit.target_uid == 0 &&
                    unit.formation_leader_uid != 0 &&
                    !unit.formation_projection_allowed;
                if (pending_service_request) {
                    // y.a() keeps the service chase's reverse/coast state
                    // while the truncated-path request is still in flight.
                    unit.movement_delay_uses_acceleration =
                        unit.reversing && unit.move_speed_factor < 0.0f;
                    if (unit.repath_cooldown_frames > 90.0f &&
                        unit.repath_cooldown_frames <=
                            unit.repath_cooldown_order_start) {
                        unit.repath_cooldown_frames = 90.0f;
                    }
                }
                if (pending_build_follower_request &&
                    unit.repath_cooldown_order_start >= 0.0f &&
                    unit.repath_cooldown_frames > 90.0f) {
                    // The handoff frame exposes the request at 500. On the
                    // following object update it takes the same 90-frame cap
                    // as the JAR's live follower chase.
                    unit.repath_cooldown_frames = 90.0f;
                }
                const bool pending_formation_projection =
                    !unit.order_queue.empty() &&
                    (unit.order_queue.front().type == WeaponType::MOVE ||
                     unit.order_queue.front().type == WeaponType::ATTACK_MOVE ||
                     unit.order_queue.front().type == WeaponType::ATTACK ||
                     unit.order_queue.front().type == WeaponType::BUILD ||
                     unit.order_queue.front().type == WeaponType::REPAIR ||
                     unit.order_queue.front().type == WeaponType::RECLAIM ||
                     unit.order_queue.front().type == WeaponType::LOAD_INTO ||
                     unit.order_queue.front().type == WeaponType::LOAD_UP) &&
                    unit.formation_leader_uid != 0 &&
                    unit.formation_projection_allowed;
                const bool pending_large_attack_move_request =
                    unit.path_request_pending &&
                    !unit.order_queue.empty() &&
                    unit.order_queue.front().type == WeaponType::ATTACK_MOVE &&
                    unit.order_queue.front().replay_command_group_size >= 19 &&
                    unit.order_queue.front().replay_command_has_long_prepath &&
                    !unit.order_queue.front().replay_command_has_handoff_cache &&
                    unit.formation_leader_uid != 0 &&
                    !unit.using_path &&
                    !unit.path_request_uses_prepath;
                // A preserved route can exhaust its last node before aU is
                // ready. Without af or a live combat j proposal, ATTACK_MOVE
                // keeps k/l/m but cannot steer until the result is adopted.
                if ((!unit.path_request_preserves_active_path ||
                     pending_guard_request_without_path ||
                     pending_move_request_without_path ||
                     pending_build_request_without_path ||
                     pending_attack_move_request_without_proposal) &&
                    !pending_guard_combat_proposal &&
                    !pending_direct_attack_chase &&
                    !pending_patrol_recent_proposal &&
                    !pending_formation_projection &&
                    !pending_large_attack_move_request &&
                    !replay_handoff_request_ready &&
                    !(unit.waypoint_consumed_tick == tick_ &&
                      unit.has_deferred_navigation_target)) {
                    // aJ() removes av after the physical helper captured
                    // its node. That consuming turn still steers and sets
                    // cK; only the following node-less turn coasts for aU.
                    unit.movement_activation_delay_frames = delta_frames;
                    if (pending_move_request_without_path ||
                        pending_build_request_without_path) {
                        // A retained aU cannot supply af after a replacement
                        // MOVE cleared av. Its coast has no cK proposal.
                        unit.movement_delay_keeps_prediction = false;
                    }
                }
            }
        }
        if (!order_phase) {
            if (move_request_was_active && unit.formation_stall_clock_refresh_pending) {
                // y.a(engine,delta,order,ad) refreshes an before dispatching
                // formation projection or steering, even when the old node
                // will be consumed in this same object update.
                unit.formation_tick = simulation_clock_ms_;
            }
            unit.formation_stall_clock_refresh_pending = false;
        }
        // y.a(delta, pathNode, proposal, order): a linked follower that has
        // remained stale or wall-clamped temporarily abandons the leader's
        // projected route. Grid requests retain the current chain; a clear
        // line rebuilds av in place, including the captured af node.
        bool formation_cleared_path = false;
        bool formation_helper_selected = false;
        if (!order_phase && delta_frames > 0.0f &&
            move_request_was_active &&
            unit.formation_leader_uid != 0 &&
            unit.formation_projection_allowed) {
            Unit* leader = formation_unit_(unit.formation_leader_uid);
            const bool matching_orders = leader &&
                (leader->order_queue.empty() ||
                 (!unit.order_queue.empty() &&
                  unit.order_queue.front().type ==
                      leader->order_queue.front().type &&
                  unit.order_queue.front().unit_type ==
                      leader->order_queue.front().unit_type &&
                  unit.order_queue.front().build_tech_level ==
                      leader->order_queue.front().build_tech_level &&
                  unit.order_queue.front().target_uid ==
                      leader->order_queue.front().target_uid &&
                  std::abs(unit.order_queue.front().x -
                      leader->order_queue.front().x) <= 1.0f &&
                  std::abs(unit.order_queue.front().y -
                      leader->order_queue.front().y) <= 1.0f));
            if (leader && leader->is_active() && matching_orders) {
                formation_helper_selected = true;
                const i32 formation_age = std::max(
                    0, simulation_clock_ms_ - leader->formation_tick);
                const f32 slot_x = leader->x + unit.formation_offset_x;
                const f32 slot_y = leader->y + unit.formation_offset_y;
                const f32 slot_distance_sq = dist2(
                    unit.x, unit.y, slot_x, slot_y);

                const bool idle_patrol_formation =
                    !unit.order_queue.empty() &&
                    unit.order_queue.front().type == WeaponType::PATROL &&
                    !unit.has_move_target && !unit.using_path;
                if ((formation_age > 300 && !idle_patrol_formation) ||
                    unit.wall_bump_frames > 1.0f) {
                    unit.formation_path_failure_frames += delta_frames;
                }
                // A historical leader link can survive after the follower's
                // own order has completed. JAR keeps that idle object out of
                // the private fallback request; only an active follower order
                // may turn the stale/failing formation state into a fallback.
                const bool fallback_triggered =
                    !idle_patrol_formation && !unit.order_queue.empty() &&
                    (unit.formation_path_failure_frames > 300.0f ||
                     (formation_age > 300 &&
                      slot_distance_sq > 250000.0f) ||
                     (unit.wall_bump_frames > 1.0f &&
                      (unit.formation_path_fallback_frames != 0.0f ||
                       unit.formation_path_failure_frames > 10.0f)));
                if (fallback_triggered) {
                    unit.formation_path_fallback_frames = 90.0f;
                }

                if (unit.formation_path_fallback_frames == 0.0f) {
                    if (unit.path_request_pending &&
                        unit.path_request_preserves_active_path) {
                        // aJ() can exhaust a fallback path before its grid
                        // refresh is published. Normal slot projection still
                        // supplies ad.b on the next update while aU waits.
                        unit.movement_activation_delay_frames = 0.0f;
                        unit.movement_prediction_enabled =
                            unit.has_move_target && unit.is_mobile();
                    }
                    // y.a(delta,af,ad,au) offset 184: the projecting branch
                    // opens with aH(). A follower steering at its leader's
                    // slot therefore carries no solved chain and no repath
                    // cooldown, however recently it was given one.
                    formation_cleared_path = true;
                    unit.using_path = false;
                    unit.waypoints.clear();
                    unit.current_waypoint = 0;
                    // aH() runs before the generic physical helper reads aw.
                    // Slot steering cannot inherit the private final-node
                    // stop radius or speed adjustments captured earlier.
                    unit.movement_navigation_points = 0;
                    unit.path_incomplete = false;
                    unit.repath_cooldown_frames = 0.0f;
                    unit.waypoint_stall_frames = 0.0f;
                    unit.last_waypoint_stall_frames = 0.0f;
                    unit.single_node_path_counter = 0;
                    // Near the live formation slot, the normal projection
                    // branch clears y.d after evaluating the fallback tests.
                    // Wall-clamped followers use the tighter radius.
                    const f32 recovery_radius =
                        unit.wall_bump_frames <= 1.0f ? 60.0f : 45.0f;
                    if (slot_distance_sq <
                        recovery_radius * recovery_radius) {
                        unit.formation_path_failure_frames = 0.0f;
                    }
                    if (move_request_was_active && !unit.order_queue.empty()) {
                        const WeaponType active_type =
                            unit.order_queue.front().type;
                        if (active_type == WeaponType::MOVE ||
                            active_type == WeaponType::ATTACK_MOVE ||
                            active_type == WeaponType::PATROL) {
                            // aH() drops av/aw but leaves the order handler's
                            // l/m proposal intact. A just-adopted private node
                            // must not become next turn's chainless order point.
                            // An idle PATROL handler has not published k/l/m.
                            unit.target_x = unit.order_queue.front().x;
                            unit.target_y = unit.order_queue.front().y;
                            unit.has_move_target = true;
                            unit.arrived = false;
                            unit.path_target_override_active = false;
                        }
                    }
                }

                if (unit.formation_path_fallback_frames != 0.0f) {
                    unit.formation_projection_allowed = false;

                    f32 fallback_x = leader->x;
                    f32 fallback_y = leader->y;
                    const i32 leader_points =
                        remaining_navigation_points(*leader);
                    if (leader->using_path && leader_points > 8) {
                        const i32 waypoint_index =
                            leader->current_waypoint + 7;
                        if (waypoint_index >= 0 && waypoint_index <
                                static_cast<i32>(leader->waypoints.size())) {
                            fallback_x =
                                leader->waypoints[waypoint_index].first;
                            fallback_y =
                                leader->waypoints[waypoint_index].second;
                        }
                    }

                    const f32 close_radius =
                        unit.radius + leader->radius + 15.0f;
                    if (dist2(unit.x, unit.y, fallback_x, fallback_y) <
                        close_radius * close_radius) {
                        unit.formation_path_failure_frames = 0.0f;
                        unit.formation_path_fallback_frames = 0.0f;
                    }

                    const bool had_deferred_path_node =
                        unit.has_deferred_navigation_target;
                    const bool had_path_node =
                        (unit.using_path && unit.has_move_target &&
                         !unit.arrived) || had_deferred_path_node;
                    // BUILD and service handlers publish l/m separately from
                    // the retained av node. A pending grid refresh leaves that
                    // physical node in place for this object's update.
                    const bool had_path_override =
                        unit.using_path && unit.path_target_override_active;
                    const f32 old_path_x = had_deferred_path_node
                        ? unit.deferred_navigation_target_x
                        : had_path_override ? unit.path_target_override_x
                                            : unit.target_x;
                    const f32 old_path_y = had_deferred_path_node
                        ? unit.deferred_navigation_target_y
                        : had_path_override ? unit.path_target_override_y
                                            : unit.target_y;
                    bool replaced_path = false;
                    if (!unit.path_request_pending && had_path_node &&
                        (std::abs(unit.last_request_x - fallback_x) > 300.0f ||
                         std::abs(unit.last_request_y - fallback_y) > 300.0f) &&
                        unit.repath_cooldown_frames > 30.0f) {
                        unit.repath_cooldown_frames = 30.0f;
                    }
                    if (!unit.path_request_pending &&
                        unit.repath_cooldown_frames == 0.0f) {
                        // The formation helper calls y.a(FFIZZ) directly.
                        // Its clear-line branch replaces av immediately;
                        // grid requests retain av until aU is published.
                        const i32 build_target_size = unit.path_request_target_size;
                        formation_fallback_path_request_active_ = true;
                        move_unit_to_(unit, fallback_x, fallback_y, 0,
                                      true, true);
                        formation_fallback_path_request_active_ = false;
                        if (unit.order_queue.front().type == WeaponType::BUILD) {
                            // The worker captures n=0, but y.a(FFIZZ) leaves
                            // the allowance already published by f(BUILD).
                            unit.path_request_target_size = build_target_size;
                        }
                        // This call is made from the physical pass, after the
                        // normal pending-request countdown has already run.
                        // Consume that pass's solver frame so adoption lands
                        // on the same tick as y's aU result.
                        advance_path_request_countdown(unit, delta_frames);
                        // requestPathTo caps a truncated building escape at
                        // ten frames. Keep that cap when restoring the
                        // formation fallback's normal 700-frame cooldown.
                        if (!(unit.using_path && unit.path_incomplete &&
                              unit.repath_cooldown_frames <= 10.0f)) {
                            unit.repath_cooldown_frames = 700.0f;
                        }
                        replaced_path = true;
                    }

                    if (!had_path_node) {
                        // The fallback helper publishes ad.b only from the
                        // af captured at entry. An exhausted chain therefore
                        // coasts even before s permits another request; k/l/m
                        // still retain the order handler's logical target.
                        unit.movement_activation_delay_frames = delta_frames;
                        unit.movement_delay_keeps_prediction = false;
                    } else if (replaced_path && unit.path_request_pending) {
                        // A grid result has not overwritten av yet, so this
                        // update still steers using the previous af node.
                        unit.has_deferred_navigation_target = true;
                        unit.deferred_navigation_target_x = old_path_x;
                        unit.deferred_navigation_target_y = old_path_y;
                    } else if (replaced_path) {
                        // y.a(FFIZZ)'s clear-line branch overwrites av[0] in
                        // place. The af captured by the formation helper now
                        // names the new node for this same physical update.
                        unit.has_deferred_navigation_target = false;
                        unit.path_target_override_active = false;
                    }
                }
            }
        }

        if (!order_phase && !formation_cleared_path &&
            unit.waypoint_consumed_tick == tick_ &&
            !unit.using_path) {
            // For a final node, y.aJ() clears formation d/c after the helper
            // has already selected this update's movement proposal. The C++
            // split order pass consumed the node earlier, so delay this reset
            // until the physical helper has observed the old fallback state.
            // The helper's aH() also clears a still-populated private chain
            // when fallback expires; consuming an intermediate node earlier
            // in the order pass must not turn that clear into a final node.
            unit.formation_path_failure_frames = 0.0f;
            unit.formation_path_fallback_frames = 0.0f;
        }

        if (!order_phase && unit.order_queue.empty() &&
            unit.direct_attack_chase_this_update && unit.using_path &&
            unit.has_move_target && !unit.arrived &&
            !unit.path_request_pending && unit.spawn_delay_frames == 0.0f &&
            unit.repath_cooldown_frames == 0.0f &&
            (unit.move_type == MoveType::AIR || unit.attack_range_adds_radii)) {
            // With no waypoint, y's generic controller still applies the
            // ct()/aV() range drift gate to the passive j/k/l/m proposal.
            // Its waypoint-only r gate does not apply to this chase.
            const f32 goal_x = unit.target_x;
            const f32 goal_y = unit.target_y;
            const f32 drift = unit.weapon_range - 1.0f;
            if (std::abs(unit.last_request_x - goal_x) > drift ||
                std::abs(unit.last_request_y - goal_y) > drift) {
                const f32 old_node_x = unit.path_target_override_active
                    ? unit.path_target_override_x : unit.target_x;
                const f32 old_node_y = unit.path_target_override_active
                    ? unit.path_target_override_y : unit.target_y;
                move_unit_to_(unit, goal_x, goal_y, 0, true, true);
                if (unit.path_request_pending) {
                    unit.path_target_override_active = unit.using_path;
                    unit.path_target_override_x = old_node_x;
                    unit.path_target_override_y = old_node_y;
                    advance_path_request_countdown(unit, delta_frames);
                } else {
                    // A direct request overwrites the captured av[0] before
                    // this same object turn selects steering and calls aJ().
                    unit.has_deferred_navigation_target = false;
                    unit.path_target_override_active = unit.using_path &&
                        (unit.target_x != goal_x || unit.target_y != goal_y);
                    unit.path_target_override_x = unit.target_x;
                    unit.path_target_override_y = unit.target_y;
                }
                order_path_refreshed = unit.using_path;
                unit.target_x = goal_x;
                unit.target_y = goal_y;
                unit.movement_navigation_points = unit.using_path
                    ? remaining_navigation_points(unit) : 0;
            }
        }

        const bool attack_move_combat_navigation =
            !unit.order_queue.empty() &&
            unit.order_queue.front().type == WeaponType::ATTACK_MOVE &&
            unit.attack_move_combat_target_valid &&
            unit.attack_move_private_navigation_handoff;
        const bool attack_move_order_navigation =
            !unit.order_queue.empty() &&
            unit.order_queue.front().type == WeaponType::ATTACK_MOVE &&
            unit.target_uid != 0 && !unit.attack_move_combat_target_valid;
        if (!order_phase && !formation_helper_selected &&
            unit.has_move_target &&
            !unit.arrived && !unit.path_request_pending &&
            unit.spawn_delay_frames == 0.0f &&
            !unit.order_queue.empty() &&
            (unit.using_path || attack_move_combat_navigation ||
             attack_move_order_navigation ||
             (unit.order_queue.front().type == WeaponType::ATTACK &&
              unit.direct_attack_chase_this_update)) &&
            (unit.order_queue.front().type == WeaponType::MOVE ||
             unit.order_queue.front().type == WeaponType::BUILD ||
             (unit.order_queue.front().type == WeaponType::ATTACK_MOVE &&
              unit.target_uid != 0 &&
              (!unit.attack_move_combat_target_valid ||
               attack_move_combat_navigation)) ||
             (unit.order_queue.front().type == WeaponType::PATROL &&
              !unit.patrol_logical_target_active) ||
             (unit.order_queue.front().type == WeaponType::ATTACK &&
              unit.direct_attack_chase_this_update)) &&
            (unit.formation_leader_uid == 0 ||
             !unit.formation_projection_allowed)) {
            // y.a(engine,...) compares o/p with the proposal after passive
            // combat and after s has spent this turn's delta. An existing
            // av node does not suppress a drift refresh when s reaches zero.
            // An in-range ATTACK_MOVE target rejected by both i(R) and j(R)
            // leaves the outer order's proposal active and takes this gate too.
            const UnitOrder& active = unit.order_queue.front();
            const bool captured_private_node = unit.using_path ||
                (unit.waypoint_consumed_tick == tick_ &&
                 unit.has_deferred_navigation_target);
            const f32 goal_x = attack_move_combat_navigation
                ? unit.attack_move_combat_target_x
                : active.type == WeaponType::ATTACK
                    ? unit.attack_order_target_x : active.x;
            const f32 goal_y = attack_move_combat_navigation
                ? unit.attack_move_combat_target_y
                : active.type == WeaponType::ATTACK
                    ? unit.attack_order_target_y : active.y;
            const f32 movement_drift = unit.weapon_range - 1.0f;
            const bool range_drifted =
                (unit.move_type == MoveType::AIR ||
                 unit.attack_range_adds_radii) &&
                (std::abs(unit.last_request_x - goal_x) > movement_drift ||
                 std::abs(unit.last_request_y - goal_y) > movement_drift);
            const bool order_drifted =
                std::abs(unit.last_request_x - goal_x) > 150.0f ||
                std::abs(unit.last_request_y - goal_y) > 150.0f;
            if (order_drifted && unit.repath_cooldown_frames > 30.0f) {
                unit.repath_cooldown_frames = 30.0f;
            }
            if ((!unit.using_path || range_drifted || order_drifted) &&
                unit.repath_cooldown_frames == 0.0f) {
                const bool had_path_node = unit.using_path;
                const f32 old_node_x = unit.path_target_override_active
                    ? unit.path_target_override_x : unit.target_x;
                const f32 old_node_y = unit.path_target_override_active
                    ? unit.path_target_override_y : unit.target_y;
                // f(BUILD) publishes its placement allowance before this
                // generic drift refresh. Keep n when replacing a retained
                // formation fallback chain; ordinary MOVE still requests 0.
                const i32 target_size =
                    active.type == WeaponType::BUILD ||
                    attack_move_combat_navigation
                    ? unit.path_request_target_size : 0;
                move_unit_to_(unit, goal_x, goal_y, target_size, true, true);
                if (unit.path_request_pending) {
                    // The grid branch leaves av untouched. Publishing the
                    // order's l/m must not overwrite its captured first node,
                    // including a node adopted earlier in this physical pass.
                    unit.path_target_override_active = unit.using_path;
                    unit.path_target_override_x = old_node_x;
                    unit.path_target_override_y = old_node_y;
                    advance_path_request_countdown(unit, delta_frames);
                    // A queued grid refresh keeps the captured old av node.
                    // Its normal aJ() check still runs after the request.
                    order_path_refreshed = had_path_node && unit.using_path;
                } else {
                    // Direct requests rewrite the captured av[0] in this
                    // turn. Retain a separate physical point only when the
                    // new first node differs from the raw order proposal.
                    unit.has_deferred_navigation_target = false;
                    unit.path_target_override_active = unit.using_path &&
                        (unit.target_x != goal_x || unit.target_y != goal_y);
                    unit.path_target_override_x = unit.target_x;
                    unit.path_target_override_y = unit.target_y;
                    order_path_refreshed = had_path_node && unit.using_path;
                }
                if (!had_path_node) {
                    // Only j supplies raw l/m when aE() was empty. A coast
                    // must not carry that point into the next turn, whose af
                    // now names the first node installed by a direct request.
                    unit.has_deferred_navigation_target =
                        (!attack_move_combat_navigation &&
                         !attack_move_order_navigation) ||
                        unit.attack_move_direct_chase_proposal ||
                        unit.direct_attack_chase_this_update;
                    unit.deferred_navigation_target_x = goal_x;
                    unit.deferred_navigation_target_y = goal_y;
                }
                unit.target_x = goal_x;
                unit.target_y = goal_y;
                unit.movement_navigation_points = unit.using_path
                    ? remaining_navigation_points(unit) : 0;
            }
            if ((attack_move_combat_navigation || attack_move_order_navigation) &&
                !captured_private_node && !unit.attack_move_direct_chase_proposal &&
                !unit.direct_attack_chase_this_update) {
                // The combat helper observed ad before the common controller
                // dropped an inactive leader or rejected its new waypoint.
                // Its live k/l/m/n still requests a private route, but j=false
                // and an empty captured af cannot supply this turn's steering.
                unit.movement_activation_delay_frames = delta_frames;
                unit.movement_delay_keeps_prediction = false;
            }
        }

        if (unit.custom_action_movement_blocked) {
            // custom.j.g gates y's physical helper via I(). The proposal and
            // path remain queued, while cK becomes false and no stall or
            // collision-clear timer advances from an immobile update. The
            // zero-delta order pass must also preserve ci: recalculating its
            // navigation reverse flag would reset every turret's idle timer
            // when the queued action restores its own reverse heading.
            unit.movement_prediction_enabled = false;
            unit.move_speed_factor = 0.0f;
            unit.vel_x = 0.0f;
            unit.vel_y = 0.0f;
            unit.collision_impulse_x = 0.0f;
            unit.collision_impulse_y = 0.0f;
            if (!order_phase) {
                // I() suppresses physical steering after path selection; it
                // still finishes this object's one-turn movement handoff.
                // Carrying that delay through a queued conversion prevents
                // the next ATTACK turn from requesting its cleared path.
                unit.movement_activation_delay_frames = 0.0f;
            }
            continue;
        }

        // am.cl delays path requests for a factory product, but it does not
        // suppress the unit's already-selected exit movement.
        if (!order_phase && delta_frames > 0.0f &&
            unit.movement_activation_delay_frames > 0.0f) {
            // This is the split controller's one-object-turn handoff, not
            // a native elapsed-time cooldown. A fractional delta still
            // finishes the coast pass before the next y.k()/y.a() update.
            unit.movement_activation_delay_frames = 0.0f;
            // No ad.b proposal calls y.a(delta,heading,...) during this
            // coast. Preserve ci even when cf is still negative after a
            // previous steering update ended reverse movement.
            // A newly selected waypoint does not provide an active movement
            // request until the next object update. Existing momentum still
            // decelerates and advances along the previous heading this frame.
            unit.movement_prediction_enabled =
                unit.movement_delay_keeps_prediction;
            // y.a(float,ad,au,boolean) chooses C()/D() from cf relative to
            // the zero proposal. Residual reverse motion still uses C()
            // after ci has turned off; that flag only preserves the heading
            // state above. A retained ci also gives the proposal a -0 sign.
            const f32 movement_delay_rate =
                unit.move_speed_factor < 0.0f
                    ? unit.move_acceleration : unit.move_deceleration;
            const f32 coast_factor = unit.reversing ? -0.0f : 0.0f;
            if (unit.strafes) {
                const bool had_velocity = unit.vel_x != 0.0f ||
                                          unit.vel_y != 0.0f;
                unit.move_speed_factor = 0.0f;
                approach_velocity(unit, 0.0f, 0.0f,
                                  unit.move_deceleration, delta_frames);
                const f32 clamp_old_x = unit.x, clamp_old_y = unit.y;
                unit.x += unit.vel_x * delta_frames;
                unit.y += unit.vel_y * delta_frames;
                const bool collision_displacement = integrate_external_displacement();
                if (had_velocity || collision_displacement) {
                    apply_movement_tile_clamp_(
                        unit, clamp_old_x, clamp_old_y, delta_frames);
                }
                if (had_velocity) unit.collision_moved = true;
            } else if (unit.move_speed_factor != 0.0f) {
                unit.move_speed_factor = approach_value(
                    unit.move_speed_factor, coast_factor,
                    movement_delay_rate * delta_frames);
                const f32 move_amount =
                    unit.move_speed_per_frame() *
                    unit.move_speed_factor * delta_frames;
                if (move_amount != 0.0f) unit.collision_moved = true;
                const f32 clamp_old_x = unit.x, clamp_old_y = unit.y;
                unit.x += jar_cosine_degrees(unit.rotation) * move_amount;
                unit.y += jar_sine_degrees(unit.rotation) * move_amount;
                const bool collision_displacement = integrate_external_displacement();
                // y.a(float) enters its tile clamp only while ay is set.
                // The final braking update can reduce cf to zero before
                // that gate, so it must not advance the wall-clear streak.
                if (move_amount != 0.0f || collision_displacement) {
                    apply_movement_tile_clamp_(
                        unit, clamp_old_x, clamp_old_y, delta_frames);
                }
            } else {
                const f32 clamp_old_x = unit.x, clamp_old_y = unit.y;
                if (integrate_external_displacement()) {
                    apply_movement_tile_clamp_(
                        unit, clamp_old_x, clamp_old_y, delta_frames);
                    grid_.update(unit);
                }
            }
            unit.movement_delay_keeps_prediction = false;
            unit.movement_delay_uses_acceleration = false;
            grid_.update(unit);
            continue;
        }

        if ((!unit.has_move_target || unit.arrived) && unit.strafes) {
            const bool had_velocity = unit.vel_x != 0.0f ||
                                      unit.vel_y != 0.0f;
            // y.a(float,ad,au,...) retains ci after heading completion and
            // applies its sign even to an idle zero proposal. The velocity
            // components still decay through D() after the request ends.
            if (!order_phase) {
                unit.move_speed_factor = unit.reversing && !unit.move_ignores_body
                    ? -0.0f : 0.0f;
            }
            approach_velocity(unit, 0.0f, 0.0f,
                              unit.move_deceleration, delta_frames);
            {
                const f32 clamp_old_x = unit.x, clamp_old_y = unit.y;
                unit.x += unit.vel_x * delta_frames;
                unit.y += unit.vel_y * delta_frames;
                const bool collision_displacement = integrate_external_displacement();
                if (had_velocity || collision_displacement) {
                    apply_movement_tile_clamp_(
                        unit, clamp_old_x, clamp_old_y, delta_frames);
                }
            }
            if (!order_phase && had_velocity) {
                unit.collision_moved = true;
            }
            unit.patrol_soft_stop_coast = false;
            grid_.update(unit);
            continue;
        }
        if (!unit.has_move_target || unit.arrived) {
            // A zero ad.b only brakes; y.aJ()/aH() own W/X changes.
            // In particular, coasting after the last ATTACK node must
            // retain the history archived by that node's aJ().
            if (!unit.strafes && unit.move_speed_factor != 0.0f) {
                const f32 stop_rate = unit.move_speed_factor < 0.0f
                    ? unit.move_acceleration : unit.move_deceleration;
                const f32 stop_factor = unit.reversing ? -0.0f : 0.0f;
                unit.move_speed_factor = approach_value(
                    unit.move_speed_factor, stop_factor,
                    stop_rate * delta_frames);
                const f32 move_amount =
                    unit.move_speed_per_frame() *
                    unit.move_speed_factor * delta_frames;
                if (!order_phase && move_amount != 0.0f) {
                    unit.collision_moved = true;
                }
                const f32 clamp_old_x = unit.x, clamp_old_y = unit.y;
                unit.x += jar_cosine_degrees(unit.rotation) * move_amount;
                unit.y += jar_sine_degrees(unit.rotation) * move_amount;
                const bool collision_displacement = integrate_external_displacement();
                if (move_amount != 0.0f || collision_displacement) {
                    apply_movement_tile_clamp_(
                        unit, clamp_old_x, clamp_old_y, delta_frames);
                }
                grid_.update(unit);
            } else {
                const f32 clamp_old_x = unit.x, clamp_old_y = unit.y;
                if (integrate_external_displacement()) {
                    apply_movement_tile_clamp_(
                        unit, clamp_old_x, clamp_old_y, delta_frames);
                    grid_.update(unit);
                }
            }
            unit.patrol_soft_stop_coast = false;
            continue;
        }
        if (!unit.is_mobile()) continue;

        // A new GUARD combat route has no af captured at entry. Its j
        // proposal supplies the raw chase point to both steering and aJ(),
        // including a collision that covers that point rather than av[0].
        const bool fresh_guard_combat_path =
            unit.guard_path_created_without_node &&
            unit.guard_movement_chases_main_target &&
            unit.has_deferred_navigation_target;
        const bool has_path_target_override =
            unit.using_path && unit.path_target_override_active;
        const f32 current_path_target_x = fresh_guard_combat_path
            ? unit.deferred_navigation_target_x
            : has_path_target_override ? unit.path_target_override_x : unit.target_x;
        const f32 current_path_target_y = fresh_guard_combat_path
            ? unit.deferred_navigation_target_y
            : has_path_target_override ? unit.path_target_override_y : unit.target_y;
        f32 dx = current_path_target_x - unit.x;
        f32 dy = current_path_target_y - unit.y;
        f32 d = std::sqrt(dx * dx + dy * dy);

        const i32 navigation_points = remaining_navigation_points(unit);
        const bool bomber_attack_proposal =
            unit.bomber_attack_movement && !unit.order_queue.empty() &&
            ((unit.order_queue.front().type == WeaponType::ATTACK &&
              unit.order_queue.front().target_uid != 0) ||
             (unit.order_queue.front().type == WeaponType::ATTACK_MOVE &&
              unit.attack_move_combat_target_valid) ||
             (unit.order_queue.front().type == WeaponType::PATROL &&
              unit.patrol_logical_target_active));
        // Passive combat publishes its chase after the zero-delta order
        // pass, including when an ATTACK waypoint stopped asserting k.
        // Sample live aw before consumption; that earlier pass had no move.
        if (order_phase ||
            (unit.direct_attack_chase_this_update &&
             !unit.has_deferred_navigation_target)) {
            unit.movement_navigation_points = unit.using_path
                ? navigation_points : 0;
        }
        // y.a(float,ad,au,boolean) offsets 517/813-849: the physical pass
        // consumes an intermediate node at 16 and the last one at the f13 it
        // hard-codes to 4. The separate ordered-point dispatch below owns
        // its 7/16/36 dwell tolerance; order age does not enlarge a path
        // node's radius.
        f32 arrival_distance;
        bool patrol_soft_stop = false;
        // remaining_navigation_points() reserves one logical point even
        // when aw is zero. AIR's direct ct() request has no exposed aw node
        // and reaches within 16 units; a chainless request stands in for the
        // order-point test the dispatch runs at 7.
        if (bomber_attack_proposal && !unit.using_path) {
            // With aw==0 the bomber's j proposal still supplies ad.b. The
            // physical helper calls aJ() inside 16, retaining k/l/m even
            // when that call has no path node left to remove.
            arrival_distance = 16.0f;
        } else if (order_phase && unit.using_path && !unit.order_queue.empty() &&
            unit.order_queue.front().type == WeaponType::PATROL) {
            // y.a(float, au, ad, boolean) dispatches this before looking at
            // av/aw. Its non-final PATROL collision branch can clear k/l/m
            // while retaining a multi-node av/aw chain.
            // Its boolean is the k value captured before k(float) reset it.
            const bool recent_patrol_collision =
                unit.collision_partner_tick == tick_ ||
                unit.collision_partner_tick == tick_ - 1;
            // Ordinary multi-node patrols retain their proposal while they
            // pass the ordered point (uid 652 at old tick 17657). The native
            // collision path is the exception: it can suppress k with aw
            // still above one (uid 609 at old tick 22294), except while a
            // final-node passive patrol handoff is already coasting.
            if (navigation_points != 1 &&
                (!recent_patrol_collision ||
                 unit.patrol_final_node_coast_pending)) {
                arrival_distance = 16.0f;
            } else {
                // A follow-up PATROL uses 70, while the final PATROL uses
                // 80 when the live proposal and collision record overlap.
                const f32 collision_arrival_distance =
                    unit.order_queue.size() == 1 ? 80.0f : 70.0f;
                const bool final_patrol_order = unit.order_queue.size() == 1;
                const f32 patrol_order_tolerance =
                    move_request_was_active && recent_patrol_collision
                        ? collision_arrival_distance : 30.0f;
                const UnitOrder& patrol_order = unit.order_queue.front();
                const f32 order_distance_sq = dist2(
                    unit.x, unit.y, patrol_order.x, patrol_order.y);
                // For a final patrol, the in-radius branch itself is a no-op
                // in the JAR. A passive target handled above can reassert k,
                // so only clear it when that branch did not keep it active.
                patrol_soft_stop = order_distance_sq <
                        patrol_order_tolerance * patrol_order_tolerance &&
                    (!final_patrol_order || !unit.patrol_logical_target_active);
                arrival_distance = unit.using_path ? 4.0f : 7.0f;
                if (navigation_points > 1) arrival_distance = 16.0f;
            }
        } else if (!unit.using_path && unit.move_type == MoveType::AIR &&
            unit.formation_leader_uid != 0) {
            arrival_distance = 16.0f;
        } else if (navigation_points == 1) {
            arrival_distance = unit.using_path ? 4.0f : 7.0f;
        } else {
            arrival_distance = 16.0f;
        }
        const auto collision_covers_point = [&](f32 point_x, f32 point_y) {
            if (unit.collision_partner_uid == 0 ||
                (unit.collision_partner_tick != tick_ &&
                 unit.collision_partner_tick != tick_ - 1)) {
                return false;
            }
            World* partner_world = unit.collision_partner_team_id == team_id_
                ? this
                : order_target_world_(
                      unit.collision_partner_team_id, other_worlds);
            Unit* partner = partner_world
                ? partner_world->waypoint_target_(
                      unit.collision_partner_uid, true)
                : nullptr;
            if (partner) {
                const f32 overlap_radius = partner->radius + 2.0f;
                // y.Z.c(nodeX,nodeY,2) is independent of the formation
                // fallback timer. A recent collision partner can cover a path
                // node after leaving the active object and spatial lists.
                return
                    dist2(partner->x, partner->y,
                          point_x, point_y) <
                    overlap_radius * overlap_radius;
            }
            return false;
        };
        const bool collision_covers_waypoint = unit.using_path &&
            collision_covers_point(current_path_target_x, current_path_target_y);
        // The order phase consumes existing nodes once per object turn.
        // A solver or command cache adopted in the physical pass can consume
        // its newly installed final node there, including BUILD and service
        // orders when a recent collision partner covers that node. Preserve the
        // completion rules and that node's steering proposal for this update.
        if (patrol_soft_stop) {
            // The JAR clears k/l/m for this dispatch but leaves av/aw intact.
            // Its physical pass still applies one braking step to the old
            // velocity, so leave move_speed_factor untouched until the idle
            // branch below handles that pass.
            unit.has_move_target = false;
            unit.arrived = true;
            unit.movement_prediction_enabled = false;
            unit.path_target_override_active = true;
            unit.path_target_override_x = current_path_target_x;
            unit.path_target_override_y = current_path_target_y;
            unit.target_x = 0.0f;
            unit.target_y = 0.0f;
            unit.patrol_soft_stop_coast = true;
            continue;
        }

        const bool passive_chase_node =
            !order_phase &&
            (unit.order_queue.empty() ||
             unit.order_queue.front().type == WeaponType::ATTACK) &&
            unit.direct_attack_chase_this_update && unit.using_path &&
            !unit.has_deferred_navigation_target;
        // A path created after the generic controller sampled aE() has no
        // active ad.b during its coast update. Its visible new node must not
        // be consumed by either proximity or the retained collision partner.
        const bool preserved_path_result_ready =
            order_phase && unit.path_request_pending &&
            unit.path_request_preserves_active_path &&
            (!unit.path_request_uses_prepath ||
             unit.path_request_prepath_submission_tick != tick_) &&
            unit.path_request_delay_frames <= 0.0f;
        const bool defer_formation_fallback_consumption =
            order_phase && unit.using_path &&
            unit.formation_leader_uid != 0 &&
            unit.formation_projection_allowed &&
            // The physical formation helper may clear this old av through
            // aH() before the generic controller samples its node count.
            (unit.formation_path_fallback_frames == 0.0f ||
             (unit.formation_fallback_countdown_active &&
              unit.formation_path_fallback_frames <= combat_delta_frames_) ||
             (!unit.path_request_pending &&
              unit.repath_cooldown_frames <=
                  (early_cooldown_order ? 0.0f : combat_delta_frames_)));
        if (defer_formation_fallback_consumption) {
            unit.formation_fallback_consumption_pending = true;
        }
        const bool consume_after_formation_refresh =
            !order_phase && unit.formation_fallback_consumption_pending &&
            unit.using_path;
        if (!order_phase) {
            unit.formation_fallback_consumption_pending = false;
        }
        // The formation helper selects leader+slot after the order pass and
        // clears av/aw before physical steering. With no private node, the
        // ordered l/m point cannot stand in for that slot's aJ() or completion
        // test. The helper below owns slot arrival and heading alignment.
        const bool projected_without_private_path =
            !unit.using_path && unit.formation_leader_uid != 0 &&
            unit.formation_projection_allowed;
        // b(l) replaces av/aw before the physical controller tests aJ().
        // Leave the old chain and W/X intact when that result will be adopted
        // in the physical pass of this same object update.
        const bool build_dispatch_aims = order_phase &&
            !unit.order_queue.empty() &&
            unit.order_queue.front().type == WeaponType::BUILD &&
            [&]() {
                const f32 range = service_range_to_(
                    unit, unit.order_queue.front().unit_type);
                return dist2(unit.x, unit.y, unit.order_queue.front().x,
                    unit.order_queue.front().y) <= range * range;
            }();
        bool patrol_refresh_before_consumption = false;
        if (order_phase &&
            unit.using_path && unit.has_move_target && !unit.arrived &&
            !unit.path_request_pending && !unit.order_queue.empty() &&
            unit.order_queue.front().type == WeaponType::PATROL &&
            (unit.formation_leader_uid == 0 ||
             !unit.formation_projection_allowed)) {
                const UnitOrder& patrol = unit.order_queue.front();
                const f32 goal_x = unit.patrol_logical_target_active
                    ? unit.patrol_logical_target_x : patrol.x;
                const f32 goal_y = unit.patrol_logical_target_active
                    ? unit.patrol_logical_target_y : patrol.y;
                const f32 dx = std::abs(unit.last_request_x - goal_x);
                const f32 dy = std::abs(unit.last_request_y - goal_y);
                const f32 range = unit.weapon_range - 1.0f;
                const bool order_drifted = dx > 150.0f || dy > 150.0f;
                if (order_drifted && unit.repath_cooldown_frames > 30.0f) {
                    unit.repath_cooldown_frames = 30.0f;
                }
                patrol_refresh_before_consumption =
                    unit.repath_cooldown_frames <=
                        (early_cooldown_order ? 0.0f : combat_delta_frames_) &&
                    (order_drifted ||
                    ((unit.move_type == MoveType::AIR ||
                      unit.attack_range_adds_radii) &&
                     (dx > range || dy > range)));
        }
        // The native generic controller can overwrite its captured av[0]
        // during a refresh before aJ runs. Keep the old node and W/X through
        // the split order pass until the physical controller refreshes it.
        if (!build_dispatch_aims && !preserved_path_result_ready &&
            !patrol_refresh_before_consumption &&
            (!unit.guard_path_created_without_node || fresh_guard_combat_path) &&
            !defer_formation_fallback_consumption &&
            !projected_without_private_path &&
            (order_phase || (adopted_order_final_node && unit.using_path) ||
             (order_path_refreshed && unit.waypoint_consumed_tick != tick_) ||
             passive_chase_node || consume_after_formation_refresh) &&
            (unit.movement_activation_delay_frames == 0.0f ||
             unit.movement_delay_keeps_prediction) &&
            (d < arrival_distance || collision_covers_waypoint)) {
            // y.aJ(): a normal waypoint consumption archives and resets the
            // stall counter.
            unit.last_waypoint_stall_frames = unit.waypoint_stall_frames;
            unit.waypoint_stall_frames = 0.0f;
            // aJ() preserves q; only aH() or a direct path request clears
            // the repeated one-node result count used by BUILD.
            unit.waypoint_consumed_tick = tick_;
            unit.arrived = true;
            unit.has_move_target = false;

            if (unit.current_waypoint < static_cast<i32>(unit.waypoints.size())) {
                unit.attack_final_node_coast_pending = false;
                unit.has_deferred_navigation_target = true;
                unit.deferred_navigation_target_x = current_path_target_x;
                unit.deferred_navigation_target_y = current_path_target_y;
                auto& wp = unit.waypoints[unit.current_waypoint];
                if (!unit.order_queue.empty() &&
                    unit.order_queue.front().type == WeaponType::PATROL &&
                    unit.path_target_override_active) {
                    // aJ() rotates av without changing the PATROL handler's
                    // l/m proposal. Keep its next physical node separate.
                    unit.path_target_override_x = wp.first;
                    unit.path_target_override_y = wp.second;
                } else {
                    unit.path_target_override_active = false;
                    unit.target_x = wp.first;
                    unit.target_y = wp.second;
                }

                unit.has_move_target = true;
                unit.arrived = false;
                unit.current_waypoint++;
            } else {
                const bool consumed_chain_node = unit.using_path;
                if (consumed_chain_node) {
                    // The split order pass consumes av before the formation
                    // helper selects ad.b. Preserve its old c/d for that
                    // selection, then clear them after the physical pass.
                    unit.formation_timers_clear_after_movement = true;
                }
                unit.attack_final_node_coast_pending =
                    consumed_chain_node && !unit.path_request_pending &&
                    !unit.order_queue.empty() &&
                    unit.order_queue.front().type == WeaponType::ATTACK;
                unit.path_target_override_active = false;
                unit.using_path = false;
                unit.waypoints.clear();
                unit.current_waypoint = 0;
                if (unit.path_request_pending &&
                    unit.path_request_preserves_active_path &&
                    !unit.order_queue.empty() &&
                    (unit.order_queue.front().type == WeaponType::ATTACK ||
                     unit.order_queue.front().type == WeaponType::PATROL)) {
                    // A preserved chase request can outlive its old av/aw
                    // chain. JAR stops movement prediction on the following
                    // object turn while the replacement request is pending.
                    unit.path_request_prediction_suppressed = true;
                }
                if (!unit.order_queue.empty() &&
                    (unit.order_queue.front().type == WeaponType::BUILD ||
                     unit.order_queue.front().type == WeaponType::REPAIR ||
                     unit.order_queue.front().type == WeaponType::RECLAIM)) {
                    // y.aJ() leaves k and the just-consumed node available to
                    // the physical pass. The build/service handler clears that
                    // context on the next object turn before re-pathing.
                    unit.has_move_target = true;
                    unit.arrived = false;
                    unit.has_deferred_navigation_target = true;
                    unit.deferred_navigation_target_x = current_path_target_x;
                    unit.deferred_navigation_target_y = current_path_target_y;
                    const UnitOrder& service_order = unit.order_queue.front();
                    unit.service_final_waypoint_coast =
                        service_order.type != WeaponType::BUILD;
                    if (service_order.type == WeaponType::BUILD) {
                        // f(BUILD) keeps publishing the placement while a
                        // linked follower returns to its formation slot.
                        unit.target_x = service_order.x;
                        unit.target_y = service_order.y;
                    }
                    unit.movement_delay_uses_acceleration =
                        unit.move_speed_factor < 0.0f;
                }
                if (bomber_attack_proposal) {
                    unit.has_move_target = true;
                    unit.arrived = false;
                    const WeaponType type = unit.order_queue.front().type;
                    if (type == WeaponType::ATTACK) {
                        unit.target_x = unit.attack_order_target_x;
                        unit.target_y = unit.attack_order_target_y;
                    } else if (type == WeaponType::ATTACK_MOVE) {
                        unit.target_x = unit.attack_move_combat_target_x;
                        unit.target_y = unit.attack_move_combat_target_y;
                    } else {
                        unit.target_x = unit.patrol_logical_target_x;
                        unit.target_y = unit.patrol_logical_target_y;
                    }
                    unit.has_deferred_navigation_target = true;
                    unit.deferred_navigation_target_x = current_path_target_x;
                    unit.deferred_navigation_target_y = current_path_target_y;
                    unit.attack_final_node_coast_pending = false;
                    unit.attack_move_final_node_coast_pending = false;
                    unit.patrol_final_node_coast_pending = false;
                } else if (unit.path_incomplete && !unit.order_queue.empty()) {
                    // y: a truncated path re-requests toward the ordered
                    // destination on the following object update, paced at
                    // s < 450 (at least 50 frames after the previous request).
                    // aJ() does not clear y.u. The captured af remains this
                    // update's physical target even when the retry is still
                    // cooling down; later updates coast with no active af.
                    const UnitOrder& active = unit.order_queue.front();
                    unit.path_retry_next_frame = true;
                    unit.target_x = active.x;
                    unit.target_y = active.y;
                    unit.has_move_target = true;
                    unit.arrived = false;
                    unit.has_deferred_navigation_target = true;
                    unit.deferred_navigation_target_x = current_path_target_x;
                    unit.deferred_navigation_target_y = current_path_target_y;
                    unit.movement_activation_delay_frames = 0.0f;
                    unit.movement_delay_keeps_prediction = false;
                } else if (!unit.order_queue.empty()) {
                    const UnitOrder& front = unit.order_queue.front();
                    if (front.type == WeaponType::MOVE &&
                        consumed_chain_node &&
                        unit.formation_leader_uid == 0) {
                        // y.a(float,ad,au,boolean) offset 1094: consuming the
                        // last node of an untruncated chain ends a plain MOVE
                        // outright, however far the ordered point still is.
                        // y.u and a formation link (y.ad) each veto it, and
                        // the ordered point has its own tolerance in the
                        // dispatch, which ran before this pass.
                        const f32 consumed_node_x = current_path_target_x;
                        const f32 consumed_node_y = current_path_target_y;
                        const f32 ordered_x = front.x;
                        const f32 ordered_y = front.y;
                        complete_formation_followers_(unit, other_worlds);
                        complete_current_order_(unit, other_worlds, false, true);
                        if (unit.order_queue.empty() ||
                            unit.activate_order_next_frame) {
                            // The dispatch published k/l/m at the ordered
                            // point earlier in this same object update and the
                            // drop happens afterwards, so the proposal
                            // survives the update. ad.b was taken from the
                            // node that has just been consumed. A queued
                            // waypoint activates on the next object turn and
                            // retains the same old proposal here.
                            unit.has_move_target = true;
                            unit.arrived = false;
                            unit.target_x = ordered_x;
                            unit.target_y = ordered_y;
                            unit.has_deferred_navigation_target = true;
                            unit.deferred_navigation_target_x =
                                consumed_node_x;
                            unit.deferred_navigation_target_y =
                                consumed_node_y;
                            unit.clear_exposed_move_next_frame = true;
                        }
                    } else if (front.type == WeaponType::MOVE ||
                               front.type == WeaponType::ATTACK_MOVE) {
                        bool keep_short_formation_attack_move = false;
                        Unit* short_formation_attack_target = nullptr;
                        if (front.type == WeaponType::ATTACK_MOVE &&
                            consumed_chain_node &&
                            unit.formation_is_leader &&
                            unit.formation_group_size > 1 &&
                            !unit.formation_preferred &&
                            unit.target_uid != 0) {
                            World* target_world = order_target_world_(
                                unit.target_team_id, other_worlds);
                            Unit* target = target_world
                                ? target_world->get_unit(unit.target_uid)
                                : nullptr;
                            if (target && target->is_alive &&
                                target->parent_uid == 0 &&
                                target_world != this &&
                                is_enemy_with(*target_world) &&
                                can_target_(unit, *target)) {
                                const f32 follow_range = attack_follow_stop_range(unit, *target);
                                keep_short_formation_attack_move =
                                    unit.bomber_attack_movement ||
                                    dist2(unit.x, unit.y, target->x, target->y) >=
                                    follow_range * follow_range ||
                                    attack_target_needs_approach(*this, unit, *target);
                                if (keep_short_formation_attack_move) {
                                    short_formation_attack_target = target;
                                }
                            }
                        }
                        if (keep_short_formation_attack_move) {
                            // A short, inactive formation leader keeps its
                            // ATTACK_MOVE order after the final av node is
                            // consumed when the live combat target is still
                            // outside follow range. JAR leaves k/l/m on that
                            // target and lets the next pass submit a chase.
                            unit.has_move_target = true;
                            unit.arrived = false;
                            unit.target_x = short_formation_attack_target->x;
                            unit.target_y = short_formation_attack_target->y;
                            unit.has_deferred_navigation_target = true;
                            unit.deferred_navigation_target_x =
                                current_path_target_x;
                            unit.deferred_navigation_target_y =
                                current_path_target_y;
                            unit.attack_move_final_node_coast_pending =
                                !unit.path_request_pending;
                        } else if (front.type == WeaponType::ATTACK_MOVE &&
                                   consumed_chain_node &&
                                   unit.formation_leader_uid == 0) {
                            // aJ() only calls ax() for plain MOVE. An
                            // unlinked ATTACK_MOVE keeps its dispatch proposal
                            // after either a complete or truncated final node.
                            unit.has_move_target = true;
                            unit.arrived = false;
                            unit.target_x = unit.attack_move_combat_target_valid
                                ? unit.attack_move_combat_target_x : front.x;
                            unit.target_y = unit.attack_move_combat_target_valid
                                ? unit.attack_move_combat_target_y : front.y;
                            unit.has_deferred_navigation_target = true;
                            unit.deferred_navigation_target_x =
                                current_path_target_x;
                            unit.deferred_navigation_target_y =
                                current_path_target_y;
                        } else {
                        const bool linked_chain_exempts_completion =
                            consumed_chain_node &&
                            unit.formation_leader_uid != 0;
                        const bool keep_projected_prepath_proposal =
                            front.prepath_from_replay &&
                            unit.formation_leader_uid != 0 &&
                            unit.formation_projection_allowed;
                        if (linked_chain_exempts_completion ||
                            keep_projected_prepath_proposal) {
                            // y.aJ() only calls ax() for a final plain MOVE
                            // when y.ad is null. A linked follower therefore
                            // keeps the order after consuming its final chain
                            // node. Projection steers at the leader slot; a
                            // private fallback keeps the consumed node as the
                            // physical target for the rest of this update.
                            unit.has_move_target = true;
                            unit.arrived = false;
                            unit.target_x = front.x;
                            unit.target_y = front.y;
                            unit.has_deferred_navigation_target =
                                linked_chain_exempts_completion;
                            if (unit.has_deferred_navigation_target) {
                                unit.deferred_navigation_target_x =
                                    current_path_target_x;
                                unit.deferred_navigation_target_y =
                                    current_path_target_y;
                            }
                            unit.path_target_override_active = false;
                        } else {
                        // y.a(float,af,ad,au) offset 1034: a formation
                        // follower that reaches its projected slot and is
                        // aligned to within 3 degrees drops its MOVE or
                        // attackMove waypoint. The chainless AIR follower's
                        // 16-unit consumption above stands in for that test.
                            if (dist(unit.x, unit.y, front.x, front.y) <
                                    EngineConstants::ARRIVAL_THRESHOLD ||
                                collision_covers_waypoint) {
                                const f32 exposed_target_x = unit.target_x;
                                const f32 exposed_target_y = unit.target_y;
                                complete_current_order_(unit, other_worlds);
                                if (unit.order_queue.empty() &&
                                    (collision_covers_waypoint ||
                                     (unit.move_type == MoveType::AIR &&
                                      unit.formation_leader_uid != 0))) {
                                    // y.aJ() completes a final MOVE, but k(float)
                                    // still uses that movement proposal for this
                                    // object update. Keep the old target through
                                    // the physical pass and clear it next turn.
                                    unit.has_move_target = true;
                                    unit.arrived = false;
                                    unit.target_x = exposed_target_x;
                                    unit.target_y = exposed_target_y;
                                    unit.has_deferred_navigation_target = true;
                                    unit.deferred_navigation_target_x =
                                        exposed_target_x;
                                    unit.deferred_navigation_target_y =
                                        exposed_target_y;
                                    unit.clear_exposed_move_next_frame = true;
                                }
                            }
                        }
                        }
                    } else if (front.type == WeaponType::LOAD_INTO ||
                               front.type == WeaponType::LOAD_UP) {
                        // h()/e() publish the transport's live position before
                        // aJ() consumes the last node. Keep that proposal and
                        // finish this physical pass against the captured node;
                        // the next object turn owns any replacement request.
                        unit.has_move_target = true;
                        unit.arrived = false;
                        unit.target_x = unit.transport_order_target_x;
                        unit.target_y = unit.transport_order_target_y;
                        unit.has_deferred_navigation_target = true;
                        unit.deferred_navigation_target_x = current_path_target_x;
                        unit.deferred_navigation_target_y = current_path_target_y;
                    } else if ((front.type == WeaponType::GUARD ||
                                front.type == WeaponType::FOLLOW) &&
                               unit.guard_movement_target_valid) {
                        // c(GUARD) published k/l/m before the physical helper
                        // captured this node. aJ() only removes av/aw, so keep
                        // that proposal and steer at the consumed node for
                        // this object turn. The next dispatch owns any coast
                        // or fresh follow request once there is no node left.
                        unit.has_move_target = true;
                        unit.arrived = false;
                        unit.target_x = unit.guard_movement_target_x;
                        unit.target_y = unit.guard_movement_target_y;
                        unit.has_deferred_navigation_target = true;
                        unit.deferred_navigation_target_x = current_path_target_x;
                        unit.deferred_navigation_target_y = current_path_target_y;
                    } else if (front.type == WeaponType::ATTACK &&
                               front.target_uid != 0 && unit.is_mobile()) {
                        // The ATTACK handler publishes k/l/m before a final
                        // path node is consumed. y.aJ() removes av/aw but
                        // leaves that live chase proposal in place when the
                        // target is still outside follow range; the next
                        // object turn then submits the replacement path.
                        // Preserve it here instead of exposing one idle
                        // frame between the two requests.
                        World* attacked_world = order_target_world_(
                            front.target_team_id, other_worlds);
                        Unit* attacked = attacked_world
                            ? attacked_world->get_unit(front.target_uid) : nullptr;
                        if (attacked && attacked->is_alive &&
                            attacked->parent_uid == 0 &&
                            attacked_world != this) {
                            // g(ATTACK) can follow another player's unit,
                            // including an ally, even when its layer
                            // cannot be fired on. Weapon eligibility does not
                            // cancel the dispatch or its captured path node.
                            const f32 follow_range = attack_follow_stop_range(unit, *attacked);
                            if (unit.direct_attack_chase_this_update ||
                                unit.bomber_attack_movement ||
                                dist2(unit.x, unit.y, attacked->x,
                                      attacked->y) >=
                                    follow_range * follow_range ||
                                attack_target_needs_approach(*this, unit, *attacked)) {
                                unit.has_deferred_navigation_target = true;
                                unit.deferred_navigation_target_x =
                                    current_path_target_x;
                                unit.deferred_navigation_target_y =
                                    current_path_target_y;
                                unit.target_x = unit.attack_order_target_x;
                                unit.target_y = unit.attack_order_target_y;
                                unit.has_move_target = true;
                                // aJ() runs after the physical helper selected
                                // the old node. Keep steering and cK for this
                                // consuming turn; attack_final_node_coast_pending
                                // suppresses them on the next object update.
                                unit.arrived = false;
                            }
                        }
                    } else if (front.type == WeaponType::PATROL) {
                        // aJ() does not finish a PATROL waypoint after its
                        // final chain node. Keep the dispatch's ordered or
                        // passive-target proposal and the existing s, just
                        // as for a final node consumed before this adoption.
                        if (!consumed_chain_node &&
                            !unit.patrol_logical_target_active &&
                            !unit.path_request_pending) {
                            complete_current_order_(unit, other_worlds, true);
                        } else {
                            unit.has_move_target = true;
                            unit.arrived = false;
                            unit.target_x = unit.patrol_logical_target_active
                                ? unit.patrol_logical_target_x : front.x;
                            unit.target_y = unit.patrol_logical_target_active
                                ? unit.patrol_logical_target_y : front.y;
                            // aJ() clears av/aw after the physical helper has
                            // selected this node. Preserve its turn and zero
                            // requested speed for the consuming object turn.
                            unit.has_deferred_navigation_target = true;
                            unit.deferred_navigation_target_x =
                                current_path_target_x;
                            unit.deferred_navigation_target_y =
                                current_path_target_y;
                            unit.movement_activation_delay_frames = 0.0f;
                            unit.movement_delay_keeps_prediction = false;
                            unit.patrol_final_node_coast_pending = true;
                        }
                    }
                } else if (passive_chase_node) {
                    // With no waypoint, passive combat still publishes k/l/m.
                    // aJ() clears av/aw after the physical helper captured
                    // this node, leaving that proposal active for this turn.
                    unit.has_move_target = true;
                    unit.arrived = false;
                    unit.has_deferred_navigation_target = true;
                    unit.deferred_navigation_target_x = current_path_target_x;
                    unit.deferred_navigation_target_y = current_path_target_y;
                }
            }
            if (consume_after_formation_refresh && !unit.using_path) {
                unit.formation_path_failure_frames = 0.0f;
                unit.formation_path_fallback_frames = 0.0f;
            }
            if (order_phase) continue;
        }

        if (replay_move_cache_refresh_pending) {
            // The native replay cache refresh is submitted on the following
            // object pass. Its zero-delta pass keeps the prior coast state
            // instead of steering toward the ordered point once more.
            continue;
        }

        const bool use_deferred_navigation_target =
            !order_phase &&
            (unit.has_deferred_navigation_target ||
             (unit.using_path && unit.path_target_override_active));
        // aJ() advances av/aw after selecting this update's movement context.
        // Combat can then save the next av[0] before publishing live l/m;
        // that persistent override must not replace the captured old node.
        const bool use_consumed_navigation_target =
            !order_phase && unit.has_deferred_navigation_target;
        f32 movement_target_x = use_deferred_navigation_target
            ? (use_consumed_navigation_target
                ? unit.deferred_navigation_target_x
                : unit.path_target_override_x)
            : unit.target_x;
        f32 movement_target_y = use_deferred_navigation_target
            ? (use_consumed_navigation_target
                ? unit.deferred_navigation_target_y
                : unit.path_target_override_y)
            : unit.target_y;
        bool formation_following = false;
        bool formation_hold = false;
        bool formation_turn_hold = false;
        bool formation_uses_early_path_target = false;
        bool formation_aligning_to_heading = false;
        bool formation_alignment_completes_order = false;
        if (unit.formation_leader_uid != 0) {
            Unit* leader = formation_unit_(unit.formation_leader_uid);
            // y only compares waypoints when both the follower and its
            // leader still have one. A leader that has just completed its
            // waypoint remains linked and the follower keeps closing on the
            // cached slot behind that now-idle leader.
            const bool build_follower_repair_handoff = leader &&
                !unit.order_queue.empty() &&
                unit.order_queue.front().type == WeaponType::BUILD &&
                unit.target_uid == 0 &&
                !leader->order_queue.empty() &&
                (leader->order_queue.front().type == WeaponType::REPAIR ||
                 leader->order_queue.front().type == WeaponType::RECLAIM) &&
                leader->order_queue.front().target_uid != 0;
            const bool matching_orders = leader &&
                (leader->order_queue.empty() ||
                 (!unit.order_queue.empty() &&
                  unit.order_queue.front().type ==
                      leader->order_queue.front().type &&
                  unit.order_queue.front().unit_type ==
                      leader->order_queue.front().unit_type &&
                  unit.order_queue.front().build_tech_level ==
                      leader->order_queue.front().build_tech_level &&
                  unit.order_queue.front().target_uid ==
                      leader->order_queue.front().target_uid &&
                  std::abs(unit.order_queue.front().x - leader->order_queue.front().x) <= 1.0f &&
                  std::abs(unit.order_queue.front().y - leader->order_queue.front().y) <= 1.0f) ||
                 build_follower_repair_handoff);
            if (!leader || !leader->is_active()) {
                // The common helper drops an invalid leader. Different
                // waypoints disable projection while retaining the ad link.
                unit.formation_leader_uid = 0;
            }
            if (leader && leader->is_active()) {
                formation_following = true;
                if (!matching_orders) unit.formation_projection_allowed = false;
                if (unit.formation_projection_allowed) {
                    const bool leader_path_hidden =
                        leader->formation_path_activation_pending &&
                        unit.object_creation_order <
                            leader->object_creation_order;
                    const f32 leader_path0_x = leader->path_target_override_active
                        ? leader->path_target_override_x : leader->target_x;
                    const f32 leader_path0_y = leader->path_target_override_active
                        ? leader->path_target_override_y : leader->target_y;
                    movement_target_x = leader->x + unit.formation_offset_x;
                    movement_target_y = leader->y + unit.formation_offset_y;
                    const f32 formation_slot_distance_sq = dist2(
                        unit.x, unit.y,
                        movement_target_x, movement_target_y);
                    const i32 leader_points = leader_path_hidden
                        ? 0 : remaining_navigation_points(*leader);
                    // ad.b is still set when a follower is outside its slot
                    // tolerance even after the leader exhausts its path. It
                    // keeps closing on leader+offset instead of braking merely
                    // because the leader has no remaining nodes.
                    formation_hold = false;
                    // A follower retaining its own path holds its body turn
                    // while the leader has aU pending. A projection-only
                    // follower still steers toward the live leader slot.
                    formation_turn_hold = leader->path_request_pending &&
                        unit.using_path &&
                        !unit.order_queue.empty() &&
                        unit.order_queue.front().type == WeaponType::ATTACK;
                    const i32 leader_total_points = leader_path_hidden
                        ? 0
                        : (leader->using_path
                           ? 1 + static_cast<i32>(leader->waypoints.size())
                           : 0);
                    const i32 formation_age = std::max(
                        0, simulation_clock_ms_ - leader->formation_tick);
                    auto leader_path_point = [&](i32 index,
                                                 f32& point_x,
                                                 f32& point_y) {
                        if (!leader->using_path || index < 0 ||
                            index >= leader_points) {
                            return false;
                        }
                        if (index == 0) {
                            point_x = leader_path0_x;
                            point_y = leader_path0_y;
                            return true;
                        }
                        const i32 waypoint_index =
                            leader->current_waypoint + index - 1;
                        if (waypoint_index < 0 || waypoint_index >=
                                static_cast<i32>(leader->waypoints.size())) {
                            return false;
                        }
                        point_x = leader->waypoints[waypoint_index].first;
                        point_y = leader->waypoints[waypoint_index].second;
                        return true;
                    };

                    // During the first part of a newly assigned formation,
                    // y's follower helper looks farther down the leader path.
                    // With three or more nodes and at most two consumed it
                    // projects o(2); a fresh one/two-node path projects an
                    // artificial point 80 units beyond the leader instead.
                    f32 early_x = 0.0f, early_y = 0.0f;
                    if (formation_age < 3000 && leader_total_points > 2 &&
                        leader_total_points - leader_points <= 2 &&
                        leader_path_point(2, early_x, early_y)) {
                        movement_target_x =
                            early_x + unit.formation_offset_x;
                        movement_target_y =
                            early_y + unit.formation_offset_y;
                        formation_uses_early_path_target = true;
                        formation_hold = false;
                    } else if (formation_age < 1500 &&
                               leader_total_points > 0 &&
                               leader_points >= leader_total_points &&
                               leader_path_point(0, early_x, early_y)) {
                        const f32 early_heading = jar_direction_degrees(
                            leader->x, leader->y, early_x, early_y);
                        f32 early_distance = 80.0f;
                        if (formation_age > 300) {
                            early_distance -=
                                static_cast<f32>(formation_age - 300) *
                                0.06666667f;
                        }
                        movement_target_x = leader->x +
                            jar_cosine_degrees(early_heading) * early_distance +
                            unit.formation_offset_x;
                        movement_target_y = leader->y +
                            jar_sine_degrees(early_heading) * early_distance +
                            unit.formation_offset_y;
                        formation_uses_early_path_target = true;
                        formation_hold = false;
                    } else if (leader->using_path &&
                               leader_total_points >= 2 &&
                               leader_points >= 1) {
                        const f32 path0_x = leader_path0_x;
                        const f32 path0_y = leader_path0_y;
                        f32 path1_x = path0_x;
                        f32 path1_y = path0_y;
                        if (leader_points >= 2 &&
                            leader->current_waypoint <
                                static_cast<i32>(leader->waypoints.size())) {
                            const auto& path1 =
                                leader->waypoints[leader->current_waypoint];
                            path1_x = path1.first;
                            path1_y = path1.second;
                        }
                        const i32 distance_to_path =
                            static_cast<i32>(std::lround(std::sqrt(
                                static_cast<f32>(static_cast<i32>(dist2(
                                    leader->x, leader->y,
                                    path0_x, path0_y))))));
                        const f32 path_factor = std::clamp(
                            1.0f - static_cast<f32>(distance_to_path - 15) *
                                       0.05f,
                            0.0f, 2.0f);
                        f32 path_delta_x = path1_x - path0_x;
                        f32 path_delta_y = path1_y - path0_y;
                        if (path_factor > 1.0f) {
                            if (leader_points >= 3 &&
                                leader->current_waypoint + 1 <
                                    static_cast<i32>(leader->waypoints.size())) {
                                const auto& path2 = leader->waypoints[
                                    leader->current_waypoint + 1];
                                path_delta_x += (path2.first - path1_x) *
                                                (path_factor - 1.0f);
                                path_delta_y += (path2.second - path1_y) *
                                                (path_factor - 1.0f);
                            }
                            // y.a(float, af, ad, au) stops at o(1) when
                            // the leader has no third node to project onto.
                        } else {
                            path_delta_x *= path_factor;
                            path_delta_y *= path_factor;
                        }
                        movement_target_x =
                            path0_x + unit.formation_offset_x + path_delta_x;
                        movement_target_y =
                            path0_y + unit.formation_offset_y + path_delta_y;
                    } else {
                        movement_target_x =
                            leader->x + unit.formation_offset_x;
                        movement_target_y =
                            leader->y + unit.formation_offset_y;
                    }

                    // y.e is charged only after the linked leader's active
                    // order has disappeared. The follower's own order type
                    // only selects the later tolerance bucket.
                    if (leader->order_queue.empty() && !order_phase) {
                        unit.formation_idle_leader_frames += delta_frames;
                    }

                    // Once an idle leader has reached its order point, a
                    // nearby follower stops driving toward the exact slot and
                    // rotates into the heading cached by the formation
                    // assignment. y performs this turn from the formation
                    // helper even though ad.b remains false for the generic
                    // movement pass. MOVE/ATTACK_MOVE completes once the
                    // pre-turn heading error is below three degrees.
                    if (leader->order_queue.empty() &&
                        !unit.order_queue.empty() &&
                        (unit.order_queue.front().type == WeaponType::MOVE ||
                         unit.order_queue.front().type ==
                             WeaponType::ATTACK_MOVE)) {
                        const f32 elapsed =
                            unit.formation_idle_leader_frames;
                        f32 slot_radius = 16.0f;
                        if (elapsed > 600.0f)      slot_radius = 260.0f;
                        else if (elapsed > 360.0f) slot_radius = 140.0f;
                        else if (elapsed > 180.0f) slot_radius = 70.0f;
                        else if (elapsed > 120.0f) slot_radius = 50.0f;
                        if (formation_slot_distance_sq <
                            slot_radius * slot_radius) {
                            formation_aligning_to_heading = true;
                            formation_hold = true;
                            unit.movement_prediction_enabled = false;
                        }
                    }
                }
            }
        }
        dx = movement_target_x - unit.x;
        dy = movement_target_y - unit.y;
        const f32 physical_target_distance_sq = dx * dx + dy * dy;
        // y.b(l) installs a ready retained worker before af is captured.
        // A GUARD replacement therefore tests that adopted node's distance,
        // including on the command turn that publishes a different l/m.
        const f32 turn_tolerance_distance_sq =
            consume_guard_logical_tolerance &&
                    !adopted_path_without_prior_node &&
                    !unit.order_queue.empty() &&
                    unit.order_queue.front().type == WeaponType::GUARD &&
                    unit.guard_movement_target_valid
                ? dist2(unit.x, unit.y,
                        unit.guard_movement_target_x,
                        unit.guard_movement_target_y)
                : dx * dx + dy * dy;

        // A temporarily grounded/launching aircraft can have no horizontal
        // speed while its JAR object still updates body turn and cf. Keep the
        // kinematic pass alive; the zero speed naturally suppresses distance
        // integration below.
        const f32 max_speed_per_frame = unit.move_speed_per_frame();
        if (max_speed_per_frame < 0.0f) continue;
        if (!order_phase && max_speed_per_frame == 0.0f &&
            unit.move_speed_factor != 0.0f) {
            unit.collision_moved = true;
        }
        const f32 desired_rotation = formation_aligning_to_heading
            ? unit.formation_heading
            : jar_direction_degrees(
                  unit.x, unit.y, movement_target_x, movement_target_y);
        auto turn_body_with_turrets = [&](f32 desired) {
            // The split order pass only plans steering. Calling i(0) here
            // would wrap a transport's +/-180-degree unload heading before
            // a delayed first physical pass coasts using that original angle.
            if (order_phase) return;
            const f32 old_rotation = unit.rotation;
            // y.i(float) receives the applied turn before cg's rounded
            // addition. Reconstructing it from the two body angles changes
            // ap.a by a ULP, especially on an independent nano turret.
            const f32 body_delta = turn_toward(unit, desired, delta_frames);
            // custom.l.dL is false for scout.ini's
            // turretRotateWithBody:false. Its idle return ran before k(float),
            // and neither target acquisition nor this body turn adds the body
            // delta to the independent turret.
            // Acceleration can reach exactly zero while y.a(...) still
            // calls i(0). That call wraps coupled turret angles too.
            if (body_delta == 0.0f &&
                (std::abs(shortest_angle_delta(old_rotation, desired)) <
                     0.01f || unit.turn_speed <= 0.0f)) return;
            // Stock hover tanks always turn only the body. b.c.i(float)
            // does the same for a jet while Z() reports a main target;
            // without R it delegates to y.i and carries the turrets too.
            // Custom units inherit y.i, so turretRotateWithBody still applies
            // even when moveIgnoringBody is enabled (heavyInterceptor).
            const bool stock_body_only_turn =
                unit.unit_type == "hoverTank" ||
                unit.unit_type == "heavyHoverTank" ||
                (unit.unit_type == "amphibiousJet" && unit.target_uid != 0);
            if (stock_body_only_turn) return;
            for (WeaponSlot& slot : unit.weapon_slots) {
                if (!slot.turret_rotate_with_body) continue;
                const f32 old_turret_angle = slot.turret_angle;
                slot.turret_angle = rotate_weapon_angle_with_body(
                    slot.turret_angle, body_delta);
                if (unit.unit_type == "scout" &&
                    unit.weapon_target_acquired_this_frame &&
                    slot.muzzle_offset != 0.0f) {
                    // The locked scout's projectile is committed after the
                    // body turn in the JAR, so its muzzle uses the new locked
                    // angle even though combat ran before movement here.
                    for (auto& [_, projectile] : projectiles_) {
                        if (projectile.owner_uid != unit.uid ||
                            projectile.age_frames != 0.0f) continue;
                        projectile.x +=
                            jar_cosine_degrees(slot.turret_angle) *
                                slot.muzzle_offset -
                            jar_cosine_degrees(old_turret_angle) *
                                slot.muzzle_offset;
                        projectile.y +=
                            jar_sine_degrees(slot.turret_angle) *
                                slot.muzzle_offset -
                            jar_sine_degrees(old_turret_angle) *
                                slot.muzzle_offset;
                    }
                }
            }
        };

        auto recover_stalled_path = [&](f32 requested_factor) {
            if (order_phase) return;
            const f32 stall_dist_sq = dx * dx + dy * dy;
            const bool consume_chainless_target = !unit.using_path &&
                (formation_cleared_path || unit.movement_navigation_points == 0) &&
                (stall_dist_sq < 256.0f ||
                 collision_covers_point(movement_target_x, movement_target_y));
            const auto archive_chainless_arrival = [&] {
                if (consume_chainless_target) {
                    // y.aJ() still archives W in X when aw is already zero.
                    // Factory launch and direct proposals keep their k/l/m;
                    // this clock handoff also runs when steering requests no
                    // forward speed. It follows the recovery ladder's aH().
                    unit.last_waypoint_stall_frames = unit.waypoint_stall_frames;
                    unit.waypoint_stall_frames = 0.0f;
                }
            };
            if (requested_factor <= 0.0f) {
                archive_chainless_arrival();
                return;
            }
            // y's recovery ladder runs after body steering and before motion.
            // aH() drops av/aw without changing the selected ad context or k,
            // so this frame still uses the old node and factor. The following
            // object turn submits the replacement path.
            if (unit.waypoint_consumed_tick == tick_) {
                // Native steering increments W before aJ() archives it in
                // X, including a passive chase with no order. ax()/ay()
                // subsequently clear both counters on MOVE completion.
                if (!unit.clear_exposed_move_next_frame &&
                    !unit.activate_order_next_frame) {
                    unit.last_waypoint_stall_frames += delta_frames;
                }
            } else {
                if (formation_following && unit.formation_projection_allowed &&
                    !unit.using_path) {
                    unit.last_waypoint_stall_frames = unit.waypoint_stall_frames;
                    unit.waypoint_stall_frames = 0.0f;
                }
                unit.waypoint_stall_frames += delta_frames;
            }
            // The zero-delta pass already applied the ordinary aJ(). Native
            // recovery runs before that pop, against W and the original aw.
            // Replay its earlier skips using the archived W and count the
            // consumed node until all recovery gates have been evaluated.
            const bool recovery_precedes_consumed_node =
                unit.waypoint_consumed_tick == tick_ &&
                unit.movement_navigation_points >= 1 &&
                !formation_cleared_path;
            const f32 recovery_frames = recovery_precedes_consumed_node
                ? unit.last_waypoint_stall_frames : unit.waypoint_stall_frames;
            const auto recovery_path_points = [&] {
                return (unit.using_path ? remaining_navigation_points(unit) : 0) +
                       (recovery_precedes_consumed_node ? 1 : 0);
            };
            auto skip_waypoint = [&] {
                if (!unit.using_path) return;
                unit.last_waypoint_stall_frames = recovery_frames;
                if (unit.current_waypoint <
                    static_cast<i32>(unit.waypoints.size())) {
                    const auto point = unit.waypoints[unit.current_waypoint++];
                    if (unit.path_target_override_active) {
                        // aJ() advances av without rewriting the handler's
                        // independently published k/l/m proposal.
                        unit.path_target_override_x = point.first;
                        unit.path_target_override_y = point.second;
                    } else {
                        unit.target_x = point.first;
                        unit.target_y = point.second;
                    }
                } else if (recovery_precedes_consumed_node) {
                    // Recovery skipped the penultimate node before the
                    // ordinary pop removed the last one. Its captured z3 was
                    // false, so it leaves the order and k/l/m intact.
                    unit.using_path = false;
                    unit.waypoints.clear();
                    unit.current_waypoint = 0;
                    unit.path_target_override_active = false;
                    if (!unit.order_queue.empty()) {
                        const UnitOrder& active = unit.order_queue.front();
                        if (active.type == WeaponType::MOVE ||
                            active.type == WeaponType::ATTACK_MOVE ||
                            active.type == WeaponType::PATROL) {
                            unit.target_x = active.x;
                            unit.target_y = active.y;
                        }
                    }
                }
            };
            if (recovery_frames > 200.0f &&
                stall_dist_sq < 3600.0f &&
                recovery_path_points() >= 2) {
                skip_waypoint();
            }
            const bool clear_path =
                (recovery_frames > 600.0f &&
                 recovery_path_points() >= 2) ||
                (recovery_frames > 80.0f &&
                 unit.wall_bump_frames > 30.0f);
            if (clear_path) {
                unit.using_path = false;
                unit.waypoints.clear();
                unit.current_waypoint = 0;
                unit.path_target_override_active = false;
                unit.waypoint_stall_frames = 0.0f;
                unit.last_waypoint_stall_frames = 0.0f;
                unit.single_node_path_counter = 0;
                unit.path_incomplete = false;
                unit.repath_cooldown_frames = 0.0f;
                // aH() keeps aU alive, but the next controller update has
                // neither af nor a movement proposal from that worker yet.
                // Retain this turn's captured node; coast while it waits.
                unit.path_request_prediction_suppressed = unit.path_request_pending;
                if (!unit.order_queue.empty()) {
                    const UnitOrder& active = unit.order_queue.front();
                    if (active.type == WeaponType::MOVE ||
                        active.type == WeaponType::ATTACK_MOVE ||
                        active.type == WeaponType::PATROL) {
                        unit.target_x = active.x;
                        unit.target_y = active.y;
                        unit.path_retry_next_frame = true;
                    }
                }
            } else if (recovery_frames > 40.0f &&
                       recovery_path_points() >= 2) {
                const std::pair<f32, f32> next = recovery_precedes_consumed_node
                    ? std::pair{unit.path_target_override_active
                                    ? unit.path_target_override_x : unit.target_x,
                                unit.path_target_override_active
                                    ? unit.path_target_override_y : unit.target_y}
                    : unit.waypoints[unit.current_waypoint];
                if (dist2(unit.x, unit.y, next.first, next.second) <
                    stall_dist_sq) {
                    skip_waypoint();
                }
            }
            archive_chainless_arrival();
        };

        // Slot alignment calls y.c(delta, heading), whose two reverse flags
        // are both ci && bb(). It cannot initiate reverse driving, and when
        // bb() is false it preserves ci while turning toward the heading.
        const bool reverse_allowed = formation_aligning_to_heading
            ? unit.reversing && unit.reverse_speed_multiplier > 0.95f
            : jar_reverse_allowed(
                  unit, dx * dx + dy * dy, unit.movement_navigation_points) &&
                (!formation_following || formation_rebuild_reverse_coast ||
                 unit.reversing || unit.reverse_speed_multiplier > 0.95f);
        const bool steering_changes_reverse =
            !formation_aligning_to_heading || reverse_allowed;

        if (unit.strafes) {
            const f32 speed_per_frame = max_speed_per_frame;
            // y.a(delta,ad,au,...) sets ch instead of turning the body when
            // R != null && E() && bj() && !aV(). Combat has already updated
            // cg/ce; even a movement turn that snaps back to the old heading
            // would incorrectly reset its retained angular acceleration.
            const bool combat_owns_body_heading =
                unit.main_target_uid() != 0 && unit.attack_follow_e_flag &&
                unit.move_ignores_body && !unit.attack_range_adds_radii &&
                !formation_aligning_to_heading;
            const f32 raw_error = shortest_angle_delta(
                unit.rotation, desired_rotation);
            const bool movement_reversing =
                reverse_allowed && std::abs(raw_error) > 100.0f;
            // The zero-time order pass only plans movement. y.a(...) is
            // called by the physical helper when it actually steers; a new
            // waypoint's coast frame must retain ci and its idle delays.
            if (!order_phase && !formation_turn_hold &&
                !combat_owns_body_heading && steering_changes_reverse) {
                set_reversing_with_turret_delay(unit, movement_reversing);
            }
            // Keep the reverse target unnormalized. JAR computes the signed
            // error by reducing both operands separately inside f.c(...),
            // whereas normalizing desired+180 first changes a few ULPs at
            // the exact snap-to-heading boundary.
            const f32 movement_rotation =
                desired_rotation + (movement_reversing ? 180.0f : 0.0f);
            const f32 turn_error = combat_owns_body_heading ? 179.0f
                : std::abs(shortest_angle_delta(unit.rotation, movement_rotation));
            if (!formation_turn_hold && !combat_owns_body_heading) {
                turn_body_with_turrets(movement_rotation);
            }
            // y.c(delta, formationHeading) returns the pre-turn heading
            // error. The follower completes MOVE only when that error is
            // already below three degrees; the turn performed this frame is
            // not part of the completion test.
            formation_alignment_completes_order =
                !order_phase && formation_aligning_to_heading &&
                turn_error < 3.0f;
            if (formation_alignment_completes_order) {
                const f32 exposed_target_x = unit.attack_move_combat_target_valid
                    ? unit.attack_move_combat_target_x : unit.order_queue.front().x;
                const f32 exposed_target_y = unit.attack_move_combat_target_valid
                    ? unit.attack_move_combat_target_y : unit.order_queue.front().y;
                complete_current_order_(unit, other_worlds);
                unit.formation_leader_uid = 0;
                if (unit.order_queue.empty()) {
                    // y.ay() removes the final waypoint here but leaves this
                    // frame's selected k/l/m proposal exposed until the next
                    // update, independently of any retained physical node.
                    unit.has_move_target = true;
                    unit.arrived = false;
                    unit.target_x = exposed_target_x;
                    unit.target_y = exposed_target_y;
                    unit.clear_exposed_move_next_frame = true;
                }
            }
            f32 target_factor =
                turn_error <= jar_movement_turn_tolerance(
                    unit, turn_tolerance_distance_sq)
                ? (formation_rebuild_reverse_coast && formation_following
                    ? 1.0f
                    : (movement_reversing && !unit.move_ignores_body
                    ? -unit.reverse_speed_multiplier : 1.0f)
                  )
                : 0.0f;
            // y.a(float,ad,au,boolean) leaves fBc at zero inside the last
            // node's 4-unit radius, and sets it to zero below distance^2 9
            // for every physical target. This includes a captured node from
            // a replacement path adopted while an old chain was active.
            if (physical_target_distance_sq < 9.0f ||
                (unit.movement_navigation_points == 1 &&
                 physical_target_distance_sq < 16.0f)) {
                target_factor = 0.0f;
            }
            if (formation_hold) target_factor = 0.0f;
            if (formation_following && target_factor != 0.0f) {
                const f32 target_distance_sq = dx * dx + dy * dy;
                const bool reverse_target = target_factor < 0.0f;
                if (reverse_target) target_factor = -target_factor;
                if (formation_uses_early_path_target) {
                    if (target_distance_sq < 2500.0f) target_factor -= 0.15f;
                    if (target_distance_sq < 900.0f) target_factor -= 0.15f;
                    if (target_distance_sq < 225.0f) target_factor -= 0.3f;
                } else {
                    if (target_distance_sq > 400.0f) target_factor += 0.2f;
                    if (target_distance_sq < 49.0f) target_factor -= 0.15f;
                    if (target_distance_sq < 9.0f) target_factor -= 0.15f;
                }
                if (target_distance_sq < 9.0f) target_factor = 0.0f;
                if (reverse_target && target_factor != 0.0f) {
                    target_factor = -target_factor;
                }
            }
            if (formation_rebuild_reverse_coast && formation_following &&
                unit.reversing && !unit.move_ignores_body &&
                target_factor != 0.0f) {
                // y.a(float,ad,au,...) applies bc() using ci after the body
                // turn. A new MOVE may have already ended the old reverse.
                target_factor = -target_factor * unit.reverse_speed_multiplier;
            }
            // The JAR gates endpoint slowdown on y.aw==1. A formation-only
            // follower has no av/aw chain even though the exposed movement
            // proposal still looks like one logical destination.
            if (unit.movement_navigation_points == 1 &&
                target_factor != 0.0f) {
                f32 magnitude = std::abs(target_factor);
                if (dx * dx + dy * dy < 324.0f &&
                    unit.move_deceleration < 0.13f && speed_per_frame > 1.0f) {
                    magnitude *= 0.5f;
                }
                if (dx * dx + dy * dy < 169.0f &&
                    unit.move_deceleration < 0.15f && speed_per_frame > 0.9f) {
                    magnitude *= 0.5f;
                }
                if (speed_per_frame > 5.0f && dx * dx + dy * dy < 324.0f) {
                    magnitude = std::min(magnitude, 0.5f);
                }
            if (speed_per_frame > 5.0f && dx * dx + dy * dy < 81.0f) {
                magnitude = std::min(magnitude, 0.25f);
            }
            target_factor = std::copysign(magnitude, target_factor);
            }
            // Native W/recovery gates run before ci applies bc()'s sign.
            recover_stalled_path(std::abs(target_factor));
            if (formation_aligning_to_heading && unit.reversing &&
                !unit.move_ignores_body) {
                target_factor = -0.0f;
            }
            // y.a(float,ad,au,...) assigns cf only in the physical helper.
            // The order pass can expose live combat l/m while av still
            // steers at an old node. Writing its factor here would widen
            // that node's subsequent turn tolerance through the cf>0.4 gate.
            if (!order_phase) unit.move_speed_factor = target_factor;
            const f32 velocity_heading = unit.move_ignores_body
                ? desired_rotation : unit.rotation;
            const f32 target_vel_x =
                target_factor == 0.0f ? 0.0f :
                jar_cosine_degrees(velocity_heading) * speed_per_frame *
                    unit.move_speed_factor;
            const f32 target_vel_y =
                target_factor == 0.0f ? 0.0f :
                jar_sine_degrees(velocity_heading) * speed_per_frame *
                    unit.move_speed_factor;
            // y.a(float) uses C() whenever a strafing factor remains active;
            // D() is selected only when the requested factor is zero. A
            // positive factor can decrease (for example 1.2 -> 1.0) without
            // switching to braking, even though its magnitude dropped.
            // y.a(float) raises ay from the movement state selected before
            // integration. A braking frame can end with zero velocity while
            // still requiring the collision pass to refresh its cache.
            const bool had_strafe_motion =
                unit.move_speed_factor != 0.0f ||
                unit.vel_x != 0.0f || unit.vel_y != 0.0f;
            const f32 velocity_acceleration =
                target_factor == 0.0f &&
                        (unit.vel_x != 0.0f || unit.vel_y != 0.0f)
                    ? unit.move_deceleration : unit.move_acceleration;
            approach_velocity(unit, target_vel_x, target_vel_y,
                              velocity_acceleration, delta_frames);
            if (!order_phase &&
                (had_strafe_motion || unit.vel_x != 0.0f ||
                 unit.vel_y != 0.0f)) {
                unit.collision_moved = true;
            }
            {
                const f32 clamp_old_x = unit.x, clamp_old_y = unit.y;
                unit.x += unit.vel_x * delta_frames;
                unit.y += unit.vel_y * delta_frames;
                const bool collision_displacement = integrate_external_displacement();
                if (had_strafe_motion || collision_displacement) {
                    apply_movement_tile_clamp_(
                        unit, clamp_old_x, clamp_old_y, delta_frames);
                }
            }
        } else {
            const f32 raw_error = shortest_angle_delta(
                unit.rotation, desired_rotation);
            const bool movement_reversing =
                reverse_allowed && std::abs(raw_error) > 100.0f;
            if (!order_phase && !formation_turn_hold && steering_changes_reverse) {
                set_reversing_with_turret_delay(unit, movement_reversing);
            }
            const f32 movement_rotation =
                desired_rotation + (movement_reversing ? 180.0f : 0.0f);
            const f32 turn_error = std::abs(shortest_angle_delta(
                unit.rotation, movement_rotation));
            if (!formation_turn_hold) {
                turn_body_with_turrets(movement_rotation);
            }
            formation_alignment_completes_order =
                !order_phase && formation_aligning_to_heading &&
                turn_error < 3.0f;
            if (formation_alignment_completes_order) {
                const f32 exposed_target_x = unit.attack_move_combat_target_valid
                    ? unit.attack_move_combat_target_x : unit.order_queue.front().x;
                const f32 exposed_target_y = unit.attack_move_combat_target_valid
                    ? unit.attack_move_combat_target_y : unit.order_queue.front().y;
                complete_current_order_(unit, other_worlds);
                unit.formation_leader_uid = 0;
                if (unit.order_queue.empty()) {
                    // See the strafing branch above: preserve the just-
                    // completed combat or order proposal for this frame only.
                    unit.has_move_target = true;
                    unit.arrived = false;
                    unit.target_x = exposed_target_x;
                    unit.target_y = exposed_target_y;
                    unit.clear_exposed_move_next_frame = true;
                }
            }
            f32 target_factor =
                turn_error <= jar_movement_turn_tolerance(
                    unit, turn_tolerance_distance_sq)
                ? (formation_rebuild_reverse_coast && formation_following
                    ? 1.0f
                    : (movement_reversing ? -unit.reverse_speed_multiplier : 1.0f))
                : 0.0f;
            // The generic three-unit gate and the last-node four-unit gate
            // use the captured physical target before aJ() consumes it.
            if (physical_target_distance_sq < 9.0f ||
                (unit.movement_navigation_points == 1 &&
                 physical_target_distance_sq < 16.0f)) {
                target_factor = 0.0f;
            }
            if (formation_hold) target_factor = 0.0f;
            if (formation_following && target_factor != 0.0f) {
                const f32 target_distance_sq = dx * dx + dy * dy;
                const bool reverse_target = target_factor < 0.0f;
                if (reverse_target) target_factor = -target_factor;
                if (formation_uses_early_path_target) {
                    if (target_distance_sq < 2500.0f) target_factor -= 0.15f;
                    if (target_distance_sq < 900.0f) target_factor -= 0.15f;
                    if (target_distance_sq < 225.0f) target_factor -= 0.3f;
                } else {
                    if (target_distance_sq > 400.0f) target_factor += 0.2f;
                    if (target_distance_sq < 49.0f) target_factor -= 0.15f;
                    if (target_distance_sq < 9.0f) target_factor -= 0.15f;
                }
                if (target_distance_sq < 9.0f) target_factor = 0.0f;
                if (reverse_target && target_factor != 0.0f) {
                    target_factor = -target_factor;
                }
            }
            if (formation_rebuild_reverse_coast && formation_following &&
                unit.reversing && !unit.move_ignores_body &&
                target_factor != 0.0f) {
                target_factor = -target_factor * unit.reverse_speed_multiplier;
            }
            if (unit.movement_navigation_points == 1 &&
                target_factor != 0.0f) {
                const f32 speed_per_frame =
                    unit.move_speed_per_frame();
                f32 magnitude = std::abs(target_factor);
                if (dx * dx + dy * dy < 324.0f &&
                    unit.move_deceleration < 0.13f && speed_per_frame > 1.0f) {
                    magnitude *= 0.5f;
                }
                if (dx * dx + dy * dy < 169.0f &&
                    unit.move_deceleration < 0.15f && speed_per_frame > 0.9f) {
                    magnitude *= 0.5f;
                }
                if (speed_per_frame > 5.0f && dx * dx + dy * dy < 324.0f) {
                    magnitude = std::min(magnitude, 0.5f);
                }
                if (speed_per_frame > 5.0f && dx * dx + dy * dy < 81.0f) {
                    magnitude = std::min(magnitude, 0.25f);
                }
                target_factor = std::copysign(magnitude, target_factor);
            }
            recover_stalled_path(std::abs(target_factor));
            const f32 rate = unit.move_speed_factor < target_factor
                ? unit.move_acceleration : unit.move_deceleration;
            if ((movement_reversing ||
                 (formation_aligning_to_heading && unit.reversing)) &&
                target_factor == 0.0f) {
                target_factor = -0.0f;
            }
            // y.a() only calls f.a(cf, proposal, C/D) when cf differs.
            // Equal signed zeros retain their previous bits; braking reverse
            // momentum to zero adopts the negative proposal's sign.
            if (!order_phase &&
                (unit.move_speed_factor < target_factor ||
                 unit.move_speed_factor > target_factor)) {
                unit.move_speed_factor = approach_value(
                    unit.move_speed_factor, target_factor, rate * delta_frames);
            }
            if (!order_phase && unit.move_speed_factor != 0.0f) {
                unit.collision_moved = true;
            }

            const f32 move_amount =
                max_speed_per_frame *
                unit.move_speed_factor * delta_frames;
            if (move_amount != 0.0f) {
                const f32 clamp_old_x = unit.x, clamp_old_y = unit.y;
                unit.x += jar_cosine_degrees(unit.rotation) * move_amount;
                unit.y += jar_sine_degrees(unit.rotation) * move_amount;
                integrate_external_displacement();
                apply_movement_tile_clamp_(
                    unit, clamp_old_x, clamp_old_y, delta_frames);
            } else {
                const f32 clamp_old_x = unit.x, clamp_old_y = unit.y;
                // y.a(float) still commits a pending external displacement
                // when ordinary movement contributes no distance.
                if (integrate_external_displacement()) {
                    apply_movement_tile_clamp_(
                        unit, clamp_old_x, clamp_old_y, delta_frames);
                }
            }
        }

        if (use_consumed_navigation_target) {
            unit.has_deferred_navigation_target = false;
        }
        if (unit.has_move_target &&
            (unit.order_queue.empty() ||
             unit.order_queue.front().type == WeaponType::ATTACK) &&
            unit.direct_attack_chase_this_update && unit.target_uid != 0) {
            World* target_world = order_target_world_(
                unit.target_team_id, other_worlds);
            const Unit* target = target_world
                ? target_world->get_unit(unit.target_uid) : nullptr;
            if (target) {
                if (unit.using_path && !unit.path_target_override_active) {
                    unit.path_target_override_active = true;
                    unit.path_target_override_x = unit.target_x;
                    unit.path_target_override_y = unit.target_y;
                }
                unit.target_x = unit.order_queue.empty()
                    ? target->x : unit.attack_order_target_x;
                unit.target_y = unit.order_queue.empty()
                    ? target->y : unit.attack_order_target_y;
            }
        }
        if (unit.has_move_target &&
            (unit.attack_move_combat_target_valid || unit.target_uid != 0) &&
            !unit.order_queue.empty() &&
            unit.order_queue.front().type == WeaponType::ATTACK_MOVE) {
            // aJ() can advance the physical node after dispatch published
            // the combat proposal, or retained the order point when both
            // weapon arc checks rejected R. Restore l/m after consumption.
            if (unit.using_path && !unit.path_target_override_active) {
                unit.path_target_override_active = true;
                unit.path_target_override_x = unit.target_x;
                unit.path_target_override_y = unit.target_y;
            }
            unit.target_x = unit.attack_move_combat_target_valid
                ? unit.attack_move_combat_target_x : unit.order_queue.front().x;
            unit.target_y = unit.attack_move_combat_target_valid
                ? unit.attack_move_combat_target_y : unit.order_queue.front().y;
        }
        if (!order_phase && unit.has_move_target && unit.using_path &&
            !unit.order_queue.empty() &&
            unit.order_queue.front().type == WeaponType::GUARD &&
            unit.order_queue.front().replay_command_group_size >= 19 &&
            unit.formation_leader_uid != 0 &&
            unit.formation_group_size > 1 &&
            unit.movement_prediction_enabled &&
            remaining_navigation_points(unit) >= 17 &&
            unit.guard_target_uid != 0) {
            World* guarded_world = order_target_world_(
                unit.guard_target_team_id, other_worlds);
            Unit* guarded = guarded_world
                ? guarded_world->get_unit(unit.guard_target_uid) : nullptr;
            if (guarded && guarded->is_alive && guarded->parent_uid == 0 &&
                unit.distance_to(*guarded) > 500.0f) {
                unit.guard_movement_target_valid = true;
                unit.guard_movement_target_uid = unit.guard_target_uid;
                unit.guard_movement_target_team_id =
                    unit.guard_target_team_id;
                unit.guard_movement_target_x = guarded->x;
                unit.guard_movement_target_y = guarded->y;
            }
        }
        if (unit.has_move_target && !unit.activate_order_next_frame &&
            !unit.clear_exposed_move_next_frame && !unit.order_queue.empty() &&
            unit.order_queue.front().type == WeaponType::BUILD) {
            // BUILD's l/m names the placement point throughout travel.
            // Adoption and node consumption only change av/aw. A queued
            // BUILD exposed by this turn's MOVE completion has not run yet.
            if (unit.using_path && !unit.path_target_override_active) {
                unit.path_target_override_active = true;
                unit.path_target_override_x = unit.target_x;
                unit.path_target_override_y = unit.target_y;
            }
            unit.target_x = unit.order_queue.front().x;
            unit.target_y = unit.order_queue.front().y;
        }
        if (unit.has_move_target && !unit.order_queue.empty() &&
            (unit.order_queue.front().type == WeaponType::REPAIR ||
             unit.order_queue.front().type == WeaponType::RECLAIM)) {
            World* service_world = order_target_world_(
                unit.target_team_id, other_worlds);
            Unit* service_target = service_world
                ? service_world->get_unit(unit.target_uid) : nullptr;
            if (service_target && service_target->is_alive) {
                // y.b(l) replaces av/aw without changing the live service
                // proposal in l/m. Keep the adopted or advanced node separate
                // so the next object turn steers at av[0], not the building.
                if (unit.using_path && !unit.path_target_override_active) {
                    unit.path_target_override_active = true;
                    unit.path_target_override_x = unit.target_x;
                    unit.path_target_override_y = unit.target_y;
                }
                unit.target_x = service_target->x;
                unit.target_y = service_target->y;
            }
        }
        grid_.update(unit);
    }
    // The finishing physical pass can update a newly loaded unit's position,
    // but the cargo must remain outside the active spatial grid afterwards.
    if (only_unit_uid != 0) {
        Unit* unit = get_unit(only_unit_uid);
        if (unit && unit->parent_uid != 0) grid_.remove(*unit);
    } else {
        for (auto& [_, unit] : units_) {
            if (unit.parent_uid != 0) grid_.remove(unit);
        }
    }
}

} // namespace rwrl
