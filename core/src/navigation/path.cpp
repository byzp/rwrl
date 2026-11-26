#include "../world_internal.h"

#include <cstdio>
#include <cstdlib>

namespace rwrl {
static bool jar_segments_intersect(f32 ax, f32 ay, f32 bx, f32 by,
                                   f32 cx, f32 cy, f32 dx, f32 dy) {
    const f32 denominator = (dy - cy) * (bx - ax) - (dx - cx) * (by - ay);
    if (denominator == 0.0f) return false;
    const f32 u_numerator = (dx - cx) * (ay - cy) - (dy - cy) * (ax - cx);
    const f32 t_numerator = (bx - ax) * (ay - cy) - (by - ay) * (ax - cx);
    const f32 u_parameter = u_numerator / denominator;
    const f32 t_parameter = t_numerator / denominator;
    return u_parameter >= 0.0f && u_parameter <= 1.0f &&
           t_parameter >= 0.0f && t_parameter <= 1.0f;
}

void World::apply_movement_tile_clamp_(Unit& u, f32 old_x, f32 old_y,
                                       f32 delta_frames) {
    // game.units.y.a(f2, l, x, y): the frame's displacement clamps against
    // the blocked-tile layer. Works in tile-unit coordinates like the JAR.
    // k.l.a() rejects off-map tiles before its AIR terrain exception, so
    // aircraft use the same edge slide and collision history as ground units.
    const bool clamped_move_type =
        u.move_type == MoveType::AIR ||
        u.move_type == MoveType::LAND || u.move_type == MoveType::WATER ||
        u.move_type == MoveType::HOVER || u.move_type == MoveType::OVER_CLIFF ||
        u.move_type == MoveType::OVER_CLIFF_WATER;
    // y.a(float) checked bT() before k(float). Loading during that order
    // turn still finishes its displacement and tile clamp; earlier cargo
    // attachments are rejected by update_unit_movement's entry gate.
    if (!clamped_move_type) return;

    const f32 inverse_tile = 1.0f / pathfinder_.tile_size();
    const f32 from_x = old_x * inverse_tile;
    const f32 from_y = old_y * inverse_tile;
    const f32 to_x = u.x * inverse_tile;
    const f32 to_y = u.y * inverse_tile;
    const i32 old_tx = static_cast<i32>(std::floor(from_x));
    const i32 old_ty = static_cast<i32>(std::floor(from_y));
    const i32 new_tx = static_cast<i32>(std::floor(to_x));
    const i32 new_ty = static_cast<i32>(std::floor(to_y));

    auto blocked = [&](i32 tx, i32 ty) {
        return !pathfinder_.is_passable_tile(tx, ty, u.move_type);
    };
    // aq.a(ao, ..., tileX, tileY, false): slide the segment against one
    // edge of the blocked tile, offset 0.01 tile units outside it. The
    // horizontal result overrides the vertical one like the JAR.
    auto edge_slide = [&](i32 tx, i32 ty, f32& out_x, f32& out_y) {
        i32 edge = -1;
        const f32 left = static_cast<f32>(tx);
        const f32 top = static_cast<f32>(ty);
        const f32 right = left + 1.0f;
        const f32 bottom = top + 1.0f;
        if (from_y < to_y) {
            if (!blocked(tx, ty - 1) &&
                jar_segments_intersect(from_x, from_y, to_x, to_y,
                                       left, top, right, top)) {
                edge = 3;
            }
        } else {
            if (!blocked(tx, ty + 1) &&
                jar_segments_intersect(from_x, from_y, to_x, to_y,
                                       left, bottom, right, bottom)) {
                edge = 1;
            }
        }
        if (from_x < to_x) {
            if (!blocked(tx - 1, ty) &&
                jar_segments_intersect(from_x, from_y, to_x, to_y,
                                       left, top, left, bottom)) {
                edge = 2;
            }
        } else {
            if (!blocked(tx + 1, ty) &&
                jar_segments_intersect(from_x, from_y, to_x, to_y,
                                       right, top, right, bottom)) {
                edge = 0;
            }
        }
        if (edge == -1) return false;
        out_x = to_x;
        out_y = to_y;
        if (edge == 0) out_x = right + 0.01f;
        if (edge == 2) out_x = left - 0.01f;
        if (edge == 1) out_y = bottom + 0.01f;
        if (edge == 3) out_y = top - 0.01f;
        return true;
    };

    bool have_clamp = false;
    bool fully_stuck = false;
    f32 clamp_x = to_x, clamp_y = to_y;
    if ((old_tx != new_tx || old_ty != new_ty) &&
        u.spawn_delay_frames == 0.0f && blocked(new_tx, new_ty)) {
        if (old_tx != new_tx && old_ty != new_ty) {
            const bool corner_a = blocked(old_tx, new_ty);
            const bool corner_b = blocked(new_tx, old_ty);
            if (corner_a && corner_b) {
                fully_stuck = true;
                have_clamp = true;
            }
            if (!have_clamp && corner_a) {
                have_clamp = edge_slide(old_tx, new_ty, clamp_x, clamp_y);
            }
            if (!have_clamp && corner_b) {
                have_clamp = edge_slide(new_tx, old_ty, clamp_x, clamp_y);
            }
        }
        if (!have_clamp && !fully_stuck) {
            have_clamp = edge_slide(new_tx, new_ty, clamp_x, clamp_y);
        }
        if (!have_clamp) {
            fully_stuck = true;
            have_clamp = true;
        }
    }

    bool clamped = false;
    if (have_clamp) {
        // Standing on a blocked tile with a terrain-open destination moves
        // through raw (the building-escape exception).
        if (blocked(old_tx, old_ty) &&
            pathfinder_.is_passable_ignoring_buildings(
                new_tx, new_ty, u.move_type)) {
            // keep the raw position
        } else if (fully_stuck) {
            u.x = old_x;
            u.y = old_y;
            clamped = true;
        } else {
            u.x = clamp_x * pathfinder_.tile_size();
            u.y = clamp_y * pathfinder_.tile_size();
            clamped = true;
        }
    }
    if (clamped) {
        u.wall_bump_frames += delta_frames;
        u.wall_clear_streak = 0;
    } else if (u.wall_bump_frames != 0.0f && delta_frames > 0.0f) {
        if (++u.wall_clear_streak >= 3) {
            u.wall_bump_frames = 0.0f;
        }
    }
}

void World::consume_solver_path_(Unit& u,
                                 const Vec<std::pair<f32, f32>>& path) {
    u.attack_final_node_coast_pending = false;
    u.attack_move_final_node_coast_pending = false;
    // y.b(l) clears u before copying every completed request, including
    // empty and short results that replace an earlier truncated chain.
    u.path_incomplete = false;
    if (path.empty()) {
        u.path_target_override_active = false;
        u.path_request_prediction_suppressed = false;
        u.using_path = false;
        u.has_move_target = false;
        u.arrived = true;
        u.vel_x = 0.0f;
        u.vel_y = 0.0f;
        return;
    }
    const auto point_at = [&](i32 index) -> std::pair<f32, f32> {
        if (static_cast<std::size_t>(index) + 1 != path.size()) return path[index];
        const i32 tx = pathfinder_.tile_x(path[index].first);
        const i32 ty = pathfinder_.tile_y(path[index].second);
        // y.b(l) first copies tile centers, then replaces the last point
        // only when the raw request still names that tile and the list was
        // not truncated. A clamped grid goal cannot authorize this rewrite.
        if (!u.path_incomplete &&
            pathfinder_.unclamped_tile_x(u.last_request_x) == tx &&
            pathfinder_.unclamped_tile_y(u.last_request_y) == ty) {
            return {u.last_request_x, u.last_request_y};
        }
        return {pathfinder_.world_x(tx), pathfinder_.world_y(ty)};
    };
    // find_path returns the JAR list shape: the start tile is already
    // removed, so path[0] is the first travel target.
    if (path.size() > 1) {
        u.path_target_override_active = false;
        const bool restore_prediction =
            u.path_request_prediction_suppressed;
        u.path_request_prediction_suppressed = false;
        u.waypoints.clear();
        u.current_waypoint = 0;
        u.using_path = true;
        // y.b(l) increments q only for a single-node result. Larger
        // results preserve it; successful intermediate movement must not
        // erase the BUILD dispatcher's accumulated failed-path count.
        // y.b(l): at most 120 navigation points are copied; longer paths
        // truncate with the incomplete flag so the order re-requests from
        // the truncation point.
        i32 consumed = static_cast<i32>(path.size());
        if (consumed >= 120) {
            consumed = 120;
            u.path_incomplete = true;
        }
        for (i32 i = 1; i < consumed; ++i) {
            u.waypoints.push_back(point_at(i));
        }
        const auto first = point_at(0);
        u.target_x = first.first;
        u.target_y = first.second;
        u.has_move_target = true;
        u.arrived = false;
        if (restore_prediction) u.movement_prediction_enabled = true;
        return;
    }
    // y.b(l) keeps even a one-node solver result as an active av/aw chain;
    // the node is still exposed as the current target. The line shortcut is
    // separate and explicitly clears using_path when it has only its final
    // point.
    // y.b(l) aw==1 increments q. BUILD tests the count during the next
    // waypoint dispatch, before this turn's result can be adopted.
    u.single_node_path_counter++;
    u.path_target_override_active = false;
    const bool restore_prediction = u.path_request_prediction_suppressed;
    u.path_request_prediction_suppressed = false;
    u.using_path = true;
    // b(l) replaces aw, including a one-node result adopted over an older
    // multi-node path. No unconsumed nodes from that old chain survive.
    u.waypoints.clear();
    u.current_waypoint = 0;
    const auto first = point_at(0);
    u.target_x = first.first;
    u.target_y = first.second;
    u.has_move_target = true;
    u.arrived = false;
    if (restore_prediction) u.movement_prediction_enabled = true;
}

void World::move_unit_to(Unit& u, f32 x, f32 y) {
    move_unit_to(u, x, y, 0);
}

void World::move_unit_to_async_(Unit& u, f32 x, f32 y, i32 target_size) {
    move_unit_to_(u, x, y, target_size, true);
}

i32 World::attack_path_target_size(const Unit& attacker,
                                   const Unit* target) const {
    // y.q(am): custom.j.aU() uses custom.l.dJ, the minimum configured
    // range among shooting turrets. It is independent of this target's
    // layer, including when another turret can shoot it from farther away.
    f32 effective_range = attacker.weapon_range;
    if (attacker.custom_unit) {
        for (const WeaponSlot& slot : attacker.weapon_slots) {
            if (slot.fire_enabled && slot.limiting_range < 99999.0f) {
                effective_range = std::min(effective_range, slot.limiting_range);
            }
        }
    }
    // p(target) adds both radii only for units whose aV() returns true.
    if (attacker.attack_range_adds_radii && target) {
        effective_range += attacker.radius + target->radius;
    }
    if (effective_range <= 58.0f) return 0;
    return static_cast<i32>(
        (effective_range - 41.0f) /
        (pathfinder_.tile_size() * 1.414f));
}

void World::move_unit_to(Unit& u, f32 x, f32 y, i32 target_size) {
    move_unit_to_(u, x, y, target_size, false);
}

void World::move_unit_to_(Unit& u, f32 x, f32 y, i32 target_size,
                          bool defer_grid_path, bool preserve_active_path) {
    // y.a(float) checks bT() before k(float). A passenger attached during
    // that order turn can still submit the ensuing passive combat request.
    if (!u.is_active_for_update(tick_)) return;
    u.attack_final_node_coast_pending = false;
    u.attack_move_final_node_coast_pending = false;
    // y.a(FFIZZ) has no mobility test. An immobile unit with an explicit
    // waypoint still records y.o/y.p and gets a path. This also applies to
    // custom units with movementType=NONE that are not marked as buildings.
    const bool force_grid_path = u.force_grid_path_request;
    // Any genuine request replaces the projection-only service proposal.
    u.service_projection_proposal = false;
    u.force_grid_path_request = false;
    // With aw==0 there is no physical node to preserve. A stale logical
    // override must not become the head of a newly installed route.
    if (!preserve_active_path || !u.using_path) {
        u.path_target_override_active = false;
    }
    const bool uses_ground_path =
        u.move_type == MoveType::LAND || u.move_type == MoveType::WATER ||
        u.move_type == MoveType::HOVER || u.move_type == MoveType::OVER_CLIFF ||
        u.move_type == MoveType::OVER_CLIFF_WATER;

    // When ad.d still permits formation projection, y.a(engine,...) calls the
    // formation helper and skips its entire private-path branch. A linked
    // ATTACK follower builds its own path only after the order handler clears
    // that gate because either member is close to the explicit target (or the
    // leader already shares it as the main target).
    const bool has_attack_waypoint = !u.order_queue.empty() &&
        u.order_queue.front().type == WeaponType::ATTACK;
    const Unit* attack_leader = has_attack_waypoint && u.formation_leader_uid != 0
        ? formation_unit_(u.formation_leader_uid) : nullptr;
    bool attack_leader_can_project = attack_leader && attack_leader->is_active();
    if (attack_leader_can_project && !attack_leader->order_queue.empty() &&
        !u.order_queue.empty()) {
        const UnitOrder& order = u.order_queue.front();
        const UnitOrder& lead = attack_leader->order_queue.front();
        // The common native controller drops an inactive ad and applies
        // au.b() before it chooses projection or a private request. Eager
        // ATTACK activation must observe those same gates while leaving
        // the link and y.ai for the order handler to update normally.
        attack_leader_can_project = lead.type == order.type &&
            lead.unit_type == order.unit_type &&
            lead.build_tech_level == order.build_tech_level &&
            lead.target_uid == order.target_uid &&
            std::abs(lead.x - order.x) <= 1.0f &&
            std::abs(lead.y - order.y) <= 1.0f;
    }
    const bool linked_attack_follower =
        !formation_fallback_path_request_active_ &&
        !u.using_path && u.formation_leader_uid != 0 &&
        attack_leader_can_project &&
        has_attack_waypoint;
    const bool linked_build_command_follower =
        command_path_request_active_ && !u.using_path &&
        u.formation_leader_uid != 0 && !u.order_queue.empty() &&
        u.order_queue.front().type == WeaponType::BUILD;
    const bool attack_projection_still_active =
        u.formation_projection_allowed ||
        (u.formation_projection_disabled_frames < 120.0f &&
         !(preserve_active_path && u.direct_attack_chase_this_update));
    if (linked_build_command_follower ||
        (linked_attack_follower && attack_projection_still_active)) {
        // az()/aH() has cleared the old node. The next ATTACK object turn
        // resolves ad.d and ai before the private-path controller runs;
        // retaining R does not bypass that formation handoff. A later
        // passive b() proposal reaches this controller with j=true after
        // the waypoint stopped refreshing s=0.1; ai alone cannot hold it.
        u.path_request_pending = false;
        u.path_request_solver_ready = false;
        u.path_request_needs_worker_handoff = false;
        u.path_request_uses_prepath = false;
        u.path_request_prepath.clear();
        u.path_request_prepath_from_replay = false;
        u.path_request_preserves_active_path = false;
        u.path_request_cost_snapshot.reset();
        u.path_request_delay_frames = 0.0f;
        u.path_request_target_size = target_size;
        u.using_path = false;
        u.waypoints.clear();
        u.current_waypoint = 0;
        u.target_x = x;
        u.target_y = y;
        u.has_move_target = true;
        u.arrived = false;
        u.path_incomplete = false;
        u.has_deferred_navigation_target = false;
        u.repath_cooldown_frames = 0.0f;
        u.movement_activation_delay_frames = 0.0f;
        u.movement_delay_keeps_prediction = false;
        return;
    }

    u.path_request_pending = false;
    u.path_request_solver_ready = false;
    u.path_request_needs_worker_handoff = false;
    u.path_request_uses_prepath = false;
    u.path_request_prepath.clear();
    u.path_request_prepath_from_replay = false;
    u.path_request_preserves_active_path = false;
    u.path_request_cost_snapshot.reset();
    u.path_request_delay_frames = 0.0f;
    // y.a(f,f,i,z,z) records the raw request point in y.o/y.p before any
    // branch; the runtime refresh trigger measures drift against it.
    if (x != u.last_request_x || y != u.last_request_y) {
        u.single_node_path_counter = 0;
    }
    u.last_request_x = x;
    u.last_request_y = y;
    // preserve_active_path requests enter the ground branch even with an
    // adopted path: JAR's grid branch submits to k.l without touching
    // au/aw, so movement continues on the old path until adoption.
    if ((preserve_active_path || !u.using_path) &&
        (uses_ground_path || u.is_building || !u.can_move)) {
        // y: issuing a movement request charges the repath cooldown.
        u.repath_cooldown_frames = 500.0f;
        // y.a() marks the ordering unit as moving (am.cK = true) before any
        // request, so it never contributes to the object layer refreshed by
        // its own request.
        u.movement_prediction_enabled = true;
        // game.units.y.a(): clamp the request to the map like b.b.a(float).
        const f32 map_max_x =
            static_cast<f32>(pathfinder_.width()) * pathfinder_.tile_size();
        const f32 map_max_y =
            static_cast<f32>(pathfinder_.height()) * pathfinder_.tile_size();
        const f32 goal_x = std::min(std::max(x, 0.0f), map_max_x);
        const f32 goal_y = std::min(std::max(y, 0.0f), map_max_y);

        // y.a(): a unit standing on a tile blocked only by the building
        // layer walks straight toward the target, capped at 60 units. The
        // capped case flags the path as truncated so the order re-requests
        // once the escape point is consumed. Escape orders skip the line
        // check and therefore never touch the object-cost refresh gate.
        if (u.is_building || !u.can_move ||
            pathfinder_.is_building_blocked_tile(
                pathfinder_.tile_x(u.x), pathfinder_.tile_y(u.y),
                u.move_type)) {
            u.path_incomplete = false;
            const f32 dx = goal_x - u.x;
            const f32 dy = goal_y - u.y;
            const f32 d = std::sqrt(dx * dx + dy * dy);
            // y.a() routes the offset through the JAR trig tables even when
            // the distance is under the cap.
            const f32 angle = jar_direction_degrees(u.x, u.y, goal_x, goal_y);
            f32 reach = d;
            if (d > 60.0f) {
                reach = 60.0f;
                u.path_incomplete = true;
                // y.a(FFIZZ) caps the cooldown it was just charged with, so
                // the truncated escape re-requests 10 frames later rather
                // than after the full 500.
                if (u.repath_cooldown_frames > 10.0f) {
                    u.repath_cooldown_frames = 10.0f;
                }
            }
            // y.a() stores the escape target in av[0] with aw=1 even though
            // there are no queued nodes after it.
            // This branch rewrites av[0] immediately, including a refresh
            // that would retain the old node while waiting for a grid path.
            u.path_target_override_active = false;
            u.using_path = true;
            u.waypoints.clear();
            u.current_waypoint = 0;
            u.target_x = u.x + jar_cosine_degrees(angle) * reach;
            u.target_y = u.y + jar_sine_degrees(angle) * reach;
            u.has_move_target = true;
            u.arrived = false;
            return;
        }

        // gameFramework.k.l.a(i, true): line/grid requests refresh the
        // stationary object cost layer of the REQUESTING move type only
        // when that layer's stamp is over 30 frames old. Between
        // refreshes, paths see the stale layer.
        const i32 cost_layer = Pathfinder::ground_layer_index(u.move_type);
        if (cost_layer >= 0) {
            // The native object loop reaches units in creation order. A
            // runtime request sees live cK values for objects already visited,
            // while future objects still expose their frame-start values.
            // Replay command requests use their serialized-group snapshot.
            Map<u32, bool> runtime_prediction_snapshot;
            if (!handoff_path_request_active_ &&
                !command_path_request_active_) {
                runtime_prediction_snapshot[u.uid] = true;
                const auto capture_future_predictions =
                    [&](const Map<u32, Unit>& source) {
                        for (const auto& [source_uid, source_unit] : source) {
                            if (source_uid == u.uid ||
                                source_unit.object_creation_order <
                                    u.object_creation_order) {
                                continue;
                            }
                            const Map<u32, bool>& frame_start_predictions =
                                handoff_command_snapshot_tick_ == tick_
                                    ? handoff_command_prediction_snapshot_
                                    : step_start_prediction_snapshot_;
                            const auto snapshot =
                                frame_start_predictions.find(source_uid);
                            runtime_prediction_snapshot[source_uid] =
                                snapshot == frame_start_predictions.end()
                                    ? source_unit.movement_prediction_enabled
                                    : snapshot->second;
                        }
                    };
                if (object_cost_sources_.empty()) {
                    capture_future_predictions(units_);
                } else {
                    for (const Map<u32, Unit>* source : object_cost_sources_) {
                        if (source) capture_future_predictions(*source);
                    }
                }
            }
            const Map<u32, bool>* runtime_prediction_snapshot_ptr =
                runtime_prediction_snapshot.empty()
                    ? nullptr : &runtime_prediction_snapshot;
            // Native replay executes a recorded command during the frame
            // that will become visible next. The lockstep driver submits the
            // same group immediately before Engine::tick(), while tick_ still
            // names the preceding visible state.
            const i32 path_frame = tick_ +
                (command_path_request_active_ ? 1 : 0);
            i32& refresh_frame = global_object_cost_refresh_frames_
                ? global_object_cost_refresh_frames_[cost_layer]
                : object_cost_refresh_frame_[cost_layer];
            if (handoff_path_request_active_) {
                refresh_frame = path_frame;
                const Map<u32, bool>* prediction_snapshot =
                    handoff_command_snapshot_tick_ == tick_
                        ? &handoff_command_prediction_snapshot_
                        : &step_start_prediction_snapshot_;
                if (object_cost_sources_.empty()) {
                    pathfinder_.rebuild_object_costs(
                        u.move_type, {&units_}, prediction_snapshot);
                } else {
                    pathfinder_.rebuild_object_costs(
                        u.move_type, object_cost_sources_, prediction_snapshot);
                }
                if (global_object_cost_layers_) {
                    global_object_cost_layers_[cost_layer] =
                        pathfinder_.object_cost_layer(u.move_type);
                }
                object_cost_applied_frame_[cost_layer] = refresh_frame;
            } else if (refresh_frame + 30 < path_frame) {
                const i32 old_refresh_frame = refresh_frame;
                refresh_frame = path_frame;
                const Map<u32, bool>* prediction_snapshot = nullptr;
                if (handoff_path_request_active_) {
                    prediction_snapshot =
                        handoff_command_snapshot_tick_ == tick_
                            ? &handoff_command_prediction_snapshot_
                            : &step_start_prediction_snapshot_;
                } else if (command_path_request_active_) {
                    prediction_snapshot = &command_path_prediction_snapshot_;
                } else {
                    prediction_snapshot = runtime_prediction_snapshot_ptr;
                }
                if (object_cost_sources_.empty()) {
                    pathfinder_.rebuild_object_costs(
                        u.move_type, {&units_}, prediction_snapshot);
                } else {
                    pathfinder_.rebuild_object_costs(
                        u.move_type, object_cost_sources_,
                        prediction_snapshot);
                }
                if (global_object_cost_layers_) {
                    global_object_cost_layers_[cost_layer] =
                        pathfinder_.object_cost_layer(u.move_type);
                }
                object_cost_applied_frame_[cost_layer] = refresh_frame;
            } else if (object_cost_applied_frame_[cost_layer] != refresh_frame) {
                if (handoff_path_request_active_) {
                    const Map<u32, bool>* prediction_snapshot =
                        handoff_command_snapshot_tick_ == tick_
                            ? &handoff_command_prediction_snapshot_
                            : &step_start_prediction_snapshot_;
                    if (object_cost_sources_.empty()) {
                        pathfinder_.rebuild_object_costs(
                            u.move_type, {&units_}, prediction_snapshot);
                    } else {
                        pathfinder_.rebuild_object_costs(
                            u.move_type, object_cost_sources_,
                            prediction_snapshot);
                    }
                    if (global_object_cost_layers_) {
                        global_object_cost_layers_[cost_layer] =
                            pathfinder_.object_cost_layer(u.move_type);
                    }
                } else if (global_object_cost_layers_ &&
                    !global_object_cost_layers_[cost_layer].empty()) {
                    pathfinder_.set_object_cost_layer(
                        u.move_type, global_object_cost_layers_[cost_layer]);
                } else {
                    const Map<u32, bool>* prediction_snapshot = nullptr;
                    if (handoff_path_request_active_) {
                        prediction_snapshot =
                            handoff_command_snapshot_tick_ == tick_
                                ? &handoff_command_prediction_snapshot_
                                : &step_start_prediction_snapshot_;
                    } else if (command_path_request_active_) {
                        prediction_snapshot = &command_path_prediction_snapshot_;
                    } else {
                        prediction_snapshot = runtime_prediction_snapshot_ptr;
                    }
                    if (object_cost_sources_.empty()) {
                        pathfinder_.rebuild_object_costs(
                            u.move_type, {&units_}, prediction_snapshot);
                    } else {
                        pathfinder_.rebuild_object_costs(
                            u.move_type, object_cost_sources_,
                            prediction_snapshot);
                    }
                }
                object_cost_applied_frame_[cost_layer] = refresh_frame;
            }
            pathfinder_.synchronize_clearance(u.move_type);
        }

        // y.a(float): runtime requests use the unit's current formation
        // state (ae && ah>1). The same flag selects clearance 3 for the
        // direct-line probe and enables prefer-clearance grid weighting.
        const bool order_prefer_clearance =
            force_grid_path ||
            (u.formation_is_leader && u.formation_group_size > 1);

        // y.a() + aq.a(): a clear Bresenham line skips the grid solver and
        // lays intermediate waypoints every 20 units toward the exact end.
        // The line check runs on the RAW request point (clamping happens
        // afterwards), so off-map targets fail it via out-of-bounds tiles.
        const bool line_clear = pathfinder_.line_path_clear(
                u.x, u.y, x, y, u.move_type, 80,
                order_prefer_clearance ? 3 : 0, 1);
        if (line_clear && !force_grid_path) {
            u.path_incomplete = false;
            // y.a(FFIZZ) overwrites av[0] in place even for a refresh that
            // would preserve the old chain in the grid branch. An override
            // from a cleared earlier chain cannot remain the physical node.
            u.path_target_override_active = false;
            u.using_path = true;
            u.waypoints.clear();
            u.current_waypoint = 0;
            const f32 dx = goal_x - u.x;
            const f32 dy = goal_y - u.y;
            const f32 d = std::sqrt(dx * dx + dy * dy);
            Vec<std::pair<f32, f32>> line_points;
            const i32 intermediates = static_cast<i32>(d * 0.05f - 1.0f);
            if (d > 0.0f && intermediates > 0) {
                const f32 angle =
                    jar_direction_degrees(u.x, u.y, goal_x, goal_y);
                const f32 step_x = jar_cosine_degrees(angle) * 20.0f;
                const f32 step_y = jar_sine_degrees(angle) * 20.0f;
                i32 skip = intermediates < 4 ? 0 : 1;
                f32 px = u.x, py = u.y;
                for (i32 i = 0; i < intermediates; ++i) {
                    px += step_x;
                    py += step_y;
                    if (skip > 0) {
                        --skip;
                        continue;
                    }
                    line_points.push_back({px, py});
                    if (line_points.size() >= 119) {
                        u.path_incomplete = true;
                        break;
                    }
                }
            }
            if (!u.path_incomplete) {
                line_points.push_back({goal_x, goal_y});
            }
            u.target_x = line_points[0].first;
            u.target_y = line_points[0].second;
            for (i32 i = 1; i < static_cast<i32>(line_points.size()); ++i) {
                u.waypoints.push_back(line_points[i]);
            }
            u.has_move_target = true;
            u.arrived = false;
            return;
        }

        // gameFramework.e.b()/y.a() per-order path cache: every selected
        // waypoint shares ab.g. Search its solved entries at request time;
        // entry.b (the serialized unit uid) is not part of the match. A
        // preserve-active-path refresh can also consume a native replay
        // cache: JAR leaves av/aw visible while the cached aU result is
        // adopted by the next object update. Synthetic/local prepaths remain
        // restricted to path-replacing requests.
        const bool replay_cache_available =
            !u.order_queue.empty() &&
            !u.order_queue.front().replay_prepath_cache.empty();
        const bool replay_cache_refresh =
            preserve_active_path && replay_cache_available;
        const bool replay_move_cache_refresh =
            !preserve_active_path && !u.using_path &&
            !u.path_request_pending && !u.order_queue.empty() &&
            u.order_queue.front().type == WeaponType::MOVE &&
            u.order_queue.front().has_prepath &&
            u.order_queue.front().prepath_from_replay &&
            u.order_queue.front().prepath_attach_frame < tick_ &&
            !u.order_queue.front().replay_prepath_cache.empty();
        if (replay_move_cache_refresh) {
            // A native replay MOVE can refresh after its first cached path
            // was consumed. Re-enter the serialized cache selector for the
            // new request instead of treating the old prepath as active.
            u.order_queue.front().has_prepath = false;
        }
        if ((!preserve_active_path || replay_cache_refresh) &&
            (!u.formation_rebuild_path_pending || replay_cache_available) &&
            !u.order_queue.empty()) {
            UnitOrder& active = u.order_queue.front();
            // y.a() also searches ab.g when aD()/ab.c() has promoted a
            // follower without an active path. Regrouping must not replace a
            // qualifying solved replay cache with a new worker request.
            if (active.has_prepath && active.prepath_from_replay &&
                active.prepath_attach_frame < tick_ &&
                !active.replay_prepath_cache.empty()) {
                // y.a() reselects ab.g on every request. A previously
                // consumed cache does not bypass the current start-distance
                // gate or the next-turn aU adoption when it qualifies again.
                active.has_prepath = false;
            }
            if (!active.has_prepath &&
                !active.replay_prepath_cache.empty()) {
                const UnitOrderPrepath* selected = nullptr;
                for (const UnitOrderPrepath& candidate :
                     active.replay_prepath_cache) {
                    if (std::abs(candidate.target_x - x) > 10.0f ||
                        std::abs(candidate.target_y - y) > 10.0f ||
                        candidate.frame + 180 < tick_ ||
                        candidate.move_type != u.move_type ||
                        dist2(u.x, u.y,
                            candidate.start_x, candidate.start_y) >=
                            3600.0f) {
                        continue;
                    }
                    // The JAR bytecode never updates its nominal best-distance
                    // local, so the last qualifying entry wins. Cache ownership
                    // and node count do not change this selection or adoption.
                    selected = &candidate;
                }
                if (selected) {
                    active.prepath_frame = selected->frame;
                    active.prepath_attach_frame = tick_;
                    active.prepath_target_x = selected->target_x;
                    active.prepath_target_y = selected->target_y;
                    active.prepath_move_type = selected->move_type;
                    active.prepath = selected->path;
                    active.prepath_from_replay = true;
                    active.has_prepath = true;
                }
            }
            if (active.has_prepath &&
                tick_ - active.prepath_frame <= 180 &&
                std::abs(active.prepath_target_x - x) <= 10.0f &&
                std::abs(active.prepath_target_y - y) <= 10.0f &&
                active.prepath_move_type == u.move_type) {
                const bool attached_this_frame =
                    active.prepath_attach_frame == tick_ ||
                    (active.type == WeaponType::ATTACK &&
                     active.prepath_attach_frame + 1 == tick_);
                if (attached_this_frame &&
                    (!u.formation_rebuild_path_pending ||
                     active.prepath_from_replay)) {
                    u.path_request_pending = true;
                    u.path_request_uses_prepath = true;
                    u.path_request_prepath = active.prepath;
                    u.path_request_prepath_from_replay = active.prepath_from_replay;
                    u.path_request_prepath_submission_tick = tick_;
                    u.path_request_delay_frames = preserve_active_path ? 0.0f : 1.0f;
                    u.path_request_start_x = u.x;
                    u.path_request_start_y = u.y;
                    u.path_request_goal_x = goal_x;
                    u.path_request_goal_y = goal_y;
                    u.path_request_rotation = u.rotation;
                    u.path_request_flexible_reverse =
                        u.reverse_speed_multiplier > 0.95f || u.reversing;
                    u.path_request_target_size = target_size;
                    u.path_request_solver_target_size = target_size;
                    u.path_request_prefer_clearance =
                        active.prefer_clearance;
                    u.path_request_cost_snapshot.reset();
                    u.path_request_preserves_active_path =
                        preserve_active_path;
                    if (replay_move_cache_refresh) {
                        // The pending cached result coasts for one frame with
                        // the acceleration branch, matching y.b(l)'s MOVE
                        // refresh before the nodes become visible next tick.
                        u.movement_delay_uses_acceleration = true;
                    }
                    if (!preserve_active_path) {
                        u.using_path = false;
                        u.waypoints.clear();
                        u.current_waypoint = 0;
                        u.target_x = goal_x;
                        u.target_y = goal_y;
                    }
                    u.has_move_target = true;
                    u.arrived = false;
                    return;
                }
                const Vec<std::pair<f32, f32>> adopted = active.prepath;
                consume_solver_path_(u, adopted);
                if (adopted.size() > 1) {
                    // k.o includes the request's start tile. When an older
                    // command prepath is adopted directly, y still uses that
                    // hidden node for one physical update before aJ pops it.
                    const f32 start_node_x = pathfinder_.world_x(
                        pathfinder_.tile_x(u.x));
                    const f32 start_node_y = pathfinder_.world_y(
                        pathfinder_.tile_y(u.y));
                    const f32 start_distance_sq = dist2(
                        u.x, u.y, start_node_x, start_node_y);
                    const f32 second_distance_sq = dist2(
                        u.x, u.y, adopted.front().first,
                        adopted.front().second);
                    const bool pre_popped =
                        u.move_speed_per_frame() > 5.0f &&
                        (start_distance_sq < 36.0f ||
                         second_distance_sq < 361.0f);
                    if (!pre_popped && start_distance_sq < 256.0f) {
                        u.has_deferred_navigation_target = true;
                        u.deferred_navigation_target_x = start_node_x;
                        u.deferred_navigation_target_y = start_node_y;
                    }
                }
                return;
            }
        }

        // This is deliberately independent of the command-time prepath,
        // whose request always enables the weighting.
        if (defer_grid_path) {
            const i32 tile_dx = std::abs(
                pathfinder_.tile_x(u.x) - pathfinder_.tile_x(goal_x));
            const i32 tile_dy = std::abs(
                pathfinder_.tile_y(u.y) - pathfinder_.tile_y(goal_y));
            f32 allowed_delay = 300.0f;
            if (tile_dx < 15 && tile_dy < 15) {
                allowed_delay = 12.0f;
            } else if (tile_dx < 50 && tile_dy < 50) {
                allowed_delay = 16.0f;
            } else if (tile_dx < 200 && tile_dy < 200) {
                allowed_delay = 24.0f;
            } else if (tile_dx < 400 && tile_dy < 400) {
                allowed_delay = 50.0f;
            } else if (tile_dx < 1000 && tile_dy < 1000) {
                allowed_delay = 100.0f;
            } else if (tile_dx < 2000 && tile_dy < 2000) {
                allowed_delay = 200.0f;
            }
            // k.l.a(k, ...) applies one last adjustment after the bucket:
            // a request carrying k.r waits twice as long plus 50 frames.
            // y sets k.r from its own y.t, which y.k(float) clears at the
            // top of every object update. PATROL sets it only when its
            // ordered point is outside the current hold radius; q(3)'s
            // attacker proposal alone does not set t.
            if (u.path_request_low_priority) {
                allowed_delay = allowed_delay * 2.0f + 50.0f;
            }
            u.path_request_pending = true;
            u.path_request_needs_worker_handoff = handoff_path_request_active_;
            // k.l reaches t==0 first, solves on the following path-engine
            // update, and y consumes the result on its next object update.
            u.path_request_delay_frames = allowed_delay + 1.0f;
            if (!handoff_path_request_active_ &&
                u.repath_cooldown_order_start == 0.1f &&
                u.formation_is_leader && u.formation_group_size > 1 &&
                !u.formation_preferred && !preserve_active_path &&
                target_size > 0 && !u.order_queue.empty() &&
                u.order_queue.front().type == WeaponType::ATTACK) {
                // After a short direct proposal, a promoted leader requests
                // its grid route on the next object turn. The native worker
                // spends that turn's countdown before its later handoff;
                // finish_step's usual one-frame compensation is now absent.
                u.path_request_delay_frames = std::max(
                    0.0f, u.path_request_delay_frames - 1.0f);
                u.path_request_needs_worker_handoff = true;
            }
            u.path_request_start_x = u.x;
            u.path_request_start_y = u.y;
            u.path_request_goal_x = goal_x;
            u.path_request_goal_y = goal_y;
            u.path_request_rotation = u.rotation;
            u.path_request_flexible_reverse =
                u.reverse_speed_multiplier > 0.95f || u.reversing;
            u.path_request_target_size = target_size;
            u.path_request_solver_target_size = target_size;
            u.path_request_prefer_clearance = order_prefer_clearance;
            u.path_request_cost_snapshot =
                pathfinder_.capture_path_layer(u.move_type);
            if (preserve_active_path) {
                // y.a's grid branch returns with au/aw untouched: the
                // adopted path and its movement target stay active while
                // the request delay counts down, and y.b(l) adopts the
                // solved list once k.l finishes it.
                u.path_request_preserves_active_path = true;
                return;
            }
            u.using_path = false;
            u.waypoints.clear();
            u.current_waypoint = 0;
            u.target_x = goal_x;
            u.target_y = goal_y;
            u.has_move_target = true;
            u.arrived = false;
            return;
        }
        auto path = pathfinder_.find_path(
            u.x, u.y, goal_x, goal_y, u.move_type, u.radius, u.rotation,
            u.reverse_speed_multiplier > 0.95f || u.reversing,
            order_prefer_clearance, target_size, u.path_min_clearance);
        consume_solver_path_(u, path);
        return;
    }

    // y.a() ct() branch: air (and any residual) targets are still clamped
    // to the map like b.b.a(float) before becoming the single waypoint.
    const f32 clamp_max_x =
        static_cast<f32>(pathfinder_.width()) * pathfinder_.tile_size();
    const f32 clamp_max_y =
        static_cast<f32>(pathfinder_.height()) * pathfinder_.tile_size();
    // y.a()'s ct() branch exposes every air request as a one-node av/aw path.
    // This includes runtime ATTACK chases as well as factory exit movement.
    const bool direct_air_path = u.move_type == MoveType::AIR;
    // y.a(...) charges the same 500-frame repath cooldown before ct() builds
    // an AIR node. Ground requests do this in the grid branch above; keeping
    // the charge here makes initial PATROL/Move AIR requests follow the same
    // countdown instead of exposing a zero cooldown from the first frame.
    if (direct_air_path) {
        u.repath_cooldown_frames = 500.0f;
    }
    u.using_path = direct_air_path;
    if (direct_air_path) {
        // ct() rewrites av[0] synchronously, including a request that
        // preserves a former combat path. The captured node now names the
        // new destination for this same movement update.
        u.path_target_override_active = false;
        u.waypoints.clear();
        u.current_waypoint = 0;
    }
    u.target_x = std::min(std::max(x, 0.0f), clamp_max_x);
    u.target_y = std::min(std::max(y, 0.0f), clamp_max_y);
    u.has_move_target = true;
    u.arrived = false;
}

} // namespace rwrl
