#include "world_internal.h"
#include "combat/targeting_helpers.h"

#include <tuple>

namespace rwrl {

bool World::can_target_(const Unit& attacker, const Unit& target) const {
    // LogicBoolean HeightBoolean.flying uses eq > 5, independently of
    // custom.j.i()'s AIR collision boundary at eq >= 4. Combat evaluates it
    // before custom.j advances this object's vertical movement.
    const bool ground_allowed = attacker.targets_ground &&
        (!attacker.ground_attack_disabled_while_flying || attacker.height <= 5.0f);
    return target_layer_allowed(
        *this, attacker, target, attacker.targets_air, ground_allowed,
        attacker.targets_underwater, attacker.targets_not_touching_water);
}

f32 World::engagement_range_(const Unit& unit, bool extended) const {
    f32 range = unit.weapon_range;
    // y.b(boolean) reads ar() even when ax() has just exposed the next
    // waypoint and its navigation state will activate on the following turn.
    // Keep the direct World helpers' implicit orders as the queue-less
    // fallback; a stationary weapon's default GUARD role is not a waypoint.
    const WeaponType order_type = !unit.order_queue.empty()
        ? unit.order_queue.front().type
        : (unit.has_patrol
            ? WeaponType::PATROL
            : (unit.guard_target_uid != 0
                ? WeaponType::GUARD
                : (unit.weapon_type == WeaponType::ATTACK_MOVE
                    ? WeaponType::ATTACK_MOVE : WeaponType::MOVE)));
    if (order_type == WeaponType::ATTACK_MOVE ||
        order_type == WeaponType::PATROL ||
        order_type == WeaponType::GUARD) {
        if (order_type == WeaponType::PATROL) {
            range += 110.0f;
        } else if (order_type == WeaponType::GUARD) {
            range += 90.0f;
        } else {
            range += 20.0f;
        }
        range = std::max(range, 190.0f);
    }

    switch (unit.behavior) {
    case Behavior::OUT_OF_RANGE:
        range += 250.0f;
        break;
    case Behavior::GUARD_AREA:
        range += 150.0f;
        break;
    case Behavior::AGGRESSIVE:
        range += 180.0f;
        break;
    default:
        // Canonical units inherit y.am() == 0. y.b(true) adds 110 only
        // for modes which do not have their own fixed range extension.
        if (extended) range += 110.0f;
        break;
    }
    return range;
}

Unit* World::select_passive_target_(
        const Unit& unit, const Vec<World*>& enemy_worlds, f32 scan_range,
        World*& selected_world) {
    struct Candidate {
        World* world;
        Unit* unit;
    };
    Vec<Candidate> candidates;
    Unit* selected = nullptr;
    selected_world = nullptr;

    // f.c queries the last global geo-index pass. Constructors enter am.bE
    // immediately, but a new unit is unavailable to these callbacks until
    // the index runs after all object turns. A moved unit is filtered by
    // both its saved resident cell and its current position.
    const i32 cell_width = std::max(
        1, static_cast<i32>(map_width() * tile_size()) /
               EngineConstants::SPATIAL_GRID_SIZE);
    const i32 cell_height = std::max(
        1, static_cast<i32>(map_height() * tile_size()) /
               EngineConstants::SPATIAL_GRID_SIZE);
    const f32 inverse_width = 1.0f / static_cast<f32>(cell_width);
    const f32 inverse_height = 1.0f / static_cast<f32>(cell_height);
    const auto cell = [](f32 coordinate, f32 inverse_size) {
        return clamp_i(static_cast<i32>(coordinate * inverse_size),
                       0, EngineConstants::SPATIAL_GRID_SIZE - 1);
    };
    const f32 min_x = unit.x - scan_range;
    const f32 max_x = unit.x + scan_range;
    const f32 min_y = unit.y - scan_range;
    const f32 max_y = unit.y + scan_range;
    const i32 min_cell_x = cell(min_x, inverse_width);
    const i32 max_cell_x = cell(max_x, inverse_width);
    const i32 min_cell_y = cell(min_y, inverse_height);
    const i32 max_cell_y = cell(max_y, inverse_height);
    for (World* candidate_world : enemy_worlds) {
        if (!candidate_world || candidate_world == this ||
            !is_enemy_with(*candidate_world)) {
            continue;
        }
        for (auto& [_, candidate] : candidate_world->units_) {
            if (!candidate.is_alive || candidate.parent_uid != 0 ||
                !can_target_(unit, candidate)) {
                continue;
            }
            // ae.setup() resets the callback's distance, not y.R. With no
            // closer eligible candidate, the previous main target survives.
            if (candidate.uid == unit.target_uid) {
                selected = &candidate;
                selected_world = candidate_world;
            }
            if (candidate.collision_spatial_cell_x < min_cell_x ||
                candidate.collision_spatial_cell_x > max_cell_x ||
                candidate.collision_spatial_cell_y < min_cell_y ||
                candidate.collision_spatial_cell_y > max_cell_y ||
                candidate.x < min_x || candidate.x > max_x ||
                candidate.y < min_y || candidate.y > max_y) {
                continue;
            }
            candidates.push_back({candidate_world, &candidate});
        }
    }
    // onlyEnemiesOfTeam visits teams first, then cells in x/y order, then
    // residents in insertion order. Equal squared distances keep the first
    // callback; UID order and square-root rounding cannot break these ties.
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& lhs, const Candidate& rhs) {
                  return std::tuple(lhs.world->team_id_,
                                    lhs.unit->collision_spatial_cell_x,
                                    lhs.unit->collision_spatial_cell_y,
                                    lhs.unit->collision_spatial_entry_tick,
                                    lhs.unit->object_creation_order) <
                         std::tuple(rhs.world->team_id_,
                                    rhs.unit->collision_spatial_cell_x,
                                    rhs.unit->collision_spatial_cell_y,
                                    rhs.unit->collision_spatial_entry_tick,
                                    rhs.unit->object_creation_order);
              });
    const auto scan_candidates = [&](bool counter_attack_only) {
        f32 selected_distance_sq = scan_range * scan_range + 1.0f;
        for (const Candidate& entry : candidates) {
            Unit& candidate = *entry.unit;
            if (counter_attack_only &&
                (!candidate.can_attack || !can_target_(candidate, unit))) {
                continue;
            }
            const f32 distance_sq = dist2(
                unit.x, unit.y, candidate.x, candidate.y);
            if (distance_sq < selected_distance_sq) {
                selected_distance_sq = distance_sq;
                selected_world = entry.world;
                selected = &candidate;
            }
        }
    };

    scan_candidates(true);
    // aQ counts all compatible rectangle residents before its counter-attack
    // filter. aR runs only when that count is nonzero and h(R) fails its
    // strict squared-distance check against actual weapon range.
    f32 actual_target_range = unit.weapon_range;
    if (selected && unit.attack_range_adds_radii) {
        actual_target_range += unit.radius + selected->radius;
    }
    if (!candidates.empty() &&
        (!selected || dist2(unit.x, unit.y, selected->x, selected->y) >=
                          actual_target_range * actual_target_range)) {
        scan_candidates(false);
    }
    return selected;
}

