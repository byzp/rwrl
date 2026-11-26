#include "world_internal.h"
#include "combat/targeting_helpers.h"

namespace rwrl {
namespace {

void apply_projectile_wobble(Projectile& projectile, f32 age_frames,
                             f32 delta_frames) {
    if (projectile.wobble_amplitude == 0.0f) return;
    const f32 phase_degrees = std::fmod(
        age_frames * 360.0f / projectile.wobble_frequency_frames +
        360.0f * projectile.random_phase, 360.0f);
    const f32 offset = jar_sine_degrees(phase_degrees) *
                       projectile.wobble_amplitude * delta_frames;
    projectile.x += jar_cosine_degrees(projectile.heading + 90.0f) * offset;
    projectile.y += jar_sine_degrees(projectile.heading + 90.0f) * offset;
}

}  // namespace

void World::apply_projectile_damage_(Projectile& p, const Vec<World*>& enemy_worlds) {
    const auto apply_damage = [&](World& world, Unit& target, f32 damage) {
        const bool standing_tree = !target.custom_unit &&
            target.unit_type == "tree" && !target.is_dead;
        target.apply_damage(damage, p.owner_uid, tick_, p.shield_mult,
                            p.shield_absorb, p.hp_mult);
        if (standing_tree && target.is_dead) {
            // al.a(am,float,f) calls the base death callback first. The
            // trunk moves along its old angle, then points away from the
            // projectile using the resulting position.
            world.remove_unit(target.uid);
            target.rotation = jar_direction_degrees(
                target.x, target.y, p.x, p.y) + 180.0f;
        }
    };
    World* best_world = nullptr;
    u32 best_uid = 0;
    bool found = false;

    if (p.target_uid != 0) {
        for (World* candidate_world : enemy_worlds) {
            if (!candidate_world || candidate_world->team_id_ != p.target_team_id) continue;
            Unit* target = candidate_world->get_unit(p.target_uid);
            // A projectile keeps its direct target pointer when that unit is
            // loaded after launch. game.f only excludes transported units
            // from area scans; the direct-impact path can still damage them.
            if (target && target->is_alive) {
                best_world = candidate_world;
                best_uid = p.target_uid;
                found = true;
            }
            break;
        }
    }

    Vec<std::pair<World*, u32>> killed;
    if (found && !p.is_nuke && p.damage != 0.0f) {
        Unit* target = best_world ? best_world->get_unit(best_uid) : nullptr;
        if (target) {
            f32 dmg = p.damage * damage_category_multiplier(
                *target, p.air_damage_mult, p.experimental_damage_mult);
            if (target->is_building) dmg *= p.building_damage_mult;
            apply_damage(*best_world, *target, dmg);
            if (target->is_dead) killed.push_back({best_world, best_uid});
        }
    }

    if (p.flame_weapon && !p.continuous_beam && !p.flame_impact_emitted) {
        // game.f resolves its direct damage before f.b(x,y) creates the
        // flame emitter. That visual object's constructor consumes the same
        // UID and creation-order stream as units and projectiles. Expanding
        // area damage and retained projectiles must only allocate it once.
        p.flame_impact_emitted = true;
        if (global_next_uid_) {
            ++(*global_next_uid_);
        } else {
            ++next_uid_;
        }
        (void)allocate_object_order_();
    }

    f32 effective_splash = p.is_flame ? p.flame_current_radius : p.splash_radius;
    if (p.expanding_splash && p.splash_impact_started) {
        const f32 duration = p.splash_expand_duration_frames;
        const f32 fraction = duration > 0.0f
            ? 1.0f - p.splash_expand_remaining_frames / duration
            : 1.0f;
        effective_splash = p.splash_radius * clamp(fraction, 0.0f, 1.0f);
    }
    if (effective_splash > 0.0f && p.splash_damage != 0.0f) {
        const f32 impact_x = p.impact_at_target ? p.target_x : p.x;
        const f32 impact_y = p.impact_at_target ? p.target_y : p.y;
        const i32 cell_width = std::max(
            1, static_cast<i32>(map_width() * tile_size()) /
                   EngineConstants::SPATIAL_GRID_SIZE);
        const i32 cell_height = std::max(
            1, static_cast<i32>(map_height() * tile_size()) /
                   EngineConstants::SPATIAL_GRID_SIZE);
        const auto cell = [](f32 coordinate, i32 size) {
            return clamp_i(static_cast<i32>(coordinate *
                                           (1.0f / static_cast<f32>(size))),
                           0, EngineConstants::SPATIAL_GRID_SIZE - 1);
        };
        // game.f queries cc.b() using saved cells with a 50-unit margin.
        // Constructors have no bucket until the next global index pass.
        const i32 min_cell_x = cell(impact_x - effective_splash - 50.0f, cell_width);
        const i32 max_cell_x = cell(impact_x + effective_splash + 50.0f, cell_width);
        const i32 min_cell_y = cell(impact_y - effective_splash - 50.0f, cell_height);
        const i32 max_cell_y = cell(impact_y + effective_splash + 50.0f, cell_height);
        struct Candidate { World* world; Unit* unit; };
        Vec<Candidate> candidates;
        Vec<World*> splash_worlds = enemy_worlds;
        // Stock nuclear projectiles have friendly fire enabled and do not
        // require an enemy relation, so neutral units are also affected.
        // Allied teams remain protected. Ordinary area projectiles only scan
        // explicitly hostile worlds.
        if (p.is_nuke || p.splash_friendly_fire) splash_worlds.push_back(this);
        for (World* candidate_world : splash_worlds) {
            if (!candidate_world) continue;
            if (candidate_world == this) {
                if (!p.is_nuke && !p.splash_friendly_fire) continue;
            } else if (p.is_nuke || p.splash_friendly_fire) {
                if (is_allied_with(*candidate_world)) continue;
            } else if (!is_enemy_with(*candidate_world)) {
                continue;
            }
            for (auto& [uid, target] : candidate_world->units_) {
                if (!target.is_alive || target.parent_uid != 0) continue;
                if (target.collision_spatial_cell_x < min_cell_x ||
                    target.collision_spatial_cell_x > max_cell_x ||
                    target.collision_spatial_cell_y < min_cell_y ||
                    target.collision_spatial_cell_y > max_cell_y) continue;
                candidates.push_back({candidate_world, &target});
            }
        }
        // This query visits all teams together, in cell and resident order.
        std::sort(candidates.begin(), candidates.end(),
                  [](const Candidate& lhs, const Candidate& rhs) {
                      return std::tuple(lhs.unit->collision_spatial_cell_x,
                                        lhs.unit->collision_spatial_cell_y,
                                        lhs.unit->collision_spatial_entry_tick,
                                        lhs.unit->object_creation_order) <
                             std::tuple(rhs.unit->collision_spatial_cell_x,
                                        rhs.unit->collision_spatial_cell_y,
                                        rhs.unit->collision_spatial_entry_tick,
                                        rhs.unit->object_creation_order);
                  });
        for (const Candidate& candidate : candidates) {
            World* candidate_world = candidate.world;
            Unit& target = *candidate.unit;
            const u32 uid = target.uid;
            const u64 target_key =
                (static_cast<u64>(static_cast<u32>(candidate_world->team_id_)) << 32) |
                static_cast<u64>(uid);
            if (p.expanding_splash &&
                p.splash_hit_units.find(target_key) !=
                    p.splash_hit_units.end()) {
                continue;
            }
            // game.f.b(am,float,float) rejects targets below -5 height
            // for projectiles at or above -2.  The boolean Q()/submerged
            // state flips at -1, so use the JAR's exact height threshold
            // rather than treating the whole transition as underwater.
            if (target.height < -5.0f && p.height >= -2.0f) {
                continue;
            }
            if (p.splash_same_layer &&
                target.airborne != (p.height >= 5.0f)) continue;
            f32 d = dist(impact_x, impact_y, target.x, target.y);
            if (d <= effective_splash) {
                const f32 falloff_radius = p.expanding_splash
                    ? p.splash_radius : effective_splash;
                // game.f.a(delta,target,distance) rounds the float
                // subtraction before adding the double literal 0.1.
                const f32 falloff = std::min(
                    1.0f, static_cast<f32>(
                        static_cast<double>(1.0f - d / falloff_radius) + 0.1));
                f32 sdmg = p.splash_damage * falloff * damage_category_multiplier(
                    target, p.air_damage_mult, p.experimental_damage_mult);
                if (target.is_building) sdmg *= p.building_damage_mult;
                apply_damage(*candidate_world, target, sdmg);
                if (p.expanding_splash) {
                    p.splash_hit_units.insert(target_key);
                }
                const auto dead = std::pair<World*, u32>{candidate_world, uid};
                if (target.is_dead &&
                    std::find(killed.begin(), killed.end(), dead) == killed.end()) {
                    killed.push_back(dead);
                }
            }
        }
    }

    if (p.chain_count > 0 && found) {
        Vec<u32> chained;
        chained.push_back(best_uid);
        f32 prev_x = p.target_x;
        f32 prev_y = p.target_y;
        f32 chain_dmg = p.damage * 0.5f;

        for (i32 c = 0; c < p.chain_count; ++c) {
            World* next_world = nullptr;
            u32 next_uid = 0;
            f32 next_dist = std::numeric_limits<f32>::max();
            for (World* candidate_world : enemy_worlds) {
                if (!candidate_world || candidate_world == this ||
                    !is_enemy_with(*candidate_world)) continue;
                for (auto& [uid, target] : candidate_world->units_) {
                    if (!target.is_alive || target.parent_uid != 0) continue;
                    if (std::find(chained.begin(), chained.end(), uid) != chained.end()) continue;
                    f32 d = dist(prev_x, prev_y, target.x, target.y);
                    if (d < p.chain_range && d < next_dist) {
                        next_dist = d;
                        next_world = candidate_world;
                        next_uid = uid;
                    }
                }
            }
            if (next_uid == 0) break;

            Unit* chain_target = next_world ? next_world->get_unit(next_uid) : nullptr;
            if (chain_target) {
                f32 cdmg = chain_dmg * damage_category_multiplier(
                    *chain_target, p.air_damage_mult, p.experimental_damage_mult);
                if (chain_target->is_building) cdmg *= p.building_damage_mult;
                apply_damage(*next_world, *chain_target, cdmg);
                const auto dead = std::pair<World*, u32>{next_world, next_uid};
                if (chain_target->is_dead &&
                    std::find(killed.begin(), killed.end(), dead) == killed.end()) {
                    killed.push_back(dead);
                }
                chained.push_back(next_uid);
                prev_x = chain_target->x;
                prev_y = chain_target->y;
            }
            chain_dmg *= 0.5f;
        }
    }

    for (const auto& [target_world, uid] : killed) {
        if (!target_world) continue;
        kills_++;
        target_world->losses_++;
        target_world->remove_unit(uid);
    }
}

void World::update_projectiles(f32 dt, World& enemy_world) {
    Vec<World*> enemies{&enemy_world};
    update_projectiles(dt, enemies);
}

void World::update_projectiles(f32 dt, const Vec<World*>& enemy_worlds) {
    update_projectiles_(dt, enemy_worlds, 0);
}

void World::update_projectile(f32 dt, u32 projectile_uid,
                              const Vec<World*>& enemy_worlds) {
    update_projectiles_(dt, enemy_worlds, projectile_uid);
}

void World::update_projectiles_(f32 dt, const Vec<World*>& enemy_worlds,
                                u32 only_projectile_uid) {
    Vec<u32> expired;
    const auto reveal_fog = [&](Projectile& projectile) {
        if (!projectile.should_reveal_fog || projectile.owner_uid == 0) return;
        projectile.should_reveal_fog = false;
        if (Unit* revealer = spawn_map_unit("fogRevealer", projectile.x, projectile.y)) {
            revealer->sight_range = 300.0f;
            revealer->lifetime_frames = 360.0f;
            revealer->collision_enabled = false;
            revealer->pending_global_commit = true;
        }
    };
    const auto emit_nuclear_impact_objects = [&](Projectile& projectile) {
        if (!projectile.is_nuke || projectile.has_hit) return;
        projectile.has_hit = true;
        const auto allocate_auxiliary_object = [&]() {
            if (global_next_uid_) ++(*global_next_uid_);
            else ++next_uid_;
            (void)allocate_object_order_();
        };
        // game.f's default nuclear explosion always creates one d.f smoke
        // emitter before the typed game.l scorch and expanding area damage.
        allocate_auxiliary_object();
        const u8 terrain = pathfinder_.terrain(
            pathfinder_.tile_x(projectile.x), pathfinder_.tile_y(projectile.y));
        if ((terrain & (TERRAIN_WATER | TERRAIN_LAVA)) == 0 &&
            try_create_scorch_mark_(projectile.x, projectile.y, true)) {
            allocate_auxiliary_object();
        }
    };
    for (auto& [pid, p] : projectiles_) {
        if (only_projectile_uid != 0 && pid != only_projectile_uid) continue;
        const bool removed_before_update = !p.active;
        World* enemy_world = nullptr;
        for (World* candidate : enemy_worlds) {
            if (candidate && candidate->team_id_ == p.target_team_id) {
                enemy_world = candidate;
                break;
            }
        }
        if (p.is_antinuke && p.target_team_id == team_id_) enemy_world = this;
        if (!enemy_world && p.is_antinuke) {
            expired.push_back(pid);
            continue;
        }

        // Projectile lifetime and launch delays in the JAR are measured in
        // simulation frames, not wall-clock milliseconds.
        const f32 elapsed_frames = dt * EngineConstants::FRAME_RATE;
        f32 active_frames = elapsed_frames;
        if (p.activation_delay_frames > 0.0f) {
            // game.f.a(float) approaches the delay by the full update delta.
            // Once it reaches zero, that same update continues with the full
            // delta; it does not discard the portion before activation.
            const f32 remaining_delay = p.activation_delay_frames;
            p.activation_delay_frames = std::max(
                0.0f, remaining_delay - elapsed_frames);
            if (p.activation_delay_frames > 0.0f) {
                if (removed_before_update) expired.push_back(pid);
                continue;
            }
        }
        const f32 active_dt = active_frames / EngineConstants::FRAME_RATE;
        p.lifetime -= active_frames;
        const f32 previous_age_frames = p.age_frames;
        p.age_frames += active_frames;
        // game.f.a(float) decrements h near the start of the update, but
        // only removes the projectile at the end.  A projectile whose
        // lifetime reaches zero on this frame still gets its normal target,
        // movement, and impact checks before it is discarded.
        const bool expires_after_update =
            p.lifetime <= 0.0f || removed_before_update;

        if (p.is_antinuke) {
            auto target_it = enemy_world->projectiles_.find(p.target_projectile_uid);
            if (target_it != enemy_world->projectiles_.end()) {
                p.target_x = target_it->second.x + p.target_offset_x;
                p.target_y = target_it->second.y + p.target_offset_y;
                p.target_height = target_it->second.height;
            }
        }

        if (p.is_flame) {
            p.flame_current_radius += p.flame_expand_rate * active_dt;
        }

        const bool resolved_expanding_impact =
            p.expanding_splash && p.splash_impact_started;
        const auto advance_expanding_impact = [&]() {
            p.splash_expand_remaining_frames = std::max(
                0.0f, p.splash_expand_remaining_frames - active_frames);
            apply_projectile_damage_(p, enemy_worlds);
            if (p.splash_expand_remaining_frames <= 0.0f ||
                expires_after_update) {
                expired.push_back(pid);
            }
        };

        // Direct projectiles retain the target object chosen at launch even
        // if diplomacy changes while they are in flight. Area scans and
        // on-death retargeting still use the current hostility relation.
        Unit* retained_target = p.target_uid != 0 && enemy_world
            ? enemy_world->waypoint_target_(p.target_uid, true) : nullptr;
        Unit* projectile_target = retained_target && retained_target->is_alive
            ? retained_target : nullptr;
        if (!projectile_target && p.target_uid != 0 &&
            p.retarget_on_death) {
            // game.f.j remains the actual shooter object after its removal
            // from am.bE/w.er; retarget gates still read that object's state.
            Unit* owner = waypoint_target_(p.owner_uid, true);
            if (owner && owner->behavior != Behavior::HOLD_FIRE &&
                owner->behavior != Behavior::RETURN_FIRE) {
                const WeaponSlot* firing_slot =
                    p.firing_slot >= 0 &&
                    p.firing_slot < static_cast<i32>(owner->weapon_slots.size())
                        ? &owner->weapon_slots[p.firing_slot] : nullptr;
                const f32 heading_x = jar_cosine_degrees(p.heading);
                const f32 heading_y = jar_sine_degrees(p.heading);
                const f32 search_x = p.x + heading_x * p.retarget_lead;
                const f32 search_y = p.y + heading_y * p.retarget_lead;
                const f32 retarget_range_sq = p.retarget_range * p.retarget_range;
                // game.f.a(...) queries the 32x32 spatial index. The query
                // fills cells in ascending order, while its iterator walks
                // the collected array backwards. Residents retain their
                // insertion order; a unit re-entering a cell is appended.
                // Use the last global geo-index pass, before these object
                // turns, when applying the current-main-target exception.
                struct RetargetCandidate {
                    World* world;
                    Unit* unit;
                    f32 distance_sq;
                    i32 cell_x;
                    i32 cell_y;
                };
                Vec<RetargetCandidate> candidates;
                World* selected_world = nullptr;
                Unit* selected = nullptr;
                const i32 spatial_cell_width = std::max(
                    1, static_cast<i32>(pathfinder_.width() *
                                       pathfinder_.tile_size()) /
                           EngineConstants::SPATIAL_GRID_SIZE);
                const i32 spatial_cell_height = std::max(
                    1, static_cast<i32>(pathfinder_.height() *
                                       pathfinder_.tile_size()) /
                           EngineConstants::SPATIAL_GRID_SIZE);
                const f32 inverse_cell_width =
                    1.0f / static_cast<f32>(spatial_cell_width);
                const f32 inverse_cell_height =
                    1.0f / static_cast<f32>(spatial_cell_height);
                auto spatial_cell = [](f32 value, f32 inverse_dimension) {
                    return clamp_i(static_cast<i32>(value * inverse_dimension),
                                   0, EngineConstants::SPATIAL_GRID_SIZE - 1);
                };
                const i32 min_cell_x = spatial_cell(
                    search_x - p.retarget_range, inverse_cell_width);
                const i32 max_cell_x = spatial_cell(
                    search_x + p.retarget_range, inverse_cell_width);
                const i32 min_cell_y = spatial_cell(
                    search_y - p.retarget_range, inverse_cell_height);
                const i32 max_cell_y = spatial_cell(
                    search_y + p.retarget_range, inverse_cell_height);
                for (World* candidate_world : enemy_worlds) {
                    if (!candidate_world || !is_enemy_with(*candidate_world)) {
                        continue;
                    }
                    for (auto& [_, candidate] : candidate_world->units_) {
                        // game.f.a(...) first calls y.b(candidate,true),
                        // then y.a(k,candidate,true,false). Unit and current
                        // turret conditions are independent; neither uses
                        // the owner's weapon range for this search.
                        if (!candidate.is_alive || candidate.parent_uid != 0 ||
                            !can_target_(*owner, candidate) ||
                            (firing_slot && !target_layer_allowed(
                                *this, *owner, candidate,
                                firing_slot->targets_air,
                                firing_slot->targets_ground,
                                firing_slot->targets_underwater,
                                firing_slot->targets_not_touching_water))) {
                            continue;
                        }
                        const f32 candidate_dx = candidate.x - search_x;
                        const f32 candidate_dy = candidate.y - search_y;
                        const f32 candidate_distance_sq =
                            candidate_dx * candidate_dx +
                            candidate_dy * candidate_dy;
                        if (candidate_distance_sq >= retarget_range_sq) continue;
                        // cc.a() only returns residents of the last geo-index
                        // pass. A factory product created earlier in this
                        // object sweep has no bucket until that pass runs.
                        const i32 cell_x = candidate.collision_spatial_cell_x;
                        const i32 cell_y = candidate.collision_spatial_cell_y;
                        if (cell_x < min_cell_x || cell_x > max_cell_x ||
                            cell_y < min_cell_y || cell_y > max_cell_y) {
                            continue;
                        }
                        candidates.push_back({
                            candidate_world, &candidate, candidate_distance_sq,
                            cell_x, cell_y});
                    }
                }
                std::sort(candidates.begin(), candidates.end(),
                          [](const RetargetCandidate& lhs,
                             const RetargetCandidate& rhs) {
                              if (lhs.cell_x != rhs.cell_x) {
                                  return lhs.cell_x > rhs.cell_x;
                              }
                              if (lhs.cell_y != rhs.cell_y) {
                                  return lhs.cell_y > rhs.cell_y;
                              }
                              if (lhs.unit->collision_spatial_entry_tick !=
                                  rhs.unit->collision_spatial_entry_tick) {
                                  return lhs.unit->collision_spatial_entry_tick >
                                         rhs.unit->collision_spatial_entry_tick;
                              }
                              return lhs.unit->object_creation_order >
                                  rhs.unit->object_creation_order;
                          });
                f32 selected_distance_sq = -1.0f;
                const auto retained_preference = [&](u32 target_uid,
                                                      i32 target_team) -> Unit* {
                    if (target_uid == 0) return nullptr;
                    for (World* candidate_world : enemy_worlds) {
                        if (!candidate_world ||
                            (target_team >= 0 &&
                             candidate_world->team_id_ != target_team)) {
                            continue;
                        }
                        Unit* candidate = candidate_world->waypoint_target_(
                            target_uid, true);
                        if (candidate && !candidate->is_dead) {
                            return candidate;
                        }
                    }
                    return nullptr;
                };
                // game.f.a(range,lead,tags) reads y.ab(): a living R first,
                // otherwise the current waypoint's living target. That
                // preference can replace a closer resident, after which
                // later residents are still compared with its distance.
                Unit* preferred_target = retained_preference(
                    owner->main_target_uid(), owner->main_target_team_id());
                if (!preferred_target && !owner->order_queue.empty()) {
                    const UnitOrder& order = owner->order_queue.front();
                    preferred_target = retained_preference(
                        order.target_uid, order.target_team_id);
                }
                for (const RetargetCandidate& candidate : candidates) {
                    const bool closer = selected_distance_sq < 0.0f ||
                        candidate.distance_sq < selected_distance_sq;
                    if (!closer && candidate.unit != preferred_target) continue;
                    selected_distance_sq = candidate.distance_sq;
                    selected_world = candidate.world;
                    selected = candidate.unit;
                }
                if (selected && selected_world) {
                    enemy_world = selected_world;
                    retained_target = selected;
                    projectile_target = selected;
                    p.target_uid = selected->uid;
                    p.target_team_id = selected_world->team_id_;
                }
            }
        }
        // Ordinary live-target projectiles refresh l each frame. Stock
        // artillery instead has aC/m set and stores only the predicted n/o
        // impact point computed when it fires.
        if (p.sweep_offset != 0.0f || p.sweep_offset_from_target_radius != 0.0f) {
            f32 amplitude = p.sweep_offset;
            if (retained_target) amplitude += retained_target->radius * p.sweep_offset_from_target_radius;
            p.target_offset_x = jar_sine_degrees(std::fmod(
                360.0f * p.random_phase + previous_age_frames * 1.0f, 360.0f)) * amplitude;
            p.target_offset_y = jar_sine_degrees(std::fmod(
                360.0f * p.random_phase + previous_age_frames * 1.5f, 360.0f)) * amplitude;
        }
        if (p.continuous_beam && p.target_uid != 0) {
            // game.f.E overrides K/L from the old J charge on each object
            // turn. The hover tank reads these offsets on its next aim pass.
            const f32 radius = retained_target
                ? retained_target->radius : p.target_radius;
            p.target_offset_x = jar_sine_degrees(
                std::fmod(p.beam_charge_frames, 360.0f)) * radius * 0.4f;
            p.target_offset_y = jar_sine_degrees(
                std::fmod(p.beam_charge_frames * 1.5f, 360.0f)) * radius * 0.4f;
        }
        if (retained_target && !p.fixed_target) {
            p.target_x = retained_target->x + p.target_offset_x;
            p.target_y = retained_target->y + p.target_offset_y;
            p.target_height = retained_target->height;
            p.has_target_snapshot = true;
            p.target_radius = retained_target->radius;
            p.target_shield = retained_target->shield;
            p.target_is_stock_building =
                retained_target->is_building && !retained_target->custom_unit;
        }
        const auto move_with_parent = [&]() {
            if (!p.move_with_parent) return;
            Unit* parent = waypoint_target_(p.owner_uid, true);
            if (!parent || parent->list_removal_tick >= 0) return;
            f32 x = parent->x, y = parent->y;
            if (p.parent_turret_slot >= 0 && !parent->weapon_slots.empty()) {
                if (p.parent_turret_slot >= static_cast<i32>(parent->weapon_slots.size())) p.parent_turret_slot = 0;
                const auto origin = custom_weapon_muzzle_origin(*parent, p.parent_turret_slot);
                x = origin.first;
                y = origin.second;
            }
            p.x += x - p.parent_anchor_x;
            p.y += y - p.parent_anchor_y;
            p.height += parent->height - p.parent_anchor_height;
            p.parent_anchor_x = x;
            p.parent_anchor_y = y;
            p.parent_anchor_height = parent->height;
        };

        if (p.continuous_beam) {
            if (p.wobble_amplitude != 0.0f) {
                p.heading = jar_direction_degrees(p.x, p.y, p.target_x, p.target_y);
                apply_projectile_wobble(p, p.beam_charge_frames, active_frames);
            }
            if (projectile_target) {
                // game.f.E applies U/60 each frame, ramping by J/F with
                // F=230. J is incremented only after the current hit.
                const f32 beam_factor = std::min(
                    1.0f, p.beam_charge_frames / 230.0f);
                const f32 full_damage = p.damage;
                p.damage = full_damage / EngineConstants::FRAME_RATE *
                           active_frames * beam_factor;
                apply_projectile_damage_(p, enemy_worlds);
                p.damage = full_damage;
            }
            p.beam_charge_frames += active_frames;
            if (expires_after_update) expired.push_back(pid);
            continue;
        }
        if (p.instant_hit) {
            // game.f selects its bearing before applying the parent's D(i)
            // displacement, even for an instant or already resolved beam.
            p.heading = jar_direction_degrees(p.x, p.y, p.target_x, p.target_y);
            move_with_parent();
            if (p.wobble_amplitude != 0.0f) {
                apply_projectile_wobble(p, previous_age_frames, active_frames);
            }
            if (!p.has_hit) {
                reveal_fog(p);
                emit_nuclear_impact_objects(p);
                if (p.expanding_splash) {
                    // f.bn starts the expanding effect after an instant
                    // ground hit, without requiring a live unit target.
                    p.splash_impact_started = true;
                    p.splash_expand_remaining_frames = std::max(
                        0.0f, p.splash_expand_remaining_frames - active_frames);
                    apply_projectile_damage_(p, enemy_worlds);
                } else if (projectile_target || p.fixed_target) {
                    apply_projectile_damage_(p, enemy_worlds);
                }
                p.has_hit = true;
                if (!p.persist_after_hit &&
                    (!p.expanding_splash ||
                     p.splash_expand_remaining_frames <= 0.0f)) expired.push_back(pid);
            }
            if (resolved_expanding_impact) advance_expanding_impact();
            if (expires_after_update) expired.push_back(pid);
            continue;
        }

        f32 dx = p.target_x - p.x;
        f32 dy = p.target_y - p.y;
        p.heading = jar_direction_degrees(p.x, p.y, p.target_x, p.target_y);
        const f32 distance_sq = dx * dx + dy * dy;
        move_with_parent();
        // game.f captures its target geometry before adding u/v/w. Gravity
        // tests the height after w, and H still changes w if G then crosses
        // below zero during that same block.
        p.x += p.unguided_speed_x * active_frames;
        p.y += p.unguided_speed_y * active_frames;
        if (p.unguided_speed_height != 0.0f) {
            p.height += p.unguided_speed_height * active_frames;
        }
        if (p.height > 0.0f) {
            if (p.gravity != 0.0f) p.height -= p.gravity * active_frames;
            if (p.true_gravity != 0.0f) {
                p.unguided_speed_height -= p.true_gravity * active_frames;
            }
        }
        const bool horizontal_ready = !p.height_guided || p.arc_descending ||
            p.height > p.launch_height;
        f32 collision_distance_sq = distance_sq;
        if (horizontal_ready) {
            f32 move_amount = p.speed_per_frame * active_frames;
            // The native overshoot gate is strict and compares squared
            // distances. Exact-step arrivals retain the old collision
            // distance until the next update.
            if (distance_sq < move_amount * move_amount) {
                move_amount = std::sqrt(distance_sq);
                collision_distance_sq = 0.0f;
            }
            p.x += jar_cosine_degrees(p.heading) * move_amount;
            p.y += jar_sine_degrees(p.heading) * move_amount;
        }

        // game.f computes the vertical collision delta before applying the
        // height-guided update.  Height-guided projectiles therefore use the
        // post-gravity height for this frame's collision gate; ordinary
        // projectiles refresh the delta after their straight-slope update.
        const f32 collision_height_delta_before_update =
            p.target_height - p.height;
        bool descending_arc = false;
        if (p.height_guided) {
            const f32 target_height = p.target_height;
            const f32 vertical_step = (p.vertical_speed < 0.0f
                ? p.speed_per_frame : p.vertical_speed) * active_frames;
            if (!p.arc_descending) {
                p.height = approach_value(
                    p.height, p.cruise_height, vertical_step);
                if (p.height >= p.cruise_height) p.arc_descending = true;
            } else if (collision_distance_sq < 400.0f) {
                p.height = approach_value(
                    p.height, target_height, vertical_step);
                descending_arc = std::abs(p.height - target_height) > 0.5f;
            }
        } else {
            // Ordinary JAR projectiles follow a straight height slope toward
            // the target in parallel with their horizontal movement.
            const f32 height_error = p.target_height - p.height;
            if (height_error != 0.0f) {
                f32 height_step = p.speed_per_frame * active_frames;
                if (static_cast<f64>(collision_distance_sq) > 0.1) {
                    height_step = std::min(
                        std::abs(height_error) / std::sqrt(collision_distance_sq) * p.speed_per_frame *
                            active_frames,
                        height_step);
                }
                p.height += clamp(height_error, -height_step, height_step);
            }
        }

        // game.f sets z4 when it enters the flight controller, even when
        // the horizontal distance is zero. A guided shot waiting for its
        // height collision gate therefore still approaches r each update.
        if ((horizontal_ready || (p.height_guided && p.vertical_speed < 0.0f)) &&
            p.target_speed > 0.0f) {
            // game.f stores t/r/s in units per simulation frame and
            // applies f.a(t, r, s * delta) at that precision. Updating
            // our per-second values first loses the repeated float-add
            // rounding (for example 2.0 + 0.2 * 3 is 2.6000001 in the
            // JAR, while (120 + 12 * 3) / 60 is 2.6).
            const f32 target_speed_per_frame =
                p.target_speed / EngineConstants::FRAME_RATE;
            const f32 acceleration_per_frame =
                p.speed_acceleration / EngineConstants::FRAME_RATE;
            p.speed_per_frame = approach_value(
                p.speed_per_frame, target_speed_per_frame,
                acceleration_per_frame * active_frames);
            p.speed = p.speed_per_frame *
                EngineConstants::FRAME_RATE;
        }

        // game.f applies wobble after flight and acceleration but before
        // impact, using J from the start of this update. Keep the captured
        // distance and height delta for collision even after the offset.
        apply_projectile_wobble(p, previous_age_frames, active_frames);

        f32 impact_radius = 6.0f;
        if (p.has_target_snapshot) {
            if (p.target_is_stock_building) {
                impact_radius = std::max(6.0f, p.target_radius * 0.8f);
            }
            if (p.target_shield > 10.0f + p.damage) {
                impact_radius = p.target_radius * 1.1f;
            }
        }
        const f32 collision_height_delta = p.height_guided
            ? collision_height_delta_before_update
            : (p.target_height - p.height);
        f32 height_tolerance = 3.0f;
        if (p.unguided_speed_height != 0.0f || p.gravity != 0.0f) {
            height_tolerance += std::abs(p.unguided_speed_height * active_frames) +
                                std::abs(p.gravity * active_frames);
        }
        const bool height_ready = std::abs(collision_height_delta) < height_tolerance;
        const bool hits_target = collision_distance_sq < impact_radius * impact_radius && height_ready;
        if (!resolved_expanding_impact &&
            ((p.height_guided && descending_arc && p.height < 30.0f) || hits_target)) {
            reveal_fog(p);
        }
        if (!resolved_expanding_impact && hits_target) {
            emit_nuclear_impact_objects(p);
            if (p.is_antinuke) {
                enemy_world->projectiles_.erase(p.target_projectile_uid);
                expired.push_back(pid);
            } else if (p.expanding_splash) {
                // game.f enters bn and consumes the full current delta from
                // W in the same update that detects the impact.
                p.splash_impact_started = true;
                p.splash_expand_remaining_frames = std::max(
                    0.0f, p.splash_expand_remaining_frames - active_frames);
                apply_projectile_damage_(p, enemy_worlds);
                if (p.splash_expand_remaining_frames <= 0.0f) {
                    expired.push_back(pid);
                }
            } else {
                apply_projectile_damage_(p, enemy_worlds);
                expired.push_back(pid);
            }
        }
        // game.f still runs guidance, acceleration and wobble after bn;
        // only the one-time impact is skipped while W drives the area wave.
        if (resolved_expanding_impact) advance_expanding_impact();
        if (expires_after_update) expired.push_back(pid);
    }
    for (u32 pid : expired) {
        auto projectile = projectiles_.find(pid);
        if (projectile == projectiles_.end()) continue;
        if (projectile->second.continuous_beam) {
            const Unit* owner = get_unit(projectile->second.owner_uid);
            if (owner && owner->is_alive &&
                owner->unit_type == "experimentalHoverTank" &&
                owner->active_beam_projectile_uid == pid) {
                Projectile retained = projectile->second;
                retained.active = false;
                retired_beam_projectiles_[pid] = std::move(retained);
            }
        }
        projectiles_.erase(projectile);
    }
}

} // namespace rwrl
