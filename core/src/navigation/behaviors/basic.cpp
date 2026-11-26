#include "world_internal.h"
#include "combat/targeting_helpers.h"

#include <cstdio>
#include <cstdlib>

namespace rwrl {
void World::attack_move_unit_to(Unit& u, f32 x, f32 y) {
    move_unit_to(u, x, y);
    u.weapon_type = WeaponType::ATTACK_MOVE;
}

void World::patrol_unit_to(Unit& u, f32 x, f32 y) {
    u.has_patrol = true;
    u.patrol_points.push_back({x, y});
    if (u.patrol_points.size() == 1) {
        u.patrol_points.push_back({u.x, u.y});
    }
    u.patrol_index = 0;
}

void World::guard_unit(Unit& u, Unit& target) {
    guard_unit(u, *this, target);
}

void World::guard_unit(Unit& u, World& target_world, Unit& target) {
    if (!u.is_active() || !target.is_alive) {
        return;
    }
    // The GUARD waypoint and y.R's passive/main combat target are
    // independent. Activating a replacement guard keeps the existing enemy
    // target until the normal combat scan rejects or replaces it.
    const i32 saved_main_target_team_id = u.target_team_id;
    clear_navigation_orders(u);
    if (u.target_uid != 0) {
        u.target_team_id = saved_main_target_team_id;
    }
    u.guard_target_uid = target.uid;
    u.guard_target_team_id = target_world.team_id_;
    if (u.target_uid != 0) {
        // y.c(GUARD) lets the retained main target replace the guarded-unit
        // follow proposal before issuing a movement request. The order phase
        // below will validate that target and request the final route once.
        u.has_move_target = false;
        u.arrived = true;
        return;
    }
    if (u.path_request_pending || command_path_request_active_ || step_in_progress_) {
        // e.k()/y.a(au) only install the waypoint. c(GUARD) resolves the
        // follow or recent-attacker proposal on the object's turn before
        // the common controller captures aE() and submits a request. In
        // particular, an older aU must survive a replacement GUARD.
        u.has_move_target = false;
        u.arrived = true;
        return;
    }
    // y.c(GUARD) uses the previous object's cK state: a unit that had an
    // active movement request keeps the tighter +80 allowance for this
    // activation, while an idle unit uses +100.
    const f32 follow_range = u.radius + target.radius +
        (u.movement_prediction_enabled ? 80.0f : 100.0f);
    if (u.distance_to(target) > follow_range) {
        // GUARD follows a live unit through y's delayed grid request. q(am)
        // supplies a target-size allowance of 2 for a mobile guarded unit.
        move_unit_to_async_(u, target.x, target.y, 2);
        if (u.move_type == MoveType::AIR) {
            // GUARD's live-target proposal installs the air destination as a
            // one-node av/aw chain immediately. Physical steering can still
            // coast for the command frame via the activation delay below.
            u.using_path = true;
            u.waypoints.clear();
            u.current_waypoint = 0;
            u.path_request_target_size = 2;
            u.repath_cooldown_frames = 500.0f;
        }
    } else {
        u.has_move_target = false;
        u.arrived = true;
    }
}

void World::guard_unit_at(Unit& u, f32 x, f32 y) {
    const u32 main_target_uid = has_weapon(u.weapon_type) ? u.target_uid : 0;
    const i32 main_target_team = u.target_team_id;
    clear_navigation_orders(u);
    u.target_uid = main_target_uid;
    u.target_team_id = main_target_uid != 0 ? main_target_team : -1;
    u.has_guard_pos = true;
    u.guard_x = x;
    u.guard_y = y;
    move_unit_to_async_(u, x, y, 2);
}

void World::follow_unit(Unit& u, Unit& target) {
    follow_unit(u, *this, target);
}

void World::follow_unit(Unit& u, World& target_world, Unit& target) {
    if (!u.is_active() || !target.is_alive ||
        (&target_world == this && u.uid == target.uid)) {
        return;
    }
    const u32 main_target_uid = has_weapon(u.weapon_type) ? u.target_uid : 0;
    const i32 main_target_team = u.target_team_id;
    clear_navigation_orders(u);
    u.target_uid = main_target_uid;
    u.target_team_id = main_target_uid != 0 ? main_target_team : -1;
    u.follow_target_uid = target.uid;
    u.follow_target_team_id = target_world.team_id_;
    if (u.path_request_pending || command_path_request_active_ || step_in_progress_) {
        // e.k() installs FOLLOW before c() publishes its live target. The
        // common controller must adopt an older aU before requesting a new
        // route, just as it does for a replacement GUARD.
        u.has_move_target = false;
        u.arrived = true;
        return;
    }
    const f32 follow_range = u.radius + target.radius + 50.0f;
    if (u.distance_to(target) > follow_range) {
        move_unit_to_async_(u, target.x, target.y, 2);
    } else {
        u.has_move_target = false;
        u.arrived = true;
    }
}

void World::attack_unit(Unit& u, Unit& target) {
    if (!u.is_active() || !target.is_alive || target.parent_uid != 0) return;
    clear_navigation_orders(u);
    u.target_uid = target.uid;
    u.target_team_id = target.team_id;
    u.attack_order_target_x = target.x;
    u.attack_order_target_y = target.y;
    u.weapon_type = WeaponType::ATTACK;
    const f32 follow_range = attack_follow_stop_range(u, target);
    // No mobility test here either: the JAR asks for a path from whatever
    // unit received the ATTACK command. A building simply gets the
    // standing-on-blocked-tile answer and never moves along it.
    if (u.bomber_attack_movement || u.distance_to(target) >= follow_range ||
        attack_target_needs_approach(*this, u, target)) {
        if (u.path_request_pending) {
            // Replacing the waypoint clears av/aw through aH(), but an
            // outstanding aU still reaches b(l). Publish the new attack
            // point without replacing or cancelling that older request.
            u.target_x = target.x;
            u.target_y = target.y;
            u.has_move_target = true;
            u.arrived = false;
            return;
        }
        if (u.spawn_delay_frames > 0.0f) {
            // Factory products expose the ATTACK target while am.cl is still
            // positive, but y.a() does not create the chase path until the
            // launch delay has elapsed.
            u.target_x = target.x;
            u.target_y = target.y;
            u.has_move_target = true;
            u.arrived = false;
            u.using_path = false;
            u.waypoints.clear();
            u.current_waypoint = 0;
            return;
        }
        u.repath_cooldown_frames = 500.0f;
        move_unit_to_async_(
            u, target.x, target.y, attack_path_target_size(u, &target));
    } else {
        u.has_move_target = false;
        u.arrived = true;
    }
}

void World::touch_target(Unit& u, Unit& target) {
    if (!u.is_active() || !target.is_alive || target.parent_uid != 0) return;
    clear_navigation_orders(u);
    u.target_uid = target.uid;
    u.target_team_id = target.team_id;
    u.weapon_type = WeaponType::TOUCH_TARGET;
    if (u.is_mobile()) {
        move_unit_to(u, target.x, target.y);
    }
}

} // namespace rwrl
