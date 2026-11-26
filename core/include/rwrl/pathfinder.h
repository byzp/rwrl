#pragma once
#include <rwrl/common.h>
#include <rwrl/enums.h>
#include <rwrl/constants.h>
#include <rwrl/unit.h>
#include <queue>
#include <cstring>
#include <array>

namespace rwrl {

struct PathLayerSnapshot {
    Vec<u8> passable;
    Vec<i8> object_cost;
    Vec<i8> clearance;
};

struct BuildingClearanceState {
    Str type;
    std::array<i32, 12> tiles;
    bool operator==(const BuildingClearanceState&) const = default;
};

// k.l has one retained distance layer per movement type for all players.
struct PathClearanceState {
    Vec<i8> layers[5];
    u64 revision = 0;
    Map<u32, BuildingClearanceState> buildings;
    std::array<i32, 5> changes_since_sync{};
    std::array<bool, 5> deferred_rebuild{};
};

// Map tile properties used by game-lib.jar's path cost layers. Multiple
// flags may be present on a tile (for example a water bridge over water).
enum TerrainFlag : u8 {
    TERRAIN_NONE = 0,
    TERRAIN_WATER = 1u << 0,
    TERRAIN_WATER_BRIDGE = 1u << 1,
    TERRAIN_LAVA = 1u << 2,
    TERRAIN_CLIFF = 1u << 3,
    TERRAIN_LARGE_CLIFF = 1u << 4,
    TERRAIN_TREES = 1u << 5,
    TERRAIN_BLOCK_LAND = 1u << 6,
};

struct PathNode {
    i32 x, y;
    f32 g_cost, h_cost;
    i32 parent_x, parent_y;
    f32 f_cost() const { return g_cost + h_cost; }
};

class Pathfinder {
public:
    Pathfinder(i32 map_w = 120, i32 map_h = 120, f32 tile_size = 20.0f);

    void reset(i32 map_w, i32 map_h, f32 tile_size);

    Vec<std::pair<f32, f32>> find_path(f32 sx, f32 sy, f32 ex, f32 ey,
                                        MoveType mt, f32 unit_radius = 15.0f,
                                        f32 start_angle = 0.0f,
                                        bool flexible_reverse = false,
                                        bool prefer_clearance = false,
                                        i32 target_size = 0,
                                        i32 min_clearance = 0,
                                        const PathLayerSnapshot* snapshot = nullptr) const;

    SP<const PathLayerSnapshot> capture_path_layer(MoveType mt) const;
    i32 path_clearance(i32 tx, i32 ty, MoveType mt) const;
    void initialize_clearance_layers(bool force = false,
                                     bool preserve_water = false) const;
    void share_clearance_state(const SP<PathClearanceState>& state);
    void synchronize_clearance(MoveType mt);
    void reset_clearance_budget();

    bool is_blocked(i32 tx, i32 ty) const;
    void set_terrain(i32 tx, i32 ty, u8 flags);
    void set_tile_metadata(i32 tx, i32 ty, bool resource_pool,
                           bool block_buildings, i8 path_cost);
    u8 terrain(i32 tx, i32 ty) const;
    bool has_terrain(i32 tx, i32 ty) const;
    bool is_resource_pool_tile(i32 tx, i32 ty) const;
    bool is_resource_pool_world(f32 wx, f32 wy) const;
    bool blocks_buildings(i32 tx, i32 ty) const;
    i8 path_cost(i32 tx, i32 ty) const;
    i8 object_cost(i32 tx, i32 ty, MoveType mt = MoveType::LAND) const {
        if (tx < 0 || tx >= map_w_ || ty < 0 || ty >= map_h_) return 0;
        return object_costs_(mt)[ty * map_w_ + tx];
    }
    bool is_passable_tile(i32 tx, i32 ty, MoveType mt) const;
    bool is_passable_world(f32 wx, f32 wy, MoveType mt) const;
    // gameFramework.k.l.a()/b(): true when the tile is blocked with the
    // building layer applied but open without it. Units standing on such a
    // tile skip pathfinding and walk straight out (game.units.y.a()).
    bool is_building_blocked_tile(i32 tx, i32 ty, MoveType mt) const;
    // k.l.b(ao, x, y): passability with the building layer ignored, used by
    // the movement clamp's stand-on-blocked escape exception.
    bool is_passable_ignoring_buildings(i32 tx, i32 ty, MoveType mt) const {
        return is_passable_tile_(tx, ty, mt, true);
    }
    // game.units.aq.a(): Bresenham line walk between the two world points.
    // Fails on a blocked tile, on accumulated tile cost >= cost_limit, or on
    // clearance below min_clearance; the first `skip` tiles do not
    // accumulate cost.
    bool line_path_clear(f32 sx, f32 sy, f32 ex, f32 ey, MoveType mt,
                         i32 cost_limit = 80, i32 min_clearance = 0,
                         i32 skip = 1) const;
    bool is_buildable_world(f32 wx, f32 wy, bool allow_resource_pool) const;
    bool is_buildable_footprint(f32 wx, f32 wy,
                                const TileRect& footprint,
                                MoveType placement_type = MoveType::BUILDING,
                                f32 offset_x = 10.0f, f32 offset_y = 10.0f) const;
    void rebuild_from_buildings(const Map<u32, Unit>& units);
    void rebuild_from_buildings(
        const Vec<const Map<u32, Unit>*>& unit_sets);
    // gameFramework.k.i.e(): stationary-unit soft costs per ground move
    // type, refreshed lazily on the k.l.a() path-request gate rather than
    // on building changes. Layer contents are identical when refreshed at
    // the same frame; only the per-movetype refresh timing differs.
    void rebuild_object_costs(
        MoveType mt, const Vec<const Map<u32, Unit>*>& unit_sets,
        const Map<u32, bool>* movement_prediction_snapshot = nullptr);
    const Vec<i8>& object_cost_layer(MoveType mt) const {
        return object_costs_(mt);
    }
    void set_object_cost_layer(MoveType mt, const Vec<i8>& costs) {
        const i32 layer = ground_layer_index(mt);
        object_cost_layers_[layer < 0 ? 0 : layer] = costs;
    }
    // k.l.a(k2.o): every request resolves the layer for its move type.
    static i32 ground_layer_index(MoveType mt);
    // gameFramework.k.i.b(): terrain-only 4-connected component ids per
    // ground move type (-1 blocked, ids from 1). Buildings and units never
    // change groups; the layer rebuilds lazily after terrain edits.
    const Vec<i16>& connectivity_groups(MoveType mt) const;

