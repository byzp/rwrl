#pragma once
#include <rwrl/common.h>
#include <rwrl/constants.h>
#include <rwrl/unit.h>

namespace rwrl {

class SpatialGrid {
public:
    SpatialGrid(i32 w = EngineConstants::SPATIAL_GRID_SIZE,
                i32 h = EngineConstants::SPATIAL_GRID_SIZE,
                f32 cs = EngineConstants::SPATIAL_CELL_SIZE);

    void add(Unit& u);
    void remove(Unit& u);
    void update(Unit& u);

    Vec<u32> query_circle(f32 x, f32 y, f32 radius) const;
    std::optional<u32> find_nearest_enemy(const Unit& unit,
                                          const Map<u32, Unit>& enemy_units) const;
    std::optional<u32> find_nearest_enemy(const Unit& unit,
                                          const Map<u32, Unit>& enemy_units,
                                          bool air_only) const;
    Vec<u32> query_circle_by_move_type(f32 x, f32 y, f32 radius,
                                        MoveType mt, const Map<u32, Unit>& units) const;

private:
    i32 w_, h_;
    f32 cs_;
    std::unordered_map<std::pair<i32,i32>, Vec<u32>, PairHash> cells_;
    Map<u32, std::pair<i32,i32>> pos_;

    std::pair<i32,i32> cell_(f32 x, f32 y) const;
    i32 clamp_cx_(i32 cx) const { return clamp_i(cx, 0, w_ - 1); }
    i32 clamp_cy_(i32 cy) const { return clamp_i(cy, 0, h_ - 1); }
};

}
