#include "world_internal.h"

#include <cstdio>
#include <cstdlib>

namespace rwrl {

f32 World::service_build_speed_(const Unit& source, const Unit& target) const {
    // y.a_(am) scales the target's base D() by V() before applying the
    // builder's b(am) multiplier. Preserve each native float operation.
    f32 speed = registry_.get_build_speed(target.unit_type);
    if (target.tech_level == 2) speed *= 0.5f;
    if (target.tech_level == 3) speed *= 0.25f;
    return speed * source.nano_build_speed;
}

f32 World::service_range_to_(const Unit& source, const Str& target_type) const {
    const f32 range = service_range(source);
    const UnitDef* target = registry_.get(target_type);
    i32 bonus = 0;
    if (target && registry_.is_custom(target_type)) {
        // custom.l.a(am) truncates a positive explicit allowance. Its small
        // builder fallback applies only to buildings on resource pools.
        if (target->extra_build_range_when_building_this > 0.0f) {
            bonus = static_cast<i32>(target->extra_build_range_when_building_this);
        } else if (target->place_only_on_res_pool && target->is_building &&
                   range < 20.0f) {
            bonus = 17;
        }
    } else if (target_type == "seaFactory") {
        bonus = 110;
    } else if (target_type == "extractor") {
        // ar.extractor.a(am) adds 110 over water. The shipped custom
        // extractor instead uses the resource-pool branch above.
        const bool over_water = pathfinder_.has_terrain(
            pathfinder_.tile_x(source.x), pathfinder_.tile_y(source.y)) &&
            (pathfinder_.terrain(pathfinder_.tile_x(source.x),
                                 pathfinder_.tile_y(source.y)) &
             (TERRAIN_WATER | TERRAIN_WATER_BRIDGE)) != 0;
        if (over_water) bonus = 110;
    }
    // h.b.f(as) adds a resource-pool allowance even when as.a(am)==0,
    // including the custom extractor supplied with the game.
    if (bonus == 0 && target && target->place_only_on_res_pool &&
        !source.custom_unit && source.unit_type == "builderShip") {
        bonus = 110;
    }
    return range + static_cast<f32>(bonus);
}

bool World::service_target_in_range_(const Unit& source, const Unit& target) const {
    const f32 range = service_range_to_(source, target.unit_type) +
        (source.service_range_extended ? 5.0f : 0.0f);
    return dist2(source.x, source.y, target.x, target.y) <= range * range;
}

i32 World::service_path_target_size_(const Unit& source, const Str& target_type) const {
    const f32 range = service_range_to_(source, target_type);
    return range > 58.0f
        ? static_cast<i32>((range - 41.0f) / (pathfinder_.tile_size() * 1.414f))
        : 0;
}

void World::repair_unit(Unit& healer, Unit& target, f32 delta_frames) {
    repair_unit(healer, *this, target, delta_frames);
}

bool World::can_repair_target(const Unit& source, const Unit& target) const {
    return source.can_repair && source.uid != target.uid &&
        target.can_be_repaired &&
        (target.is_building ? source.can_repair_buildings : source.can_repair_any);
}

bool World::can_reclaim_target(const Unit& source, const Unit& target) const {
    if (!source.can_reclaim) return false;
    return (target.unit_type == "crystalResource" && source.can_reclaim_resources) ||
        can_repair_target(source, target);
}

void World::begin_service_chase_(Unit& healer, const Unit& target) {
    const i32 target_size = service_path_target_size_(healer, target.unit_type);
    const bool projects = healer.formation_leader_uid != 0 &&
        healer.formation_projection_allowed;
    const bool had_path = healer.using_path;
    // y.i() republishes k/l/m even after aJ() consumes the final node. The
    // common controller still waits for s before requesting a replacement.
    // Keep retained av/aw separate from this live service proposal.
    if (had_path && !healer.path_target_override_active) {
        healer.path_target_override_active = true;
        healer.path_target_override_x = healer.target_x;
        healer.path_target_override_y = healer.target_y;
    }
    healer.has_move_target = true;
    healer.arrived = false;
    healer.target_x = target.x;
    healer.target_y = target.y;
    healer.service_order_target_x = target.x;
    healer.service_order_target_y = target.y;
    healer.service_projection_proposal = projects;
    // am.cl is decremented before y.i(). While it remains positive, the
    // common controller selects l/m directly and bypasses path requests.
    const bool launch_steers_directly = healer.spawn_delay_frames > 0.0f;
    if (!projects && !launch_steers_directly && !healer.path_request_pending) {
        // y.i() sets r=n before the common controller checks o/p. A
        // drifting target refreshes asynchronously while av/aw survives.
        const bool drifted =
            std::abs(healer.last_request_x - target.x) > target_size ||
            std::abs(healer.last_request_y - target.y) > target_size;
        if (drifted && healer.repath_cooldown_frames > 30.0f) {
            healer.repath_cooldown_frames = 30.0f;
        }
        const bool retry_missing_path = !had_path &&
            (healer.repath_cooldown_frames == 0.0f ||
             (healer.path_incomplete && healer.repath_cooldown_frames < 450.0f));
        if (retry_missing_path ||
            (drifted && healer.repath_cooldown_frames == 0.0f)) {
            if (had_path) {
                move_unit_to_(healer, target.x, target.y,
                              target_size, true, true);
                if (!healer.path_request_pending) {
                    // The line/escape branch overwrites av in this turn;
                    // the captured physical node now names the new route.
                    healer.path_target_override_active = false;
                    healer.has_deferred_navigation_target = false;
                }
            } else {
                move_unit_to_async_(healer, target.x, target.y, target_size);
            }
        }
    }
    const bool captured_node = healer.waypoint_consumed_tick == tick_ &&
        healer.has_deferred_navigation_target;
    if (!had_path && !captured_node && !projects && !launch_steers_directly) {
        // The controller captured no af, whether s allowed a new request or
        // is still cooling down. Its physical pass coasts without steering.
        healer.movement_activation_delay_frames = 1.0f;
        healer.movement_delay_keeps_prediction = false;
        healer.movement_delay_uses_acceleration =
            healer.reversing && healer.move_speed_factor < 0.0f;
        healer.movement_prediction_enabled = false;
    }
}

void World::repair_unit(Unit& healer, World& target_world, Unit& target,
                        f32 delta_frames) {
    if (!can_repair_target(healer, target) ||
        !healer.is_active() || !target.is_alive || target.parent_uid != 0 ||
        healer.uid == target.uid) {
        finish_service_order(healer);
        return;
    }
    if (!service_target_in_range_(healer, target)) {
        // y.i() also consumes au.k==0 waypoints when their target leaves
        // nano range, including automatic repair from mobile custom units.
        if (!healer.is_mobile() ||
            (!healer.order_queue.empty() &&
             healer.order_queue.front().service_chase_range_extra == 0.0f)) {
            finish_service_order(healer);
            return;
        }
        if (healer.is_mobile() && !healer.path_request_pending &&
            (!healer.has_move_target ||
             healer.service_projection_proposal)) {
            begin_service_chase_(healer, target);
        }
        return;
    }
    // y.k() drops the active movement request on entering nano range but
    // leaves the remaining path nodes in aw/av for the next update.
    if (healer.has_move_target) {
        healer.has_move_target = false;
        healer.arrived = true;
        healer.movement_prediction_enabled = false;
    }
    if (target.is_complete() && target.hp >= target.max_hp) {
        finish_service_order(healer);
        return;
    }
    if (!service_action_ready(healer, target, delta_frames)) return;

    if (!target.is_complete()) {
        const f32 build_amount = service_build_speed_(healer, target) * delta_frames;
        target.build_progress = std::min(1.0f, target.build_progress + build_amount);
        for (BuildTask& task : target_world.pending_builds_) {
            if (task.target_uid == target.uid) {
                task.progress = std::max(task.progress, target.build_progress);
                break;
            }
        }
        return;
    }

    target.hp += healer.nano_repair_speed * std::max(0.0f, delta_frames);
    if (target.hp > target.max_hp) {
        target.hp = target.max_hp;
        finish_service_order(healer);
    }
}

void World::reclaim_target(Unit& u, Unit& target, f32 delta_frames) {
    reclaim_target(u, *this, target, delta_frames);
}

void World::reclaim_target(Unit& u, World& target_world, Unit& target,
                           f32 delta_frames) {
    if ((!singleplayer_mode_ && target.team_id != u.team_id &&
         target.unit_type != "crystalResource") ||
        !can_reclaim_target(u, target) || !u.is_active() || !target.is_alive ||
        target.parent_uid != 0 ||
        u.uid == target.uid) {
        finish_service_order(u);
        return;
    }
    if (!service_target_in_range_(u, target)) {
        // y.i() rejects an out-of-range service waypoint when aR() is false
        // or au.k is zero, for both repair and reclaim. A stationary repair
        // bay therefore consumes the order instead of retaining it forever.
        if (!u.is_mobile() ||
            (!u.order_queue.empty() &&
             u.order_queue.front().service_chase_range_extra == 0.0f)) {
            finish_service_order(u);
            return;
        }
        // av.d and av.g share y.i(float,au,ad), so a reclaim chase reaches the
        // same request site as a repair chase and returns through the same
        // formation-projection branch before it.
        if (u.is_mobile() && !u.path_request_pending &&
            (!u.has_move_target || u.service_projection_proposal)) {
            begin_service_chase_(u, target);
        }
        return;
    }
    if (u.has_move_target) {
        u.has_move_target = false;
        u.arrived = true;
        u.movement_prediction_enabled = false;
    }

    const f32 frames = std::max(0.0f, delta_frames);
    if (!service_action_ready(u, target, frames)) return;
    const bool resource_target = target.unit_type == "crystalResource";
    // y.i() still aims and warms the nano slot while a young RECLAIM waits
    // on a damaged or barely started upfront-cost target. y.k() charges V
    // before service; this split scheduler charges it in the physical pass.
    if (!resource_target &&
        u.active_order_elapsed_frames + frames <
            EngineConstants::RECLAIM_STARTUP_FRAMES &&
        (target.build_progress < 0.5f || target.hp / target.max_hp < 0.5f)) {
        return;
    }
    if (!target.is_complete()) {
        target.build_progress = std::max(
            0.0f,
            target.build_progress - u.nano_unbuild_speed * frames);
        target.build_progress_secondary = target.build_progress;
        for (BuildTask& task : target_world.pending_builds_) {
            if (task.target_uid == target.uid) {
                task.progress = std::min(task.progress, target.build_progress);
                break;
            }
        }
        if (target.build_progress > 0.0f) return;
    } else {
        f32 reclaim_amount =
            (resource_target ? 0.02f * u.resource_reclaim_multiplier
                             : u.nano_reclaim_speed) * frames;
        if (reclaim_amount >= target.hp) {
            reclaim_amount = target.hp;
            target.hp = -1.0f;
        } else {
            target.hp -= reclaim_amount;
        }
        if (resource_target && target.max_hp > 0.0f) {
            u.resource_reclaim_credit_fraction +=
                reclaim_amount / target.max_hp * static_cast<f32>(target.cost);
            // JAR uses a strict >1 boundary, retaining exactly one credit in
            // y.ab until the following reclaim pulse.
            if (u.resource_reclaim_credit_fraction > 1.0f) {
                const i32 payout =
                    static_cast<i32>(u.resource_reclaim_credit_fraction);
                credits_ += static_cast<f32>(payout);
                u.resource_reclaim_credit_fraction -= static_cast<f32>(payout);
            }
        }
        if (target.hp > 0.0f) return;
    }

    const u32 target_uid = target.uid;
    if (target.unit_type != "crystalResource") {
        const f32 reclaim_cost = target.reclaim_cost > 0.0f
            ? target.reclaim_cost : static_cast<f32>(target.cost);
        refund_credits(reclaim_cost, EngineConstants::RECLAIM_REFUND);
    }
    const i32 target_team_id = target_world.team_id_;
    // am.bu() clamps only positive cu to zero. An unfinished target still
    // has positive HP; the completed-target branch above already wrote -1.
    const f32 death_hp = target.hp > 0.0f ? 0.0f : target.hp;
    target_world.remove_unit(target.uid);
    if (Unit* reclaimed = target_world.get_unit(target_uid)) {
        reclaimed->hp = death_hp;
    }
    target_world.losses_++;
    // ci()/bv() invalidates general references, but the active RECLAIM au is
    // not consumed from inside this service branch. Keep its target pointer
    // equivalent alive until the next object update rejects the dead target
    // and calls ay(), preserving the current waypoint and av/aw chain for one
    // visible frame.
    u.target_uid = target_uid;
    u.target_team_id = target_team_id;
}

} // namespace rwrl
