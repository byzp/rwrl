#include <rwrl/spatial.h>

namespace rwrl {

SpatialGrid::SpatialGrid(i32 w, i32 h, f32 cs)
    : w_(w), h_(h), cs_(cs) {}

std::pair<i32,i32> SpatialGrid::cell_(f32 x, f32 y) const {
    return {clamp_i(static_cast<i32>(x / cs_), 0, w_ - 1),
            clamp_i(static_cast<i32>(y / cs_), 0, h_ - 1)};
}

void SpatialGrid::add(Unit& u) {
    auto [cx, cy] = cell_(u.x, u.y);
    cells_[{cx, cy}].push_back(u.uid);
    pos_[u.uid] = {cx, cy};
}

void SpatialGrid::remove(Unit& u) {
    auto it = pos_.find(u.uid);
    if (it == pos_.end()) return;
    auto& vec = cells_[it->second];
    vec.erase(std::remove(vec.begin(), vec.end(), u.uid), vec.end());
    pos_.erase(it);
}

void SpatialGrid::update(Unit& u) {
    remove(u);
    add(u);
}

Vec<u32> SpatialGrid::query_circle(f32 x, f32 y, f32 radius) const {
    i32 min_cx = clamp_i(static_cast<i32>((x - radius) / cs_), 0, w_ - 1);
    i32 max_cx = clamp_i(static_cast<i32>((x + radius) / cs_), 0, w_ - 1);
    i32 min_cy = clamp_i(static_cast<i32>((y - radius) / cs_), 0, h_ - 1);
    i32 max_cy = clamp_i(static_cast<i32>((y + radius) / cs_), 0, h_ - 1);

    Vec<u32> result;
    for (i32 cx = min_cx; cx <= max_cx; ++cx) {
        for (i32 cy = min_cy; cy <= max_cy; ++cy) {
            auto it = cells_.find({cx, cy});
            if (it != cells_.end()) {
                result.insert(result.end(), it->second.begin(), it->second.end());
            }
        }
    }
    return result;
}

std::optional<u32> SpatialGrid::find_nearest_enemy(const Unit& unit,
                                                    const Map<u32, Unit>& enemy_units) const {
    return find_nearest_enemy(unit, enemy_units, false);
}

std::optional<u32> SpatialGrid::find_nearest_enemy(const Unit& unit,
                                                    const Map<u32, Unit>& enemy_units,
                                                    bool air_only) const {
    f32 search_range = std::max(EngineConstants::ATTACK_DETECT_RANGE, unit.weapon_range);
    auto candidates = query_circle(unit.x, unit.y, search_range);

    u32 best_uid = 0;
    f32 best_dist = std::numeric_limits<f32>::max();
    bool found = false;

    for (u32 uid : candidates) {
        auto it = enemy_units.find(uid);
        if (it == enemy_units.end()) continue;
        const Unit& other = it->second;
        if (!other.is_alive || other.team_id == unit.team_id) continue;
        const bool amphibious_submersible =
            unit.unit_type == "attackSubmarine" ||
            unit.unit_type == "amphibiousJet";
        const bool can_target_submerged =
            unit.unit_type == "missileShip" ||
            (amphibious_submersible && unit.is_submerged());
        if (other.is_submerged()) {
            if (!can_target_submerged || !unit.targets_ground) continue;
        } else {
            if (amphibious_submersible && unit.is_submerged()) continue;
            if (air_only && !other.airborne) continue;
            if (other.airborne && !unit.targets_air) continue;
            if (!other.airborne && !unit.targets_ground) continue;
        }
        f32 d = unit.distance_to(other);
        if (d < best_dist) {
            best_dist = d;
            best_uid = uid;
            found = true;
        }
    }
    return found ? std::optional<u32>(best_uid) : std::nullopt;
}

Vec<u32> SpatialGrid::query_circle_by_move_type(f32 x, f32 y, f32 radius,
                                                  MoveType mt, const Map<u32, Unit>& units) const {
    auto candidates = query_circle(x, y, radius);
    Vec<u32> result;
    for (u32 uid : candidates) {
        auto it = units.find(uid);
        if (it == units.end()) continue;
        const Unit& u = it->second;
        if (!u.is_alive) continue;
        if (mt == MoveType::AIR && u.move_type != MoveType::AIR) continue;
        if (mt != MoveType::AIR && u.move_type == MoveType::AIR) continue;
        result.push_back(uid);
    }
    return result;
}

}
