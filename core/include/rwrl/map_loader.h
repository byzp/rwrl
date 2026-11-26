#pragma once

#include <rwrl/common.h>
#include <rwrl/enums.h>

namespace rwrl {

struct MapUnitSpawn {
    Str unit_type;
    bool custom_unit = false;
    i32 team_id = -1;
    f32 x = 0.0f;
    f32 y = 0.0f;
    bool random_rotate = false;
    Str subtype;
};

struct MapData {
    i32 width = 0;
    i32 height = 0;
    // game.b.b keeps 20x20 runtime tiles even when TMX declares another
    // source tile size.
    f32 tile_size = 20.0f;
    std::optional<FogMode> fog_mode;
    Vec<u8> terrain;
    Vec<bool> terrain_set;
    Vec<bool> resource_pool;
    Vec<bool> block_buildings;
    Vec<i8> path_cost;
    Vec<MapUnitSpawn> units;
};

MapData load_tmx_map(const Str& path);

} // namespace rwrl
