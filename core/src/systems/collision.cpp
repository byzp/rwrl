#include "world_internal.h"

namespace rwrl {

bool World::check_victory() const {
    for (auto& [uid, u] : units_) {
        // JAR n.e(float) scans the global unit list and filters with cT(),
        // which does not reject a unit merely because cN (transport parent)
        // is set. Loaded builders therefore still preserve their team.
        if (u.is_dead || !u.is_complete()) continue;
        if (!u.is_alive && u.parent_uid == 0) continue;
        // These are the exact canonical types whose JAR bJ() or ak() role is
        // true. Other buildings and combat units do not preserve the team.
        if (is_victory_key_unit(u)) return true;
    }
    return false;
}

bool World::is_cc_alive() const {
    if (cc_uid_ == 0) return false;
    const Unit* cc = get_unit(cc_uid_);
    return cc && cc->is_alive;
}

void World::update_collision(f32 dt) {
    Vec<World*> worlds{this};
    update_collision(dt, worlds);
}

void World::update_collision(World& other, f32 dt) {
    Vec<World*> worlds{this};
    if (&other != this) worlds.push_back(&other);
    update_collision(dt, worlds);
}

void World::refresh_unit_spatial_index_(Unit& unit) {
    const i32 map_cell_width = std::max(
        1, static_cast<i32>(map_width() * tile_size()) /
               EngineConstants::SPATIAL_GRID_SIZE);
    const i32 map_cell_height = std::max(
        1, static_cast<i32>(map_height() * tile_size()) /
               EngineConstants::SPATIAL_GRID_SIZE);
    // f.c.a(am) multiplies by its stored float reciprocal. Division by the
    // integer size can change an exact boundary's resident-entry history.
    const f32 inverse_cell_width = 1.0f / static_cast<f32>(map_cell_width);
    const f32 inverse_cell_height = 1.0f / static_cast<f32>(map_cell_height);
    const i32 cell_x = clamp_i(
        static_cast<i32>(unit.x * inverse_cell_width),
        0, EngineConstants::SPATIAL_GRID_SIZE - 1);
    const i32 cell_y = clamp_i(
        static_cast<i32>(unit.y * inverse_cell_height),
        0, EngineConstants::SPATIAL_GRID_SIZE - 1);
    if (unit.collision_spatial_cell_x != cell_x ||
        unit.collision_spatial_cell_y != cell_y) {
        unit.collision_spatial_cell_x = cell_x;
        unit.collision_spatial_cell_y = cell_y;
        unit.collision_spatial_entry_tick = tick_;
    }
}

void World::update_collision(f32 dt, const Vec<World*>& worlds) {
    const f32 delta_frames = dt * EngineConstants::FRAME_RATE;
    // y.aL is scheduled against l.by, the millisecond-style engine clock,
    // while replay snapshots expose l.bx (one logical tick per update).
    const i32 collision_clock = simulation_clock_ms_;
    struct GlobalUnit {
        World* world;
        Unit* unit;
    };
    Vec<GlobalUnit> ordered;
    for (World* world : worlds) {
        if (!world) continue;
        for (auto& [_, unit] : world->units_) {
            // Newly created objects are not visible in the committed w list
            // yet, but their constructors have already inserted units into
            // am.bE, which is the list consumed by y.g(float). Conversely,
            // am.bu() removes destroyed units from bE immediately even when
            // their wreck/object remains in the main object list.
            // al.k() retains fallen trees; damage death enters am.bu() and
            // removes them. Both can remain in the global object/wreck list.
            if (!unit.native_unit_list_member) continue;
            ordered.push_back({world, &unit});
        }
    }
    std::sort(ordered.begin(), ordered.end(),
              [](const GlobalUnit& lhs, const GlobalUnit& rhs) {
                  return lhs.unit->object_creation_order <
                         rhs.unit->object_creation_order;
              });
    // f.c preserves resident order inside each spatial cell. Re-entering a
    // cell appends the unit after objects that remained there.
    for (GlobalUnit& entry : ordered) {
        entry.world->refresh_unit_spatial_index_(*entry.unit);
    }

    auto find_candidate = [&](i32 team_id, u32 uid) -> GlobalUnit {
        for (World* world : worlds) {
            if (!world || world->team_id_ != team_id) continue;
            Unit* candidate = world->get_unit(uid);
            if (!candidate) {
                const auto retained = world->retained_collision_units_.find(uid);
                if (retained != world->retained_collision_units_.end()) {
                    candidate = &retained->second;
                }
            }
            return {world, candidate};
        }
        return {nullptr, nullptr};
    };

    // y.g(float), phase one: periodically rebuild each moving unit's ten
    // nearest collision candidates. The geo index scans cells in x/y order
    // and preserves global insertion order within a cell.
    for (std::size_t index = 0; index < ordered.size(); ++index) {
        Unit& primary = *ordered[index].unit;
        if ((!primary.collision_moved && !primary.collision_repeat) ||
            !primary.is_mobile() ||
            primary.custom_action_movement_blocked ||
            primary.collision_next_tick > collision_clock) {
            continue;
        }

        // y.g() gates this controller on I(), independently of am.bT()
        // and the bU collision layer. A passenger that moves on its loading
        // turn, or still has cb armed, must advance aL and clear its candidate
        // list. The pair filter rejects its disabled layer instead.
        primary.collision_repeat = false;
        primary.collision_moved = true;
        const std::size_t old_count = primary.collision_candidates.size();
        const bool active_request = primary.movement_prediction_enabled;
        primary.collision_next_tick = active_request
            ? collision_clock + (old_count > 9 ? 200 : 50) +
                  static_cast<i32>(index % 50)
            : collision_clock + 250 + static_cast<i32>(index % 50);
        primary.collision_candidates.clear();

        const f32 query_radius = primary.radius +
            (active_request ? 7.0f : 5.0f);
        // f.c initializes its spatial scale from integer cell dimensions:
        // (map tiles * tile pixels) / 32, then stores 1.0f / dimension.
        // Keep the integer division here; multiplying by 32 / map pixels
        // assigns different cells whenever the map size is not divisible by
        // 32 and can change equal-rank collision candidate order.
        const i32 map_cell_width = std::max(
            1, static_cast<i32>(ordered[index].world->map_width() *
                                ordered[index].world->tile_size()) /
                   EngineConstants::SPATIAL_GRID_SIZE);
        const i32 map_cell_height = std::max(
            1, static_cast<i32>(ordered[index].world->map_height() *
                                ordered[index].world->tile_size()) /
                   EngineConstants::SPATIAL_GRID_SIZE);
        const f32 inverse_cell_width =
            1.0f / static_cast<f32>(map_cell_width);
        const f32 inverse_cell_height =
            1.0f / static_cast<f32>(map_cell_height);
        const auto spatial_cell = [](f32 value, f32 inverse_size) {
            return clamp_i(static_cast<i32>(value * inverse_size), 0,
                           EngineConstants::SPATIAL_GRID_SIZE - 1);
        };
        const f32 min_x = primary.x - query_radius;
        const f32 max_x = primary.x + query_radius;
        const f32 min_y = primary.y - query_radius;
        const f32 max_y = primary.y + query_radius;
        // f.c.b() visits cells with a fixed 50-pixel margin, then tests
        // resident radii. A 53-radius carrier can pass the latter bounds
        // while belonging to a cell the native query never visits.
        const i32 min_cell_x = spatial_cell(min_x - 50.0f, inverse_cell_width);
        const i32 max_cell_x = spatial_cell(max_x + 50.0f, inverse_cell_width);
        const i32 min_cell_y = spatial_cell(min_y - 50.0f, inverse_cell_height);
        const i32 max_cell_y = spatial_cell(max_y + 50.0f, inverse_cell_height);
        Vec<GlobalUnit> nearby;
        for (const GlobalUnit& entry : ordered) {
            Unit& candidate = *entry.unit;
            if (candidate.uid == primary.uid &&
                entry.world == ordered[index].world) {
                continue;
            }
            if (candidate.collision_spatial_cell_x < min_cell_x ||
                candidate.collision_spatial_cell_x > max_cell_x ||
                candidate.collision_spatial_cell_y < min_cell_y ||
                candidate.collision_spatial_cell_y > max_cell_y ||
                !(min_x - candidate.radius <= candidate.x) ||
                !(candidate.x <= max_x + candidate.radius) ||
                !(min_y - candidate.radius <= candidate.y) ||
                !(candidate.y <= max_y + candidate.radius)) {
                continue;
            }
            nearby.push_back(entry);
        }
        std::sort(nearby.begin(), nearby.end(),
                  [&](const GlobalUnit& lhs, const GlobalUnit& rhs) {
                      auto key = [&](const GlobalUnit& entry) {
                          const i32 cell_x = clamp_i(static_cast<i32>(
                              entry.unit->x * inverse_cell_width), 0,
                              EngineConstants::SPATIAL_GRID_SIZE - 1);
                          const i32 cell_y = clamp_i(static_cast<i32>(
                              entry.unit->y * inverse_cell_height), 0,
                              EngineConstants::SPATIAL_GRID_SIZE - 1);
                          return std::tuple<i32, i32, i32, u64>(
                              cell_x, cell_y,
                              entry.unit->collision_spatial_entry_tick,
                              entry.unit->object_creation_order);
                      };
                      return key(lhs) < key(rhs);
                  });

        for (const GlobalUnit& entry : nearby) {
            Unit& candidate = *entry.unit;
            if (jar_collision_layer(primary) < 0 ||
                jar_collision_layer(primary) !=
                    jar_collision_layer(candidate) ||
                primary.collision_approach_target_uid == candidate.uid ||
                candidate.collision_approach_target_uid == primary.uid) {
                continue;
            }
            bool already_owned = false;
            for (const auto& id : candidate.collision_candidates) {
                if (id.first == primary.team_id &&
                    id.second == primary.uid) {
                    already_owned = true;
                    break;
                }
            }
            if (already_owned) continue;

            const f32 primary_x = primary.x + primary.collision_impulse_x;
            const f32 primary_y = primary.y + primary.collision_impulse_y;
            const f32 candidate_x = candidate.x +
                candidate.collision_impulse_x;
            const f32 candidate_y = candidate.y +
                candidate.collision_impulse_y;
            const f32 distance_squared = dist2(
                primary_x, primary_y, candidate_x, candidate_y);
            const f32 combined_radius = primary.radius + candidate.radius;
            const f32 rank = distance_squared <
                    combined_radius * combined_radius
                ? 0.0f : distance_squared;
            i32 insertion = -1;
            const i32 old_size = static_cast<i32>(
                primary.collision_candidates.size());
            for (i32 candidate_index = 0;
                 candidate_index < old_size; ++candidate_index) {
                if (rank < primary.collision_candidate_ranks[
                               static_cast<std::size_t>(candidate_index)]) {
                    insertion = candidate_index;
                    break;
                }
            }
            if (insertion == -1) {
                if (old_size >= 10) continue;
                insertion = old_size;
            }
            if (old_size < 10) {
                primary.collision_candidates.emplace_back(-1, 0);
            }
            // y.a(am,float,true) shifts only aJ. Its parallel aK rank array
            // is not shifted, which affects later insertions in this rebuild.
            for (i32 candidate_index = static_cast<i32>(
                     primary.collision_candidates.size()) - 1;
                 candidate_index > insertion; --candidate_index) {
                primary.collision_candidates[
                    static_cast<std::size_t>(candidate_index)] =
                    primary.collision_candidates[
                        static_cast<std::size_t>(candidate_index - 1)];
            }
            primary.collision_candidates[static_cast<std::size_t>(insertion)] =
                {entry.world->team_id_, candidate.uid};
            primary.collision_candidate_ranks[
                static_cast<std::size_t>(insertion)] = rank;
        }
        if (primary.collision_candidates.size() > 9 &&
            primary.spawn_clock_ms > collision_clock - 400) {
            primary.collision_next_tick = collision_clock + 5 +
                static_cast<i32>(index % 5);
            primary.collision_repeat = true;
        }
    }

    // Phase two resolves the cached list for every unit that moved in this
    // object-update frame. Pending impulses remain visible to later pairs.
    for (GlobalUnit& entry : ordered) {
        Unit& primary = *entry.unit;
        if (!primary.collision_moved ||
            primary.collision_candidates.empty() ||
            !primary.is_mobile() ||
            primary.custom_action_movement_blocked) {
            continue;
        }
        primary.collision_repeat = true;
        for (const auto& id : primary.collision_candidates) {
            GlobalUnit candidate = find_candidate(id.first, id.second);
            if (!candidate.world || !candidate.unit) continue;
            resolve_collision_pair_(
                primary, *candidate.world, *candidate.unit, delta_frames);
        }
    }

    // Removed objects are unavailable to fresh spatial queries. Native aJ
    // caches, bomber ATTACK waypoints and projectile targets/owners still hold
    // their object references.
    Set<std::pair<i32, u32>> referenced;
    for (World* world : worlds) {
        if (!world) continue;
        for (const auto& [_, unit] : world->units_) {
            referenced.insert(unit.collision_candidates.begin(),
                              unit.collision_candidates.end());
            // y.Z is separate from the aJ candidate cache. The generic
            // movement controller still calls Z.c(point, 2) on a removed
            // collider while aa is current or one frame old.
            if (unit.collision_partner_uid != 0 &&
                (unit.collision_partner_tick == tick_ ||
                 unit.collision_partner_tick == tick_ - 1)) {
                referenced.insert({unit.collision_partner_team_id,
                                   unit.collision_partner_uid});
            }
            if (unit.bomber_attack_movement) {
                for (const UnitOrder& order : unit.order_queue) {
                    if (order.type == WeaponType::ATTACK && order.target_uid != 0) {
                        referenced.insert({order.target_team_id, order.target_uid});
                    }
                }
            }
        }
        for (const auto& [_, projectile] : world->projectiles_) {
            if (projectile.owner_uid != 0) {
                referenced.insert({world->team_id_, projectile.owner_uid});
            }
            if (projectile.target_uid != 0) {
                referenced.insert({projectile.target_team_id,
                                   projectile.target_uid});
            }
        }
    }
    for (World* world : worlds) {
        if (!world) continue;
        for (auto it = world->retained_collision_units_.begin();
             it != world->retained_collision_units_.end();) {
            if (referenced.count({world->team_id_, it->first}) == 0) {
                it = world->retained_collision_units_.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void World::resolve_collision_pair_(Unit& a, World& other_world, Unit& b,
                                    f32 delta_frames) {
    if (!a.collision_enabled || !b.collision_enabled) return;
    // Cached aJ contacts can outlive loading into a transport. Native
    // y.a(am,float,boolean) rejects bU=-1 before comparing the two layers,
    // even when both passengers now have the same disabled layer.
    const i32 layer = jar_collision_layer(a);
    if (layer < 0 || layer != jar_collision_layer(b)) return;
    if (a.collision_approach_target_uid == b.uid ||
        b.collision_approach_target_uid == a.uid) return;
    // Every pair in y.g(float) sees the displacement accumulated by earlier
    // pairs in the same collision pass, even though that displacement is not
    // committed to eo/ep until the unit's next object update.
    const f32 a_collision_x = a.x + a.collision_impulse_x;
    const f32 a_collision_y = a.y + a.collision_impulse_y;
    const f32 b_collision_x = b.x + b.collision_impulse_x;
    const f32 b_collision_y = b.y + b.collision_impulse_y;
    f32 dx = b_collision_x - a_collision_x;
    f32 dy = b_collision_y - a_collision_y;
    const f32 distance_squared = dx * dx + dy * dy;
    f32 d = std::sqrt(distance_squared);
    f32 min_dist = a.radius + b.radius;
    // y.a(am,float) compares squared distances before taking the root.
    // An inside contact can have a float sqrt equal to the summed radius;
    // the native 0.001 overlap still produces a deferred impulse there.
    const bool overlapping = distance_squared < min_dist * min_dist;

    if (overlapping) {
        auto collide_tree = [&](Unit& tree, World& tree_world,
                                const Unit& collider) {
            if (tree.unit_type != "tree") return false;
            if (tree.is_dead) return true;
            tree.hp -= collider.mass / 3000.0f * tree.max_hp *
                       0.06f * delta_frames;
            if (tree.hp > 0.0f) return false;

            tree.rotation = jar_direction_degrees(
                tree.x, tree.y, collider.x, collider.y) + 180.0f;
            tree_world.fell_tree_(tree);
            return true;
        };
        if (collide_tree(b, other_world, a) ||
            collide_tree(a, *this, b)) {
            return;
        }
    }

    if (overlapping) {
        // y.a(am,float) softens deep overlap in stages before applying the
        // mass split. It does not inspect either unit's player/alliance.
        f32 overlap = min_dist - d + 0.001f;
        const i32 soft_collision = std::max(
            a.soft_collision_on_all, b.soft_collision_on_all);
        if (soft_collision != 0) {
            overlap = std::min(
                overlap / static_cast<f32>(soft_collision) * delta_frames,
                overlap);
        }
        overlap *= 0.95f;
        if (overlap > 1.0f) overlap *= 0.7f;
        if (overlap > 3.0f) overlap = 3.0f + (overlap - 3.0f) * 0.7f;
        if (overlap > 6.0f) overlap = 6.0f + (overlap - 6.0f) * 0.7f;
        if (overlap > 10.0f) overlap = 10.0f + (overlap - 10.0f) * 0.7f;
        const f32 collision_heading = jar_direction_degrees(
            a_collision_x, a_collision_y,
            b_collision_x, b_collision_y);
        const f32 nx = jar_cosine_degrees(collision_heading);
        const f32 ny = jar_sine_degrees(collision_heading);

        const bool a_mobile = a.is_mobile();
        const bool b_mobile = b.is_mobile();
        f32 b_share = 0.0f;
        if (a_mobile && b_mobile) {
            f32 a_mass = std::max(0.0f, a.mass);
            f32 b_mass = std::max(0.0f, b.mass);
            if (a.team_id == b.team_id) {
                const f32 formation_factor =
                    (a.waypoint_stall_frames > 200.0f ||
                     b.waypoint_stall_frames > 200.0f)
                    ? 5.0f : 1.7f;
                if (a.formation_leader_uid == b.uid) {
                    b_mass *= formation_factor;
                } else if (b.formation_leader_uid == a.uid) {
                    a_mass *= formation_factor;
                } else if (a.formation_is_leader &&
                           b.formation_leader_uid != 0) {
                    a_mass *= formation_factor;
                } else if (b.formation_is_leader &&
                           a.formation_leader_uid != 0) {
                    b_mass *= formation_factor;
                } else if (a.formation_path_fallback_frames == 0.0f &&
                           b.formation_path_fallback_frames != 0.0f) {
                    a_mass *= formation_factor;
                } else if (b.formation_path_fallback_frames == 0.0f &&
                           a.formation_path_fallback_frames != 0.0f) {
                    b_mass *= formation_factor;
                }
            }
            const f32 total_mass = a_mass + b_mass;
            b_share = total_mass > 0.0f ? a_mass / total_mass : 0.5f;
        } else if (b_mobile) {
            b_share = 1.0f;
        }
        const f32 a_share = 1.0f - b_share;

        // y.a(am,float) always accumulates bZ/ca. The next y.a(float) turn
        // clamps and consumes that transient displacement, so collision
        // resolution must remain deferred until the following object turn.
        // Keep the bytecode's multiply order: it stores overlap * share in
        // local 25 before multiplying that value by the direction vector.
        if (a_mobile) {
            const f32 scaled_overlap = overlap * a_share;
            const f32 impulse_x = nx * scaled_overlap;
            const f32 impulse_y = ny * scaled_overlap;
            a.collision_impulse_x -= impulse_x;
            a.collision_impulse_y -= impulse_y;
        }
        if (b_mobile) {
            const f32 scaled_overlap = overlap * b_share;
            const f32 impulse_x = nx * scaled_overlap;
            const f32 impulse_y = ny * scaled_overlap;
            b.collision_impulse_x += impulse_x;
            b.collision_impulse_y += impulse_y;
        }

        a.collision_partner_uid = b.uid;
        a.collision_partner_team_id = b.team_id;
        a.collision_partner_tick = tick_;
        b.collision_partner_uid = a.uid;
        b.collision_partner_team_id = a.team_id;
        b.collision_partner_tick = other_world.tick_;

        // y.a(am,float) drops a same-formation MOVE/ATTACK_MOVE when the
        // colliding partner has no active waypoint. The formation controller
        // identity remains on a unit after y.ay(), unlike the order queue.
        // Cancel only after splitting the impulse: ay() clears the W/c
        // history that weighted this contact's masses above.
        if (a.formation_controller_id != 0 &&
            a.formation_controller_id == b.formation_controller_id) {
            Vec<World*> a_other_worlds;
            Vec<World*> b_other_worlds;
            if (&other_world != this) {
                a_other_worlds.push_back(&other_world);
                b_other_worlds.push_back(this);
            }
            const auto is_movement_order = [](const Unit& unit) {
                if (unit.order_queue.empty()) return false;
                const WeaponType type = unit.order_queue.front().type;
                return type == WeaponType::MOVE ||
                       type == WeaponType::ATTACK_MOVE;
            };
            const auto cancel_movement_order = [](
                    World& owner, Unit& moving,
                    const Vec<World*>& other_worlds) {
                const bool exposed_move = moving.has_move_target;
                const bool arrived = moving.arrived;
                // y.l/m still name the logical MOVE proposal here even when
                // a private fallback path last requested a different point.
                // ay() preserves that proposal after the collision drops the
                // last order; target_x/y may instead hold a physical node.
                const UnitOrder& active = moving.order_queue.front();
                const bool combat_proposal =
                    active.type == WeaponType::ATTACK_MOVE &&
                    moving.attack_move_combat_target_valid;
                const f32 exposed_target_x = combat_proposal
                    ? moving.attack_move_combat_target_x : active.x;
                const f32 exposed_target_y = combat_proposal
                    ? moving.attack_move_combat_target_y : active.y;
                owner.complete_current_order_(moving, other_worlds);
                if (moving.order_queue.empty()) {
                    // y.ay()/aH() retain the k/l/m selected by dispatch.
                    // An in-range attack can already have cleared k, even
                    // though contact still cancels its movement waypoint.
                    moving.has_move_target = exposed_move;
                    moving.arrived = arrived;
                    moving.target_x = exposed_move ? exposed_target_x : 0.0f;
                    moving.target_y = exposed_move ? exposed_target_y : 0.0f;
                    moving.clear_exposed_move_next_frame = exposed_move;
                }
            };
            if (a.order_queue.empty() && is_movement_order(b)) {
                cancel_movement_order(other_world, b, b_other_worlds);
            } else if (b.order_queue.empty() && is_movement_order(a)) {
                cancel_movement_order(*this, a, a_other_worlds);
            }
        }
    }
}


} // namespace rwrl
