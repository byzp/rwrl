#include "world_internal.h"

#include <cstdio>
#include <cstdlib>

namespace rwrl {

void World::update_command_behaviors(f32 delta_frames) {
    const Vec<World*> no_other_worlds;
    update_command_behaviors(delta_frames, no_other_worlds);
}

void World::update_command_behaviors(f32 delta_frames,
                                     const Vec<World*>& other_worlds) {
    Vec<u32> unit_uids;
    unit_uids.reserve(units_.size());
    for (const auto& [uid, _] : units_) unit_uids.push_back(uid);
    for (u32 uid : unit_uids) {
        update_unit_command_behaviors(delta_frames, uid, other_worlds);
    }
}

void World::update_unit_command_behaviors(
        f32 delta_frames, u32 only_unit_uid,
        const Vec<World*>& other_worlds) {
    Unit* unit = get_unit(only_unit_uid);
    if (!unit) return;

    const bool current_waypoint_is_service =
        !unit->activate_order_next_frame &&
        !unit->order_queue.empty() &&
        (unit->order_queue.front().type == WeaponType::REPAIR ||
         unit->order_queue.front().type == WeaponType::RECLAIM);
    const WeaponType current_waypoint_type = current_waypoint_is_service
        ? unit->order_queue.front().type : WeaponType::MOVE;
    const auto complete_finished_service_waypoint = [&]() {
        if (!current_waypoint_is_service || unit->target_uid != 0 ||
            unit->order_queue.empty() ||
            unit->order_queue.front().type != current_waypoint_type) {
            return;
        }
        // A GUARD insertion has not dispatched y.i() yet. Clearing its old
        // dead R must not finish the newly exposed automatic repair order.
        // y.i(...) calls ay() from inside the service branch. The next
        // waypoint is therefore visible in this same object update, and a
        // nano beam that completed its target decays later in the update.
        unit->service_active_this_frame = false;
        complete_current_order_(*unit, other_worlds);
    };

    // Typed GUARD dispatch already chooses R/q(2) before the repair branch.
    // Repeating repair here would steal its accepted combat proposal after
    // the order pass. Keep this compatibility path for direct World guards.
    if (unit->is_active() && unit->guard_target_uid != 0 &&
        unit->order_queue.empty()) {
        World* guarded_world = order_target_world_(
            unit->guard_target_team_id, other_worlds);
        Unit* guarded = guarded_world
            ? guarded_world->get_unit(unit->guard_target_uid) : nullptr;
        if (!guarded || !guarded->is_alive) {
            unit->guard_target_uid = 0;
            unit->guard_target_team_id = -1;
        } else if (guarded_world == this && is_service_unit(*unit) &&
                   (guarded->hp < guarded->max_hp || !guarded->is_complete())) {
            const u32 saved_target_uid = unit->target_uid;
            const WeaponType saved_weapon_type = unit->weapon_type;
            unit->target_uid = guarded->uid;
            unit->weapon_type = WeaponType::REPAIR;
            repair_unit(*unit, *guarded_world, *guarded, delta_frames);
            unit->target_uid = saved_target_uid;
            unit->weapon_type = saved_weapon_type;
        }
    }
    if (!unit->is_active()) return;
    if (unit->target_uid == 0) {
        // The build pass runs earlier in the same object update and can
        // finish a repair that saturated its target's HP. y.i(...) consumes
        // the waypoint inside that same update, so the queue must not carry
        // the finished service order into the next one.
        if (unit->service_completed_in_build_pass) {
            unit->service_completed_in_build_pass = false;
            complete_finished_service_waypoint();
        }
        return;
    }

    World* target_world = order_target_world_(
        unit->target_team_id, other_worlds);
    Unit* target = target_world
        ? target_world->get_unit(unit->target_uid) : nullptr;
    if (!target || !target->is_alive) {
        if (unit->weapon_type == WeaponType::REPAIR ||
            unit->weapon_type == WeaponType::RECLAIM) {
            finish_service_order(*unit);
        } else {
            // y.b(engine,delta) validates R only when ad()/l() permits
            // combat. An ATTACK scheduling tag cannot enable that pass for
            // an unarmed transport; its old dead R reference survives.
            if (!unit->can_attack && has_weapon(unit->weapon_type)) return;
            const bool republish_invalid_attack_move_order_point =
                !unit->order_queue.empty() &&
                unit->order_queue.front().type == WeaponType::ATTACK_MOVE &&
                unit->is_mobile() && !unit->path_request_pending;
            const bool deferred_formation_attack_move_handoff =
                unit->formation_rebuild_handoff_pending &&
                !unit->order_queue.empty() &&
                unit->order_queue.front().type == WeaponType::ATTACK_MOVE &&
                unit->is_mobile() && !unit->using_path &&
                !unit->path_request_pending;
            bool replay_short_attack_move_republish = false;
            bool replay_matching_short_attack_move_republish = false;
            if (!unit->order_queue.empty() &&
                unit->order_queue.front().type == WeaponType::ATTACK_MOVE &&
                (unit->formation_leader_uid != 0 ||
                 unit->formation_is_leader) &&
                unit->formation_group_size > 1 &&
                (unit->formation_is_leader ||
                 !unit->formation_projection_allowed) && unit->is_mobile() &&
                !unit->using_path && !unit->path_request_pending &&
                !unit->has_move_target &&
                unit->order_queue.front().replay_command_group_size < 19) {
                const UnitOrder& active = unit->order_queue.front();
                bool has_short_cache = false;
                bool has_matching_short_cache = false;
                for (const UnitOrderPrepath& cached :
                     active.replay_prepath_cache) {
                    if (cached.path.size() > 2) continue;
                    has_short_cache = true;
                    if (cached.unit_uid != unit->uid ||
                        std::abs(cached.target_x - active.x) > 10.0f ||
                        std::abs(cached.target_y - active.y) > 10.0f ||
                        cached.frame + 180 < tick_ ||
                        cached.move_type != unit->move_type ||
                        dist2(unit->x, unit->y, cached.start_x,
                              cached.start_y) >= 3600.0f) {
                        continue;
                    }
                    has_matching_short_cache = true;
                    break;
                }
                replay_short_attack_move_republish =
                    has_short_cache && !has_matching_short_cache;
                replay_matching_short_attack_move_republish =
                    has_matching_short_cache;
            }
            unit->target_uid = 0;
            unit->target_team_id = -1;
            if (deferred_formation_attack_move_handoff) {
                const UnitOrder& active = unit->order_queue.front();
                if (unit->formation_is_leader) {
                    // ab.c() relinks the survivor without resetting s/u.
                    // Losing its retained R cannot bypass those request gates.
                    if (unit->repath_cooldown_frames == 0.0f ||
                        (unit->path_incomplete &&
                         unit->repath_cooldown_frames < 450.0f)) {
                        move_unit_to_async_(*unit, active.x, active.y);
                        unit->movement_activation_delay_frames = 1.0f;
                        unit->movement_delay_keeps_prediction = false;
                    }
                } else {
                    unit->target_x = active.x;
                    unit->target_y = active.y;
                    unit->has_move_target = true;
                    unit->arrived = false;
                    // The rebuilt follower publishes y.l/y.m on this object
                    // turn, but y.a() still coasts once before prediction is
                    // enabled. Keep the logical proposal without steering it
                    // in the same physical pass.
                    if (!unit->formation_projection_allowed) {
                        unit->movement_activation_delay_frames = 1.0f;
                        unit->movement_delay_keeps_prediction = false;
                    }
                }
                unit->formation_rebuild_handoff_pending = false;
            }
            if (republish_invalid_attack_move_order_point) {
                // MOVE-family dispatch restores the ordered point before
                // y.b() drops the dead R. The generic controller has already
                // applied its s/u request gates in the order pass; clearing
                // R here only republishes l/m and retains that path decision.
                const UnitOrder& active = unit->order_queue.front();
                if (unit->using_path && !unit->path_target_override_active) {
                    // target_x/target_y are av[0]; waypoints[0] is the
                    // following node and must remain queued behind it.
                    unit->path_target_override_x = unit->target_x;
                    unit->path_target_override_y = unit->target_y;
                    unit->path_target_override_active = true;
                }
                unit->target_x = active.x;
                unit->target_y = active.y;
                unit->has_move_target = true;
                unit->arrived = false;
                if (!unit->using_path &&
                    !unit->formation_projection_allowed &&
                    unit->formation_projection_disabled_frames < 120.0f) {
                    // A young projection-disabled follower publishes the
                    // point now but coasts through this physical pass.
                    unit->movement_activation_delay_frames = 1.0f;
                    unit->movement_delay_keeps_prediction = false;
                }
            }
            if ((replay_short_attack_move_republish ||
                 replay_matching_short_attack_move_republish) &&
                !unit->path_request_pending) {
                const UnitOrder& active = unit->order_queue.front();
                move_unit_to_async_(*unit, active.x, active.y);
                if (replay_matching_short_attack_move_republish) {
                    // The serialized short result is already complete in the
                    // JAR order cache, so its nodes are adopted this pass.
                    unit->path_request_delay_frames = 0.0f;
                }
                // A missing short entry stays a normal asynchronous grid
                // request; its pending state remains visible this pass.
                unit->movement_activation_delay_frames = 1.0f;
                unit->movement_delay_keeps_prediction = false;
            }
        }
        complete_finished_service_waypoint();
        return;
    }

    switch (unit->weapon_type) {
    case WeaponType::REPAIR:
        if (can_repair_target(*unit, *target)) {
            repair_unit(*unit, *target_world, *target, delta_frames);
        } else {
            finish_service_order(*unit);
        }
        break;
    case WeaponType::RECLAIM: {
        if (target->uid == unit->uid || !target_world) break;
        const bool resource_target = target->unit_type == "crystalResource";
        if (can_reclaim_target(*unit, *target) &&
            (resource_target || target_world == this ||
             is_allied_with(*target_world))) {
            reclaim_target(*unit, *target_world, *target, delta_frames);
        } else {
            finish_service_order(*unit);
        }
        break;
    }
    case WeaponType::LOAD_INTO:
    case WeaponType::LOAD_UP:
        // Transport waypoints run before the common navigation helper.
        break;
    default:
        break;
    }
    if (current_waypoint_is_service && !unit->order_queue.empty() &&
        unit->order_queue.front().automatic && unit->order_queue.size() > 1) {
        const WeaponType next = unit->order_queue[1].type;
        // y.k() lets an automatic service waypoint run once, then yields
        // to a queued command unless it is the parent GUARD/PATROL order.
        if (next != WeaponType::GUARD && next != WeaponType::PATROL) {
            finish_service_order(*unit);
        }
    }
    complete_finished_service_waypoint();
}

} // namespace rwrl
