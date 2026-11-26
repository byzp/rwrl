#include <rwrl/pathfinder.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace rwrl {

Vec<std::pair<f32, f32>> Pathfinder::find_path(f32 sx, f32 sy, f32 ex, f32 ey,
                                                 MoveType mt, f32 unit_radius,
                                                 f32 start_angle,
                                                 bool flexible_reverse,
                                                 bool prefer_clearance,
                                                 i32 target_size,
                                                 i32 min_clearance,
                                                 const PathLayerSnapshot* snapshot) const {
    Vec<std::pair<f32, f32>> path;

    if (mt == MoveType::AIR) {
        path.push_back({ex, ey});
        return path;
    }

    i32 stx = tile_x(sx), sty = tile_y(sy);
    const i32 requested_etx = tile_x(ex), requested_ety = tile_y(ey);
    i32 etx = requested_etx, ety = requested_ety;

    (void)unit_radius;
    const std::size_t expected_layer_size =
        static_cast<std::size_t>(map_w_) * map_h_;
    const bool use_snapshot = snapshot &&
        snapshot->passable.size() == expected_layer_size &&
        snapshot->object_cost.size() == expected_layer_size &&
        snapshot->clearance.size() == expected_layer_size;
    auto blocked_for_unit = [&](i32 tx, i32 ty) {
        if (use_snapshot) {
            if (tx < 0 || tx >= map_w_ || ty < 0 || ty >= map_h_) return true;
            return snapshot->passable[ty * map_w_ + tx] == 0;
        }
        return !passable_(tx, ty, mt);
    };
    auto clearance = [&](i32 tx, i32 ty) {
        if (use_snapshot) {
            if (tx < 0 || tx >= map_w_ || ty < 0 || ty >= map_h_) return 0;
            return static_cast<i32>(
                snapshot->clearance[ty * map_w_ + tx]);
        }
        return path_clearance(tx, ty, mt);
    };
    const Vec<i8>& object_cost_layer = use_snapshot
        ? snapshot->object_cost : object_costs_(mt);
    if (stx == requested_etx && sty == requested_ety) {
        // gameFramework.k.o.e() returns the end tile before any blocked-end
        // retargeting when start==end, regardless of tile passability.
        if (unclamped_tile_x(ex) == stx && unclamped_tile_y(ey) == sty) {
            path.push_back({ex, ey});
        } else {
            path.push_back({world_x(stx), world_y(sty)});
        }
        return path;
    }

    // gameFramework.k.o.e() isolated-groups pre-pass: when the start's
    // terrain component has no tile within +/-target_size of the end, the
    // goal retargets to the nearest same-component tile (+/-25 window
    // around the real end first, then map-wide; squared distance with a
    // strict-less first-found tie rule) and the blocked-area pre-check
    // radius drops to zero. Groups ignore buildings (k.i.b() reads only
    // the terrain layer), so the goal checks still use the original
    // target_size afterwards.
    i32 pre_check_radius = target_size;
    {
        const Vec<i16>& groups = connectivity_groups(mt);
        if (!groups.empty()) {
            const i16 start_group = groups[sty * map_w_ + stx];
            if (start_group != -1) {
                bool group_matched = false;
                for (i32 tx = etx - target_size; tx <= etx + target_size;
                     ++tx) {
                    for (i32 ty = ety - target_size; ty <= ety + target_size;
                         ++ty) {
                        if (tx < 0 || tx >= map_w_ ||
                            ty < 0 || ty >= map_h_) continue;
                        if (groups[ty * map_w_ + tx] == start_group) {
                            group_matched = true;
                        }
                    }
                }
                if (!group_matched) {
                    i32 best_x = etx, best_y = ety;
                    f32 best_distance = -1.0f;
                    auto consider_group_goal = [&](i32 tx, i32 ty) {
                        if (tx < 0 || tx >= map_w_ ||
                            ty < 0 || ty >= map_h_) return;
                        const i16 group = groups[ty * map_w_ + tx];
                        if (group != start_group && group != 0) return;
                        const f32 dx = static_cast<f32>(tx - requested_etx);
                        const f32 dy = static_cast<f32>(ty - requested_ety);
                        const f32 distance = dx * dx + dy * dy;
                        if (best_distance != -1.0f &&
                            distance >= best_distance) return;
                        best_distance = distance;
                        best_x = tx;
                        best_y = ty;
                    };
                    for (i32 tx = etx - 25; tx <= etx + 25; ++tx) {
                        for (i32 ty = ety - 25; ty <= ety + 25; ++ty) {
                            consider_group_goal(tx, ty);
                        }
                    }
                    if (best_distance == -1.0f) {
                        for (i32 tx = 0; tx < map_w_; ++tx) {
                            for (i32 ty = 0; ty < map_h_; ++ty) {
                                consider_group_goal(tx, ty);
                            }
                        }
                    }
                    if (best_distance != -1.0f) {
                        etx = best_x;
                        ety = best_y;
                        pre_check_radius = 0;
                    }
                }
            }
        }
    }

    // gameFramework.k.o.e(): the end area counts as open when any tile
    // within the pre-check radius is passable. Only a fully blocked area
    // retargets to the nearest passable tile (+/-9 window first, then the
    // full map), matching the JAR scan order and strict-less tie rule.
    bool end_area_open = false;
    for (i32 tx = etx - pre_check_radius; tx <= etx + pre_check_radius;
         ++tx) {
        for (i32 ty = ety - pre_check_radius; ty <= ety + pre_check_radius;
             ++ty) {
            if (tx < 0 || tx >= map_w_ || ty < 0 || ty >= map_h_) continue;
            if (!blocked_for_unit(tx, ty)) { end_area_open = true; break; }
        }
        if (end_area_open) break;
    }
    if (!end_area_open) {
        i32 best_distance = std::numeric_limits<i32>::max();
        i32 best_x = etx, best_y = ety;
        auto consider_goal = [&](i32 tx, i32 ty) {
            if (tx < 0 || tx >= map_w_ || ty < 0 || ty >= map_h_ ||
                blocked_for_unit(tx, ty)) return;
            // k.o.e() searches around the terrain-group replacement when
            // that tile is also blocked by a building. Its second distance
            // comparison uses that replacement, rather than the raw goal.
            const i32 dx = tx - etx;
            const i32 dy = ty - ety;
            const i32 distance = dx * dx + dy * dy;
            if (distance < best_distance) {
                best_distance = distance;
                best_x = tx;
                best_y = ty;
            }
        };
        for (i32 tx = etx - 9; tx <= etx + 9; ++tx) {
            for (i32 ty = ety - 9; ty <= ety + 9; ++ty) {
                consider_goal(tx, ty);
            }
        }
        if (best_distance == std::numeric_limits<i32>::max()) {
            for (i32 tx = 0; tx < map_w_; ++tx) {
                for (i32 ty = 0; ty < map_h_; ++ty) {
                    consider_goal(tx, ty);
                }
            }
        }
        // With no passable goal anywhere, k.o.e() still searches from its
        // forward root and returns the closest visited node on exhaustion.
        if (best_distance != std::numeric_limits<i32>::max()) {
            etx = best_x;
            ety = best_y;
        }
    }

    // gameFramework.k.o.e(): integer bidirectional A* with the JAR's exact
    // bucket open list, neighbor window order and reconstruction rules. Tie
    // behavior between equal-cost paths depends on all three, so each is
    // replicated instead of using a std::priority_queue.
    const i32 total = map_w_ * map_h_;
    auto idx = [this](i32 x, i32 y) -> i32 { return y * map_w_ + x; };

    auto direction_difference = [](i32 from, i32 to) {
        i32 difference = std::abs(from - to);
        return difference > 4 ? 8 - difference : difference;
    };
    // k.o.b(): plain turn cost. k.o.c(): the reverse-friendly variant only
    // used for the forward root when the request allows flexible reversing.
    auto turn_cost = [&](i32 from, i32 to, bool reverse_friendly) {
        const i32 difference = direction_difference(from, to);
        if (difference == 0) return 0;
        if (difference == 1) return 4;
        if (difference == 2) return 21;
        if (reverse_friendly && difference == 3) return 4;
        if (reverse_friendly && difference == 4) return 0;
        return 25;
    };

    struct SearchSide {
        Vec<i32> g_cost;      // k.o.l[side] without the generation base; -1 unvisited
        Vec<i8> arrival_dir;  // k.p direction bits
        Vec<bool> root_node;  // k.p root bit
        Vec<bool> closed;     // k.p closed bit
    };

    // gameFramework.k.m/k.e/k.n: the open list keeps every node at the
    // current minimum score in one array popped LIFO. Pushing a smaller
    // score flushes that array into an overflow list; refills scan the
    // overflow backwards with swap-removal. Scores are g + the JAR's
    // 11*(dx+dy) - 7*min(dx,dy) heuristic toward the side's goal.
    struct BucketNode { i32 score; i32 x, y; };
    struct JarOpenList {
        Vec<BucketNode> current;
        Vec<BucketNode> overflow;
        i32 bucket_score = std::numeric_limits<i32>::max();
        i32 goal_x = 0, goal_y = 0;

        void push(i32 key, i32 x, i32 y) {
            const i32 dx = std::abs(goal_x - x);
            const i32 dy = std::abs(goal_y - y);
            const i32 score = key + (dx + dy) * 11 - std::min(dx, dy) * 7;
            if (score <= bucket_score) {
                if (score == bucket_score) {
                    current.push_back({score, x, y});
                    return;
                }
                for (const BucketNode& node : current) {
                    overflow.push_back(node);
                }
                current.clear();
                bucket_score = score;
                current.push_back({score, x, y});
                return;
            }
            overflow.push_back({score, x, y});
        }

        bool pop(i32& x, i32& y) {
            if (current.empty()) {
                if (overflow.empty()) {
                    bucket_score = std::numeric_limits<i32>::max();
                    return false;
                }
                i32 min_score = std::numeric_limits<i32>::max();
                for (i32 i = static_cast<i32>(overflow.size()) - 1; i >= 0;
                     --i) {
                    min_score = std::min(min_score, overflow[i].score);
                }
                for (i32 i = static_cast<i32>(overflow.size()) - 1; i >= 0;
                     --i) {
                    if (overflow[i].score != min_score) continue;
                    current.push_back(overflow[i]);
                    overflow[i] = overflow.back();
                    overflow.pop_back();
                }
                bucket_score = min_score;
            }
            x = current.back().x;
            y = current.back().y;
            current.pop_back();
            return true;
        }
    };

    SearchSide fwd;
    fwd.g_cost.assign(total, -1);
    fwd.arrival_dir.assign(total, 0);
    fwd.root_node.assign(total, false);
    fwd.closed.assign(total, false);

    SearchSide rev;
    rev.g_cost.assign(total, -1);
    rev.arrival_dir.assign(total, 0);
    rev.root_node.assign(total, false);
    rev.closed.assign(total, false);

    const i32 si = idx(stx, sty);
    fwd.g_cost[si] = 0;
    // k.p.a(o, byte, float): exactly two +8/-8 correction rounds, then any
    // still-out-of-range direction clamps to 0.
    i32 initial_direction = static_cast<i32>(start_angle / 360.0f * 8.0f + 0.5f);
    if (initial_direction < 0) initial_direction += 8;
    if (initial_direction > 7) initial_direction -= 8;
    if (initial_direction < 0) initial_direction += 8;
    if (initial_direction > 7) initial_direction -= 8;
    if (initial_direction < 0 || initial_direction > 7) initial_direction = 0;
    fwd.arrival_dir[si] = static_cast<i8>(initial_direction);
    fwd.root_node[si] = true;

    const i32 ei = idx(etx, ety);
    rev.g_cost[ei] = 0;
    // k.o.a(short, short, short) stores direction 0 on the reverse root; the
    // value is unused because roots skip the window limit and turn cost.
    rev.arrival_dir[ei] = 0;
    rev.root_node[ei] = true;

    JarOpenList fwd_open, rev_open;
    fwd_open.goal_x = etx;
    fwd_open.goal_y = ety;
    rev_open.goal_x = stx;
    rev_open.goal_y = sty;
    fwd_open.push(0, stx, sty);
    rev_open.push(0, etx, ety);

    bool found = false;
    bool goal_reached = false;   // popped the goal without an other-side meet
    i32 meet_node = -1;
    bool meet_on_reverse = false;
    bool run_reverse = false;
    i32 phase_remaining = BIDIR_PHASE_STEPS;
    i32 iterations = 0;
    // Safety bound only; k.o.e() runs until the forward side empties.
    i32 max_iterations = total * 24;

    while (!found) {
        ++iterations;
        if (--max_iterations <= 0) break;
        if (phase_remaining > 0) {
            phase_remaining--;
        } else {
            run_reverse = !run_reverse;
        }
        JarOpenList& cur_open = run_reverse ? rev_open : fwd_open;
        SearchSide& cur_side = run_reverse ? rev : fwd;
        SearchSide& other_side = run_reverse ? fwd : rev;

        i32 cx, cy;
        if (!cur_open.pop(cx, cy)) {
            // k.o.e(): an empty reverse list burns the iteration; an empty
            // forward list ends the search.
            if (run_reverse) continue;
            break;
        }
        const i32 ci = idx(cx, cy);
        // Stale duplicate queue entry; re-expanding it cannot relax anything
        // and its goal/meet checks fired on the first pop.
        if (cur_side.closed[ci]) continue;

        bool over_goal = false;
        if (!run_reverse) {
            if (cx == etx && cy == ety) over_goal = true;
            if (std::abs(cx - requested_etx) <= target_size &&
                std::abs(cy - requested_ety) <= target_size) {
                over_goal = true;
            }
        }
        const bool closed_on_other = other_side.closed[ci];
        if (closed_on_other || over_goal) {
            found = true;
            goal_reached = !closed_on_other;
            meet_node = ci;
            meet_on_reverse = run_reverse;
            break;
        }
        cur_side.closed[ci] = true;

        const i32 old_direction = cur_side.arrival_dir[ci];
        const bool root = cur_side.root_node[ci];
        i32 window_from = 0, window_to = 7;
        if (!root) {
            const i32 limit = clearance(cx, cy) > 1 ? 1 : 2;
            window_from = old_direction - limit;
            window_to = old_direction + limit;
        }
        for (i32 raw = window_from; raw <= window_to; ++raw) {
            i32 d = raw;
            if (d > 7) d -= 8;
            if (d < 0) d += 8;
            const i32 nx = cx + DX[d], ny = cy + DY[d];
            if (nx < 0 || nx >= map_w_ || ny < 0 || ny >= map_h_) continue;
            if (blocked_for_unit(nx, ny)) continue;
            const i32 ni = idx(nx, ny);
            if (cur_side.closed[ni]) continue;
            i32 ng = cur_side.g_cost[ci];
            if (nx != cx && ny != cy) {
                if (blocked_for_unit(nx, cy)) continue;
                if (blocked_for_unit(cx, ny)) continue;
                ng += 15;
            } else {
                ng += 11;
            }
            ng += std::max<i32>(0, path_cost(nx, ny)) +
                static_cast<i32>(object_cost_layer[ni]) * 10;
            if (old_direction != d) {
                if (!root) {
                    ng += turn_cost(old_direction, d, false);
                } else if (!run_reverse) {
                    ng += turn_cost(old_direction, d, flexible_reverse);
                }
                // The reverse root expands without any turn cost.
            }
            ng += (4 - clearance(nx, ny)) * (prefer_clearance ? 7 : 1);
            if (min_clearance > 0 && clearance(nx, ny) <= min_clearance) {
                ng += 100;
            }
            if (cur_side.g_cost[ni] >= 0 && ng >= cur_side.g_cost[ni]) {
                continue;
            }
            cur_side.arrival_dir[ni] = static_cast<i8>(d);
            cur_side.root_node[ni] = false;
            cur_side.g_cost[ni] = ng;
            cur_open.push(ng, nx, ny);
        }
    }

    // k.o.a(byte, short, short): walk arrival directions back to the root.
    auto tile_chain = [&](const SearchSide& side, i32 from_index) {
        Vec<std::pair<i32, i32>> chain;
        i32 x = from_index % map_w_, y = from_index / map_w_;
        chain.push_back({x, y});
        while (true) {
            const i32 index = idx(x, y);
            if (side.g_cost[index] < 0 || side.root_node[index]) break;
            const i32 dir = side.arrival_dir[index];
            x -= DX[dir];
            y -= DY[dir];
            chain.push_back({x, y});
        }
        return chain;
    };

    Vec<std::pair<i32, i32>> tiles;
    if (found) {
        if (goal_reached) {
            tiles = tile_chain(fwd, meet_node);
            std::reverse(tiles.begin(), tiles.end());
        } else {
            const SearchSide& pop_side = meet_on_reverse ? rev : fwd;
            if (pop_side.root_node[meet_node]) {
                // k.o.e(): otherConnection==null aborts with an empty path.
                return path;
            }
            const i32 mx = meet_node % map_w_, my = meet_node / map_w_;
            const i32 dir = pop_side.arrival_dir[meet_node];
            const i32 parent_index = idx(mx - DX[dir], my - DY[dir]);
            const i32 fwd_end = meet_on_reverse ? meet_node : parent_index;
            const i32 rev_start = meet_on_reverse ? parent_index : meet_node;
            tiles = tile_chain(fwd, fwd_end);
            std::reverse(tiles.begin(), tiles.end());
            for (const auto& tile : tile_chain(rev, rev_start)) {
                tiles.push_back(tile);
            }
        }
    } else {
        // Forward exhaustion: nearest visited forward node to the real end,
        // scanned x-outer/y-inner with a strict-less tie rule like the JAR.
        i32 closest = -1;
        i32 closest_distance = std::numeric_limits<i32>::max();
        for (i32 tx = 0; tx < map_w_; ++tx) {
            for (i32 ty = 0; ty < map_h_; ++ty) {
                const i32 candidate = idx(tx, ty);
                if (fwd.g_cost[candidate] < 0) continue;
                const i32 dx = tx - requested_etx;
                const i32 dy = ty - requested_ety;
                const i32 distance = dx * dx + dy * dy;
                if (distance < closest_distance) {
                    closest_distance = distance;
                    closest = candidate;
                }
            }
        }
        if (closest == -1) return path;
        tiles = tile_chain(fwd, closest);
        std::reverse(tiles.begin(), tiles.end());
    }

    // k.o.e() drops the start tile from every multi-node result. Paths that
    // actually reach the requested end tile keep the caller's exact point.
    if (tiles.size() > 1) tiles.erase(tiles.begin());
    for (const auto& [tx, ty] : tiles) {
        path.push_back({world_x(tx), world_y(ty)});
    }
    if (!tiles.empty() && tiles.back().first == unclamped_tile_x(ex) &&
        tiles.back().second == unclamped_tile_y(ey)) {
        path.back() = {ex, ey};
    }
    return path;
}