    i32 width() const { return map_w_; }
    i32 height() const { return map_h_; }
    f32 tile_size() const { return tile_size_; }

    i32 tile_x(f32 wx) const { return clamp_i(static_cast<i32>(wx / tile_size_), 0, map_w_ - 1); }
    i32 tile_y(f32 wy) const { return clamp_i(static_cast<i32>(wy / tile_size_), 0, map_h_ - 1); }
    // b.b.a(float,float) also exposes raw indices for y.b(l)'s endpoint
    // check; unlike path requests, that conversion does not clamp to the map.
    i32 unclamped_tile_x(f32 wx) const {
        return static_cast<i32>(wx * (1.0f / tile_size_));
    }
    i32 unclamped_tile_y(f32 wy) const {
        return static_cast<i32>(wy * (1.0f / tile_size_));
    }
    f32 world_x(i32 tx) const { return (static_cast<f32>(tx) + 0.5f) * tile_size_; }
    f32 world_y(i32 ty) const { return (static_cast<f32>(ty) + 0.5f) * tile_size_; }
    i32 footprint_tile_x(f32 wx, f32 offset = 10.0f) const {
        return static_cast<i32>((wx - offset + 1.0f) * (1.0f / tile_size_));
    }
    i32 footprint_tile_y(f32 wy, f32 offset = 10.0f) const {
        return static_cast<i32>((wy - offset + 1.0f) * (1.0f / tile_size_));
    }

    static constexpr i32 DIRS = 8;
    // gameFramework.k.p direction encoding: east, south-east, south,
    // south-west, west, north-west, north, north-east.
    static constexpr i32 DX[8] = {1, 1, 0, -1, -1, -1, 0, 1};
    static constexpr i32 DY[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    // gameFramework.k.o adds 10/14 for cardinal/diagonal movement and then
    // one common step point before adding the destination tile's g.j cost.
    static constexpr f32 D_COST[8] = {
        11.0f, 15.0f, 11.0f, 15.0f,
        11.0f, 15.0f, 11.0f, 15.0f};

private:
    i32 map_w_, map_h_;
    f32 tile_size_;
    // Explicit API/map obstacles are stable across dynamic building rebuilds.
    Vec<bool> blocked_;
    Vec<bool> unit_blocked_;
    Vec<bool> building_unit_blocked_;
    Vec<u8> terrain_;
    Vec<bool> terrain_set_;
    Vec<bool> resource_pool_;
    Vec<bool> block_buildings_;
    Vec<i8> path_cost_;
    // One object-cost layer per ground move type (k.i per-movetype layers).
    Vec<i8> object_cost_layers_[5];
    // k.i.a(null) generates all five j layers with outside seeds at -1 and
    // size+1. k.i.b(y) replaces only the building's local rectangle, using
    // every outside tile as an obstacle; k.l.a(y) omits WATER altogether.
    // Queries and async requests must retain these different refresh results.
    mutable SP<PathClearanceState> clearance_state_ = mk<PathClearanceState>();
    // Terrain edit counter driving the lazy connectivity-group rebuild.
    u64 terrain_revision_ = 1;
    mutable u64 group_revision_[5] = {0, 0, 0, 0, 0};
    mutable Vec<i16> group_layers_[5];
    const Vec<i8>& object_costs_(MoveType mt) const {
        const i32 layer = ground_layer_index(mt);
        return object_cost_layers_[layer < 0 ? 0 : layer];
    }

    // gameFramework.k.o.e() bidirectional search: alternates forward/reverse
    // every BIDIR_PHASE_STEPS node expansions. Forward targets the goal;
    // reverse targets the start. Both use identical cost model.
    static constexpr i32 BIDIR_PHASE_STEPS = 400;

    bool passable_(i32 tx, i32 ty, MoveType mt) const;
    bool is_passable_tile_(i32 tx, i32 ty, MoveType mt,
                           bool ignore_buildings) const;
    i32 local_clearance_(i32 tx, i32 ty, MoveType mt) const;
    void refresh_building_clearance_(const BuildingClearanceState& building);
    void initialize_clearance_layer_(i32 layer) const;
    void detach_clearance_state_();
    void mark_footprint_(Vec<bool>& layer, i32 cx, i32 cy,
                         const TileRect& footprint);
};

}