World* World::order_target_world_(i32 target_team_id,
                                  const Vec<World*>& other_worlds) {
    if (target_team_id == team_id_) return this;
    for (World* other : other_worlds) {
        if (other && other->team_id_ == target_team_id) return other;
    }
    // -1 is also the stock neutral team id. Only fall back to the local world
    // when no neutral world exists and the caller supplied the legacy unknown
    // target sentinel.
    if (target_team_id < 0) return this;
    return nullptr;
}

void World::auto_combat(World& enemy_world) {
    Vec<World*> enemies{&enemy_world};
    auto_combat(enemies);
}

void World::auto_combat(const Vec<World*>& enemy_worlds) {
    auto_combat_(enemy_worlds, 0);
}

void World::update_unit_combat(u32 unit_uid,
                               const Vec<World*>& enemy_worlds) {
    auto_combat_(enemy_worlds, unit_uid);
}

__attribute__((noinline))
void World::publish_single_attack_move_proposal_(
        u32 unit_uid, const Vec<World*>& enemy_worlds) {
    Unit* unit = get_unit(unit_uid);
    const bool short_inactive_formation_leader =
        unit && unit->formation_group_size > 1 &&
        !unit->formation_preferred && !unit->movement_prediction_enabled &&
        remaining_navigation_points(*unit) <= 3;
    const bool attack_move_final_node_coast =
        unit && unit->attack_move_final_node_coast_pending &&
        (unit->movement_activation_delay_frames > 0.0f ||
         !unit->movement_prediction_enabled);
    if (!unit || !unit->is_active() || unit->order_queue.empty() ||
        unit->order_queue.front().type != WeaponType::ATTACK_MOVE ||
        unit->weapon_type != WeaponType::ATTACK_MOVE ||
        !unit->formation_is_leader ||
        (unit->formation_group_size != 1 &&
         !short_inactive_formation_leader &&
         !attack_move_final_node_coast) ||
        unit->formation_leader_uid != 0 || unit->target_uid == 0 ||
        !unit->is_mobile() ||
        (!unit->using_path && !attack_move_final_node_coast) ||
        unit->path_request_pending ||
        (unit->movement_activation_delay_frames > 0.0f &&
         !attack_move_final_node_coast)) {
        return;
    }

    World* target_world = order_target_world_(
        unit->target_team_id, enemy_worlds);
    Unit* target = target_world
        ? target_world->get_unit(unit->target_uid) : nullptr;
    const f32 main_target_range = engagement_range_(*unit);
    if (!target || !target->is_alive || target->parent_uid != 0 ||
        target_world == this || !is_enemy_with(*target_world) ||
        unit->behavior == Behavior::HOLD_FIRE ||
        unit->behavior == Behavior::RETURN_FIRE ||
        !can_target_(*unit, *target) ||
        dist2(unit->x, unit->y, target->x, target->y) >=
            main_target_range * main_target_range) {
        return;
    }

    const f32 distance_sq = dist2(
        unit->x, unit->y, target->x, target->y);
    const f32 follow_range = attack_follow_stop_range(*unit, *target);
    if (!unit->bomber_attack_movement &&
        distance_sq < follow_range * follow_range &&
        !attack_target_needs_approach(*this, *unit, *target)) return;

    const bool proposal_already_active = unit->attack_move_combat_target_valid;
    unit->attack_move_combat_target_valid = true;
    if (!unit->bomber_attack_movement || !proposal_already_active) {
        const auto [attack_x, attack_y] = bomber_attack_target(
            *unit, distance_sq, target->x, target->y);
        unit->attack_move_combat_target_x = attack_x;
        unit->attack_move_combat_target_y = attack_y;
    }
    unit->path_request_target_size = attack_path_target_size(*unit, target);
    if (!proposal_already_active) {
        // A proposal already selected by the order pass has also charged
        // or capped s there. Publishing it again must not freeze its timer
        // or cap a new direct-line request's 500-frame charge afterwards.
        unit->repath_cooldown_frames = std::min(
            unit->repath_cooldown_frames, 90.0f);
        unit->repath_cooldown_order_start = -1.0f;
    }

    if (!unit->path_target_override_active) {
        unit->path_target_override_active = true;
        unit->path_target_override_x = unit->target_x;
        unit->path_target_override_y = unit->target_y;
    }
    unit->target_x = unit->attack_move_combat_target_x;
    unit->target_y = unit->attack_move_combat_target_y;
    unit->has_move_target = true;
    unit->arrived = false;
}