SP<const PathLayerSnapshot> Pathfinder::capture_path_layer(MoveType mt) const {
    auto snapshot = mk<PathLayerSnapshot>();
    const std::size_t count = static_cast<std::size_t>(map_w_) * map_h_;
    snapshot->passable.assign(count, 0);
    snapshot->object_cost = object_costs_(mt);
    snapshot->clearance.assign(count, 0);

    for (i32 ty = 0; ty < map_h_; ++ty) {
        for (i32 tx = 0; tx < map_w_; ++tx) {
            const i32 index = ty * map_w_ + tx;
            snapshot->passable[index] = passable_(tx, ty, mt) ? 1 : 0;
        }
    }
    const i32 layer = ground_layer_index(mt);
    if (layer >= 0) {
        initialize_clearance_layers();
        snapshot->clearance = clearance_state_->layers[layer];
        return snapshot;
    }
    auto blocked = [&](i32 tx, i32 ty) {
        if (tx < 0 || tx >= map_w_ || ty < 0 || ty >= map_h_) return true;
        return snapshot->passable[ty * map_w_ + tx] == 0;
    };
    for (i32 ty = 0; ty < map_h_; ++ty) {
        for (i32 tx = 0; tx < map_w_; ++tx) {
            const i32 index = ty * map_w_ + tx;
            if (blocked(tx, ty)) continue;
            i32 value = 4;
            for (i32 ox = -3; ox <= 3; ++ox) {
                for (i32 oy = -3; oy <= 3; ++oy) {
                    if (!blocked(tx + ox, ty + oy)) continue;
                    value = std::min(
                        value, std::max(std::abs(ox), std::abs(oy)));
                }
            }
            snapshot->clearance[index] = static_cast<i8>(value);
        }
    }
    return snapshot;
}

}
