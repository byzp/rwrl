#include "world_internal.h"

namespace rwrl {

void World::update_builds(f32 dt, f32 speed_mult) {
    Vec<i32> done;
    const f32 delta_frames =
        EngineConstants::FRAME_RATE * dt * speed_mult;
    const i32 existing_task_count = static_cast<i32>(pending_builds_.size());

    Vec<u32> pending_order_builders;
    for (const auto& [uid, unit] : units_) {
        if (!unit.is_active() || unit.activate_order_next_frame ||
            unit.weapon_type != WeaponType::BUILD ||
            unit.target_uid != 0 || unit.order_queue.empty() ||
            unit.order_queue.front().type != WeaponType::BUILD) {
            continue;
        }
        pending_order_builders.push_back(uid);
    }
    std::sort(pending_order_builders.begin(), pending_order_builders.end());
    for (u32 builder_uid : pending_order_builders) {
        Unit* builder = get_unit(builder_uid);
        if (!builder || builder->order_queue.empty()) continue;
        const UnitOrder order = builder->order_queue.front();
        if (!registry_.can_build_unit(builder->unit_type, order.unit_type,
                                      order.build_tech_level)) {
            finish_service_order(*builder);
            continue;
        }
        if (dist(builder->x, builder->y, order.x, order.y) >
            service_range_to_(*builder, order.unit_type)) {
            if (builder->is_mobile() && !builder->has_move_target) {
                move_unit_to(*builder, order.x, order.y);
            }
            continue;
        }

        Unit* built = start_build_(order.unit_type, order.x, order.y, true,
                                   nullptr, order.build_tech_level, builder_uid);
        if (!built) continue;
        const u32 built_uid = built->uid;
        builder = get_unit(builder_uid);
        if (!builder) continue;
        builder->build_price_last_refusal_clock_ms = -9999;
        builder->target_uid = built_uid;
        builder->target_team_id = team_id_;
        builder->has_move_target = false;
        builder->arrived = true;
        builder->using_path = false;
        builder->waypoints.clear();
        builder->current_waypoint = 0;
        builder->path_incomplete = false;
        builder->repath_cooldown_frames = 0.0f;
        builder->waypoint_stall_frames = 0.0f;
        builder->last_waypoint_stall_frames = 0.0f;
        builder->single_node_path_counter = 0;
        if (!builder->order_queue.empty() &&
            builder->order_queue.front().type == WeaponType::BUILD) {
            UnitOrder& active = builder->order_queue.front();
            // y.f() resets the au payload before switching it to REPAIR.
            // In particular au.i no longer names the old BUILD controller,
            // although the unit's historical ad/ac/slot metadata survives.
            active = UnitOrder{};
            active.type = WeaponType::REPAIR;
            active.build_tech_level = 1;
            active.x = 2.0f;
            active.y = 2.0f;
            active.target_uid = built_uid;
            active.target_team_id = team_id_;
        }
    }

    for (i32 i = 0; i < existing_task_count; ++i) {
        BuildTask& task = pending_builds_[i];
        Unit* u = get_unit(task.target_uid);
        if (!u || !u->is_alive) { done.push_back(i); continue; }

        const f32 bs = registry_.get_build_speed(u->unit_type);
        auto account_completion = [&]() {
            if (task.completion_accounted) return;
            task.completion_accounted = true;
            if (u->income_rate > 0) total_income_rate_ += u->income_rate;
            buildings_completed_++;
        };

        if (!task.requires_builder) {
            task.progress = std::min(1.0f, task.progress + bs * delta_frames);
            u->build_progress = task.progress;
            if (task.progress >= 1.0f) {
                account_completion();
                done.push_back(i);
            }
            continue;
        }

        task.progress = std::max(task.progress, u->build_progress);
        Vec<u32> builder_ids;
        for (const auto& [builder_uid, builder] : units_) {
            if (!builder.is_active() ||
                builder.weapon_type != WeaponType::BUILD ||
                builder.target_uid != task.target_uid) {
                continue;
            }
            builder_ids.push_back(builder_uid);
        }
        // The JAR updates orderable objects in creation order. UIDs preserve
        // that order and make completion-frame multi-builder behavior stable.
        std::sort(builder_ids.begin(), builder_ids.end());
        for (u32 builder_uid : builder_ids) {
            Unit* builder = get_unit(builder_uid);
            if (!builder || builder->target_uid != task.target_uid) continue;
            if (!service_target_in_range_(*builder, *u)) {
                if (builder->is_mobile() && !builder->has_move_target &&
                    !builder->path_request_pending) {
                    move_unit_to_async_(*builder, u->x, u->y);
                }
                continue;
            }

            if (builder->has_move_target) {
                builder->has_move_target = false;
                builder->arrived = true;
                builder->movement_prediction_enabled = false;
            }

            if (u->is_complete()) {
                // Active construction waypoints become ordinary repair
                // waypoints in y.f(...). A full target is consumed at the
                // start of the next builder update; a damaged target is
                // repaired by later builders in this same world frame.
                if (u->hp >= u->max_hp) {
                    finish_service_order(*builder);
                    continue;
                }
                if (!service_action_ready(*builder, *u, delta_frames)) continue;
                u->hp += builder->nano_repair_speed * delta_frames;
                if (u->hp > u->max_hp) {
                    u->hp = u->max_hp;
                    finish_service_order(*builder);
                    builder->service_completed_in_build_pass = true;
                }
                continue;
            }

            if (!service_action_ready(*builder, *u, delta_frames)) continue;
            task.progress = std::min(
                1.0f, task.progress + service_build_speed_(*builder, *u) * delta_frames);
            u->build_progress = task.progress;
            if (task.progress >= 1.0f) {
                u->build_progress = 1.0f;
                account_completion();
            }
        }

        if (u->is_complete()) {
            bool has_builder = false;
            for (u32 builder_uid : builder_ids) {
                const Unit* builder = get_unit(builder_uid);
                if (builder && builder->target_uid == task.target_uid) {
                    has_builder = true;
                    break;
                }
            }
            if (!has_builder) {
                account_completion();
                done.push_back(i);
            }
        }
    }
    for (i32 i = static_cast<i32>(done.size()) - 1; i >= 0; --i) {
        pending_builds_.erase(pending_builds_.begin() + done[i]);
    }
}

void World::update_unit_builds(f32 dt, f32 speed_mult, u32 unit_uid) {
    const f32 delta_frames = EngineConstants::FRAME_RATE * dt * speed_mult;
    Unit* updating = get_unit(unit_uid);
    if (!updating) return;
    const u32 retained_main_target_uid = updating->main_target_uid();
    const i32 retained_main_target_team_id = updating->main_target_team_id();
    u32 created_target_uid = 0;

    // A positional BUILD waypoint creates its structure during the builder's
    // own global-object update, after movement has brought it into range.
    if (updating->is_active() &&
        !updating->activate_order_next_frame &&
        updating->weapon_type == WeaponType::BUILD &&
        updating->target_uid == 0 &&
        !updating->order_queue.empty() &&
        updating->order_queue.front().type == WeaponType::BUILD) {
        const UnitOrder order = updating->order_queue.front();
        if (!registry_.can_build_unit(updating->unit_type, order.unit_type,
                                      order.build_tech_level)) {
            finish_service_order(*updating);
        } else if (dist(updating->x, updating->y, order.x, order.y) >
                   service_range_to_(*updating, order.unit_type)) {
            if (updating->is_mobile() && !updating->has_move_target &&
                !updating->path_request_pending) {
                if (updating->formation_leader_uid != 0) {
                    Unit* leader = formation_unit_(updating->formation_leader_uid);
                    const bool matching_order = leader && leader->is_active() &&
                        !leader->order_queue.empty() &&
                        leader->order_queue.front().type == order.type &&
                        leader->order_queue.front().unit_type == order.unit_type &&
                        leader->order_queue.front().build_tech_level == order.build_tech_level &&
                        leader->order_queue.front().target_uid == order.target_uid &&
                        leader->order_queue.front().x == order.x &&
                        leader->order_queue.front().y == order.y;
                    if (!matching_order) {
                        // BUILD's ad.d is cleared when the leader has a
                        // different waypoint, but the historical ad link
                        // survives the follower's private path request.
                        updating->formation_projection_allowed = false;
                    }
                }
                // y.f(...) converts the builder's type-specific nano range
                // into the path engine's tile allowance.
                move_unit_to_async_(*updating, order.x, order.y,
                    service_path_target_size_(*updating, order.unit_type));
                updating->movement_activation_delay_frames = 1.0f;
            }
            // y.f(float,au,ad) offset 290 caps s at 90 on every update that
            // publishes the travelling proposal, so the 500 a fresh request
            // charged survives only until the builder's next object turn.
            // Native replay runs the command after that turn; the C++ driver
            // runs it before, and the activation delay marks the one update
            // that stands in for it.
            if (updating->movement_activation_delay_frames == 0.0f &&
                updating->repath_cooldown_frames > 90.0f) {
                updating->repath_cooldown_frames = 90.0f;
            }
        } else {
            // y.f(float, au, ad) turns the builder toward the placement point
            // before testing its 30-degree build gate. While aiming it drops
            // the movement proposal but retains the positional BUILD order.
            const f32 desired_angle = jar_direction_degrees(
                updating->x, updating->y, order.x, order.y);
            f32 aim_error = std::abs(shortest_angle_delta(
                updating->rotation, desired_angle));
            // A refusal on price suspends the whole placement pass, so the
            // builder does not turn either until the action is ready again.
            // utility.y.a(Q,200) uses l.by's integer clock, including its
            // per-update truncation. A fixed twelve-frame countdown differs
            // when native delta is fractional or exceeds one frame.
            const i32 refusal_clock = updating->build_price_last_refusal_clock_ms;
            const bool price_wait =
                simulation_clock_ms_ <= refusal_clock + 200 &&
                simulation_clock_ms_ >= refusal_clock - 1000;
            if (!price_wait) {
                aim_error = std::abs(turn_service_toward(
                    *updating, order.x, order.y, delta_frames));
            }
            updating->has_move_target = false;
            updating->arrived = true;
            updating->movement_prediction_enabled = false;
            if (updating->using_path &&
                !updating->path_target_override_active) {
                // BUILD stops publishing l/m while aiming, but y.av[0]
                // remains available to the unit and its formation followers.
                updating->path_target_override_active = true;
                updating->path_target_override_x = updating->target_x;
                updating->path_target_override_y = updating->target_y;
            }
            updating->has_deferred_navigation_target = false;
            // y.f(float,au,ad) is the BUILD waypoint's own dispatch, so it
            // suspends the proposal before the physical pass could reach
            // aJ(). C++ takes the next chain node one pass earlier, in the
            // movement order pass, so that node has to go back.
            if (updating->waypoint_consumed_tick == tick_ &&
                updating->using_path && updating->current_waypoint > 0) {
                updating->current_waypoint--;
                updating->waypoint_stall_frames =
                    updating->last_waypoint_stall_frames;
                updating->path_target_override_x =
                    updating->deferred_navigation_target_x;
                updating->path_target_override_y =
                    updating->deferred_navigation_target_y;
                updating->waypoint_consumed_tick = -1;
            }
            // y.f(...) stops asserting y.k while it aims, but leaves the
            // av/aw chain intact: the placement pass has not ended the
            // movement request, only suspended its proposal.
            updating->target_x = 0.0f;
            updating->target_y = 0.0f;
            if (price_wait || aim_error > 30.0f) return;

            bool unaffordable = false;
            bool candidate_created = false;
            Unit* built = start_build_(order.unit_type, order.x, order.y, true,
                                       &unaffordable, order.build_tech_level, unit_uid,
                                       &candidate_created);
            Unit* existing_target = built;
            if (!existing_target && !unaffordable && candidate_created) {
                // y.a(au,as,...) calls bs() on the rejected provisional
                // building. Match its snapped position and radius, in global
                // creation order; the builder and raw waypoint are unrelated
                // to am.t()'s strict circle-overlap predicate.
                const UnitDef* definition = registry_.get(order.unit_type);
                const f32 match_x = definition ? jar_build_coordinate(
                    order.x, definition->building_to_footprint_offset_x,
                    pathfinder_.tile_size()) : order.x;
                const f32 match_y = definition ? jar_build_coordinate(
                    order.y, definition->building_to_footprint_offset_y,
                    pathfinder_.tile_size()) : order.y;
                for (auto& [_, candidate] : units_) {
                    if (!candidate.is_alive || candidate.unit_type != order.unit_type) {
                        continue;
                    }
                    const f32 match_radius = candidate.radius +
                        (definition ? definition->radius : 0.0f);
                    if (dist2(candidate.x, candidate.y, match_x, match_y) <
                            match_radius * match_radius &&
                        (!existing_target || candidate.object_creation_order <
                            existing_target->object_creation_order)) {
                        existing_target = &candidate;
                    }
                }
            }
            if (existing_target) {
                if (!built && registry_.is_building(order.unit_type)) {
                    // y.f(...) constructs the requested stock building before
                    // discovering the already-present incomplete target. The
                    // rejected candidate still consumes w.eh and object order.
                    if (global_next_uid_) {
                        ++(*global_next_uid_);
                    } else {
                        ++next_uid_;
                    }
                    (void)allocate_object_order_();
                }
                const u32 built_uid = existing_target->uid;
                created_target_uid = built_uid;
                if (!built && !existing_target->is_complete() &&
                    std::none_of(pending_builds_.begin(), pending_builds_.end(),
                        [&](const BuildTask& task) {
                            return task.target_uid == built_uid;
                        })) {
                    // Restored/injected incomplete buildings need the same
                    // service bookkeeping as structures placed by a builder.
                    // Keep the BUILD-to-REPAIR turn free of a nano pulse.
                    pending_builds_.push_back({existing_target->build_progress,
                                              built_uid, true, false});
                }
                updating = get_unit(unit_uid);
                if (updating) {
                    updating->build_price_last_refusal_clock_ms = -9999;
                    // Entering construction range still performs the
                    // builder's one-frame facing update before the movement
                    // request is cleared. The following physical pass then
                    // applies ordinary deceleration along that new heading.
                    updating->target_uid = built_uid;
                    updating->target_team_id = team_id_;
                    updating->has_move_target = false;
                    updating->arrived = true;
                    updating->movement_prediction_enabled = false;
                    updating->has_deferred_navigation_target = false;
                    updating->using_path = false;
                    updating->waypoints.clear();
                    updating->current_waypoint = 0;
                    // y.f() calls aH() when BUILD becomes REPAIR. The
                    // service's next turn must start with no old s/u/W/X/q.
                    updating->path_incomplete = false;
                    updating->repath_cooldown_frames = 0.0f;
                    updating->waypoint_stall_frames = 0.0f;
                    updating->last_waypoint_stall_frames = 0.0f;
                    updating->single_node_path_counter = 0;
                    if (!updating->order_queue.empty() &&
                        updating->order_queue.front().type ==
                            WeaponType::BUILD) {
                        UnitOrder& active = updating->order_queue.front();
                        // au.e() also discards BUILD's shared controller and
                        // cached payload. A later leader removal must only
                        // detach these repairers, without regrouping them.
                        active = UnitOrder{};
                        active.type = WeaponType::REPAIR;
                        active.build_tech_level = 1;
                        // au.b(am) switches a positional BUILD into a unit
                        // target and resets its coordinate payload to 2,2.
                        active.x = 2.0f;
                        active.y = 2.0f;
                        active.target_uid = built_uid;
                        active.target_team_id = team_id_;
                    }
                }
            } else {
                // Mobile refusals after construction consume their candidate
                // in start_build_; cap refusals never created one at all.
                if (candidate_created && registry_.is_building(order.unit_type)) {
                    if (global_next_uid_) {
                        ++(*global_next_uid_);
                    } else {
                        ++next_uid_;
                    }
                    (void)allocate_object_order_();
                }
                if (unaffordable) {
                    // The price refusal is the only one that can set z.c, and
                    // it does so only while y.V -- the age in frames of the
                    // current waypoint -- is under 1000. A young waypoint is
                    // kept and retried; once it has been current for 1000
                    // frames the same refusal returns z.c=false and y.f()
                    // drops it. y.k(float) charges V before dispatching, so
                    // this pass compares against the post-charge value.
                    updating->build_price_last_refusal_clock_ms = simulation_clock_ms_;
                    if (updating->active_order_elapsed_frames + delta_frames <
                        EngineConstants::BUILD_PRICE_RETRY_MAX_ORDER_FRAMES) {
                        return;
                    }
                }
                // y.f(...) consumes a positional BUILD once the builder is in
                // service range even when placement fails. It still applies
                // this frame's facing update before y.ay()/aH() clear the
                // movement proposal, so the physical pass decelerates along
                // the new heading while retaining the formation link.
                clear_current_order_state_(*updating);
                updating->movement_prediction_enabled = false;
                if (!updating->order_queue.empty() &&
                    updating->order_queue.front().type ==
                        WeaponType::BUILD) {
                    updating->order_queue.erase(
                        updating->order_queue.begin());
                    // y.ay() exposes the next au now; its dispatch starts on
                    // the next y.k() turn. Restore the next waypoint's active
                    // state after this failed BUILD cleared the old state.
                    updating->activate_order_next_frame =
                        !updating->order_queue.empty();
                }
                updating->restore_main_target(
                    retained_main_target_uid, retained_main_target_team_id);
            }
        }
    }

    for (std::size_t index = 0; index < pending_builds_.size();) {
        BuildTask& task = pending_builds_[index];
        Unit* target = get_unit(task.target_uid);
        if (!target || !target->is_alive) {
            pending_builds_.erase(pending_builds_.begin() + index);
            continue;
        }
        if (task.target_uid == created_target_uid) {
            ++index;
            continue;
        }

        const f32 build_speed = registry_.get_build_speed(target->unit_type);
        auto account_completion = [&]() {
            if (task.completion_accounted) return;
            task.completion_accounted = true;
            if (target->income_rate > 0.0f) {
                total_income_rate_ += target->income_rate;
            }
            buildings_completed_++;
        };

        // start_build() is a scene-injection helper. Advance its synthetic
        // task when the target object itself reaches its global turn.
        if (!task.requires_builder) {
            if (task.target_uid == unit_uid) {
                task.progress = std::min(
                    1.0f, task.progress + build_speed * delta_frames);
                target->build_progress = task.progress;
                if (task.progress >= 1.0f) {
                    account_completion();
                    pending_builds_.erase(pending_builds_.begin() + index);
                    continue;
                }
            }
            ++index;
            continue;
        }

        task.progress = std::max(task.progress, target->build_progress);
        Unit* builder = get_unit(unit_uid);
        const bool matching_builder = builder && builder->is_active() &&
            builder->weapon_type == WeaponType::BUILD &&
            builder->target_uid == task.target_uid;
        if (matching_builder) {
            if (!service_target_in_range_(*builder, *target)) {
                if (builder->is_mobile() && !builder->path_request_pending &&
                    (!builder->has_move_target ||
                     builder->service_projection_proposal)) {
                    // A builder chasing its construction target uses the same
                    // y.a() service handoff as a repairer chasing a damaged
                    // one; only the service branch differs afterwards.
                    begin_service_chase_(*builder, *target);
                }
            } else {
                if (builder->has_move_target) {
                    builder->has_move_target = false;
                    builder->arrived = true;
                    builder->movement_prediction_enabled = false;
                }
                if (target->is_complete()) {
                    if (target->hp >= target->max_hp) {
                        // y.f(...) consumes a full construction target during
                        // this late service phase. The next waypoint is exposed
                        // immediately but is not activated until a later update.
                        clear_current_order_state_(*builder);
                        if (!builder->order_queue.empty() &&
                            builder->order_queue.front().type ==
                                WeaponType::REPAIR &&
                            builder->order_queue.front().target_uid ==
                                target->uid) {
                            builder->order_queue.erase(
                                builder->order_queue.begin());
                            // ay() keeps ad after exposing a queued BUILD.
                            // A mismatching leader disables projection on
                            // the next turn without discarding this link.
                            // y.f()/y.ay() exposes the following service
                            // waypoint, but its target is not installed until
                            // the builder's next object turn. Without this
                            // marker the order phase sees target_uid==0 and
                            // consumes the newly exposed waypoint as already
                            // finished.
                            if (!builder->order_queue.empty() &&
                                (builder->order_queue.front().type == WeaponType::RECLAIM ||
                                 builder->order_queue.front().type == WeaponType::LOAD_INTO ||
                                 builder->order_queue.front().type == WeaponType::LOAD_UP)) {
                                builder->activate_order_next_frame = true;
                            }
                        }
                        builder->restore_main_target(
                            retained_main_target_uid, retained_main_target_team_id);
                    } else if (service_action_ready(
                                   *builder, *target, delta_frames)) {
                        target->hp += builder->nano_repair_speed * delta_frames;
                        if (target->hp > target->max_hp) {
                            target->hp = target->max_hp;
                            finish_service_order(*builder);
                            builder->service_completed_in_build_pass = true;
                        }
                    }
                } else if (service_action_ready(
                               *builder, *target, delta_frames)) {
                    task.progress = std::min(
                        1.0f, task.progress + service_build_speed_(*builder, *target) * delta_frames);
                    target->build_progress = task.progress;
                    if (task.progress >= 1.0f) {
                        target->build_progress = 1.0f;
                        account_completion();
                        // Completing construction does not consume the REPAIR
                        // waypoint. The same live service target remains in
                        // BUILD mode and starts repairing its remaining HP on
                        // the builder's next object update; the order is
                        // consumed only after HP reaches max_hp.
                        // y.i() tested the full-target completion gate before
                        // incrementing cm. Keep the target even with full HP:
                        // a later projectile in this object loop can damage
                        // the newly completed building before the next turn.
                    }
                }
            }
        }

        if (target->is_complete()) {
            bool has_builder = false;
            for (const auto& [_, candidate] : units_) {
                if (candidate.is_active() &&
                    candidate.weapon_type == WeaponType::BUILD &&
                    candidate.target_uid == task.target_uid) {
                    has_builder = true;
                    break;
                }
            }
            if (!has_builder) {
                account_completion();
                pending_builds_.erase(pending_builds_.begin() + index);
                continue;
            }
        }
        ++index;
    }
}

void World::cancel_build(Unit& u) {
    f32 refund = 0.0f;
    for (i32 i = 0; i < static_cast<i32>(pending_builds_.size()); ++i) {
        if (pending_builds_[i].target_uid == u.uid) {
            refund = static_cast<f32>(u.cost);
            pending_builds_.erase(pending_builds_.begin() + i);
            break;
        }
    }
    if (refund > 0.0f) refund_credits(refund, EngineConstants::CANCEL_REFUND);
    remove_unit(u.uid);
}

void World::reclaim_unit(Unit& u) {
    const f32 reclaim_cost = u.reclaim_cost > 0.0f
        ? u.reclaim_cost : static_cast<f32>(u.cost);
    refund_credits(reclaim_cost, EngineConstants::RECLAIM_REFUND);
    remove_unit(u.uid);
    losses_++;
}


} // namespace rwrl
