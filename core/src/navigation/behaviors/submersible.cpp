#include "world_internal.h"

#include <cstdio>
#include <cstdlib>

namespace rwrl {

void World::update_submersible_states(f32 dt, f32 speed_mult) {
    update_unit_submersible_states(dt, speed_mult, 0);
}

void World::update_unit_submersible_states(
        f32 dt, f32 speed_mult, u32 only_unit_uid) {
    const f32 game_frames = dt * EngineConstants::FRAME_RATE * speed_mult;
    for (auto& [_, unit] : units_) {
        if (only_unit_uid != 0 && unit.uid != only_unit_uid) continue;
        if (!unit.is_alive) continue;

        if (unit.unit_type == "attackSubmarine") {
            if (!unit.is_complete() || unit.parent_uid != 0) continue;
            const f32 target_height = unit.submerge_requested ? -8.0f : 1.0f;
            const f32 target_vertical_speed =
                std::abs(unit.height - target_height) > 2.0f ? 0.08f : 0.02f;
            unit.vertical_speed = approach_value(
                unit.vertical_speed, target_vertical_speed, 0.003f * game_frames);
            unit.height = approach_value(
                unit.height, target_height, unit.vertical_speed * game_frames);
            unit.submerged = unit.height < -1.0f;
            unit.airborne = false;
            unit.weapon_range = unit.submerged ? 180.0f : 250.0f;
            unit.move_speed = (unit.submerged ? 0.45f : 0.8f) * EngineConstants::FRAME_RATE;
            unit.projectile_speed = (unit.submerged ? 1.3f : 2.0f) * EngineConstants::FRAME_RATE;
            unit.projectile_lifetime = unit.submerged ? 250.0f : 190.0f;
            continue;
        }

        if (unit.unit_type == "amphibiousJet") {
            if (!unit.is_complete() || unit.parent_uid != 0) continue;
            unit.flight_phase += 2.0f * game_frames;
            while (unit.flight_phase > 360.0f) unit.flight_phase -= 360.0f;
            const f32 target_height = unit.submerge_requested
                ? -8.0f
                : 20.0f + jar_sine_degrees(unit.flight_phase) * 1.5f;
            // b.c updates u after super.a(delta), so this fold affects the
            // next object's aim/launch and reads the height before it moves.
            const f32 target_fold =
                !unit.submerge_requested && !unit.submerged ? 0.0f : 1.0f;
            unit.wing_fold_progress = approach_value(
                unit.wing_fold_progress, target_fold, 0.018f * game_frames);
            const bool far_from_target_height =
                std::abs(unit.height - target_height) > 3.0f;
            const f32 target_vertical_speed = far_from_target_height
                ? (unit.submerged ? 0.1f : 0.6f) : 0.07f;
            if (far_from_target_height) {
                // f.b(t,f3) clamps both signs before the speed approach.
                // Entering water from a fast dive immediately lowers t.
                unit.vertical_speed = std::clamp(
                    unit.vertical_speed, -target_vertical_speed,
                    target_vertical_speed);
            }
            unit.vertical_speed = approach_value(
                unit.vertical_speed, target_vertical_speed, 0.006f * game_frames);
            unit.height = approach_value(
                unit.height, target_height, unit.vertical_speed * game_frames);
            unit.submerged = unit.height < -1.0f;
            unit.airborne = !unit.submerge_requested && unit.height >= 0.0f;
            unit.move_type = unit.airborne ? MoveType::AIR : MoveType::WATER;
            unit.weapon_range = unit.airborne ? 170.0f : 100.0f;
            unit.move_speed = (unit.submerged ? 0.4f : 1.4f) * EngineConstants::FRAME_RATE;
            unit.turn_speed = unit.submerged ? 1.5f : 3.8f;
        }
    }
}

void World::upgrade_unit(Unit& u, const Str& requested_target) {
    upgrade_unit_now_(u, requested_target, true);
}

void World::upgrade_unit_now_(Unit& u, const Str& requested_target, bool charge_cost) {
    if (!u.is_alive || !u.is_complete()) return;

    Str target_name = requested_target.empty()
        ? (u.upgrade_to.empty() ? "__upgrade_tier__" : u.upgrade_to)
        : requested_target;
    if (target_name.empty()) return;

    if (target_name == "__upgrade_tier__") {
        i32 cost = 0;
        f32 build_speed = 0.0f;
        if (!resolve_upgrade_action_(u, target_name, cost, build_speed)) return;
        if (charge_cost && cost > 0 && !spend_credits(static_cast<f32>(cost))) return;

        const f32 old_income = u.income_rate;
        f32 next_max_hp = u.max_hp;
        f32 next_income = u.income_rate;
        f32 next_range = u.weapon_range;
        if (u.unit_type == "extractor" && u.tech_level == 1) {
            next_max_hp = 1000.0f;
            next_income = 12.0f;
        } else if (u.unit_type == "extractor" && u.tech_level == 2) {
            next_max_hp = 2000.0f;
            next_income = 18.0f;
        } else if (u.unit_type == "fabricator" && u.tech_level == 1) {
            next_max_hp = 1300.0f;
            next_income = 7.0f;
        } else if (u.unit_type == "fabricator" && u.tech_level == 2) {
            next_max_hp = 2600.0f;
            next_income = 14.0f;
        } else if (u.unit_type == "laserDefence" && u.tech_level == 1) {
            next_max_hp = 1400.0f;
            next_range = 210.0f;
        } else if (u.unit_type == "supplyDepot" && u.tech_level == 1) {
            // v.M() only switches the depot's visual/tech state. The stock
            // action does not mutate HP, income, range or unit capacity.
            next_max_hp = u.max_hp;
            next_income = u.income_rate;
            next_range = u.weapon_range;
        } else if ((u.unit_type == "landFactory" ||
                    u.unit_type == "airFactory" ||
                    u.unit_type == "seaFactory") && u.tech_level == 1) {
            // Stock factory tier actions only switch the instance's action
            // table/visual state; their HP and other runtime stats stay put.
        } else {
            return;
        }

        const f32 hp_delta = next_max_hp - u.max_hp;
        u.max_hp = next_max_hp;
        u.hp = std::min(u.max_hp, u.hp + hp_delta);
        u.income_rate = next_income;
        u.weapon_range = next_range;
        u.tech_level++;
        u.sync_stock_laser_defence_specs();
        if (old_income > 0.0f) total_income_rate_ -= old_income;
        if (u.income_rate > 0.0f) total_income_rate_ += u.income_rate;
        u.reclaim_cost += static_cast<f32>(cost);
        return;
    }

    // The stock turret exposes two weapon-conversion actions in addition to
    // its sequential gun upgrades.  They are represented by canonical unit
    // definitions in this engine and are valid only from the base turret.
    if (!requested_target.empty() && target_name != u.upgrade_to) {
        const bool turret_conversion =
            u.unit_type == "turret" &&
            (target_name == "turret_artillery" ||
             target_name == "turret_flamethrower");
        if (!turret_conversion) return;
    }

    const UnitDef* def = registry_.get(target_name);
    if (!def) return;
    const UnitDef* source_def = registry_.get(u.unit_type);
    const bool default_upgrade = target_name == u.upgrade_to;
    const i32 cost = (default_upgrade && source_def && source_def->upgrade_cost > 0)
        ? source_def->upgrade_cost
        : def->cost - u.cost;
    if (cost < 0) return;
    if (charge_cost && cost > 0 && !spend_credits(static_cast<f32>(cost))) return;

    // Stock upgrade actions use an additive HP increase; custom convertTo
    // calls the same definition switch with proportional resource pools.
    convert_unit_definition_(u, *def, false);
    u.reclaim_cost += static_cast<f32>(cost);
}

} // namespace rwrl
