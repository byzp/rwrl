#include "world_internal.h"

namespace rwrl {
namespace {

constexpr u8 FOG_VISIBLE_LIMIT = 5;
constexpr u8 FOG_EXPLORED = 5;
constexpr u8 FOG_UNEXPLORED = 10;
constexpr f32 FOG_LOS_REFRESH_FRAMES = 260.0f;
constexpr f32 FOG_INITIAL_REFRESH_FRAMES = 150.0f;

bool is_jar_fog_observer(const Unit& unit) {
    if (!unit.is_active() || unit.sight_range <= 0.0f) return false;
    // These stock map objects extend am/v rather than the orderable y class,
    // so y.c(boolean) never contributes their inherited default sight value.
    return unit.unit_type != "tree" &&
           unit.unit_type != "crystalResource" &&
           unit.unit_type != "spreadingFire";
}

} // namespace

void World::set_fog_enabled(bool enabled) {
    set_fog_mode(enabled ? FogMode::LOS : FogMode::NONE);
}

void World::set_fog_mode(FogMode mode) {
    if (fog_mode_ == mode) return;
    fog_mode_ = mode;
    // game.b.b initializes ar to 150 when a map is loaded, so the first LOS
    // rebuild occurs after 111 frames; later rebuilds use the full 261-frame
    // (>260) interval.
    fog_refresh_frames_ = mode == FogMode::LOS
        ? FOG_INITIAL_REFRESH_FRAMES : 0.0f;
    if (mode == FogMode::NONE) return;

    std::fill(fog_tiles_.begin(), fog_tiles_.end(), FOG_UNEXPLORED);
    for (const auto& [_, unit] : units_) reveal_visibility_(unit);
}

void World::reveal_visibility_(const Unit& observer) {
    if (!fog_enabled() || !is_jar_fog_observer(observer)) return;

    const f32 tile_size = pathfinder_.tile_size();
    const i32 map_width = pathfinder_.width();
    const i32 map_height = pathfinder_.height();
    const i32 radius = static_cast<i32>(observer.sight_range / tile_size);
    if (radius <= 0) return;

    // game.b.b.b(float,float,int,n,boolean): the center keeps its fractional
    // tile coordinate, while the iteration bounds use the truncated tile.
    const f32 center_x = observer.x / tile_size;
    const f32 center_y = observer.y / tile_size;
    const i32 tile_x = static_cast<i32>(center_x);
    const i32 tile_y = static_cast<i32>(center_y);
    const i32 extent = radius - 1;
    const i32 min_x = std::max(0, tile_x - extent);
    const i32 min_y = std::max(0, tile_y - extent);
    const i32 max_x = std::min(map_width - 1, tile_x + extent);
    const i32 max_y = std::min(map_height - 1, tile_y + extent);
    const f32 inner_sq = static_cast<f32>((radius - 3) * (radius - 3));
    const f32 outer_sq = static_cast<f32>(radius * radius);
    const f32 gradient = 10.0f / (outer_sq - inner_sq);

    for (i32 tx = min_x; tx <= max_x; ++tx) {
        for (i32 ty = min_y; ty <= max_y; ++ty) {
            u8& fog = fog_tiles_[ty * map_width + tx];
            if (fog == 0) continue;
            const f32 dx = center_x - static_cast<f32>(tx);
            const f32 dy = center_y - static_cast<f32>(ty);
            const f32 distance_sq = dx * dx + dy * dy;
            if (distance_sq <= inner_sq) {
                fog = 0;
            } else if (distance_sq <= outer_sq) {
                const u8 edge = static_cast<u8>(
                    static_cast<i32>((distance_sq - inner_sq) * gradient));
                if (fog > edge) fog = edge;
            }
        }
    }
}

void World::decay_visibility_() {
    for (u8& fog : fog_tiles_) {
        if (fog < FOG_VISIBLE_LIMIT) fog = FOG_EXPLORED;
    }
}

void World::update_visibility_(f32 delta_frames,
                               const Vec<World*>& other_worlds) {
    if (!fog_enabled()) return;

    if (fog_mode_ == FogMode::LOS) {
        fog_refresh_frames_ += delta_frames;
        if (fog_refresh_frames_ > FOG_LOS_REFRESH_FRAMES) {
            fog_refresh_frames_ = 0.0f;
            decay_visibility_();
        }
    }

    for (const auto& [_, unit] : units_) reveal_visibility_(unit);
    // game.b.b.a(...) writes the same reveal into every allied team's fog
    // map. Re-read allied observers here so world update order cannot drop a
    // reveal when this world's 260-frame decay happens later in the tick.
    for (const World* other : other_worlds) {
        if (!other || other == this || !is_allied_with(*other)) continue;
        for (const auto& [_, unit] : other->units_) reveal_visibility_(unit);
    }
}

bool World::can_see(const Unit& target) const {
    if (!fog_enabled() || target.team_id == team_id_) return true;
    const i32 map_width = pathfinder_.width();
    const i32 map_height = pathfinder_.height();
    const f32 tile_size = pathfinder_.tile_size();
    const i32 tx = static_cast<i32>(target.x / tile_size);
    const i32 ty = static_cast<i32>(target.y / tile_size);
    if (tx < 0 || ty < 0 || tx >= map_width || ty >= map_height) {
        // am.d(n) only applies fog rejection when the converted tile is in
        // bounds; off-map objects therefore pass the JAR visibility gate.
        return true;
    }
    return fog_tiles_[ty * map_width + tx] < FOG_VISIBLE_LIMIT;
}

bool World::is_explored(f32 x, f32 y) const {
    if (!fog_enabled()) return true;
    const i32 map_width = pathfinder_.width();
    const i32 map_height = pathfinder_.height();
    const f32 tile_size = pathfinder_.tile_size();
    const i32 tx = static_cast<i32>(x / tile_size);
    const i32 ty = static_cast<i32>(y / tile_size);
    if (tx < 0 || ty < 0 || tx >= map_width || ty >= map_height) {
        return true;
    }
    return fog_tiles_[ty * map_width + tx] < FOG_UNEXPLORED;
}

} // namespace rwrl
