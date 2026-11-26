#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <optional>
#include <cmath>
#include <algorithm>
#include <iterator>
#include <memory>

namespace rwrl {

struct PairHash {
    template<typename T1, typename T2>
    std::size_t operator()(const std::pair<T1,T2>& p) const noexcept {
        auto h1 = std::hash<T1>{}(p.first);
        auto h2 = std::hash<T2>{}(p.second);
        return h1 ^ (h2 << 1);
    }
};

using i8 = int8_t;
using u8 = uint8_t;
using i16 = int16_t;
using u16 = uint16_t;
using i32 = int32_t;
using i64 = int64_t;
using u32 = uint32_t;
using u64 = uint64_t;
using f32 = float;
using f64 = double;
using Str = std::string;

template<typename T>
using Vec = std::vector<T>;

template<typename K, typename V>
using Map = std::map<K, V>;

template<typename T>
using Set = std::set<T>;

template<typename T>
using SP = std::shared_ptr<T>;

template<typename T, typename... Args>
SP<T> mk(Args&&... args) { return std::make_shared<T>(std::forward<Args>(args)...); }

inline f32 dist2(f32 x1, f32 y1, f32 x2, f32 y2) {
    f32 dx = x1 - x2, dy = y1 - y2;
    return dx * dx + dy * dy;
}

inline f32 dist(f32 x1, f32 y1, f32 x2, f32 y2) {
    return std::sqrt(dist2(x1, y1, x2, y2));
}

inline f32 clamp(f32 v, f32 lo, f32 hi) { return std::max(lo, std::min(hi, v)); }
inline i32 clamp_i(i32 v, i32 lo, i32 hi) { return std::max(lo, std::min(hi, v)); }

// l.bJ, the map's global random seed. game.i seeds it from ad.ay.q once the
// level loads and never writes it again, so every gameFramework.f.a(w,...)
// draw in a game is offset by this one constant. Saves and replays carry it
// in their gameSetup block; the host reads it there and installs it, because
// the core never opens the map or replay itself.
void set_global_random_seed(i32 seed);
i32 global_random_seed();

}
