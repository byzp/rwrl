#include "world_internal.h"

#include <cstdio>
#include <cstdlib>

namespace rwrl {

bool World::can_load_cargo_(const Unit& transport, const Unit& cargo) const {
    if (transport.uid == cargo.uid || transport.team_id != cargo.team_id) return false;
    if (!transport.is_alive || !cargo.is_alive || cargo.parent_uid != 0) return false;
    if (transport.cargo_capacity <= 0 || transport.unloading_all) return false;
    if (!cargo.transportable) return false;
    i32 cargo_weight = 0;
    for (u32 cargo_uid : transport.cargo_units) {
        const Unit* loaded = get_unit(cargo_uid);
        if (loaded) cargo_weight += loaded->cargo_weight;
    }
    return cargo_weight + cargo.cargo_weight <= transport.cargo_capacity;
}

void World::load_into(Unit& transport, Unit& cargo) {
    // am.e(am,false) only attaches cargo. The LOAD waypoint owns approach
    // movement and its distance test; direct custom actions can load remotely.
    if (!can_load_cargo_(transport, cargo)) return;
    transport.cargo_units.push_back(cargo.uid);
    cargo.parent_uid = transport.uid;
    grid_.remove(cargo);
}

void World::release_transport_cargo_on_death_(Unit& transport) {
    Vec<u32> cargo_to_release = std::move(transport.cargo_units);
    transport.cargo_units.clear();
    for (u32 cargo_uid : cargo_to_release) {
        Unit* cargo = get_unit(cargo_uid);
        if (!cargo) continue;
        if (transport.cargo_capacity <= 0) {
            remove_unit(cargo_uid);
            continue;
        }
        cargo->parent_uid = 0;
        cargo->x = transport.x + jar_cosine_degrees(transport.rotation) * -9.0f;
        cargo->y = transport.y + jar_sine_degrees(transport.rotation) * -9.0f;
        // Stock f(true) and the shipped custom ds()/h(true) call am.cj().
        // It only writes cu=-1; bV changes on the cargo's next object turn.
        cargo->hp = -1.0f;
    }
}

void World::apply_mobile_factory_exit_(Unit& factory, Unit& product) {
    // custom.j.F(am) uses the hovercraft placement helper for a mobile
    // producer, then C(am) can retain the product without a capacity/filter
    // check. Its MOVE remains queued while cN suppresses the first update.
    const f32 offset = factory.factory_exit_has_dir_offset
        ? factory.factory_exit_dir_offset : 180.0f;
    const f32 heading = factory.rotation + offset;
    const f32 side = factory.custom_factory_exit_side ? -7.0f : 7.0f;
    f32 x = factory.x + jar_cosine_degrees(heading) * factory.factory_exit_y -
        jar_sine_degrees(heading) * factory.factory_exit_x;
    f32 y = factory.y + jar_sine_degrees(heading) * factory.factory_exit_y +
        jar_cosine_degrees(heading) * factory.factory_exit_x;
    x += jar_cosine_degrees(factory.rotation + 90.0f) * side;
    y += jar_sine_degrees(factory.rotation + 90.0f) * side;
    auto passable = [&]() { return pathfinder_.is_passable_world(x, y, product.move_type); };
    if (!passable()) x += 10.0f;
    if (!passable()) x -= 20.0f;
    if (!passable()) { x -= 10.0f; y += 10.0f; }
    if (!passable()) y -= 20.0f;
    const bool placed = passable();
    if (placed) {
        product.x = x;
        product.y = y;
        product.rotation = heading;
        product.collision_impulse_x += 0.1f;
        product.collision_ignore_uid = factory.uid;
        product.collision_ignore_frames = 45.0f;
        const u32 main_target = product.main_target_uid();
        const i32 main_team = product.main_target_team_id();
        clear_current_order_state_(product);
        product.target_uid = main_target;
        product.target_team_id = main_team;
        product.order_queue.clear();
        product.formation_controller_id = 0;
        for (WeaponSlot& slot : product.weapon_slots) {
            slot.turret_angle = heading + slot.turret_idle_direction;
        }
        const f32 exit_heading = heading + side;
        UnitOrder exit;
        exit.type = WeaponType::MOVE;
        exit.x = x + jar_cosine_degrees(exit_heading) * factory.factory_exit_move_away_amount;
        exit.y = y + jar_sine_degrees(exit_heading) * factory.factory_exit_move_away_amount;
        product.order_queue.push_back(exit);
        product.activate_order_next_frame = true;
        product.factory_exit_path_pending = false;
    }
    const bool airborne_factory = factory.move_type == MoveType::AIR && factory.height >= 4.0f;
    if ((airborne_factory || !placed || factory.factory_keep_built_units) &&
        factory.cargo_capacity > 0) {
        // d.k only queues the exit MOVE. When all placement attempts fail,
        // no y.k() has run on this newly constructed product to publish k.
        // Keep the queued waypoint until an active object turn can dispatch it.
        product.has_move_target = false;
        product.activate_order_next_frame = true;
        product.parent_uid = factory.uid;
        factory.cargo_units.push_back(product.uid);
        grid_.remove(product);
    } else {
        grid_.update(product);
    }
}

void World::update_transport_order_(Unit& unit,
                                    const Vec<World*>& other_worlds) {
    if (unit.order_queue.empty() || unit.activate_order_next_frame) return;
    const UnitOrder& order = unit.order_queue.front();
    const bool load_up = order.type == WeaponType::LOAD_UP;
    if (!load_up && order.type != WeaponType::LOAD_INTO) return;

    World* target_world = order_target_world_(order.target_team_id, other_worlds);
    Unit* target = target_world ? target_world->get_unit(order.target_uid) : nullptr;
    Unit* transport = load_up ? &unit : target;
    Unit* cargo = load_up ? target : &unit;
    if (target_world == this && target && target->is_alive &&
        (!load_up || target->is_active())) {
        // y.h()/y.e() continue with their captured au after ay() rejects a
        // load that is no longer possible. They still publish bQ, excluding
        // this live target for the collision pass of the finishing turn.
        unit.collision_approach_target_uid = target->uid;
    }
    if (target_world != this || !target || !target->is_alive ||
        (load_up && !target->is_active())) {
        complete_current_order_(unit, other_worlds);
        return;
    }

    // y.h()/y.e() keep their captured au after d(am,false) rejects a live
    // load. The same update still checks contact and can call ay() again,
    // consuming the next queued waypoint even though e(am,false) fails.
    const bool rejected_load = !can_load_cargo_(*transport, *cargo);
    if (rejected_load) {
        complete_current_order_(unit, other_worlds);
    }

    const f32 target_x = target->x;
    const f32 target_y = target->y;
    unit.transport_order_target_x = target_x;
    unit.transport_order_target_y = target_y;
    const f32 distance_sq = dist2(unit.x, unit.y, target_x, target_y);
    // am.cs() is 21 for stock transports. LOAD_UP includes the exact
    // boundary; LOAD_INTO uses a strict comparison in y.h(float,au,ad).
    bool in_range = load_up ? distance_sq <= 441.0f : distance_sq < 441.0f;
    if (!load_up && transport->is_building) {
        if (distance_sq < 961.0f) {
            unit.order_point_dwell_frames += combat_delta_frames_;
        }
        f32 contact_range = 21.0f;
        const TileRect& footprint = transport->collision_footprint;
        if (footprint.right <= footprint.left || footprint.bottom <= footprint.top) {
            contact_range = 11.0f;
        }
        if (unit.wall_bump_frames > 0.0f) {
            contact_range = transport->radius + 31.0f;
        }
        in_range = unit.order_point_dwell_frames > 240.0f ||
            distance_sq < contact_range * contact_range;
    }
    if (in_range) {
        load_into(*transport, *cargo);
        complete_current_order_(unit, other_worlds, false, true);
        return;
    }
    if (rejected_load) return;

    unit.has_move_target = true;
    unit.arrived = false;
    unit.repath_cooldown_frames = std::min(unit.repath_cooldown_frames, 90.0f);
    const bool near_target = distance_sq < (load_up ? 72900.0f : 48400.0f);
    if (near_target) unit.formation_projection_allowed = false;
    if (unit.formation_leader_uid != 0) {
        const Unit* leader = formation_unit_(unit.formation_leader_uid);
        if (!leader || !leader->is_active()) unit.formation_projection_allowed = false;
        if (unit.formation_projection_allowed) return;
    }

    const bool had_path_node = unit.using_path;
    const bool drifted = std::abs(unit.last_request_x - target_x) > 18.0f ||
                         std::abs(unit.last_request_y - target_y) > 18.0f;
    const bool load_drifted = std::abs(unit.last_request_x - target_x) > 12.0f ||
                              std::abs(unit.last_request_y - target_y) > 12.0f;
    // The common y.a(l,...) controller checks o/p drift only after b(l)
    // has adopted aU. A moving transport cannot shorten s while that
    // request is still pending, even if an older av chain remains active.
    if (drifted && !unit.path_request_pending) {
        unit.repath_cooldown_frames = std::min(unit.repath_cooldown_frames, 30.0f);
    }
    const bool request_path = !unit.path_request_pending &&
        ((!had_path_node && unit.repath_cooldown_frames == 0.0f) ||
         (!had_path_node && unit.path_incomplete && unit.repath_cooldown_frames < 450.0f) ||
         (unit.repath_cooldown_frames == 0.0f && load_drifted));
    if (request_path) {
        // LOAD updates y.r but retains y.n from its preceding waypoint.
        // A load following construction can therefore still request size 1.
        move_unit_to_(unit, target_x, target_y,
                      unit.path_request_target_size, true, had_path_node);
        if (!had_path_node) {
            // y.a(engine,...) captured aE() before the new request. A line
            // path is exposed now but its first physical pass still coasts.
            unit.movement_activation_delay_frames = 1.0f;
            unit.movement_delay_keeps_prediction = false;
            unit.movement_delay_uses_acceleration = unit.move_speed_factor < 0.0f;
        }
    } else if (!had_path_node) {
        unit.target_x = target_x;
        unit.target_y = target_y;
        if (unit.path_request_pending || !near_target ||
            unit.repath_cooldown_frames == 0.0f) {
            unit.movement_activation_delay_frames = 1.0f;
            unit.movement_delay_keeps_prediction = false;
            unit.movement_delay_uses_acceleration = unit.move_speed_factor < 0.0f;
        }
    }
}

void World::unload_at(Unit& transport, f32 x, f32 y) {
    if (transport.cargo_units.empty()) return;
    u32 cargo_uid = transport.cargo_units.back();
    transport.cargo_units.pop_back();

    Unit* cargo = get_unit(cargo_uid);
    if (!cargo) return;

    const bool had_move_target = cargo->has_move_target;
    const bool was_arrived = cargo->arrived;
    const u32 combat_target_uid = has_weapon(cargo->weapon_type)
        ? cargo->target_uid : 0;
    const i32 combat_target_team_id = cargo->target_team_id;
    f32 old_target_x = cargo->target_x;
    f32 old_target_y = cargo->target_y;
    if (!cargo->order_queue.empty() &&
        (cargo->order_queue.front().type == WeaponType::LOAD_INTO ||
         cargo->order_queue.front().type == WeaponType::LOAD_UP)) {
        old_target_x = cargo->transport_order_target_x;
        old_target_y = cargo->transport_order_target_y;
    }
    clear_current_order_state_(*cargo);
    // Both stock unload controllers call y.az(), which retains y.R.
    // Cargo's own combat turn validates that target after detachment.
    if (combat_target_uid != 0) {
        cargo->target_uid = combat_target_uid;
        cargo->target_team_id = combat_target_team_id;
    }
    // Native unloading changes cN and orders without changing am.bV. A
    // retained dead passenger must keep its death flag after detachment.
    cargo->x = x;
    cargo->y = y;
    cargo->parent_uid = 0;
    // Native az()/aH() clear the orders and nodes, but retain y.k/l/m.
    // The new exit waypoint is handled on the cargo's next object turn.
    cargo->has_move_target = had_move_target;
    cargo->arrived = was_arrived;
    cargo->target_x = old_target_x;
    cargo->target_y = old_target_y;
    cargo->order_queue.clear();
    // y.az() clears the departing waypoint before aD() regroups its old
    // followers. Those followers can still be inside this transport; their
    // new leader and slot metadata survive until their own unload turns.
    rebuild_formation_after_leader_removed_(*cargo);
    cargo->formation_leader_uid = 0;
    // Stock dropship calls y.az(), which retains ac. Hovercraft explicitly
    // clears it after az(). Former dropship cargo can therefore still finish
    // each other's MOVE through a later collision.
    if (transport.unit_type != "dropship") cargo->formation_controller_id = 0;
    grid_.add(*cargo);
}

void World::begin_unload_all(Unit& transport) {
    if (transport.cargo_capacity <= 0) {
        return;
    }
    transport.unloading_all = true;
    transport.unload_timer_frames = 30.0f;
    // Stock M() arms unloading without changing the current waypoint or
    // movement. Its timer starts advancing only after the transport stops.
}

void World::cancel_unload_all(Unit& transport) {
    if (transport.cargo_capacity <= 0) return;
    transport.unloading_all = false;
}

void World::set_submerged(Unit& unit, bool submerged) {
    if (unit.unit_type != "attackSubmarine" && unit.unit_type != "amphibiousJet") return;
    unit.submerge_requested = submerged;
    if (unit.unit_type == "amphibiousJet") {
        // b.c.i() derives the air layer directly from the requested mode and
        // current height, so the command changes targeting before the next tick.
        unit.airborne = !submerged && unit.height >= 0.0f;
        unit.move_type = unit.airborne ? MoveType::AIR : MoveType::WATER;
        // b.c.m() reads the requested mode through b(), before the next
        // altitude update. A Dive command immediately changes b(false)'s
        // range and every turret's inherited firing range for this turn.
        unit.weapon_range = unit.airborne ? 170.0f : 100.0f;
    }
}

void World::update_transport_unloading(f32 dt, f32 speed_mult) {
    update_unit_transport_unloading(dt, speed_mult, 0);
}

void World::update_unit_transport_unloading(
        f32 dt, f32 speed_mult, u32 only_unit_uid) {
    const f32 game_frames = dt * EngineConstants::FRAME_RATE * speed_mult;
    for (auto& [_, transport] : units_) {
        if (only_unit_uid != 0 && transport.uid != only_unit_uid) continue;
        if (!transport.is_alive || !transport.unloading_all) continue;
        if (transport.cargo_capacity <= 0) {
            transport.unloading_all = false;
            continue;
        }
        const bool stock_transport = !transport.custom_unit &&
            (transport.unit_type == "hovercraft" || transport.unit_type == "dropship");
        if (stock_transport) {
            const i32 tile_x = pathfinder_.tile_x(transport.x);
            const i32 tile_y = pathfinder_.tile_y(transport.y);
            if (transport.movement_prediction_enabled ||
                (pathfinder_.terrain(tile_x, tile_y) &
                 (TERRAIN_WATER | TERRAIN_LAVA)) != 0 ||
                (transport.unit_type == "dropship" && transport.height >= 4.0f)) {
                continue;
            }
        } else {
            const u8 terrain = pathfinder_.terrain(
                pathfinder_.tile_x(transport.x), pathfinder_.tile_y(transport.y));
            const bool over_liquid = (terrain & (TERRAIN_WATER | TERRAIN_LAVA)) != 0;
            const Str& condition = transport.transport_unload_condition;
            const bool may_unload = condition == "true" ||
                (condition == "self.isoverliquid()" && over_liquid) ||
                (condition == "not self.isoverliquid()" && !over_liquid) ||
                (condition == "not self.isoverliquid() and not self.ismoving()" &&
                 !over_liquid && !transport.movement_prediction_enabled);
            if (!may_unload) continue;
        }

        transport.unload_timer_frames = std::max(
            0.0f, transport.unload_timer_frames - game_frames);
        if (transport.unload_timer_frames > 0.0f) continue;
        transport.unload_timer_frames = stock_transport
            ? 30.0f : transport.transport_unload_delay;
        if (transport.cargo_units.empty()) {
            transport.unloading_all = false;
            continue;
        }

        // custom.j.g() samples parity before removing the passenger.
        const bool alternate_side = transport.cargo_units.size() % 2 == 0;
        const u32 cargo_uid = transport.cargo_units.back();
        Unit* cargo = get_unit(cargo_uid);
        if (!cargo) {
            transport.cargo_units.pop_back();
            continue;
        }
        f32 unload_x;
        f32 unload_y;
        f32 cargo_rotation;
        f32 exit_x;
        f32 exit_y;
        if (transport.custom_unit) {
            cargo_rotation = transport.rotation +
                (transport.factory_exit_has_dir_offset
                    ? transport.factory_exit_dir_offset : 180.0f);
            const f32 base_x = transport.x +
                jar_cosine_degrees(cargo_rotation) * transport.factory_exit_y -
                jar_sine_degrees(cargo_rotation) * transport.factory_exit_x;
            const f32 base_y = transport.y +
                jar_sine_degrees(cargo_rotation) * transport.factory_exit_y +
                jar_cosine_degrees(cargo_rotation) * transport.factory_exit_x;
            const f32 side = alternate_side ? -7.0f : 7.0f;
            unload_x = base_x + jar_cosine_degrees(transport.rotation + 90.0f) * side;
            unload_y = base_y + jar_sine_degrees(transport.rotation + 90.0f) * side;
        } else if (transport.hovercraft_unload_style) {
            const f32 side = alternate_side ? -9.0f : 9.0f;
            unload_x = transport.x +
                       jar_cosine_degrees(transport.rotation - 180.0f) * 7.0f +
                       jar_cosine_degrees(transport.rotation + 90.0f) * side;
            unload_y = transport.y +
                       jar_sine_degrees(transport.rotation - 180.0f) * 7.0f +
                       jar_sine_degrees(transport.rotation + 90.0f) * side;
            cargo_rotation = transport.rotation - 180.0f;
            const f32 exit_angle =
                cargo_rotation + (alternate_side ? -9.0f : 9.0f);
            exit_x = unload_x + jar_cosine_degrees(exit_angle) * 70.0f;
            exit_y = unload_y + jar_sine_degrees(exit_angle) * 70.0f;
        } else {
            // The stock dropship bytecode applies both +/-7 side terms to X.
            const f32 side = alternate_side ? -7.0f : 7.0f;
            unload_x = transport.x +
                       jar_cosine_degrees(transport.rotation) * -9.0f +
                       jar_cosine_degrees(transport.rotation + 90.0f) * side +
                       jar_sine_degrees(transport.rotation + 90.0f) * side;
            unload_y = transport.y +
                       jar_sine_degrees(transport.rotation) * -9.0f;
            cargo_rotation = transport.rotation + 180.0f;
            exit_x = transport.x +
                     jar_cosine_degrees(transport.rotation) * -66.0f;
            exit_y = transport.y +
                     jar_sine_degrees(transport.rotation) * -66.0f;
        }

        const auto passable = [&] {
            return (transport.custom_unit && transport.is_building) ||
                pathfinder_.is_passable_world(unload_x, unload_y, cargo->move_type);
        };
        // Native transports try the same five world-space candidates.
        // If all are blocked, cargo stays attached and the 30-frame timer
        // above remains armed for the next attempt.
        if (!passable()) {
            unload_x += 10.0f;
        }
        if (!passable()) {
            unload_x -= 20.0f;
        }
        if (!passable()) {
            unload_x -= 10.0f;
            unload_y += 10.0f;
        }
        if (!passable()) {
            unload_y -= 20.0f;
        }
        if (!passable()) {
            continue;
        }

        if (transport.custom_unit) {
            const f32 exit_angle = cargo_rotation + (alternate_side ? -7.0f : 7.0f);
            exit_x = unload_x + jar_cosine_degrees(exit_angle) * transport.factory_exit_move_away_amount;
            exit_y = unload_y + jar_sine_degrees(exit_angle) * transport.factory_exit_move_away_amount;
        } else if (transport.hovercraft_unload_style) {
            const f32 exit_angle =
                cargo_rotation + (alternate_side ? -9.0f : 9.0f);
            exit_x = unload_x + jar_cosine_degrees(exit_angle) * 70.0f;
            exit_y = unload_y + jar_sine_degrees(exit_angle) * 70.0f;
        }
        unload_at(transport, unload_x, unload_y);
        if (cargo) {
            cargo->rotation = cargo_rotation;
            if (transport.custom_unit || transport.unit_type == "hovercraft") {
                // e.i unload calls y.j(cg): ap.a takes the heading plus B(i),
                // preserving the previous angle, reload and idle state.
                for (WeaponSlot& slot : cargo->weapon_slots) {
                    slot.turret_angle = cargo_rotation + slot.turret_idle_direction;
                }
            }
            if (transport.custom_unit) {
                cargo->vel_x = cargo->vel_y = 0.0f;
                cargo->collision_impulse_x = 0.1f;
                cargo->collision_impulse_y = 0.0f;
                if (!transport.factory_exit_height_ignore_parent) {
                    cargo->height = transport.height;
                }
                cargo->height += transport.factory_exit_height_offset;
            } else {
                cargo->collision_impulse_x += 0.1f;
            }
            cargo->collision_ignore_uid = transport.uid;
            cargo->collision_ignore_frames = 45.0f;
            UnitOrder exit_order;
            exit_order.type = WeaponType::MOVE;
            exit_order.x = exit_x;
            exit_order.y = exit_y;
            if (!transport.custom_unit || transport.factory_exit_move_away_amount != 0.0f) {
                cargo->order_queue.push_back(exit_order);
                cargo->activate_order_next_frame = true;
            }
        }
        if (transport.cargo_units.empty()) {
            transport.unloading_all = false;
        }
    }
}

} // namespace rwrl