void World::auto_combat_(const Vec<World*>& enemy_worlds,
                         u32 only_unit_uid) {
    for (auto unit_update_it = only_unit_uid == 0
             ? units_.begin() : units_.find(only_unit_uid);
         unit_update_it != units_.end();
         unit_update_it = only_unit_uid == 0
             ? std::next(unit_update_it) : units_.end()) {
        auto& [uid, unit] = *unit_update_it;
        if (!unit.is_active_for_update(tick_)) continue;
        if (!unit.can_attack) continue;
        if (!has_weapon(unit.weapon_type) && unit.weapon_type != WeaponType::TOUCH_TARGET) continue;
        for (WeaponSlot& slot : unit.weapon_slots) {
            slot.aim_ready = false;
            slot.warmup_hold_this_frame = false;
        }
        const auto decay_warmup = [&]() {
            // y.b(l,delta) uses one hold for every ap.f, including the
            // nano slot. Its previous aN also holds ordinary gun warmup
            // while the current waypoint still exposes a live X() target.
            bool hold = false;
            if (unit.service_active_previous_frame &&
                unit.service_active_this_frame &&
                unit.service_aim_target_uid != 0) {
                World* service_world = find_order_target_world_(
                    unit.service_aim_target_uid, peer_worlds_);
                const Unit* service_target = service_world
                    ? service_world->get_unit(unit.service_aim_target_uid) : nullptr;
                hold = service_target && service_target->is_alive &&
                    !service_target->is_dead;
            }
            for (const WeaponSlot& slot : unit.weapon_slots) {
                hold = hold || slot.warmup_hold_this_frame;
            }
            if (hold) return;
            for (WeaponSlot& slot : unit.weapon_slots) {
                if (slot.warmup_progress <= 0.0f) continue;
                const f32 decay = std::max(0.0f, slot.warmup_call_down_rate) *
                    std::max(0.0f, combat_delta_frames_);
                slot.warmup_progress = std::max(
                    0.0f, slot.warmup_progress - decay);
            }
        };
        // y.b(engine, delta) runs after the active waypoint behavior. This is
        // observably later than ATTACK's S=10 refresh in y.k(delta).
        unit.target_scan_timer_frames = std::max(
            0.0f, unit.target_scan_timer_frames - combat_delta_frames_);
        unit.target_refresh_timer_frames = std::max(
            0.0f, unit.target_refresh_timer_frames - combat_delta_frames_);
        const bool passive_targeting_enabled =
            unit.behavior != Behavior::HOLD_FIRE &&
            unit.behavior != Behavior::RETURN_FIRE;
        Unit* current_main_target = nullptr;
        if (unit.target_uid != 0) {
            World* current_main_world = nullptr;
            for (World* candidate_world : enemy_worlds) {
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

            // y.a(am,boolean) checks the unit-target overload am.d(am).
            // Fog visibility uses am.d(n) and does not gate simulation combat.
            bool compatible = passive_targeting_enabled &&
                              current_main_target != nullptr &&
                              can_target_(unit, *current_main_target);
            if (compatible) {
                // y.a(am,false) compares squared floats directly. Taking a
                // square root can retain R for an extra turn at the edge.
                const f32 range = engagement_range_(unit);
                if (dist2(unit.x, unit.y,
                          current_main_target->x, current_main_target->y) >=
                    range * range) {
                    compatible = false;
                }
            }
            if (!compatible) {
                if (unit.attack_move_combat_target_valid &&
                    !unit.order_queue.empty() &&
                    unit.order_queue.front().type == WeaponType::ATTACK_MOVE) {
                    // y.b() can invalidate R after ATTACK_MOVE's order phase
                    // selected its live chase target. The failed combat
                    // proposal must not leak into either the exposed y.l/m
                    // fields or the split physical movement pass.
                    unit.attack_move_combat_target_valid = false;
                    unit.direct_attack_chase_this_update = false;
                    unit.has_deferred_navigation_target = false;
                    unit.repath_cooldown_frames = 0.0f;
                    // ATTACK_MOVE keeps the leader link's projection gate
                    // closed when the leader still has a live main target,
                    // even if this follower's own chase proposal is rejected
                    // by the later combat pass.
                    const bool attack_move_leader_target_holds_gate =
                        !unit.order_queue.empty() &&
                        unit.order_queue.front().type == WeaponType::ATTACK_MOVE &&
                        unit.formation_leader_uid != 0 &&
                        formation_unit_(unit.formation_leader_uid) != nullptr &&
                        formation_unit_(unit.formation_leader_uid)->target_uid != 0;
                    if (!attack_move_leader_target_holds_gate) {
                        unit.formation_projection_allowed = true;
                    }
                    unit.last_waypoint_stall_frames =
                        unit.waypoint_stall_frames;
                    unit.waypoint_stall_frames = 0.0f;
                    unit.path_request_target_size = 0;
                }
                if (unit.guard_target_uid != 0) {
                    World* guarded_world = order_target_world_(
                        unit.guard_target_team_id, enemy_worlds);
                    Unit* guarded = guarded_world
                        ? guarded_world->get_unit(unit.guard_target_uid)
                        : nullptr;
                    if (guarded && guarded->is_alive &&
                        guarded->parent_uid == 0) {
                            const f32 follow_range =
                                unit.radius + guarded->radius +
                                (unit.movement_prediction_enabled
                                     ? 80.0f : 100.0f);
                        if (unit.distance_to(*guarded) > follow_range) {
                            // The split order pass accepted R even though the
                            // later y.b()-equivalent rejects it. Restore the
                            // GUARD proposal that y.c() would have retained,
                            // whether R had requested a chase or suppressed k
                            // from inside weapon range.
                            const bool guarded_recently_attacked =
                                current_main_target != nullptr &&
                                guarded->was_attacked &&
                                guarded->last_attacker_uid ==
                                    current_main_target->uid &&
                                guarded->last_damage_age_ms >= 0.0f &&
                                guarded->last_damage_age_ms < 2000.0f;
                            const bool unit_recently_attacked =
                                current_main_target != nullptr &&
                                unit.last_attacker_uid ==
                                    current_main_target->uid &&
                                unit.last_damage_age_ms >= 0.0f &&
                                unit.last_damage_age_ms < 2000.0f;
                            // c(GUARD) may already have selected q(2), whose
                            // attacker is independent of the stale R cleared
                            // here. Keep that same-frame l/m proposal.
                            const bool retained_combat_proposal =
                                (unit.recent_order_chase ||
                                 (current_main_target != nullptr &&
                                  (guarded_recently_attacked ||
                                   unit_recently_attacked))) &&
                                unit.guard_movement_target_valid &&
                                unit.guard_movement_chases_main_target &&
                                unit.guard_movement_target_uid != 0 &&
                                unit.guard_movement_target_uid !=
                                    unit.guard_target_uid;
                            const bool large_replay_guard_path =
                                !unit.order_queue.empty() &&
                                unit.order_queue.front().type ==
                                    WeaponType::GUARD &&
                                unit.order_queue.front().replay_command_group_size >=
                                    19 &&
                                unit.formation_leader_uid != 0 &&
                                unit.formation_group_size > 1 &&
                                unit.movement_prediction_enabled &&
                                unit.using_path &&
                                remaining_navigation_points(unit) >= 17 &&
                                unit.distance_to(*guarded) > 500.0f;
                            if (!retained_combat_proposal) {
                                unit.guard_movement_target_hold = false;
                                unit.guard_movement_target_valid = true;
                                unit.guard_movement_chases_main_target = false;
                                unit.guard_movement_target_x = guarded->x;
                                unit.guard_movement_target_y = guarded->y;
                                if (large_replay_guard_path) {
                                    for (const PositionSnapshot& snapshot :
                                         guarded->position_history) {
                                        if (snapshot.tick == tick_ - 1) {
                                            unit.guard_movement_target_x =
                                                snapshot.x;
                                            unit.guard_movement_target_y =
                                                snapshot.y;
                                            break;
                                        }
                                    }
                                }
                                unit.guard_movement_target_uid =
                                    unit.guard_target_uid;
                                unit.guard_movement_target_team_id =
                                    unit.guard_target_team_id;
                            } else {
                                unit.guard_movement_target_hold = true;
                            }
                        }
                    }
                }
                    unit.target_uid = 0;
                    unit.target_team_id = -1;
                    current_main_target = nullptr;
                    const bool deferred_formation_attack_move_handoff =
                        unit.formation_rebuild_handoff_pending &&
                        !unit.order_queue.empty() &&
                        unit.order_queue.front().type ==
                            WeaponType::ATTACK_MOVE &&
                        unit.is_mobile() && !unit.using_path &&
                        !unit.path_request_pending;
                    const bool same_turn_attack_move_handoff =
                        !unit.order_queue.empty() &&
                        unit.order_queue.front().type == WeaponType::ATTACK_MOVE &&
                        ((deferred_formation_attack_move_handoff &&
                          (!unit.formation_is_leader ||
                           unit.repath_cooldown_frames == 0.0f ||
                           (unit.path_incomplete &&
                            unit.repath_cooldown_frames < 450.0f))) ||
                         (unit.formation_leader_uid != 0 &&
                          !unit.formation_projection_allowed &&
                          unit.is_mobile() && unit.has_move_target &&
                          unit.repath_cooldown_frames == 0.0f)) &&
                        !unit.using_path && !unit.path_request_pending;
                    if (same_turn_attack_move_handoff) {
                        // y.k() clears the invalid live target and still lets
                        // the MOVE-family handler submit the order-point path
                        // before this object's physical movement pass.
                        const UnitOrder& active = unit.order_queue.front();
                        const bool rebuild_follower_proposal =
                            deferred_formation_attack_move_handoff &&
                            !unit.formation_is_leader;
                        if (rebuild_follower_proposal) {
                            unit.target_x = active.x;
                            unit.target_y = active.y;
                            unit.has_move_target = true;
                            unit.arrived = false;
                            // The rebuilt follower exposes the replacement
                            // order point before its next physical turn. JAR
                            // keeps cK false for that handoff frame.
                            if (!unit.formation_projection_allowed) {
                                unit.movement_activation_delay_frames = 1.0f;
                                unit.movement_delay_keeps_prediction = false;
                            }
                        } else {
                            move_unit_to_async_(unit, active.x, active.y);
                            unit.movement_activation_delay_frames = 1.0f;
                            unit.movement_delay_keeps_prediction = false;
                        }
                    }
                    // The rebuilt link has handed off R once. A leader with
                    // positive s keeps waiting for the ordinary path request.
                    if (deferred_formation_attack_move_handoff) {
                        unit.formation_rebuild_handoff_pending = false;
                    }
                } else {
                unit.target_team_id = current_main_world->team_id_;
            }
        }

        u32 main_target_uid = current_main_target ? unit.target_uid : 0;
        // Both holdFire and returnFire fail y.a(target, false), which clears
        // an existing passive main target. They do not run a replacement
        // scan or rewrite weapon-slot targets.
        const f32 passive_scan_interval =
            20.0f + std::fmod(unit.x, 5.0f) + std::fmod(unit.y, 5.0f);
        // custom.j.bf() gates the scan and its S rearm, independently of
        // target validation and T-driven turret refresh. Bombers keep S at
        // zero while idle or on ordinary orders, so a later attackMove or
        // patrol can scan immediately instead of inheriting an idle period.
        const bool bomber_scan_order = !unit.order_queue.empty() &&
            (unit.order_queue.front().type == WeaponType::ATTACK_MOVE ||
             unit.order_queue.front().type == WeaponType::PATROL);
        const bool passive_scan_allowed = !unit.custom_unit ||
            (!unit.disable_passive_targeting &&
             (!unit.bomber_attack_movement || bomber_scan_order ||
              unit.behavior == Behavior::OUT_OF_RANGE));
        if (!passive_targeting_enabled) {
            // y.b(engine,delta) still runs bf() and rearms S when the
            // scan is due. The callback rejects every candidate in these
            // modes, but the timer phase survives a later behavior change.
            if (passive_scan_allowed && unit.target_scan_timer_frames == 0.0f) {
                unit.target_scan_timer_frames = passive_scan_interval;
            }
            decay_warmup();
            continue;
        }

        f32 main_target_range = unit.weapon_range;
        if (current_main_target && unit.attack_range_adds_radii) {
            main_target_range += unit.radius + current_main_target->radius;
        }
        const bool main_target_out_of_weapon_range =
            current_main_target &&
            dist2(unit.x, unit.y,
                  current_main_target->x, current_main_target->y) >=
                main_target_range * main_target_range;
        if (main_target_uid == 0 || main_target_out_of_weapon_range) {
            if (!passive_scan_allowed || unit.target_scan_timer_frames > 0.0f) {
                if (main_target_uid == 0) {
                    // y.b() still reaches its final warmup decay when R is
                    // null. No slot aiming/firing is attempted in this
                    // branch, but every non-held ap.f continues to cool.
                    decay_warmup();
                    continue;
                }
            } else {
                unit.target_scan_timer_frames = passive_scan_interval;

                World* selected_world = nullptr;
                Unit* selected = select_passive_target_(
                    unit, enemy_worlds, engagement_range_(unit),
                    selected_world);
                if (selected && selected_world) {
                    unit.target_uid = selected->uid;
                    unit.target_team_id = selected_world->team_id_;
                    current_main_target = selected;
                    main_target_uid = selected->uid;
                    unit.target_refresh_timer_frames = 0.0f;
                } else if (main_target_uid == 0) {
                    // y.b() still reaches its final warmup decay after a
                    // scan that leaves R null. The scan timer is rearmed
                    // above, but no slot aiming/firing is attempted here.
                    decay_warmup();
                    continue;
                }
            }
        }

        // GUARD already applied drift handling when its order phase produced
        // a movement proposal. Only a target first acquired by this later
        // passive scan needs the post-scan check.
        if (!unit.guard_movement_target_valid && current_main_target &&
            unit.guard_target_uid != 0 &&
            unit.is_mobile() && unit.has_move_target) {
            f32 planned_x = unit.target_x;
            f32 planned_y = unit.target_y;
            if (!unit.waypoints.empty()) {
                planned_x = unit.waypoints.back().first;
                planned_y = unit.waypoints.back().second;
            }
            const bool target_drifted =
                std::abs(planned_x - current_main_target->x) > 18.0f ||
                std::abs(planned_y - current_main_target->y) > 18.0f;
            if (target_drifted && unit.repath_cooldown_frames > 30.0f) {
                unit.repath_cooldown_frames = 30.0f;
            }
        }

        const bool refresh_independent_targets =
            unit.target_refresh_timer_frames <= 0.0f;
        if (refresh_independent_targets) {
            unit.target_refresh_timer_frames = passive_scan_interval;
        }

        if (!unit.weapon_slots.empty()) {
            // custom.j.ac() enables independent acquisition only for
            // turretMultiTargeting. Otherwise y.a(engine,delta) assigns R
            // to every slot; the firing pass then clears incompatible ones.
            const bool independent_targeting = unit.weapon_slots.size() > 1 &&
                (!unit.custom_unit || unit.turret_multi_targeting);
            bool fired_any = false;
            Unit* movement_target = current_main_target;
            const f32 passive_movement_range = unit.weapon_range +
                (movement_target && unit.attack_range_adds_radii
                     ? unit.radius + movement_target->radius : 0.0f);
            const bool passive_movement_target_out_of_range =
                movement_target &&
                dist2(unit.x, unit.y, movement_target->x, movement_target->y) >=
                    passive_movement_range * passive_movement_range;
            Vec<std::pair<World*, u32>> killed;
            Vec<std::pair<World*, u32>> slot_targets(
                unit.weapon_slots.size(), {nullptr, 0});
            f32 firing_main_range = unit.weapon_range;
            if (current_main_target && unit.attack_range_adds_radii) {
                firing_main_range += unit.radius + current_main_target->radius;
            }
            // y.b() gates the whole turret aim/fire pass on R being inside
            // o(R). A retained independent target inside range cannot open
            // that pass while the main target is outside it.
            const bool main_target_in_firing_range = current_main_target &&
                dist2(unit.x, unit.y, current_main_target->x,
                      current_main_target->y) <
                    firing_main_range * firing_main_range;

            const f32 scan_range = engagement_range_(unit);
            const i32 cell_width = std::max(
                1, static_cast<i32>(map_width() * tile_size()) /
                       EngineConstants::SPATIAL_GRID_SIZE);
            const i32 cell_height = std::max(
                1, static_cast<i32>(map_height() * tile_size()) /
                       EngineConstants::SPATIAL_GRID_SIZE);
            const auto scan_cell = [](f32 coordinate, i32 size) {
                return clamp_i(static_cast<i32>(
                    coordinate * (1.0f / static_cast<f32>(size))),
                    0, EngineConstants::SPATIAL_GRID_SIZE - 1);
            };
            const i32 min_cell_x = scan_cell(unit.x - scan_range, cell_width);
            const i32 max_cell_x = scan_cell(unit.x + scan_range, cell_width);
            const i32 min_cell_y = scan_cell(unit.y - scan_range, cell_height);
            const i32 max_cell_y = scan_cell(unit.y + scan_range, cell_height);

            for (WeaponSlot& slot : unit.weapon_slots) {
                const bool linked = slot.linked_slot >= 0 &&
                    slot.linked_slot < static_cast<i32>(unit.weapon_slots.size());
                if (refresh_independent_targets && linked) continue;
                World* slot_world = nullptr;
                Unit* slot_target = nullptr;
                f32 nearest = std::numeric_limits<f32>::max();

                auto consider = [&](World* candidate_world, Unit& candidate) {
                    if (!candidate.is_alive || candidate.parent_uid != 0 ||
                        candidate_world == this) return;
                    if (!can_target_(unit, candidate)) return;
                    if (!within_turret_limiting_angle(unit, slot, candidate)) return;
                    if (candidate.collision_spatial_cell_x < min_cell_x ||
                        candidate.collision_spatial_cell_x > max_cell_x ||
                        candidate.collision_spatial_cell_y < min_cell_y ||
                        candidate.collision_spatial_cell_y > max_cell_y ||
                        candidate.x < unit.x - scan_range ||
                        candidate.x > unit.x + scan_range ||
                        candidate.y < unit.y - scan_range ||
                        candidate.y > unit.y + scan_range) return;
                    if (!target_layer_allowed(
                            *this, unit, candidate,
                            slot.targets_air, slot.targets_ground,
                            slot.targets_underwater, slot.targets_not_touching_water)) return;
                    const f32 candidate_distance_sq = dist2(
                        unit.x, unit.y, candidate.x, candidate.y);
                    const f32 turret_scan_range = std::min(
                        scan_range, slot.limiting_range);
                    if (candidate_distance_sq >=
                        turret_scan_range * turret_scan_range + 1.0f) return;
                    const auto resident_order = [](World* world, const Unit& resident) {
                        return std::tuple(world->team_id_,
                            resident.collision_spatial_cell_x,
                            resident.collision_spatial_cell_y,
                            resident.collision_spatial_entry_tick,
                            resident.object_creation_order);
                    };
                    if (candidate_distance_sq < nearest ||
                        (candidate_distance_sq == nearest &&
                         (!slot_target || resident_order(candidate_world, candidate) <
                                          resident_order(slot_world, *slot_target)))) {
                        nearest = candidate_distance_sq;
                        slot_world = candidate_world;
                        slot_target = &candidate;
                    }
                };

                auto retain_existing = [&](World* candidate_world,
                                           Unit& candidate) {
                    // a(engine,delta) retains an incompatible R's previous
                    // independent pointer until b(engine,delta) validates it.
                    // Even a dead pointer prevents this scan from replacing
                    // the slot; clearing it earlier changes the next shot.
                    slot_world = candidate_world;
                    slot_target = &candidate;
                    nearest = dist2(unit.x, unit.y, candidate.x, candidate.y);
                };

                // y.a(engine, delta) only copies the main target to weapon
                // slots when T reaches zero. Between refreshes, a slot keeps
                // its existing valid target.
                bool main_target_compatible = false;
                if (refresh_independent_targets && main_target_uid != 0) {
                    for (World* candidate_world : enemy_worlds) {
                        if (!candidate_world || candidate_world == this ||
                            !is_enemy_with(*candidate_world)) continue;
                        Unit* designated = candidate_world->get_unit(main_target_uid);
                        if (designated && designated->is_alive &&
                            (!independent_targeting ||
                             (turret_target_allowed(unit, slot, *designated) &&
                              target_layer_allowed(
                                *this, unit, *designated,
                                slot.targets_air, slot.targets_ground,
                                slot.targets_underwater, slot.targets_not_touching_water)))) {
                            main_target_compatible = true;
                            slot_world = candidate_world;
                            slot_target = designated;
                            nearest = dist2(unit.x, unit.y, designated->x, designated->y);
                            break;
                        }
                    }
                }

                const bool previous_blocks_scan = slot.target_uid != 0 &&
                    (!refresh_independent_targets || slot.target_uid != main_target_uid);
                if (!slot_target && previous_blocks_scan) {
                    for (World* candidate_world : enemy_worlds) {
                        if (!candidate_world || candidate_world == this ||
                            !is_enemy_with(*candidate_world) ||
                            candidate_world->team_id_ != slot.target_team_id) continue;
                        Unit* previous = candidate_world->get_unit(slot.target_uid);
                        if (previous) retain_existing(candidate_world, *previous);
                        break;
                    }
                }

                // The JAR's PassiveTargetCallback scans only empty,
                // independently-aimed slots whose main target is
                // incompatible. Existing independent targets are retained.
                if (!slot_target && refresh_independent_targets &&
                    independent_targeting && main_target_uid != 0 &&
                    !main_target_compatible && !previous_blocks_scan && !linked) {
                    for (World* candidate_world : enemy_worlds) {
                        if (!candidate_world || candidate_world == this ||
                            !is_enemy_with(*candidate_world)) continue;
                        for (auto& [_, candidate] : candidate_world->units_) {
                            consider(candidate_world, candidate);
                        }
                    }
                }

                if (!slot_target) {
                    slot.target_uid = 0;
                    slot.target_team_id = -1;
                    continue;
                }
                if (slot.target_uid != slot_target->uid ||
                    slot.target_team_id != slot_world->team_id_) {
                    unit.weapon_target_acquired_this_frame = true;
                }
                slot.target_uid = slot_target->uid;
                slot.target_team_id = slot_world->team_id_;
                slot_targets[&slot - unit.weapon_slots.data()] =
                    {slot_world, slot_target->uid};
            }

            // v(i) children receive their parent's pointer after all
            // independent assignments, even if they precede it in the INI.
            if (refresh_independent_targets) {
                for (std::size_t i = 0; i < unit.weapon_slots.size(); ++i) {
                    WeaponSlot& slot = unit.weapon_slots[i];
                    if (slot.linked_slot < 0 || slot.linked_slot >=
                        static_cast<i32>(unit.weapon_slots.size())) continue;
                    const WeaponSlot& parent = unit.weapon_slots[slot.linked_slot];
                    slot.target_uid = parent.target_uid;
                    slot.target_team_id = parent.target_team_id;
                    slot_targets[i] = slot_targets[slot.linked_slot];
                }
            }

            for (std::size_t i = 0; i < unit.weapon_slots.size(); ++i) {
                if (!main_target_in_firing_range) continue;
                WeaponSlot& slot = unit.weapon_slots[i];
                const auto [slot_world, target_uid] = slot_targets[i];
                Unit* target = slot_world ? slot_world->get_unit(target_uid) : nullptr;
                if (!target) continue;
                const bool independent = target->uid != main_target_uid;
                if (!target->is_alive || target->parent_uid != 0 ||
                    (independent && !can_target_(unit, *target)) ||
                    !turret_target_allowed(unit, slot, *target, independent) ||
                    !target_layer_allowed(*this, unit, *target, slot.targets_air,
                        slot.targets_ground, slot.targets_underwater,
                        slot.targets_not_touching_water)) {
                    slot.target_uid = 0;
                    slot.target_team_id = -1;
                    slot_targets[i] = {nullptr, 0};
                    continue;
                }
                aim_weapon_slot_(unit, slot, *target);
            }

            // y.b() finishes all turret aim flags before evaluating u(i),
            // warmup, reloads, or shots in the original slot order.
            for (std::size_t i = 0; i < unit.weapon_slots.size(); ++i) {
                WeaponSlot& slot = unit.weapon_slots[i];
                const auto [slot_world, target_uid] = slot_targets[i];
                Unit* slot_target = slot_world
                    ? slot_world->get_unit(target_uid) : nullptr;
                if (!slot_target || !main_target_in_firing_range) continue;

                if (fire_weapon_slot_(
                        unit, slot, *slot_target, current_main_target)) {
                    fired_any = true;
                    if (slot_target->is_dead) {
                        killed.push_back({slot_world, slot_target->uid});
                    }
                }
            }

            // y.b() uses one unit-level hold flag. It is set by any u(i) slot
            // whose ap.e was zero before a(f, target, i), then controls decay
            // for every slot, including linked children and untargeted slots.
            decay_warmup();

            for (const auto& [target_world, target_uid] : killed) {
                if (!target_world) continue;
                Unit* target = target_world->get_unit(target_uid);
                if (!target || target->cleanup_done) continue;
                kills_++;
                target_world->losses_++;
                target_world->remove_unit(target_uid);
            }
            // Final MOVE-node consumption is planned before combat in the
            // split update. Native b(engine,delta) still sees its dispatch
            // k, so passive chasing waits until the next object turn.
            if (fired_any) {
                unit.cooldown_ticks = unit.weapon_slots.front().cooldown_ticks;
                if (unit.ammo_max > 1) unit.ammo--;
            } else if (movement_target && unit.is_mobile() &&
                       !unit.clear_exposed_move_next_frame &&
                       (!unit.has_move_target || unit.order_queue.empty()) &&
                       unit.behavior != Behavior::ONLY_IN_RANGE &&
                       unit.behavior != Behavior::RETURN_FIRE &&
                       unit.behavior != Behavior::HOLD_FIRE &&
                       unit.behavior != Behavior::MIXED &&
                       passive_movement_target_out_of_range) {
                const bool passive_attack_order =
                    !unit.order_queue.empty() &&
                    unit.order_queue.front().type == WeaponType::ATTACK;
                if (unit.order_queue.empty() || passive_attack_order) {
                    // y.b(engine,delta) publishes j/k/l/m and n=0 for an
                    // out-of-range passive target. The generic controller
                    // captures the old av node before making a request; a
                    // newly created path cannot steer this object turn.
                    const bool had_active_path = unit.using_path;
                    if (unit.order_queue.empty() &&
                        !had_active_path && !unit.path_request_pending &&
                        unit.repath_cooldown_frames == 0.0f) {
                        move_unit_to_async_(unit, movement_target->x,
                                            movement_target->y, 0);
                    }
                    if (unit.using_path && !unit.path_target_override_active) {
                        unit.path_target_override_active = true;
                        unit.path_target_override_x = unit.target_x;
                        unit.path_target_override_y = unit.target_y;
                    }
                    if (!had_active_path) {
                        unit.has_deferred_navigation_target = true;
                        unit.deferred_navigation_target_x = movement_target->x;
                        unit.deferred_navigation_target_y = movement_target->y;
                    }
                    unit.direct_attack_chase_this_update = true;
                    // The live proposal's n is independent of the allowance
                    // already captured by any pending path worker.
                    unit.path_request_target_size = 0;
                    unit.target_x = movement_target->x;
                    unit.target_y = movement_target->y;
                    if (passive_attack_order) {
                        // g(ATTACK) stopped asserting k for its waypoint;
                        // y.b() replaces l/m with R before generic movement.
                        unit.attack_order_target_x = movement_target->x;
                        unit.attack_order_target_y = movement_target->y;
                    }
                    unit.has_move_target = true;
                    unit.arrived = false;
                    continue;
                }
                // JAR followers navigate toward the target's original
                // coordinates and rely on per-frame range checks + solver
                // target_size to stop early; no 0.8 interpolation.
                const i32 ts =
                    attack_path_target_size(unit, movement_target);
                if (unit.formation_leader_uid == 0) {
                    // y.a() records the 500-frame no-formation sentinel even
                    // when this passive combat chase is later replaced by a
                    // MOVE or PATROL formation command.
                    unit.formation_projection_disabled_frames = 500.0f;
                }
                move_unit_to(unit, movement_target->x, movement_target->y, ts);
            }
            continue;
        }

        bool air_only = (unit.weapon_type == WeaponType::GUARD && !unit.targets_ground);
        World* enemy_world = nullptr;
        Unit* enemy = nullptr;
        f32 nearest_dist = std::numeric_limits<f32>::max();
        auto nearest_target_in = [&](World* candidate_world) -> Unit* {
            const f32 search_range = engagement_range_(unit);
            Unit* found = nullptr;
            f32 found_distance = std::numeric_limits<f32>::max();
            for (u32 candidate_uid : candidate_world->grid_.query_circle(
                     unit.x, unit.y, search_range)) {
                Unit* candidate = candidate_world->get_unit(candidate_uid);
                if (!candidate || !candidate->is_alive || candidate->parent_uid != 0) continue;
                if (air_only && !candidate->airborne) continue;
                if (!can_target_(unit, *candidate)) continue;
                const f32 candidate_distance = unit.distance_to(*candidate);
                if (candidate_distance < found_distance ||
                    (candidate_distance == found_distance &&
                     (!found || candidate->uid < found->uid))) {
                    found_distance = candidate_distance;
                    found = candidate;
                }
            }
            return found;
        };
        for (World* candidate_world : enemy_worlds) {
            if (!candidate_world || candidate_world == this ||
                !is_enemy_with(*candidate_world)) continue;
            if (main_target_uid != 0) {
                Unit* designated = candidate_world->get_unit(main_target_uid);
                if (designated && designated->is_alive && designated->parent_uid == 0 &&
                    can_target_(unit, *designated)) {
                    enemy_world = candidate_world;
                    enemy = designated;
                    break;
                }
                continue;
            }
            Unit* candidate = nearest_target_in(candidate_world);
            if (!candidate) continue;
            const f32 candidate_dist = unit.distance_to(*candidate);
            if (candidate_dist < nearest_dist ||
                (candidate_dist == nearest_dist &&
                 (!enemy || candidate->uid < enemy->uid))) {
                nearest_dist = candidate_dist;
                enemy_world = candidate_world;
                enemy = candidate;
            }
        }
        if (!enemy || !enemy->is_alive || enemy->parent_uid != 0) continue;

        if (!can_target_(unit, *enemy)) continue;

        f32 d = unit.distance_to(*enemy);

        if (unit.is_melee) {
            if (d >= unit.radius + enemy->radius) {
                if (unit.is_mobile() && !unit.has_move_target) {
                    move_unit_to(unit, enemy->x, enemy->y);
                }
                continue;
            }
            f32 dmg = unit.damage * unit.shoot_damage_multiplier;
            if (enemy->is_building) dmg *= unit.building_damage_mult;
            enemy->apply_damage(dmg, unit.uid, tick_, unit.shield_mult, unit.shield_absorb, unit.hp_mult);
            if (enemy->is_dead) {
                kills_++;
                enemy_world->losses_++;
                enemy_world->remove_unit(enemy->uid);
            }
            set_custom_reload(unit.cooldown_max, unit.shoot_delay_multiplier,
                              unit.cooldown_ticks,
                              unit.cooldown_remainder);
            if (unit.ammo_max > 1) unit.ammo--;
            if (unit.energy_per_shot > 0 && unit.energy >= unit.energy_per_shot) {
                unit.energy -= unit.energy_per_shot;
            }
            continue;
        }

        if (d <= unit.weapon_range + enemy->radius) {
            bool fired = false;
            fired = unit_attack(unit, *enemy);
            if (fired && enemy->is_dead) {
                kills_++;
                enemy_world->losses_++;
                enemy_world->remove_unit(enemy->uid);
            }
        } else if (unit.is_mobile() && !unit.has_move_target &&
                   unit.behavior != Behavior::ONLY_IN_RANGE &&
                   unit.behavior != Behavior::RETURN_FIRE &&
                   unit.behavior != Behavior::HOLD_FIRE &&
                   unit.behavior != Behavior::MIXED) {
            const i32 ts = attack_path_target_size(unit, enemy);
            if (unit.formation_leader_uid == 0) {
                // Keep the same y.ai sentinel for the no-weapon-slot combat
                // chase path.
                unit.formation_projection_disabled_frames = 500.0f;
            }
            move_unit_to(unit, enemy->x, enemy->y, ts);
        }
    }
}


} // namespace rwrl
