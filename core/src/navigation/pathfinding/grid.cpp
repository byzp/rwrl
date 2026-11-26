#include <rwrl/pathfinder.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <array>
#include <limits>

namespace rwrl {

Pathfinder::Pathfinder(i32 map_w, i32 map_h, f32 tile_size)
    : map_w_(0), map_h_(0), tile_size_(tile_size) {
    reset(map_w, map_h, tile_size);
}

void Pathfinder::reset(i32 map_w, i32 map_h, f32 tile_size) {
    if (map_w <= 0 || map_h <= 0 || tile_size <= 0.0f) return;
    map_w_ = map_w;
    map_h_ = map_h;
    tile_size_ = tile_size;
    const std::size_t count = static_cast<std::size_t>(map_w_) * map_h_;
    blocked_.assign(count, false);
    unit_blocked_.assign(count, false);
    building_unit_blocked_.assign(count, false);
    terrain_.assign(count, TERRAIN_NONE);
    terrain_set_.assign(count, false);
    resource_pool_.assign(count, false);
    block_buildings_.assign(count, false);
    path_cost_.assign(count, 0);
    clearance_state_ = mk<PathClearanceState>();
    for (Vec<i8>& layer : object_cost_layers_) layer.assign(count, 0);
    ++terrain_revision_;
}

bool Pathfinder::passable_(i32 tx, i32 ty, MoveType mt) const {
    if (tx < 0 || tx >= map_w_ || ty < 0 || ty >= map_h_) return false;
    return is_passable_tile(tx, ty, mt);
}

void Pathfinder::mark_footprint_(Vec<bool>& layer, i32 cx, i32 cy,
                                 const TileRect& footprint) {
    for (i32 tx = cx + footprint.left; tx <= cx + footprint.right; ++tx) {
        for (i32 ty = cy + footprint.top; ty <= cy + footprint.bottom; ++ty) {
            if (tx >= 0 && tx < map_w_ && ty >= 0 && ty < map_h_) {
                layer[ty * map_w_ + tx] = true;
            }
        }
    }
}

bool Pathfinder::is_blocked(i32 tx, i32 ty) const {
    if (tx < 0 || tx >= map_w_ || ty < 0 || ty >= map_h_) return true;
    const i32 index = ty * map_w_ + tx;
    return blocked_[index] || unit_blocked_[index];
}

void Pathfinder::set_terrain(i32 tx, i32 ty, u8 flags) {
    if (tx >= 0 && tx < map_w_ && ty >= 0 && ty < map_h_) {
        detach_clearance_state_();
        const i32 index = ty * map_w_ + tx;
        terrain_[index] = flags;
        terrain_set_[index] = true;
        ++terrain_revision_;
    }
}

void Pathfinder::set_tile_metadata(i32 tx, i32 ty, bool resource_pool,
                                   bool block_buildings, i8 path_cost_value) {
    if (tx < 0 || tx >= map_w_ || ty < 0 || ty >= map_h_) return;
    detach_clearance_state_();
    const i32 index = ty * map_w_ + tx;
    resource_pool_[index] = resource_pool;
    block_buildings_[index] = block_buildings;
    path_cost_[index] = path_cost_value;
    ++terrain_revision_;
}

u8 Pathfinder::terrain(i32 tx, i32 ty) const {
    if (tx < 0 || tx >= map_w_ || ty < 0 || ty >= map_h_) {
        return TERRAIN_LARGE_CLIFF;
    }
    return terrain_[ty * map_w_ + tx];
}

bool Pathfinder::has_terrain(i32 tx, i32 ty) const {
    if (tx < 0 || tx >= map_w_ || ty < 0 || ty >= map_h_) return false;
    return terrain_set_[ty * map_w_ + tx];
}

bool Pathfinder::is_resource_pool_tile(i32 tx, i32 ty) const {
    if (tx < 0 || tx >= map_w_ || ty < 0 || ty >= map_h_) return false;
    return resource_pool_[ty * map_w_ + tx];
}

bool Pathfinder::is_resource_pool_world(f32 wx, f32 wy) const {
    if (wx < 0.0f || wy < 0.0f ||
        wx >= static_cast<f32>(map_w_) * tile_size_ ||
        wy >= static_cast<f32>(map_h_) * tile_size_) return false;
    return is_resource_pool_tile(tile_x(wx), tile_y(wy));
}

bool Pathfinder::blocks_buildings(i32 tx, i32 ty) const {
    if (tx < 0 || tx >= map_w_ || ty < 0 || ty >= map_h_) return true;
    return block_buildings_[ty * map_w_ + tx];
}

i8 Pathfinder::path_cost(i32 tx, i32 ty) const {
    if (tx < 0 || tx >= map_w_ || ty < 0 || ty >= map_h_) return -1;
    return path_cost_[ty * map_w_ + tx];
}

bool Pathfinder::is_passable_tile(i32 tx, i32 ty, MoveType mt) const {
    return is_passable_tile_(tx, ty, mt, false);
}

bool Pathfinder::is_building_blocked_tile(i32 tx, i32 ty, MoveType mt) const {
    if (tx < 0 || tx >= map_w_ || ty < 0 || ty >= map_h_) return false;
    if (mt == MoveType::AIR || mt == MoveType::BUILDING) return false;
    if (is_passable_tile_(tx, ty, mt, false)) return false;
    return is_passable_tile_(tx, ty, mt, true);
}

void Pathfinder::initialize_clearance_layers(bool force,
                                             bool preserve_water) const {
    const std::size_t count = static_cast<std::size_t>(map_w_) * map_h_;
    if (!force && clearance_state_->revision == terrain_revision_ &&
        clearance_state_->layers[0].size() == count) return;
    constexpr MoveType moves[] = {MoveType::LAND, MoveType::WATER,
        MoveType::HOVER, MoveType::OVER_CLIFF, MoveType::OVER_CLIFF_WATER};
    for (i32 layer = 0; layer < 5; ++layer) {
        const MoveType mt = moves[layer];
        Vec<i8>& values = clearance_state_->layers[layer];
        // gameSaver finishes loading with k.l.a(null). It regenerates the
        // four ground clearance layers, leaving the map's WATER layer intact.
        if (preserve_water && mt == MoveType::WATER && values.size() == count) {
            continue;
        }
        initialize_clearance_layer_(layer);
    }
    clearance_state_->revision = terrain_revision_;
}

void Pathfinder::initialize_clearance_layer_(i32 layer) const {
    constexpr MoveType moves[] = {MoveType::LAND, MoveType::WATER,
        MoveType::HOVER, MoveType::OVER_CLIFF, MoveType::OVER_CLIFF_WATER};
    const MoveType mt = moves[layer];
    auto& values = clearance_state_->layers[layer];
    values.assign(static_cast<std::size_t>(map_w_) * map_h_, 0);
    for (i32 ty = 0; ty < map_h_; ++ty) {
        for (i32 tx = 0; tx < map_w_; ++tx) {
            if (!is_passable_tile(tx, ty, mt)) continue;
            i32 value = std::min({4, tx + 1, ty + 1,
                                  map_w_ + 1 - tx, map_h_ + 1 - ty});
            for (i32 ox = -3; ox <= 3; ++ox) {
                for (i32 oy = -3; oy <= 3; ++oy) {
                    const i32 cx = tx + ox, cy = ty + oy;
                    if (cx < 0 || cy < 0 || cx >= map_w_ || cy >= map_h_) continue;
                    if (is_passable_tile(cx, cy, mt)) continue;
                    value = std::min(value, std::max(std::abs(ox), std::abs(oy)));
                }
            }
            values[ty * map_w_ + tx] = static_cast<i8>(value);
        }
    }
}

void Pathfinder::share_clearance_state(const SP<PathClearanceState>& state) {
    if (!state) return;
    const std::size_t count = static_cast<std::size_t>(map_w_) * map_h_;
    if (state->layers[0].size() != count) *state = *clearance_state_;
    clearance_state_ = state;
}

void Pathfinder::detach_clearance_state_() {
    // Keep explicit terrain edits local while Engine Worlds share map layers.
    if (!clearance_state_.unique()) {
        clearance_state_ = mk<PathClearanceState>(*clearance_state_);
    }
}

void Pathfinder::reset_clearance_budget() {
    clearance_state_->changes_since_sync.fill(0);
    clearance_state_->deferred_rebuild.fill(false);
}

void Pathfinder::synchronize_clearance(MoveType mt) {
    const i32 layer = ground_layer_index(mt);
    if (layer < 0) return;
    if (clearance_state_->deferred_rebuild[layer]) {
        // i.a(true) first calls c(null), then a(null), for this layer only.
        // The building cache is global even when this World's direct API
        // has not otherwise synchronized its local occupancy yet.
        std::fill(unit_blocked_.begin(), unit_blocked_.end(), false);
        std::fill(building_unit_blocked_.begin(), building_unit_blocked_.end(), false);
        for (const auto& [_, building] : clearance_state_->buildings) {
            const auto& tile = building.tiles;
            mark_footprint_(unit_blocked_, tile[2], tile[3],
                            {tile[4], tile[5], tile[6], tile[7]});
            mark_footprint_(building_unit_blocked_, tile[2], tile[3],
                            {tile[8], tile[9], tile[10], tile[11]});
        }
        initialize_clearance_layer_(layer);
        clearance_state_->deferred_rebuild[layer] = false;
    }
    // i.a(false) leaves this counter untouched; only synchronous path
    // layer access resets it, independently of the object refresh clock.
    clearance_state_->changes_since_sync[layer] = 0;
}

i32 Pathfinder::path_clearance(i32 tx, i32 ty, MoveType mt) const {
    if (tx < 0 || ty < 0 || tx >= map_w_ || ty >= map_h_) return 0;
    const i32 layer = ground_layer_index(mt);
    if (layer >= 0) {
        initialize_clearance_layers();
        return clearance_state_->layers[layer][ty * map_w_ + tx];
    }
    return local_clearance_(tx, ty, mt);
}

i32 Pathfinder::local_clearance_(i32 tx, i32 ty, MoveType mt) const {
    if (!is_passable_tile(tx, ty, mt)) return 0;
    i32 value = 4;
    for (i32 ox = -3; ox <= 3; ++ox) {
        for (i32 oy = -3; oy <= 3; ++oy) {
            if (is_passable_tile(tx + ox, ty + oy, mt)) continue;
            value = std::min(value,
                std::max(std::abs(ox), std::abs(oy)));
        }
    }
    return value;
}

void Pathfinder::refresh_building_clearance_(
        const BuildingClearanceState& building) {
    const auto& tile = building.tiles;
    const i32 left = std::max(0, tile[0] - 5 + tile[4]);
    const i32 top = std::max(0, tile[1] - 5 + tile[5]);
    const i32 right = std::min(map_w_, tile[0] + 5 + tile[6]);
    const i32 bottom = std::min(map_h_, tile[1] + 5 + tile[7]);
    constexpr MoveType moves[] = {MoveType::LAND, MoveType::WATER,
        MoveType::HOVER, MoveType::OVER_CLIFF, MoveType::OVER_CLIFF_WATER};
    for (i32 layer = 0; layer < 5; ++layer) {
        if (moves[layer] == MoveType::WATER) continue;
        if (++clearance_state_->changes_since_sync[layer] > 50) {
            clearance_state_->deferred_rebuild[layer] = true;
            continue;
        }
        for (i32 ty = top; ty < bottom; ++ty) {
            for (i32 tx = left; tx < right; ++tx) {
                clearance_state_->layers[layer][ty * map_w_ + tx] =
                    static_cast<i8>(local_clearance_(tx, ty, moves[layer]));
            }
        }
    }
}

bool Pathfinder::line_path_clear(f32 sx, f32 sy, f32 ex, f32 ey, MoveType mt,
                                 i32 cost_limit, i32 min_clearance,
                                 i32 skip) const {
    // game.units.aq.a(): integer Bresenham walk visiting 1+dx+dy tiles.
    // b.b.a(float,float) truncates without clamping, so off-map endpoints
    // produce out-of-bounds tiles that fail the per-tile passability check
    // exactly like the JAR (which runs this on the raw request point).
    const i32 x1 = static_cast<i32>(sx / tile_size_);
    const i32 y1 = static_cast<i32>(sy / tile_size_);
    const i32 x2 = static_cast<i32>(ex / tile_size_);
    const i32 y2 = static_cast<i32>(ey / tile_size_);
    const i32 dx = std::abs(x2 - x1);
    const i32 dy = std::abs(y2 - y1);
    const i32 step_x = x2 > x1 ? 1 : -1;
    const i32 step_y = y2 > y1 ? 1 : -1;
    i32 budget = 1 + dx + dy;
    i32 error = dx - dy;
    const i32 dx2 = dx * 2, dy2 = dy * 2;
    i32 x = x1, y = y1;
    i32 accumulated = 0;
    auto clearance_at = [&](i32 tx, i32 ty) {
        return path_clearance(tx, ty, mt);
    };
    while (budget > 0) {
        if (min_clearance != 0 && clearance_at(x, y) < min_clearance) {
            return false;
        }
        if (cost_limit != 0) {
            if (!is_passable_tile(x, y, mt)) return false;
            const i32 tile_cost = std::max<i32>(0, path_cost(x, y)) +
                static_cast<i32>(object_costs_(mt)[y * map_w_ + x]) * 10;
            if (skip > 0) {
                --skip;
            } else {
                accumulated += tile_cost;
            }
            if (accumulated >= cost_limit) return false;
        } else if (!is_passable_tile(x, y, mt)) {
            return false;
        }
        if (error > 0) {
            x += step_x;
            error -= dy2;
        } else if (error < 0) {
            y += step_y;
            error += dx2;
        } else {
            x += step_x;
            y += step_y;
            error += dx2 - dy2;
            --budget;
        }
        --budget;
    }
    return true;
}

i32 Pathfinder::ground_layer_index(MoveType mt) {
    switch (mt) {
    case MoveType::LAND: return 0;
    case MoveType::WATER: return 1;
    case MoveType::HOVER: return 2;
    case MoveType::OVER_CLIFF: return 3;
    case MoveType::OVER_CLIFF_WATER: return 4;
    default: return -1;
    }
}

const Vec<i16>& Pathfinder::connectivity_groups(MoveType mt) const {
    static const Vec<i16> no_groups;
    const i32 layer = ground_layer_index(mt);
    if (layer < 0) return no_groups;
    if (group_revision_[layer] == terrain_revision_ &&
        !group_layers_[layer].empty()) {
        return group_layers_[layer];
    }
    Vec<i16>& groups = group_layers_[layer];
    groups.assign(static_cast<std::size_t>(map_w_) * map_h_, 0);
    Vec<i32> pending;
    i16 next_id = 1;
    for (i32 sx = 0; sx < map_w_; ++sx) {
        for (i32 sy = 0; sy < map_h_; ++sy) {
            const i32 start = sy * map_w_ + sx;
            if (groups[start] != 0) continue;
            if (!is_passable_tile_(sx, sy, mt, true)) {
                groups[start] = -1;
                continue;
            }
            groups[start] = next_id;
            pending.clear();
            pending.push_back(start);
            while (!pending.empty()) {
                const i32 index = pending.back();
                pending.pop_back();
                const i32 cx = index % map_w_, cy = index / map_w_;
                const i32 neighbors[4][2] = {
                    {cx - 1, cy}, {cx + 1, cy}, {cx, cy - 1}, {cx, cy + 1}};
                for (const auto& n : neighbors) {
                    if (n[0] < 0 || n[0] >= map_w_ ||
                        n[1] < 0 || n[1] >= map_h_) continue;
                    const i32 ni = n[1] * map_w_ + n[0];
                    if (groups[ni] != 0) continue;
                    if (!is_passable_tile_(n[0], n[1], mt, true)) {
                        groups[ni] = -1;
                        continue;
                    }
                    groups[ni] = next_id;
                    pending.push_back(ni);
                }
            }
            if (next_id < 32767) ++next_id;
        }
    }
    group_revision_[layer] = terrain_revision_;
    return groups;
}

bool Pathfinder::is_passable_tile_(i32 tx, i32 ty, MoveType mt,
                                   bool ignore_buildings) const {
    if (tx < 0 || tx >= map_w_ || ty < 0 || ty >= map_h_) return false;
    if (mt == MoveType::AIR) return true;
    const i32 index = ty * map_w_ + tx;
    if (blocked_[index]) return false;
    if (mt == MoveType::BUILDING) {
        if (building_unit_blocked_[index]) return false;
    } else if (!ignore_buildings && unit_blocked_[index]) {
        return false;
    }
    // Tile metadata belongs to the JAR cost layer, independently of whether
    // a base terrain tile was installed through the standalone API.
    if (path_cost_[index] < 0) return false;
    if (mt == MoveType::BUILDING &&
        (resource_pool_[index] || block_buildings_[index])) return false;
    // gameFramework.k.i copies res_pool into the LAND path cost layer. The
    // placement layer also rejects it for ordinary buildings; extractor has
    // a dedicated resource-pool placement path in World.
    if (mt == MoveType::LAND && resource_pool_[index]) return false;
    // Worlds created through the standalone API have no map layer by
    // default. Preserve open terrain only after applying explicit metadata.
    if (!terrain_set_[index]) return true;
    const u8 flags = terrain_[index];
    const bool water = (flags & TERRAIN_WATER) != 0;
    const bool bridge = (flags & TERRAIN_WATER_BRIDGE) != 0;
    const bool lava = (flags & TERRAIN_LAVA) != 0;
    const bool cliff = (flags & TERRAIN_CLIFF) != 0;
    const bool large_cliff = (flags & TERRAIN_LARGE_CLIFF) != 0;
    const bool trees = (flags & TERRAIN_TREES) != 0;
    // Tile g.j is copied into every non-air JAR cost layer. block-land sets
    // it to -1, so despite its name it also blocks water and hover movement.
    if ((flags & TERRAIN_BLOCK_LAND) != 0) return false;
    switch (mt) {
    case MoveType::WATER:
        // JAR water units accept water and water bridges, but reject land,
        // cliffs, trees and lava. A bridge itself is a valid water tile.
        return !lava && !cliff && !large_cliff && !trees && (water || bridge);
    case MoveType::HOVER:
        // JAR hover units share the water and ordinary-cliff layers, but
        // large cliffs/trees and lava still block them.
        return !lava && !large_cliff && !trees;
    case MoveType::OVER_CLIFF:
        return !water && !lava;
    case MoveType::OVER_CLIFF_WATER:
        return !lava;
    case MoveType::LAND:
        return !water && !lava && !cliff && !large_cliff && !trees;
    case MoveType::BUILDING:
        // The JAR BUILDING layer uses the same base terrain rejection as
        // LAND (water bridges remain ordinary ground unless another flag is
        // present), then applies the building occupancy layer above it.
        return !water && !lava && !cliff && !large_cliff && !trees;
    case MoveType::NONE:
    case MoveType::ROOT:
        return false;
    }
    return false;
}

bool Pathfinder::is_passable_world(f32 wx, f32 wy, MoveType mt) const {
    // utility.y.a(am,x,y) uses b.a(float,float) before k.l's tile bounds
    // check. A negative fraction of the first tile still truncates to zero;
    // it must not trigger a different factory-exit or unload candidate.
    return passable_(unclamped_tile_x(wx), unclamped_tile_y(wy), mt);
}

bool Pathfinder::is_buildable_world(f32 wx, f32 wy,
                                    bool allow_resource_pool) const {
    if (wx < 0.0f || wy < 0.0f ||
        wx >= static_cast<f32>(map_w_) * tile_size_ ||
        wy >= static_cast<f32>(map_h_) * tile_size_) return false;
    const i32 tx = tile_x(wx), ty = tile_y(wy);
    const i32 index = ty * map_w_ + tx;
    if (!allow_resource_pool || !resource_pool_[index]) {
        return is_passable_tile(tx, ty, MoveType::BUILDING);
    }
    if (blocked_[index] || building_unit_blocked_[index] ||
        block_buildings_[index] || path_cost_[index] < 0) {
        return false;
    }
    if (!terrain_set_[index]) return true;
    const u8 flags = terrain_[index];
    return (flags & (TERRAIN_WATER | TERRAIN_LAVA | TERRAIN_CLIFF |
                     TERRAIN_LARGE_CLIFF | TERRAIN_TREES |
                     TERRAIN_BLOCK_LAND)) == 0;
}

bool Pathfinder::is_buildable_footprint(
        f32 wx, f32 wy, const TileRect& footprint, MoveType placement_type,
        f32 offset_x, f32 offset_y) const {
    const i32 cx = footprint_tile_x(wx, offset_x);
    const i32 cy = footprint_tile_y(wy, offset_y);
    for (i32 tx = cx + footprint.left; tx <= cx + footprint.right; ++tx) {
        for (i32 ty = cy + footprint.top; ty <= cy + footprint.bottom; ++ty) {
            if (!is_passable_tile(tx, ty, placement_type)) return false;
        }
    }
    return true;
}

void Pathfinder::rebuild_from_buildings(const Map<u32, Unit>& units) {
    rebuild_from_buildings(Vec<const Map<u32, Unit>*>{&units});
}

// gameFramework.k.i.c(y): the building layer rebuilds immediately on every
// building change. (The JAR skips after 50 same-frame events and catches up
// next frame via k.l.b(float); C++ stays eager, which is equivalent below
// that threshold.) The object-cost layer refreshes separately on the k.l.a()
// path-request gate; see rebuild_object_costs().
void Pathfinder::rebuild_from_buildings(
        const Vec<const Map<u32, Unit>*>& unit_sets) {
    // Preserve the pre-event full layer before replacing occupancy. A sync
    // with identical buildings must not turn into another clearance refresh.
    initialize_clearance_layers();
    Map<u32, BuildingClearanceState> buildings;
    std::fill(unit_blocked_.begin(), unit_blocked_.end(), false);
    std::fill(building_unit_blocked_.begin(), building_unit_blocked_.end(), false);
    for (const Map<u32, Unit>* units : unit_sets) {
        if (!units) continue;
        for (const auto& [_, u] : *units) {
            if (!u.is_alive || !u.is_building) continue;
            // am.a(map, Point) uses cZ()/da() for both occupancy layers.
            const i32 cx = footprint_tile_x(u.x, u.building_to_footprint_offset_x);
            const i32 cy = footprint_tile_y(u.y, u.building_to_footprint_offset_y);
            mark_footprint_(unit_blocked_, cx, cy, u.collision_footprint);
            mark_footprint_(building_unit_blocked_, cx, cy,
                            u.placement_footprint);
            const TileRect& collision = u.collision_footprint;
            const TileRect& placement = u.placement_footprint;
            buildings.emplace(u.uid, BuildingClearanceState{u.unit_type,
                {unclamped_tile_x(u.x), unclamped_tile_y(u.y), cx, cy,
                 collision.left, collision.top, collision.right, collision.bottom,
                 placement.left, placement.top, placement.right, placement.bottom}});
        }
    }
    for (const auto& [uid, previous] : clearance_state_->buildings) {
        if (!buildings.contains(uid)) refresh_building_clearance_(previous);
    }
    for (const auto& [uid, building] : buildings) {
        const auto previous = clearance_state_->buildings.find(uid);
        // custom.j conversion keeps the native distance grids when its
        // footprint and position are unchanged. Refreshing solely for a
        // prototype name change replaces full-map boundary values locally.
        if (previous == clearance_state_->buildings.end() ||
            previous->second.tiles != building.tiles) {
            refresh_building_clearance_(building);
        }
    }
    clearance_state_->buildings = std::move(buildings);
}

void Pathfinder::rebuild_object_costs(
        MoveType mt, const Vec<const Map<u32, Unit>*>& unit_sets,
        const Map<u32, bool>* movement_prediction_snapshot) {
    const i32 layer_index = ground_layer_index(mt);
    Vec<i8>& layer = object_cost_layers_[layer_index < 0 ? 0 : layer_index];
    std::fill(layer.begin(), layer.end(), 0);
    for (const Map<u32, Unit>* units : unit_sets) {
        if (!units) continue;
        for (const auto& [_, u] : *units) {
            if (!u.is_alive || u.is_building) continue;

            // gameFramework.k.i.e() uses am.bT, i() and Q(), independently
            // of completion and movement type. Landed dropships and custom
            // aircraft therefore contribute to the surface object layer.
            const bool height_controls_airborne =
                u.custom_unit || u.unit_type == "dropship";
            const bool airborne = height_controls_airborne
                ? u.move_type == MoveType::AIR && u.height >= 4.0f
                : u.airborne;
            const bool submerged = u.custom_unit
                ? u.height <= -1.0f : u.submerged;
            bool movement_prediction = u.movement_prediction_enabled;
            if (movement_prediction_snapshot) {
                const auto snapshot =
                    movement_prediction_snapshot->find(u.uid);
                if (snapshot != movement_prediction_snapshot->end()) {
                    movement_prediction = snapshot->second;
                }
            }
            if (!u.collision_enabled || movement_prediction ||
                u.parent_uid != 0 || u.unit_type == "tree" ||
                airborne || submerged) {
                continue;
            }
            const f32 inner_radius = u.radius + 5.0f;
            const f32 outer_radius = u.radius + 10.0f;
            i32 extent = 2;
            if (outer_radius < 10.0f) extent = 0;
            else if (outer_radius < 20.0f) extent = 1;
            const i32 center_x = tile_x(u.x);
            const i32 center_y = tile_y(u.y);
            for (i32 tx = center_x - extent; tx <= center_x + extent; ++tx) {
                for (i32 ty = center_y - extent; ty <= center_y + extent; ++ty) {
                    if (tx < 0 || tx >= map_w_ || ty < 0 || ty >= map_h_) {
                        continue;
                    }
                    const f32 distance =
                        (world_x(tx) - u.x) * (world_x(tx) - u.x) +
                        (world_y(ty) - u.y) * (world_y(ty) - u.y);
                    const i32 index = ty * map_w_ + tx;
                    i32 addition = 0;
                    if (distance < inner_radius * inner_radius) addition = 6;
                    else if (distance < outer_radius * outer_radius) addition = 1;
                    if (addition == 0) continue;
                    layer[index] = static_cast<i8>(std::min(
                        127, static_cast<i32>(layer[index]) + addition));
                }
            }
        }
    }
}

}
