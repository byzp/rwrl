#include "world_internal.h"

namespace rwrl {

void World::configure_command_formation_(
        const GameCommand& cmd, const Vec<World*>& other_worlds,
        u64 inherited_controller_id, bool combine_owners) {
    Vec<Unit*> selected;
    selected.reserve(pending_formation_uids_.size());
    const auto collect_members = [&](World& owner) {
        for (u32 uid : owner.pending_formation_uids_) {
            Unit* unit = formation_unit_(uid);
            if (unit && unit->is_alive && unit->is_mobile() &&
                !unit->custom_action_movement_blocked &&
                unit->joins_group_formations &&
                (!combine_owners || (!unit->order_queue.empty() &&
                 unit->order_queue.front().formation_group_id ==
                     owner.active_command_formation_group_id_))) {
                selected.push_back(unit);
            }
        }
    };
    collect_members(*this);
    if (combine_owners) {
        for (World* owner : other_worlds) {
            if (owner && owner != this) collect_members(*owner);
        }
    }
    if (selected.empty()) return;
    // aa owns one allocator across the global object list. A subgroup can
    // contain shared-control owners, so its controller identity must too.
    for (World* owner : other_worlds) {
        if (owner) next_formation_controller_id_ = std::max(
            next_formation_controller_id_, owner->next_formation_controller_id_);
    }
    // ab.a() gathers members from am.bE, independently of selection order.
    // The distance bands and follower cap consume that global creation order.
    std::stable_sort(selected.begin(), selected.end(),
        [](const Unit* lhs, const Unit* rhs) {
            return lhs->object_creation_order < rhs->object_creation_order;
        });

    // Only a replacing command calls az()/aD() on its former leader.
    // Selective pruning uses ay(), and an append only assigns the new au.i;
    // both retain unselected followers' historical ad links even when this
    // leader participates in the new controller's formation pass.
    if (!formation_rebuild_active_ && !cmd.queued &&
        !cmd.clear_non_build_repair && !cmd.clear_orders) {
        Set<u32> selected_uids;
        for (const Unit* unit : selected) selected_uids.insert(unit->uid);
        for (Unit* unit : selected) {
            if (!unit->formation_is_leader) continue;
            bool has_unselected_follower = false;
            const auto has_follower = [&](const World& owner) {
                for (const auto& [_, follower] : owner.units_) {
                    if (follower.formation_leader_uid == unit->uid &&
                        selected_uids.count(follower.uid) == 0) return true;
                }
                return false;
            };
            has_unselected_follower = has_follower(*this);
            for (const World* owner : other_worlds) {
                if (owner && has_follower(*owner)) has_unselected_follower = true;
            }
            if (has_unselected_follower) {
                rebuild_formation_after_leader_removed_(
                    *unit, &selected_uids);
            }
        }
    }

    f32 target_x = cmd.target_x;
    f32 target_y = cmd.target_y;
    Unit* command_target = nullptr;
    if (cmd.target_uid != 0) {
        World* target_world = find_order_target_world_(
            cmd.target_uid, other_worlds);
        command_target = target_world
            ? target_world->get_unit(cmd.target_uid) : nullptr;
        if (command_target) {
            target_x = command_target->x;
            target_y = command_target->y;
        }
    }

    const auto command_has_long_prepath = [&]() {
        if (!cmd.replay_prepaths_provided) return false;
        for (const ReplayPrepath& prepath : cmd.replay_prepaths) {
            if (prepath.has_path && prepath.tiles.size() > 4) {
                return true;
            }
        }
        return false;
    };
    const auto command_has_handoff_cache = [&]() {
        if (!cmd.replay_prepaths_provided) return false;
        for (const ReplayPrepath& prepath : cmd.replay_prepaths) {
            if (prepath.has_path && prepath.tiles.size() == 9) {
                return true;
            }
        }
        return false;
    };
    const bool replay_handoff_cache_command =
        cmd.action == GameCommand::ATTACK_MOVE && cmd.group_size >= 19 &&
        command_has_handoff_cache();
    if (replay_handoff_cache_command) {
        // The serialized cache belongs to the selection, not only to the
        // unit that owns its representative entry. Propagate the command
        // marker before regrouping so cache-less subgroup leaders follow the
        // same handoff rule.
        for (Unit* member : selected) {
            if (!member->order_queue.empty() &&
                member->order_queue.front().type == WeaponType::ATTACK_MOVE) {
                member->order_queue.front().replay_command_has_handoff_cache =
                    true;
                member->activate_order_next_frame = false;
            }
        }
    }

    if (!raw_commands_ && cmd.action == GameCommand::ATTACK &&
        command_target != nullptr) {
        const i32 historical_tick = tick_ - 6;
        const auto historical_position = [historical_tick](const Unit& unit) {
            std::pair<f32, f32> position{unit.x, unit.y};
            for (const PositionSnapshot& snapshot : unit.position_history) {
                if (snapshot.tick != historical_tick) continue;
                position = {snapshot.x, snapshot.y};
                break;
            }
            return position;
        };

        const auto historical_target = historical_position(*command_target);
        Vec<bool> grouped(selected.size(), false);
        Set<u32> prepath_leader_uids;
        while (true) {
            i32 leader_index = -1;
            f32 best_distance_sq = -1.0f;
            for (i32 index = 0;
                 index < static_cast<i32>(selected.size()); ++index) {
                if (grouped[index]) continue;
                const auto position = historical_position(*selected[index]);
                const f32 distance_sq = dist2(
                    historical_target.first, historical_target.second,
                    position.first, position.second);
                if (leader_index < 0 || distance_sq < best_distance_sq) {
                    leader_index = index;
                    best_distance_sq = distance_sq;
                }
            }
            if (leader_index < 0) break;

            Unit* leader = selected[leader_index];
            prepath_leader_uids.insert(leader->uid);
            const auto leader_position = historical_position(*leader);
            for (i32 index = 0;
                 index < static_cast<i32>(selected.size()); ++index) {
                if (grouped[index]) continue;
                if (selected[index]->move_type != leader->move_type) {
                    continue;
                }
                const auto position = historical_position(*selected[index]);
                if (dist2(leader_position.first, leader_position.second,
                          position.first, position.second) < 160000.0f) {
                    grouped[index] = true;
                }
            }
        }

        // e.b() creates one shared cache entry per historical broad-group
        // leader. The order later searches those entries by start proximity;
        // it does not create a private cache for every selected unit.
        for (Unit* unit : selected) {
            if (prepath_leader_uids.count(unit->uid) != 0 ||
                unit->order_queue.empty()) {
                continue;
            }
            UnitOrder& active = unit->order_queue.front();
            if (active.type == WeaponType::ATTACK &&
                active.target_uid == cmd.target_uid) {
                active.has_prepath = false;
                active.prepath.clear();
            }
        }
    }

    Vec<Unit*> center_members = selected;
    if (cmd.native_selection && !formation_rebuild_active_ &&
        cmd.group_uids.size() > selected.size()) {
        // ab.a() reads the global am.bE list. A shared-control command has
        // one center even when different owners form different move layers.
        // Queued or rejected waypoints cannot contribute their old heads.
        center_members.clear();
        const auto collect_center = [&](World& owner) {
            for (u32 uid : cmd.group_uids) {
                Unit* member = owner.get_unit(uid);
                if (!member || !member->is_alive || !member->is_mobile() ||
                    !member->native_unit_list_member ||
                    member->custom_action_movement_blocked ||
                    !member->joins_group_formations ||
                    member->order_queue.empty()) continue;
                const UnitOrder& active = member->order_queue.front();
                if (active.formation_group_id !=
                        owner.active_command_formation_group_id_ ||
                    active.type != static_cast<WeaponType>(cmd.action)) {
                    continue;
                }
                center_members.push_back(member);
            }
        };
        collect_center(*this);
        for (World* owner : other_worlds) {
            if (owner && owner != this) collect_center(*owner);
        }
        if (center_members.empty()) center_members = selected;
        std::stable_sort(center_members.begin(), center_members.end(),
            [](const Unit* lhs, const Unit* rhs) {
                return lhs->object_creation_order < rhs->object_creation_order;
            });
    }
    f32 center_x = 0.0f;
    f32 center_y = 0.0f;
    for (Unit* unit : center_members) {
        center_x += unit->x;
        center_y += unit->y;
    }
    center_x /= static_cast<f32>(center_members.size());
    center_y /= static_cast<f32>(center_members.size());
    const f32 heading = jar_direction_degrees(
        center_x, center_y, target_x, target_y);

    for (Unit* unit : selected) {
        bool preferred = unit->formation_group_size > 1 &&
                         unit->formation_is_leader;
        // ab.c(): large formations do not preserve their former leader when
        // the new command reverses the formation heading by more than 80
        // degrees. Small groups retain the normal 160-unit preference.
        if (preferred && unit->formation_group_size > 7 &&
            std::abs(shortest_angle_delta(
                unit->formation_heading, heading)) > 80.0f) {
            preferred = false;
        }
        unit->formation_preferred = preferred;
        unit->formation_leader_uid = 0;
        unit->formation_is_leader = false;
        unit->formation_controller_id = 0;
        unit->formation_path_activation_pending = false;
        unit->formation_rebuild_handoff_pending = false;
        if (!formation_rebuild_active_) {
            unit->formation_rebuild_path_pending = false;
        }
        unit->formation_path_fallback_frames = 0.0f;
        unit->formation_group_size = 0;
        unit->formation_offset_x = 0.0f;
        unit->formation_offset_y = 0.0f;
        // Replay commands are submitted immediately before Engine::tick();
        // JAR records the formation assignment on the frame that then runs,
        // while tick_ still names the preceding visible state here.
        unit->formation_tick = simulation_clock_ms_;
        unit->formation_tick_pending = !step_in_progress_;
    }

    Set<u32> grouped;
    while (grouped.size() < selected.size()) {
        Unit* leader = nullptr;
        f32 best_score = -1.0f;
        for (Unit* unit : selected) {
            if (grouped.count(unit->uid) != 0) continue;
            f32 score = dist(target_x, target_y, unit->x, unit->y);
            if (unit->formation_preferred) score -= 160.0f;
            if (!leader || score < best_score) {
                leader = unit;
                best_score = score;
            }
        }
        if (!leader) break;

        const bool first_group = grouped.empty();
        grouped.insert(leader->uid);
        leader->formation_is_leader = true;
        // ab.c() only relinks ATTACK survivors. Their own next y.k() reads
        // the retained s and submits a private path when that gate permits.
        if (formation_rebuild_active_ && cmd.action != GameCommand::ATTACK &&
            !leader->using_path &&
            !leader->path_request_pending && leader->has_move_target) {
            leader->formation_rebuild_path_pending = true;
        }

        // gameFramework.ab gathers a leader's followers in three distance
        // bands (60, 120, then 200 world units). au.j widens the last band
        // to 400 and bypasses both the 25-follower cap and the speed gate.
        // Units outside the group become leaders of subsequent groups.
        Vec<Unit*> followers;
        f32 max_radius = 0.0f;
        constexpr f32 distance_limits_sq[] = {
            3600.0f, 14400.0f, 40000.0f};
        for (const f32 distance_limit_sq : distance_limits_sq) {
            for (Unit* unit : selected) {
                // ab compares z(), including custom.j's retained terrain
                // multiplier, in world units per native frame.
                const f32 speed_difference = std::abs(
                    unit->move_speed_per_frame() - leader->move_speed_per_frame());
                const f32 distance_sq = dist2(
                    unit->x, unit->y, leader->x, leader->y);
                const bool within_group =
                    (cmd.relaxed_formation && distance_sq < 160000.0f) ||
                    (distance_sq < 40000.0f && followers.size() < 25);
                if (grouped.count(unit->uid) != 0 ||
                    unit->move_type != leader->move_type ||
                    (!cmd.relaxed_formation && speed_difference >= 0.4f) ||
                    (distance_limit_sq != 40000.0f &&
                     distance_sq > distance_limit_sq) || !within_group) {
                    continue;
                }
                grouped.insert(unit->uid);
                unit->formation_leader_uid = leader->uid;
                followers.push_back(unit);
                max_radius = std::max(max_radius, unit->radius);
            }
        }

        const i16 group_size = static_cast<i16>(followers.size() + 1);
        leader->formation_group_size = group_size;
        if (formation_rebuild_active_ &&
            cmd.action == GameCommand::ATTACK_MOVE &&
            !leader->order_queue.empty() &&
            leader->order_queue.front().type == WeaponType::ATTACK_MOVE) {
            leader->formation_rebuild_handoff_pending = true;
        }
        const bool hold_replaced_attack_move_leader =
            cmd.action == GameCommand::ATTACK_MOVE &&
            leader->parent_uid == 0 &&
            cmd.group_size >= 19 && !leader->order_queue.empty() &&
            leader->order_queue.front().type == WeaponType::ATTACK_MOVE &&
            command_has_handoff_cache();
        if (hold_replaced_attack_move_leader) {
            // Native replaces an active large-group ATTACK_MOVE on the
            // command frame but does not activate the new leader's path.
            // Keep only its logical order until a later command replaces it.
            leader->has_move_target = false;
            leader->arrived = true;
            leader->target_x = 0.0f;
            leader->target_y = 0.0f;
            leader->using_path = false;
            leader->waypoints.clear();
            leader->current_waypoint = 0;
            leader->has_deferred_navigation_target = false;
            leader->path_target_override_active = false;
            leader->path_request_pending = false;
            leader->path_request_solver_ready = false;
            leader->path_request_needs_worker_handoff = false;
            leader->path_request_prediction_suppressed = false;
            leader->path_request_uses_prepath = false;
            leader->path_request_preserves_active_path = false;
            leader->path_request_cost_snapshot.reset();
            leader->path_request_delay_frames = 0.0f;
            leader->force_grid_path_request = false;
            leader->path_incomplete = false;
            leader->repath_cooldown_frames = 0.0f;
            leader->movement_activation_delay_frames = 0.0f;
            leader->movement_delay_keeps_prediction = false;
            leader->movement_delay_uses_acceleration = false;
        }
        // ab.c() retains this controller for its first group. Later groups
        // get new controllers through aa.b()/ab.a(y), whose f stays false;
        // the waypoint's au.j is not copied into those controllers.
        const u64 controller_id = first_group && inherited_controller_id != 0
            ? inherited_controller_id : next_formation_controller_id_++;
        for (World* owner : other_worlds) {
            if (owner) owner->next_formation_controller_id_ = std::max(
                owner->next_formation_controller_id_, next_formation_controller_id_);
        }
        const auto assign_controller = [&](Unit& member) {
            member.formation_controller_id = controller_id;
            if (!member.order_queue.empty()) {
                if (combine_owners) {
                    member.order_queue.front().formation_group_id =
                        active_command_formation_group_id_;
                }
                member.order_queue.front().formation_controller_relaxed_formation =
                    first_group && cmd.relaxed_formation;
            }
        };
        assign_controller(*leader);
        for (Unit* follower : followers) {
            assign_controller(*follower);
        }
        if (group_size > 1 && leader->path_request_pending &&
            !formation_rebuild_active_ &&
            command_pending_path_uids_.count(leader->uid) == 0) {
            // Eager command activation precedes formation assignment, so a
            // new leader request needs its eventual clearance weighting.
            // ab.c() leaves an existing aU unchanged: a new command or leader
            // removal must preserve that request's original k.p parameter.
            leader->path_request_prefer_clearance = true;
        }
        const bool positional_leader_order =
            cmd.action == GameCommand::MOVE ||
            cmd.action == GameCommand::BUILD;
        World* leader_owner = find_order_target_world_(leader->uid, other_worlds);
        if (positional_leader_order && !formation_rebuild_active_ &&
            !cmd.clear_non_build_repair && !leader->activate_order_next_frame &&
            group_size > 1 &&
            leader->parent_uid == 0 &&
            !leader->order_queue.empty() &&
            leader->order_queue.front().type ==
                static_cast<WeaponType>(cmd.action) &&
            leader->using_path && !leader->path_request_pending &&
            !leader->order_queue.front().has_prepath &&
            !leader_owner->pathfinder_.is_building_blocked_tile(
                leader_owner->pathfinder_.tile_x(leader->x),
                leader_owner->pathfinder_.tile_y(leader->y), leader->move_type) &&
            !leader_owner->pathfinder_.line_path_clear(
                leader->x, leader->y,
                leader->order_queue.front().x,
                leader->order_queue.front().y,
                leader->move_type, 80, 3, 1)) {
            // Replay commands are assembled before y.k() runs. If the newly
            // chosen leader was not yet marked ae && ah>1, the early C++
            // activation probed the line with clearance 0. Repeat
            // that request now with ae && ah>1 established, matching the
            // JAR's clearance-3 probe and prefer-clearance grid fallback.
            const UnitOrder& active = leader->order_queue.front();
            leader->using_path = false;
            leader->waypoints.clear();
            leader->current_waypoint = 0;
            leader_owner->move_unit_to_async_(*leader, active.x, active.y,
                cmd.action == GameCommand::BUILD ? 1 : 0);
            leader->movement_activation_delay_frames = 1.0f;
        }
        // ab rebuilds the follower list from the global object list before
        // passing it to aa. The initial distance-band collection above is
        // only used to establish links; slot assignment observes creation
        // order, not the distance-band order.
        std::stable_sort(
            followers.begin(), followers.end(),
            [](const Unit* lhs, const Unit* rhs) {
                return lhs->object_creation_order < rhs->object_creation_order;
            });
        leader->formation_path_activation_pending =
            !formation_rebuild_active_ && !cmd.clear_non_build_repair &&
            !leader->activate_order_next_frame &&
            (cmd.action == GameCommand::MOVE ||
             cmd.action == GameCommand::ATTACK_MOVE ||
             cmd.action == GameCommand::BUILD) &&
            leader->using_path &&
            !leader->path_request_pending;
        for (Unit* follower : followers) {
            follower->formation_group_size = group_size;
        }
        if (followers.empty()) continue;

        const f32 spacing = 2.0f + max_radius * 2.0f * 1.5f;
        Vec<std::pair<f32, f32>> slots;
        slots.reserve(followers.size());
        i32 slot_index = 1;
        i32 row = 0;
        constexpr i32 columns = 6;
        constexpr i32 center_column = columns / 2;
        i32 even_count = static_cast<i32>(followers.size());
        if ((even_count & 1) != 0) ++even_count;
        for (i32 index = 0; index < even_count; ++index) {
            const i32 column = (slot_index % 2 == 0)
                ? center_column + slot_index / 2
                : center_column - (slot_index + 1) / 2;
            const f32 lateral =
                static_cast<f32>(column - center_column) * spacing;
            const f32 forward = static_cast<f32>(-row) * spacing;
            const f32 offset_x =
                forward * jar_cosine_degrees(heading) -
                lateral * jar_sine_degrees(heading);
            const f32 offset_y =
                lateral * jar_cosine_degrees(heading) +
                forward * jar_sine_degrees(heading);
            slots.push_back({offset_x, offset_y});
            if (++slot_index > columns) {
                slot_index = 0;
                ++row;
            }
        }

        Vec<bool> assigned(followers.size(), false);
        while (!slots.empty()) {
            i32 chosen_follower = -1;
            i32 chosen_slot = -1;
            f32 farthest_nearest = -1.0f;
            for (i32 follower_index = 0;
                 follower_index < static_cast<i32>(followers.size());
                 ++follower_index) {
                if (assigned[follower_index]) continue;
                Unit* follower = followers[follower_index];
                i32 nearest_slot = -1;
                f32 nearest_distance = -1.0f;
                for (i32 slot = 0;
                     slot < static_cast<i32>(slots.size()); ++slot) {
                    const f32 slot_x = leader->x + slots[slot].first;
                    const f32 slot_y = leader->y + slots[slot].second;
                    const f32 distance = dist2(
                        follower->x, follower->y, slot_x, slot_y);
                    if (nearest_slot < 0 || distance < nearest_distance) {
                        nearest_slot = slot;
                        nearest_distance = distance;
                    }
                }
                if (chosen_follower < 0 ||
                    nearest_distance > farthest_nearest) {
                    chosen_follower = follower_index;
                    chosen_slot = nearest_slot;
                    farthest_nearest = nearest_distance;
                }
            }
            if (chosen_follower < 0 || chosen_slot < 0) break;
            Unit* follower = followers[chosen_follower];
            follower->formation_offset_x = slots[chosen_slot].first;
            follower->formation_offset_y = slots[chosen_slot].second;
            follower->formation_heading = heading;
            follower->formation_distance_sq = farthest_nearest;
            assigned[chosen_follower] = true;
            slots.erase(slots.begin() + chosen_slot);
        }

        // aa.a(list, leader) makes a second pass over the assigned slots.
        // It visits the farthest unexamined follower first and swaps offsets
        // only when the sum of the two integer distances strictly decreases.
        // f.c truncates the squared float distance before taking its rounded
        // square root, so keep that quantization in the comparison.
        const auto rounded_formation_distance = [](f32 distance_sq) {
            const i32 truncated = static_cast<i32>(distance_sq);
            return static_cast<i32>(std::lround(std::sqrt(
                static_cast<f32>(truncated))));
        };
        const auto rounded_slot_distance = [&](const Unit& follower,
                                               f32 offset_x,
                                               f32 offset_y) {
            return rounded_formation_distance(dist2(
                follower.x, follower.y,
                leader->x + offset_x, leader->y + offset_y));
        };
        Vec<bool> optimization_candidate(followers.size(), true);
        while (true) {
            i32 chosen_follower = -1;
            for (i32 follower_index = 0;
                 follower_index < static_cast<i32>(followers.size());
                 ++follower_index) {
                Unit* follower = followers[follower_index];
                if (!optimization_candidate[follower_index] ||
                    follower->formation_distance_sq <= 100.0f) {
                    continue;
                }
                if (chosen_follower < 0 ||
                    follower->formation_distance_sq >
                        followers[chosen_follower]->formation_distance_sq) {
                    chosen_follower = follower_index;
                }
            }
            if (chosen_follower < 0) break;

            optimization_candidate[chosen_follower] = false;
            Unit* follower = followers[chosen_follower];
            const i32 follower_distance = rounded_formation_distance(
                follower->formation_distance_sq);
            i32 swap_follower = -1;
            f32 best_delta = 0.0f;
            for (i32 follower_index = 0;
                 follower_index < static_cast<i32>(followers.size());
                 ++follower_index) {
                if (follower_index == chosen_follower) continue;
                Unit* other = followers[follower_index];
                if (other->formation_distance_sq <= 0.0f) continue;

                const i32 current_distance = follower_distance +
                    rounded_formation_distance(other->formation_distance_sq);
                const i32 swapped_distance = rounded_slot_distance(
                    *follower, other->formation_offset_x,
                    other->formation_offset_y) +
                    rounded_slot_distance(
                        *other, follower->formation_offset_x,
                        follower->formation_offset_y);
                const f32 delta = static_cast<f32>(
                    swapped_distance - current_distance);
                if (delta < best_delta) {
                    best_delta = delta;
                    swap_follower = follower_index;
                }
            }
            if (swap_follower < 0) continue;

            Unit* other = followers[swap_follower];
            std::swap(follower->formation_offset_x,
                      other->formation_offset_x);
            std::swap(follower->formation_offset_y,
                      other->formation_offset_y);
            follower->formation_distance_sq = dist2(
                follower->x, follower->y,
                leader->x + follower->formation_offset_x,
                leader->y + follower->formation_offset_y);
            other->formation_distance_sq = dist2(
                other->x, other->y,
                leader->x + other->formation_offset_x,
                leader->y + other->formation_offset_y);
        }

        if (cmd.action == GameCommand::MOVE ||
            cmd.action == GameCommand::ATTACK_MOVE ||
            cmd.action == GameCommand::BUILD) {
            for (Unit* follower : followers) {
                if (follower->order_queue.empty()) continue;
                // ab.c() assigns slots to transported and incomplete members.
                // They cannot publish a projection until an active y.k() turn.
                if (!follower->is_active()) continue;
                const UnitOrder& active = follower->order_queue.front();
                const bool projection_order =
                    (cmd.action == GameCommand::MOVE &&
                     active.type == WeaponType::MOVE) ||
                    (cmd.action == GameCommand::ATTACK_MOVE &&
                     active.type == WeaponType::ATTACK_MOVE) ||
                    (cmd.action == GameCommand::BUILD &&
                     active.type == WeaponType::BUILD);
                if (!projection_order) continue;

                // ab.c() calls aB(), which resets formation links without
                // aH()'s navigation reset. A leader-removal rebuild and
                // e.f/aA() selective pruning retain the surviving av chain
                // and s gate until the object's projection/private dispatch.
                const bool preserve_rebuild_navigation =
                    formation_rebuild_active_ || cmd.clear_non_build_repair;
                if (cmd.action == GameCommand::MOVE &&
                    (follower->formation_rebuild_reverse_coast_candidate ||
                     (formation_rebuild_active_ && follower->using_path &&
                      (follower->reversing ||
                       follower->move_speed_factor < 0.0f)))) {
                    // y.aD()/ab.c() clears the old path during a leader
                    // removal rebuild, but this follower keeps its reverse
                    // coast through the new formation's physical pass.
                    follower->formation_rebuild_reverse_coast_pending = true;
                }

                // The command handler initially visits every selected unit,
                // but ab.c() leaves linked MOVE/ATTACK_MOVE/BUILD followers
                // without a newly submitted private path. They retain the raw order
                // point in y.l/y.m and move through their leader's projected
                // path immediately. A leader-removal rebuild only relinks the
                // surviving units; it does not replay this command-time
                // proposal publication.
                if (!preserve_rebuild_navigation) {
                    follower->using_path = false;
                    follower->waypoints.clear();
                    follower->current_waypoint = 0;
                }
                if (!preserve_rebuild_navigation) {
                    follower->target_x = active.x;
                    follower->target_y = active.y;
                    // e.k()/ab.c() leave k unchanged until this follower's
                    // own y.k() turn dispatches the replay MOVE waypoint.
                    // Its saved command-entry proposal also survives a
                    // completed unit joining the new formation while idle.
                    follower->has_move_target =
                        cmd.action == GameCommand::MOVE &&
                        cmd.replay_prepaths_provided
                            ? follower->command_start_has_move_target : true;
                    follower->arrived = !follower->has_move_target;
                }
                if (!preserve_rebuild_navigation) {
                    follower->path_incomplete = false;
                    follower->has_deferred_navigation_target = false;
                }
                // az()/aH() and formation assignment clear av/aw but leave
                // an older aU alive. Only discard a request created by this
                // command's eager activation, independently of cache size
                // or the member's former leader link.
                const bool preserve_pending_request =
                    follower->path_request_pending &&
                    (formation_rebuild_active_ ||
                     command_pending_path_uids_.count(follower->uid) != 0);
                if (!preserve_pending_request) {
                    follower->path_request_pending = false;
                    follower->path_request_uses_prepath = false;
                    follower->path_request_preserves_active_path = false;
                    follower->path_request_cost_snapshot.reset();
                    follower->path_request_delay_frames = 0.0f;
                }
                if (!preserve_rebuild_navigation) {
                    follower->repath_cooldown_frames = 0.0f;
                    follower->movement_activation_delay_frames = 0.0f;
                    follower->movement_delay_keeps_prediction = false;
                }
                if (cmd.replay_prepaths_provided) {
                    // A fresh replay MOVE follower dispatches on its own
                    // object turn. An older aU keeps its existing handoff;
                    // ATTACK_MOVE retains its unit-specific cache rule.
                    bool matching_replay_cache = false;
                    for (const UnitOrderPrepath& cached :
                         active.replay_prepath_cache) {
                        if (cmd.action == GameCommand::ATTACK_MOVE &&
                            cached.unit_uid != follower->uid) {
                            continue;
                        }
                        if (cmd.action == GameCommand::MOVE &&
                            cmd.group_size >= 8) {
                            continue;
                        }
                        if (cmd.action == GameCommand::MOVE &&
                            cmd.group_size < 8 &&
                            cached.unit_uid != follower->uid) {
                            continue;
                        }
                        if (std::abs(cached.target_x - active.x) > 10.0f ||
                            std::abs(cached.target_y - active.y) > 10.0f ||
                            cached.frame + 180 < tick_ ||
                            cached.move_type != follower->move_type ||
                            cached.path.size() > 2 ||
                            dist2(follower->x, follower->y,
                                  cached.start_x, cached.start_y) >= 3600.0f) {
                            continue;
                        }
                        matching_replay_cache = true;
                        break;
                    }
                    if (cmd.action == GameCommand::MOVE &&
                        !preserve_pending_request) {
                        // An idle member has no command-time k proposal.
                        // Dispatch its matching leader on its own object
                        // turn, including when there is no usable cache.
                        follower->activate_order_next_frame = true;
                    } else if (!matching_replay_cache) {
                        follower->activate_order_next_frame = false;
                    }
                }
            }
        }
    }

    // y.a(y)/aB() changes formation links without changing y.ai. Only the
    // combat-target handler updates that age, so a unit joining from MOVE
    // keeps zero while a prior unlinked chase keeps its 500-frame sentinel.

    for (Unit* member : selected) {
        member->formation_rebuild_reverse_coast_candidate = false;
    }

}

} // namespace rwrl
