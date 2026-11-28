#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <rwrl/engine.h>
#include <cmath>
#include <type_traits>
#include <limits>

namespace py = pybind11;

namespace {
// The host's native-save reader resolves Java fields to these runtime
// members. Keep ordinary Unit properties read-only where appropriate.
void restore_unit_runtime(rwrl::Unit& unit, const py::dict& values) {
    size_t consumed = 0;
    auto restore = [&](const char* key, auto& member) {
        if (values.contains(key)) {
            using Value = std::decay_t<decltype(member)>;
            member = py::cast<Value>(values[key]);
            ++consumed;
        }
    };
#define RESTORE_FIELD(name) restore(#name, unit.name)
    RESTORE_FIELD(hp);
    RESTORE_FIELD(weapon_type);
    RESTORE_FIELD(order_queue);
    RESTORE_FIELD(activate_order_next_frame);
    RESTORE_FIELD(cargo_units);
    RESTORE_FIELD(tech_level);
    RESTORE_FIELD(cost);
    RESTORE_FIELD(reclaim_cost);
    RESTORE_FIELD(max_hp);
    RESTORE_FIELD(shield);
    RESTORE_FIELD(max_shield);
    RESTORE_FIELD(total_shield_absorbed);
    RESTORE_FIELD(shield_disabled);
    RESTORE_FIELD(energy);
    RESTORE_FIELD(ammo);
    RESTORE_FIELD(custom_flags);
    RESTORE_FIELD(energy_disabled);
    RESTORE_FIELD(bomber_attack_heading);
    RESTORE_FIELD(bomber_retreating);
    RESTORE_FIELD(bomber_target_search_done);
    RESTORE_FIELD(build_progress);
    RESTORE_FIELD(build_progress_secondary);
    RESTORE_FIELD(rotation);
    RESTORE_FIELD(height);
    RESTORE_FIELD(lifetime_frames);
    RESTORE_FIELD(sight_range);
    RESTORE_FIELD(vel_x);
    RESTORE_FIELD(vel_y);
    RESTORE_FIELD(move_speed_factor);
    RESTORE_FIELD(turn_velocity);
    RESTORE_FIELD(reversing);
    RESTORE_FIELD(spawn_delay_frames);
    RESTORE_FIELD(spawn_clock_ms);
    RESTORE_FIELD(tree_type);
    RESTORE_FIELD(movement_prediction_enabled);
    RESTORE_FIELD(has_move_target);
    RESTORE_FIELD(arrived);
    RESTORE_FIELD(target_x);
    RESTORE_FIELD(target_y);
    RESTORE_FIELD(collision_impulse_x);
    RESTORE_FIELD(collision_impulse_y);
    RESTORE_FIELD(collision_partner_uid);
    RESTORE_FIELD(collision_partner_team_id);
    RESTORE_FIELD(collision_partner_tick);
    RESTORE_FIELD(collision_enabled);
    RESTORE_FIELD(collision_ignore_uid);
    RESTORE_FIELD(collision_ignore_frames);
    RESTORE_FIELD(wall_bump_frames);
    RESTORE_FIELD(formation_path_fallback_frames);
    RESTORE_FIELD(formation_path_failure_frames);
    RESTORE_FIELD(formation_projection_disabled_frames);
    RESTORE_FIELD(formation_leader_uid);
    RESTORE_FIELD(formation_is_leader);
    RESTORE_FIELD(formation_preferred);
    RESTORE_FIELD(formation_group_size);
    RESTORE_FIELD(formation_offset_x);
    RESTORE_FIELD(formation_offset_y);
    RESTORE_FIELD(formation_heading);
    RESTORE_FIELD(formation_tick);
    RESTORE_FIELD(formation_tick_pending);
    RESTORE_FIELD(formation_controller_id);
    RESTORE_FIELD(formation_distance_sq);
    RESTORE_FIELD(formation_idle_leader_frames);
    RESTORE_FIELD(last_request_x);
    RESTORE_FIELD(last_request_y);
    RESTORE_FIELD(repath_cooldown_frames);
    RESTORE_FIELD(path_request_target_size);
    RESTORE_FIELD(using_path);
    RESTORE_FIELD(waypoints);
    RESTORE_FIELD(current_waypoint);
    RESTORE_FIELD(path_incomplete);
    RESTORE_FIELD(single_node_path_counter);
    RESTORE_FIELD(path_target_override_active);
    RESTORE_FIELD(path_target_override_x);
    RESTORE_FIELD(path_target_override_y);
    RESTORE_FIELD(waypoint_stall_frames);
    RESTORE_FIELD(last_waypoint_stall_frames);
    RESTORE_FIELD(active_order_elapsed_frames);
    RESTORE_FIELD(order_point_dwell_frames);
    RESTORE_FIELD(deterministic_random_salt);
    RESTORE_FIELD(target_scan_timer_frames);
    RESTORE_FIELD(auto_repair_timer_frames);
    RESTORE_FIELD(target_refresh_timer_frames);
    RESTORE_FIELD(parent_uid);
    RESTORE_FIELD(target_uid);
    RESTORE_FIELD(target_team_id);
    RESTORE_FIELD(unarmed_main_target_uid);
    RESTORE_FIELD(unarmed_main_target_team_id);
    RESTORE_FIELD(service_combat_target_uid);
    RESTORE_FIELD(service_combat_target_team_id);
    RESTORE_FIELD(service_order_target_x);
    RESTORE_FIELD(service_order_target_y);
    RESTORE_FIELD(guard_combat_chase_outer);
    RESTORE_FIELD(is_alive);
    RESTORE_FIELD(is_dead);
    RESTORE_FIELD(death_clock_ms);
    RESTORE_FIELD(cleanup_done);
    RESTORE_FIELD(native_unit_list_member);
    RESTORE_FIELD(death_persistent);
    RESTORE_FIELD(crash_fall_velocity);
    RESTORE_FIELD(crash_impacted);
    RESTORE_FIELD(crash_over_water);
    RESTORE_FIELD(has_patrol);
    RESTORE_FIELD(has_guard_pos);
    RESTORE_FIELD(guard_x);
    RESTORE_FIELD(guard_y);
    RESTORE_FIELD(guard_target_uid);
    RESTORE_FIELD(guard_target_team_id);
    RESTORE_FIELD(follow_target_uid);
    RESTORE_FIELD(follow_target_team_id);
    RESTORE_FIELD(attack_order_target_x);
    RESTORE_FIELD(attack_order_target_y);
    RESTORE_FIELD(transport_order_target_x);
    RESTORE_FIELD(transport_order_target_y);
    RESTORE_FIELD(attack_move_combat_target_valid);
    RESTORE_FIELD(attack_move_combat_target_x);
    RESTORE_FIELD(attack_move_combat_target_y);
    RESTORE_FIELD(patrol_logical_target_active);
    RESTORE_FIELD(patrol_logical_target_x);
    RESTORE_FIELD(patrol_logical_target_y);
    RESTORE_FIELD(guard_movement_target_valid);
    RESTORE_FIELD(guard_movement_target_x);
    RESTORE_FIELD(guard_movement_target_y);
    RESTORE_FIELD(income_timer);
    RESTORE_FIELD(flight_phase);
    RESTORE_FIELD(custom_idle_frames);
    RESTORE_FIELD(custom_factory_exit_side);
    RESTORE_FIELD(vertical_speed);
    RESTORE_FIELD(wing_fold_progress);
    RESTORE_FIELD(submerge_requested);
    RESTORE_FIELD(unload_timer_frames);
    RESTORE_FIELD(unloading_all);
    RESTORE_FIELD(behavior);
    RESTORE_FIELD(service_warmup_progress);
    RESTORE_FIELD(service_range_extended);
#undef RESTORE_FIELD
    if (values.contains("legs")) {
        const py::list legs = py::cast<py::list>(values["legs"]);
        if (legs.size() != unit.legs.size()) {
            throw std::invalid_argument("saved legs differ from the unit definition");
        }
        for (size_t i = 0; i < legs.size(); ++i) {
            const py::dict saved = py::cast<py::dict>(legs[i]);
            auto& leg = unit.legs[i];
            leg.x = py::cast<rwrl::f32>(saved["x"]);
            leg.y = py::cast<rwrl::f32>(saved["y"]);
            leg.height = py::cast<rwrl::f32>(saved["height"]);
            leg.rotation = py::cast<rwrl::f32>(saved["rotation"]);
            leg.reset = py::cast<bool>(saved["reset"]);
            leg.moving = py::cast<bool>(saved["moving"]);
            leg.compressed_reset = py::cast<bool>(saved["compressed_reset"]);
            leg.warmup_frames = 0.0f;
            leg.over_liquid = false;
        }
        unit.leg_previous_x = unit.x;
        unit.leg_previous_y = unit.y;
        unit.leg_previous_height = unit.height;
        unit.leg_previous_rotation = unit.rotation;
        ++consumed;
    }
    if (values.contains("turrets")) {
        const py::list turrets = py::cast<py::list>(values["turrets"]);
        for (size_t i = 0; i < unit.weapon_slots.size() && i < turrets.size(); ++i) {
            const py::dict turret = py::cast<py::dict>(turrets[i]);
            auto& slot = unit.weapon_slots[i];
            slot.turret_angle = py::cast<rwrl::f32>(turret["angle"]);
            slot.turret_previous_angle = turret.contains("previous_angle")
                ? py::cast<rwrl::f32>(turret["previous_angle"])
                : slot.turret_angle;
            slot.turret_turn_velocity = py::cast<rwrl::f32>(turret["velocity"]);
            slot.turret_idle_timer = py::cast<rwrl::f32>(turret["idle_timer"]);
            slot.cooldown_remainder = py::cast<rwrl::f32>(turret["cooldown"]);
            slot.cooldown_ticks = static_cast<rwrl::i32>(std::ceil(slot.cooldown_remainder));
            slot.warmup_progress = py::cast<rwrl::f32>(turret["warmup"]);
            slot.aim_offset_x = py::cast<rwrl::f32>(turret["aim_x"]);
            slot.aim_offset_y = py::cast<rwrl::f32>(turret["aim_y"]);
            slot.target_uid = py::cast<rwrl::u32>(turret["target_uid"]);
            if (turret.contains("target_team_id")) {
                slot.target_team_id = py::cast<rwrl::i32>(turret["target_team_id"]);
            }
            if (turret.contains("odd_shot")) {
                slot.odd_shot = py::cast<bool>(turret["odd_shot"]);
            }
        }
        ++consumed;
    }
    if (consumed != values.size()) {
        throw std::invalid_argument("unknown saved unit runtime field");
    }
    if (unit.unit_type == "amphibiousJet") {
        // Q(), b(), m() and z() read eq/r directly in the native class.
        unit.submerged = unit.height < -1.0f;
        unit.airborne = !unit.submerge_requested && unit.height >= 0.0f;
        unit.move_type = unit.airborne ? rwrl::MoveType::AIR : rwrl::MoveType::WATER;
        unit.weapon_range = unit.airborne ? 170.0f : 100.0f;
        unit.move_speed = (unit.submerged ? 0.4f : 1.4f) *
                         rwrl::EngineConstants::FRAME_RATE;
        unit.turn_speed = unit.submerged ? 1.5f : 3.8f;
    } else if (unit.unit_type == "attackSubmarine") {
        unit.submerged = unit.height < -1.0f;
        unit.airborne = false;
        unit.weapon_range = unit.submerged ? 180.0f : 250.0f;
        unit.move_speed = (unit.submerged ? 0.45f : 0.8f) *
                         rwrl::EngineConstants::FRAME_RATE;
    } else if (unit.custom_unit && unit.move_type == rwrl::MoveType::AIR) {
        unit.airborne = unit.height >= 4.0f;
    }
    unit.sync_stock_laser_defence_specs();
}
}

PYBIND11_MODULE(rwrl_core, m, py::mod_gil_not_used()) {
    m.doc() = "Rusted Warfare RL — C++ simulation engine";

    py::enum_<rwrl::MoveType>(m, "MoveType")
        .value("AIR", rwrl::MoveType::AIR)
        .value("LAND", rwrl::MoveType::LAND)
        .value("HOVER", rwrl::MoveType::HOVER)
        .value("BUILDING", rwrl::MoveType::BUILDING)
        .value("NONE", rwrl::MoveType::NONE)
        .value("OVER_CLIFF", rwrl::MoveType::OVER_CLIFF)
        .value("OVER_CLIFF_WATER", rwrl::MoveType::OVER_CLIFF_WATER)
        .value("ROOT", rwrl::MoveType::ROOT)
        .value("WATER", rwrl::MoveType::WATER);

    py::enum_<rwrl::TerrainFlag>(m, "TerrainFlag", py::arithmetic())
        .value("NONE", rwrl::TERRAIN_NONE)
        .value("WATER", rwrl::TERRAIN_WATER)
        .value("WATER_BRIDGE", rwrl::TERRAIN_WATER_BRIDGE)
        .value("LAVA", rwrl::TERRAIN_LAVA)
        .value("CLIFF", rwrl::TERRAIN_CLIFF)
        .value("LARGE_CLIFF", rwrl::TERRAIN_LARGE_CLIFF)
        .value("TREES", rwrl::TERRAIN_TREES)
        .value("BLOCK_LAND", rwrl::TERRAIN_BLOCK_LAND);

    py::enum_<rwrl::WeaponType>(m, "WeaponType")
        .value("MOVE", rwrl::WeaponType::MOVE)
        .value("ATTACK", rwrl::WeaponType::ATTACK)
        .value("BUILD", rwrl::WeaponType::BUILD)
        .value("REPAIR", rwrl::WeaponType::REPAIR)
        .value("LOAD_INTO", rwrl::WeaponType::LOAD_INTO)
        .value("UNLOAD_AT", rwrl::WeaponType::UNLOAD_AT)
        .value("RECLAIM", rwrl::WeaponType::RECLAIM)
        .value("ATTACK_MOVE", rwrl::WeaponType::ATTACK_MOVE)
        .value("LOAD_UP", rwrl::WeaponType::LOAD_UP)
        .value("PATROL", rwrl::WeaponType::PATROL)
        .value("GUARD", rwrl::WeaponType::GUARD)
        .value("GUARD_AT", rwrl::WeaponType::GUARD_AT)
        .value("TOUCH_TARGET", rwrl::WeaponType::TOUCH_TARGET)
        .value("FOLLOW", rwrl::WeaponType::FOLLOW)
        .value("TRIGGER_ACTION", rwrl::WeaponType::TRIGGER_ACTION)
        .value("TRIGGER_WHEN_IN_RANGE", rwrl::WeaponType::TRIGGER_WHEN_IN_RANGE)
        .value("SET_PASSIVE_TARGET", rwrl::WeaponType::SET_PASSIVE_TARGET);

    py::enum_<rwrl::Behavior>(m, "Behavior")
        .value("AGGRESSIVE", rwrl::Behavior::AGGRESSIVE)
        .value("GUARD_AREA", rwrl::Behavior::GUARD_AREA)
        .value("HOLD_FIRE", rwrl::Behavior::HOLD_FIRE)
        .value("RETURN_FIRE", rwrl::Behavior::RETURN_FIRE)
        .value("MIXED", rwrl::Behavior::MIXED)
        .value("ONLY_IN_RANGE", rwrl::Behavior::ONLY_IN_RANGE)
        .value("OUT_OF_RANGE", rwrl::Behavior::OUT_OF_RANGE);

    py::enum_<rwrl::FogMode>(m, "FogMode")
        .value("NONE", rwrl::FogMode::NONE)
        .value("MAP", rwrl::FogMode::MAP)
        .value("LOS", rwrl::FogMode::LOS);

    py::enum_<rwrl::CustomStat>(m, "CustomStat")
        .value("MASS", rwrl::CustomStat::MASS)
        .value("MAX_ENERGY", rwrl::CustomStat::MAX_ENERGY)
        .value("ENERGY", rwrl::CustomStat::ENERGY)
        .value("MAX_HP", rwrl::CustomStat::MAX_HP)
        .value("HP", rwrl::CustomStat::HP)
        .value("MAX_SHIELD", rwrl::CustomStat::MAX_SHIELD)
        .value("SHIELD", rwrl::CustomStat::SHIELD)
        .value("SHIELD_REGEN", rwrl::CustomStat::SHIELD_REGEN)
        .value("ARMOUR", rwrl::CustomStat::ARMOUR)
        .value("MAX_ATTACK_RANGE", rwrl::CustomStat::MAX_ATTACK_RANGE)
        .value("SHOOT_DELAY_MULTIPLIER",
               rwrl::CustomStat::SHOOT_DELAY_MULTIPLIER)
        .value("SHOOT_DAMAGE_MULTIPLIER",
               rwrl::CustomStat::SHOOT_DAMAGE_MULTIPLIER)
        .value("MOVE_SPEED", rwrl::CustomStat::MOVE_SPEED)
        .value("MAX_TURN_SPEED", rwrl::CustomStat::MAX_TURN_SPEED)
        .value("FOG_OF_WAR_SIGHT_RANGE",
               rwrl::CustomStat::FOG_OF_WAR_SIGHT_RANGE)
        .value("NANO_RANGE", rwrl::CustomStat::NANO_RANGE)
        .value("SELF_REGEN_RATE", rwrl::CustomStat::SELF_REGEN_RATE)
        .value("TARGET_HEIGHT", rwrl::CustomStat::TARGET_HEIGHT)
        .value("NANO_FACTORY_SPEED", rwrl::CustomStat::NANO_FACTORY_SPEED);

    py::enum_<rwrl::CustomStatOperator>(m, "CustomStatOperator")
        .value("SET", rwrl::CustomStatOperator::SET)
        .value("ADD", rwrl::CustomStatOperator::ADD)
        .value("SUBTRACT", rwrl::CustomStatOperator::SUBTRACT);

    py::class_<rwrl::CustomStatAssignment>(m, "CustomStatAssignment")
        .def(py::init<>())
        .def(py::init<rwrl::CustomStat, rwrl::f32,
                      rwrl::CustomStatOperator>(),
             py::arg("stat"), py::arg("value"),
             py::arg("op") = rwrl::CustomStatOperator::SET)
        .def_readwrite("stat", &rwrl::CustomStatAssignment::stat)
        .def_readwrite("value", &rwrl::CustomStatAssignment::value)
        .def_readwrite("op", &rwrl::CustomStatAssignment::op);

    py::class_<rwrl::CustomResourceDelta>(m, "CustomResourceDelta")
        .def(py::init<>())
        .def_readwrite("credits", &rwrl::CustomResourceDelta::credits)
        .def_readwrite("energy", &rwrl::CustomResourceDelta::energy)
        .def_readwrite("hp", &rwrl::CustomResourceDelta::hp)
        .def_readwrite("shield", &rwrl::CustomResourceDelta::shield)
        .def_readwrite("ammo", &rwrl::CustomResourceDelta::ammo)
        .def_readwrite("set_flags", &rwrl::CustomResourceDelta::set_flags)
        .def_readwrite("unset_flags", &rwrl::CustomResourceDelta::unset_flags);

    py::enum_<rwrl::CustomResource>(m, "CustomResource")
        .value("NONE", rwrl::CustomResource::NONE)
        .value("CREDITS", rwrl::CustomResource::CREDITS)
        .value("AMMO", rwrl::CustomResource::AMMO)
        .value("ENERGY", rwrl::CustomResource::ENERGY)
        .value("HP", rwrl::CustomResource::HP)
        .value("SHIELD", rwrl::CustomResource::SHIELD);

    py::class_<rwrl::CustomResourceAmountEffect>(
            m, "CustomResourceAmountEffect")
        .def(py::init<>())
        .def_readwrite("resource",
                       &rwrl::CustomResourceAmountEffect::resource)
        .def_readwrite("has_set_value",
                       &rwrl::CustomResourceAmountEffect::has_set_value)
        .def_readwrite("set_value",
                       &rwrl::CustomResourceAmountEffect::set_value)
        .def_readwrite("add_other_resource",
                       &rwrl::CustomResourceAmountEffect::add_other_resource)
        .def_readwrite("multiply_by",
                       &rwrl::CustomResourceAmountEffect::multiply_by);

    py::class_<rwrl::CustomResourceConversionEffect>(
            m, "CustomResourceConversionEffect")
        .def(py::init<>())
        .def_readwrite("source",
                       &rwrl::CustomResourceConversionEffect::source)
        .def_readwrite("target",
                       &rwrl::CustomResourceConversionEffect::target)
        .def_readwrite("min_amount",
                       &rwrl::CustomResourceConversionEffect::min_amount)
        .def_readwrite("max_amount",
                       &rwrl::CustomResourceConversionEffect::max_amount)
        .def_readwrite(
            "multiply_amount_by",
            &rwrl::CustomResourceConversionEffect::multiply_amount_by);

    py::class_<rwrl::CustomWaypointEffect>(m, "CustomWaypointEffect")
        .def(py::init<>())
        .def_readwrite("enabled", &rwrl::CustomWaypointEffect::enabled)
        .def_readwrite("type", &rwrl::CustomWaypointEffect::type)
        .def_readwrite("prepend", &rwrl::CustomWaypointEffect::prepend)
        .def_readwrite("position_from_action",
                       &rwrl::CustomWaypointEffect::position_from_action)
        .def_readwrite("has_position_offset_from_self",
                       &rwrl::CustomWaypointEffect::has_position_offset_from_self)
        .def_readwrite("position_offset_from_self_x",
                       &rwrl::CustomWaypointEffect::position_offset_from_self_x)
        .def_readwrite("position_offset_from_self_y",
                       &rwrl::CustomWaypointEffect::position_offset_from_self_y)
        .def_readwrite("has_position_relative_offset_from_self",
                       &rwrl::CustomWaypointEffect::has_position_relative_offset_from_self)
        .def_readwrite("position_relative_offset_from_self_x",
                       &rwrl::CustomWaypointEffect::position_relative_offset_from_self_x)
        .def_readwrite("position_relative_offset_from_self_y",
                       &rwrl::CustomWaypointEffect::position_relative_offset_from_self_y)
        .def_readwrite("max_time", &rwrl::CustomWaypointEffect::max_time);

    py::class_<rwrl::CustomUnitSpawn>(m, "CustomUnitSpawn")
        .def(py::init<>())
        .def_readwrite("unit_type", &rwrl::CustomUnitSpawn::unit_type)
        .def_readwrite("count", &rwrl::CustomUnitSpawn::count)
        .def_readwrite("spawn_chance", &rwrl::CustomUnitSpawn::spawn_chance)
        .def_readwrite("max_spawn_limit",
                       &rwrl::CustomUnitSpawn::max_spawn_limit)
        .def_readwrite("grid_align", &rwrl::CustomUnitSpawn::grid_align)
        .def_readwrite("skip_if_overlapping",
                       &rwrl::CustomUnitSpawn::skip_if_overlapping)
        .def_readwrite("falling", &rwrl::CustomUnitSpawn::falling)
        .def_readwrite("tech_level", &rwrl::CustomUnitSpawn::tech_level)
        .def_readwrite("always_start_dir_at_zero",
                       &rwrl::CustomUnitSpawn::always_start_dir_at_zero)
        .def_readwrite("offset_x", &rwrl::CustomUnitSpawn::offset_x)
        .def_readwrite("offset_y", &rwrl::CustomUnitSpawn::offset_y)
        .def_readwrite("offset_height",
                       &rwrl::CustomUnitSpawn::offset_height)
        .def_readwrite("offset_dir", &rwrl::CustomUnitSpawn::offset_dir)
        .def_readwrite("offset_random_x",
                       &rwrl::CustomUnitSpawn::offset_random_x)
        .def_readwrite("offset_random_y",
                       &rwrl::CustomUnitSpawn::offset_random_y)
        .def_readwrite("offset_random_dir",
                       &rwrl::CustomUnitSpawn::offset_random_dir)
        .def_readwrite("add_resources",
                       &rwrl::CustomUnitSpawn::add_resources)
        .def_readwrite("transported_units_to_transfer",
                       &rwrl::CustomUnitSpawn::transported_units_to_transfer)
        .def_readwrite("copy_waypoints_from_self",
                       &rwrl::CustomUnitSpawn::copy_waypoints_from_self);

    py::enum_<rwrl::CustomTakeResourcesTeam>(m, "CustomTakeResourcesTeam")
        .value("OWN", rwrl::CustomTakeResourcesTeam::OWN)
        .value("ALLY", rwrl::CustomTakeResourcesTeam::ALLY)
        .value("ALLY_NOT_OWN", rwrl::CustomTakeResourcesTeam::ALLY_NOT_OWN)
        .value("ENEMY", rwrl::CustomTakeResourcesTeam::ENEMY)
        .value("NEUTRAL", rwrl::CustomTakeResourcesTeam::NEUTRAL)
        .value("ANY", rwrl::CustomTakeResourcesTeam::ANY)
        .value("NOT_OWN", rwrl::CustomTakeResourcesTeam::NOT_OWN);

    py::class_<rwrl::CustomTakeResourcesEffect>(
            m, "CustomTakeResourcesEffect")
        .def(py::init<>())
        .def_readwrite("enabled",
                       &rwrl::CustomTakeResourcesEffect::enabled)
        .def_readwrite("resources",
                       &rwrl::CustomTakeResourcesEffect::resources)
        .def_readwrite(
            "include_units_in_transport",
            &rwrl::CustomTakeResourcesEffect::include_units_in_transport)
        .def_readwrite("include_parent",
                       &rwrl::CustomTakeResourcesEffect::include_parent)
        .def_readwrite(
            "include_units_within_range",
            &rwrl::CustomTakeResourcesEffect::include_units_within_range)
        .def_readwrite("range_team",
                       &rwrl::CustomTakeResourcesEffect::range_team)
        .def_readwrite(
            "exclude_units_without_all_resources",
            &rwrl::CustomTakeResourcesEffect::exclude_units_without_all_resources)
        .def_readwrite(
            "has_exclude_units_with_resources",
            &rwrl::CustomTakeResourcesEffect::has_exclude_units_with_resources)
        .def_readwrite(
            "exclude_units_with_resources",
            &rwrl::CustomTakeResourcesEffect::exclude_units_with_resources)
        .def_readwrite("max_units",
                       &rwrl::CustomTakeResourcesEffect::max_units)
        .def_readwrite("discard_collected",
                       &rwrl::CustomTakeResourcesEffect::discard_collected)
        .def_readwrite("keep_resources_on_target",
                       &rwrl::CustomTakeResourcesEffect::keep_resources_on_target)
        .def_readwrite(
            "trigger_action_if_any_collected",
            &rwrl::CustomTakeResourcesEffect::trigger_action_if_any_collected)
        .def_readwrite(
            "trigger_action_if_none_collected",
            &rwrl::CustomTakeResourcesEffect::trigger_action_if_none_collected)
        .def_readwrite(
            "trigger_action_for_each",
            &rwrl::CustomTakeResourcesEffect::trigger_action_for_each)
        .def_readwrite(
            "direct_transfer_stopping_at_zero",
            &rwrl::CustomTakeResourcesEffect::direct_transfer_stopping_at_zero);

    py::class_<rwrl::CustomActionDef>(m, "CustomActionDef")
        .def(py::init<>())
        .def_readwrite("id", &rwrl::CustomActionDef::id)
        .def_readwrite("queueable", &rwrl::CustomActionDef::queueable)
        .def_readwrite("price", &rwrl::CustomActionDef::price)
        .def_readwrite("price_ammo", &rwrl::CustomActionDef::price_ammo)
        .def_readwrite("high_priority_queue", &rwrl::CustomActionDef::high_priority_queue)
        .def_readwrite("fire_turret_index", &rwrl::CustomActionDef::fire_turret_index)
        .def_readwrite("fire_turret_count", &rwrl::CustomActionDef::fire_turret_count)
        .def_readwrite("build_speed", &rwrl::CustomActionDef::build_speed)
        .def_readwrite("can_player_cancel", &rwrl::CustomActionDef::can_player_cancel)
        .def_readwrite("allow_multiple_in_queue", &rwrl::CustomActionDef::allow_multiple_in_queue)
        .def_readwrite("ammo_including_queued_limit",
                       &rwrl::CustomActionDef::ammo_including_queued_limit)
        .def_readwrite("ignore_factory_speed_modifiers", &rwrl::CustomActionDef::ignore_factory_speed_modifiers)
        .def_readwrite("when_building_cannot_move",
                       &rwrl::CustomActionDef::when_building_cannot_move)
        .def_readwrite("has_when_building_rotate_to",
                       &rwrl::CustomActionDef::has_when_building_rotate_to)
        .def_readwrite("when_building_rotate_to",
                       &rwrl::CustomActionDef::when_building_rotate_to)
        .def_readwrite("when_building_rotate_to_or_backwards",
                       &rwrl::CustomActionDef::when_building_rotate_to_or_backwards)
        .def_readwrite("reset_unit_stats",
                       &rwrl::CustomActionDef::reset_unit_stats)
        .def_readwrite("set_unit_stats",
                       &rwrl::CustomActionDef::set_unit_stats)
        .def_readwrite("add_resources",
                       &rwrl::CustomActionDef::add_resources)
        .def_readwrite("delete_num_units_from_transport",
                       &rwrl::CustomActionDef::delete_num_units_from_transport)
        .def_readwrite("start_unloading_transport",
                       &rwrl::CustomActionDef::start_unloading_transport)
        .def_readwrite("force_unload_transport_now",
                       &rwrl::CustomActionDef::force_unload_transport_now)
        .def_readwrite(
            "force_unload_transport_now_only_on_slot",
            &rwrl::CustomActionDef::force_unload_transport_now_only_on_slot)
        .def_readwrite("clear_all_waypoints",
                       &rwrl::CustomActionDef::clear_all_waypoints)
        .def_readwrite("clear_active_waypoint",
                       &rwrl::CustomActionDef::clear_active_waypoint)
        .def_readwrite("add_waypoint",
                       &rwrl::CustomActionDef::add_waypoint)
        .def_readwrite("produce_units",
                       &rwrl::CustomActionDef::produce_units)
        .def_readwrite("spawn_units",
                       &rwrl::CustomActionDef::spawn_units)
        .def_readwrite("resource_amount",
                       &rwrl::CustomActionDef::resource_amount)
        .def_readwrite("resource_conversion",
                       &rwrl::CustomActionDef::resource_conversion)
        .def_readwrite("take_resources",
                       &rwrl::CustomActionDef::take_resources)
        .def_readwrite("delete_self", &rwrl::CustomActionDef::delete_self)
        .def_readwrite("has_set_body_rotation",
                       &rwrl::CustomActionDef::has_set_body_rotation)
        .def_readwrite("set_body_rotation",
                       &rwrl::CustomActionDef::set_body_rotation)
        .def_readwrite("has_set_height",
                       &rwrl::CustomActionDef::has_set_height)
        .def_readwrite("set_height", &rwrl::CustomActionDef::set_height)
        .def_readwrite("clear_all_action_cooldowns",
                       &rwrl::CustomActionDef::clear_all_action_cooldowns)
        .def_readwrite("add_all_action_cooldowns_time",
                       &rwrl::CustomActionDef::add_all_action_cooldowns_time)
        .def_readwrite("add_action_cooldown_time",
                       &rwrl::CustomActionDef::add_action_cooldown_time)
        .def_readwrite("add_action_cooldown_apply_to_actions",
                       &rwrl::CustomActionDef::add_action_cooldown_apply_to_actions)
        .def_readwrite("remove_all_queued_items_without_refund",
                       &rwrl::CustomActionDef::remove_all_queued_items_without_refund)
        .def_readwrite("refund_all_queued_items",
                       &rwrl::CustomActionDef::refund_all_queued_items)
        .def_readwrite("set_built", &rwrl::CustomActionDef::set_built)
        .def_readwrite("has_offset_self_absolute",
                       &rwrl::CustomActionDef::has_offset_self_absolute)
        .def_readwrite("offset_self_absolute_x",
                       &rwrl::CustomActionDef::offset_self_absolute_x)
        .def_readwrite("offset_self_absolute_y",
                       &rwrl::CustomActionDef::offset_self_absolute_y)
        .def_readwrite("offset_self_absolute_height",
                       &rwrl::CustomActionDef::offset_self_absolute_height)
        .def_readwrite("convert_to", &rwrl::CustomActionDef::convert_to);

    py::enum_<rwrl::RoomState>(m, "RoomState")
        .value("WAITING", rwrl::RoomState::WAITING)
        .value("LOADING", rwrl::RoomState::LOADING)
        .value("PLAYING", rwrl::RoomState::PLAYING)
        .value("GAME_OVER", rwrl::RoomState::GAME_OVER);

    py::class_<rwrl::TileRect>(m, "TileRect")
        .def(py::init<>())
        .def_readwrite("left", &rwrl::TileRect::left)
        .def_readwrite("top", &rwrl::TileRect::top)
        .def_readwrite("right", &rwrl::TileRect::right)
        .def_readwrite("bottom", &rwrl::TileRect::bottom);

    py::class_<rwrl::WeaponDef>(m, "WeaponDef")
        .def(py::init<>())
        .def_readwrite("turret_name", &rwrl::WeaponDef::turret_name)
        .def_readwrite("damage", &rwrl::WeaponDef::damage)
        .def_readwrite("splash_damage", &rwrl::WeaponDef::splash_damage)
        .def_readwrite("splash_radius", &rwrl::WeaponDef::splash_radius)
        .def_readwrite("projectile_speed", &rwrl::WeaponDef::projectile_speed)
        .def_readwrite("projectile_speed_spread", &rwrl::WeaponDef::projectile_speed_spread)
        .def_readwrite("projectile_wobble_amplitude", &rwrl::WeaponDef::projectile_wobble_amplitude)
        .def_readwrite("projectile_wobble_frequency_frames", &rwrl::WeaponDef::projectile_wobble_frequency_frames)
        .def_readwrite("projectile_target_speed",
                       &rwrl::WeaponDef::projectile_target_speed)
        .def_readwrite("projectile_acceleration",
                       &rwrl::WeaponDef::projectile_acceleration)
        .def_readwrite("projectile_lifetime",
                       &rwrl::WeaponDef::projectile_lifetime)
        .def_readwrite("splash_expand_time", &rwrl::WeaponDef::splash_expand_time)
        .def_readwrite("splash_friendly_fire", &rwrl::WeaponDef::splash_friendly_fire)
        .def_readwrite("projectile_unguided_speed_height", &rwrl::WeaponDef::projectile_unguided_speed_height)
        .def_readwrite("projectile_unguided_speed_x", &rwrl::WeaponDef::projectile_unguided_speed_x)
        .def_readwrite("projectile_unguided_speed_y", &rwrl::WeaponDef::projectile_unguided_speed_y)
        .def_readwrite("projectile_gravity", &rwrl::WeaponDef::projectile_gravity)
        .def_readwrite("projectile_true_gravity", &rwrl::WeaponDef::projectile_true_gravity)
        .def_readwrite("projectile_retarget_on_death",
                       &rwrl::WeaponDef::projectile_retarget_on_death)
        .def_readwrite("projectile_retarget_range",
                       &rwrl::WeaponDef::projectile_retarget_range)
        .def_readwrite("projectile_retarget_lead",
                       &rwrl::WeaponDef::projectile_retarget_lead)
        .def_readwrite("projectile_deflection_power",
                       &rwrl::WeaponDef::projectile_deflection_power)
        .def_readwrite("projectile_tags", &rwrl::WeaponDef::projectile_tags)
        .def_readwrite("projectile_should_reveal_fog", &rwrl::WeaponDef::projectile_should_reveal_fog)
        .def_readwrite("cooldown_max", &rwrl::WeaponDef::cooldown_max)
        .def_readwrite("recoil_offset", &rwrl::WeaponDef::recoil_offset)
        .def_readwrite("recoil_out_frames",
                       &rwrl::WeaponDef::recoil_out_frames)
        .def_readwrite("recoil_return_frames",
                       &rwrl::WeaponDef::recoil_return_frames)
        .def_readwrite("tracking", &rwrl::WeaponDef::tracking)
        .def_readwrite("fixed_target", &rwrl::WeaponDef::fixed_target)
        .def_readwrite("fixed_target_include_height",
                       &rwrl::WeaponDef::fixed_target_include_height)
        .def_readwrite("disable_lead_targeting",
                       &rwrl::WeaponDef::disable_lead_targeting)
        .def_readwrite("lead_targeting_speed", &rwrl::WeaponDef::lead_targeting_speed)
        .def_readwrite("ground_target_height_offset",
                       &rwrl::WeaponDef::ground_target_height_offset)
        .def_readwrite("ground_target_spread", &rwrl::WeaponDef::ground_target_spread)
        .def_readwrite("hitscan", &rwrl::WeaponDef::hitscan)
        .def_readwrite("persist_after_hit", &rwrl::WeaponDef::persist_after_hit)
        .def_readwrite("instant_reuse_last", &rwrl::WeaponDef::instant_reuse_last)
        .def_readwrite("instant_reuse_last_change_turret_aim", &rwrl::WeaponDef::instant_reuse_last_change_turret_aim)
        .def_readwrite("instant_reuse_last_keep_area_damage_list", &rwrl::WeaponDef::instant_reuse_last_keep_area_damage_list)
        .def_readwrite("move_with_parent", &rwrl::WeaponDef::move_with_parent)
        .def_readwrite("sweep_offset", &rwrl::WeaponDef::sweep_offset)
        .def_readwrite("sweep_offset_from_target_radius", &rwrl::WeaponDef::sweep_offset_from_target_radius)
        .def_readwrite("projectile_flame_weapon", &rwrl::WeaponDef::projectile_flame_weapon)
        .def_readwrite("laser_defence_excluded",
                       &rwrl::WeaponDef::laser_defence_excluded)
        .def_readwrite("laser_defence_enabled", &rwrl::WeaponDef::laser_defence_enabled)
        .def_readwrite("laser_defence_energy_usage", &rwrl::WeaponDef::laser_defence_energy_usage)
        .def_readwrite("intercept_projectile_tags", &rwrl::WeaponDef::intercept_projectile_tags)
        .def_readwrite("intercept_ground_target_distance", &rwrl::WeaponDef::intercept_ground_target_distance)
        .def_readwrite("intercept_projectile_distance", &rwrl::WeaponDef::intercept_projectile_distance)
        .def_readwrite("intercept_projectile_height", &rwrl::WeaponDef::intercept_projectile_height)
        .def_readwrite("intercept_ammo_usage", &rwrl::WeaponDef::intercept_ammo_usage)
        .def_readwrite("intercept_required_flags", &rwrl::WeaponDef::intercept_required_flags)
        .def_readwrite("intercept_missing_flags", &rwrl::WeaponDef::intercept_missing_flags)
        .def_readwrite("limiting_range", &rwrl::WeaponDef::limiting_range)
        .def_readwrite("limiting_min_range", &rwrl::WeaponDef::limiting_min_range)
        .def_readwrite("limiting_angle", &rwrl::WeaponDef::limiting_angle)
        .def_readwrite("splash_same_layer",
                       &rwrl::WeaponDef::splash_same_layer)
        .def_readwrite("targets_air", &rwrl::WeaponDef::targets_air)
        .def_readwrite("targets_ground", &rwrl::WeaponDef::targets_ground)
        .def_readwrite("targets_underwater", &rwrl::WeaponDef::targets_underwater)
        .def_readwrite("targets_not_touching_water", &rwrl::WeaponDef::targets_not_touching_water)
        .def_readwrite("shield_mult", &rwrl::WeaponDef::shield_mult)
        .def_readwrite("shield_absorb", &rwrl::WeaponDef::shield_absorb)
        .def_readwrite("hp_mult", &rwrl::WeaponDef::hp_mult)
        .def_readwrite("building_damage_mult",
                       &rwrl::WeaponDef::building_damage_mult)
        .def_readwrite("air_damage_mult",
                       &rwrl::WeaponDef::air_damage_mult)
        .def_readwrite("experimental_damage_mult",
                       &rwrl::WeaponDef::experimental_damage_mult)
        .def_readwrite("warmup_frames", &rwrl::WeaponDef::warmup_frames)
        .def_readwrite("fire_delay_frames",
                       &rwrl::WeaponDef::fire_delay_frames)
        .def_readwrite("energy_usage", &rwrl::WeaponDef::energy_usage)
        .def_readwrite("warmup_no_reset", &rwrl::WeaponDef::warmup_no_reset)
        .def_readwrite("warmup_call_down_rate",
                       &rwrl::WeaponDef::warmup_call_down_rate)
        .def_readwrite("warmup_shoot_delay_transfer",
                       &rwrl::WeaponDef::warmup_shoot_delay_transfer)
        .def_readwrite("fire_enabled", &rwrl::WeaponDef::fire_enabled)
        .def_readwrite("linked_slot", &rwrl::WeaponDef::linked_slot)
        .def_readwrite("attached_to_slot", &rwrl::WeaponDef::attached_to_slot)
        .def_readwrite("delay_link_slot", &rwrl::WeaponDef::delay_link_slot)
        .def_readwrite("turret_turn_speed",
                       &rwrl::WeaponDef::turret_turn_speed)
        .def_readwrite("turret_turn_acceleration",
                       &rwrl::WeaponDef::turret_turn_acceleration)
        .def_readwrite("turret_turn_deceleration",
                       &rwrl::WeaponDef::turret_turn_deceleration)
        .def_readwrite("turret_resets_to_idle",
                       &rwrl::WeaponDef::turret_resets_to_idle)
        .def_readwrite("turret_idle_direction",
                       &rwrl::WeaponDef::turret_idle_direction)
        .def_readwrite("turret_idle_direction_reversing",
                       &rwrl::WeaponDef::turret_idle_direction_reversing)
        .def_readwrite("turret_idle_spin",
                       &rwrl::WeaponDef::turret_idle_spin)
        .def_readwrite("turret_idle_sweep_angle",
                       &rwrl::WeaponDef::turret_idle_sweep_angle)
        .def_readwrite("turret_idle_sweep_delay",
                       &rwrl::WeaponDef::turret_idle_sweep_delay)
        .def_readwrite("turret_idle_sweep_speed",
                       &rwrl::WeaponDef::turret_idle_sweep_speed)
        .def_readwrite("turret_idle_sweep_random_delay",
                       &rwrl::WeaponDef::turret_idle_sweep_random_delay)
        .def_readwrite("turret_idle_sweep_random_angle",
                       &rwrl::WeaponDef::turret_idle_sweep_random_angle)
        .def_readwrite("turret_idle_sweep_requires_stationary",
                       &rwrl::WeaponDef::turret_idle_sweep_requires_stationary)
        .def_readwrite("aim_tolerance", &rwrl::WeaponDef::aim_tolerance)
        .def_readwrite("aim_offset_spread",
                       &rwrl::WeaponDef::aim_offset_spread)
        .def_readwrite("muzzle_offset", &rwrl::WeaponDef::muzzle_offset)
        .def_readwrite("muzzle_side", &rwrl::WeaponDef::muzzle_side)
        .def_readwrite("muzzle_side_odd", &rwrl::WeaponDef::muzzle_side_odd)
        .def_readwrite("mount_forward", &rwrl::WeaponDef::mount_forward)
        .def_readwrite("mount_side", &rwrl::WeaponDef::mount_side)
        .def_readwrite("projectile_uses_muzzle",
                       &rwrl::WeaponDef::projectile_uses_muzzle)
        .def_readwrite("height_guided", &rwrl::WeaponDef::height_guided)
        .def_readwrite("nuke_weapon", &rwrl::WeaponDef::nuke_weapon)
        .def_readwrite("launch_height", &rwrl::WeaponDef::launch_height)
        .def_readwrite("cruise_height", &rwrl::WeaponDef::cruise_height)
        .def_readwrite("vertical_speed", &rwrl::WeaponDef::vertical_speed)
        .def_readwrite("fixed_turret", &rwrl::WeaponDef::fixed_turret)
        .def_readwrite("turret_rotate_with_body",
                       &rwrl::WeaponDef::turret_rotate_with_body);

    py::class_<rwrl::LegDef>(m, "LegDef")
        .def(py::init<>())
        .def_readwrite("x", &rwrl::LegDef::x)
        .def_readwrite("y", &rwrl::LegDef::y)
        .def_readwrite("lifting_height", &rwrl::LegDef::lifting_height)
        .def_readwrite("target_height", &rwrl::LegDef::target_height)
        .def_readwrite("height_relative", &rwrl::LegDef::height_relative)
        .def_readwrite("end_direction", &rwrl::LegDef::end_direction)
        .def_readwrite("lock_movement", &rwrl::LegDef::lock_movement)
        .def_readwrite("prediction_multiplier", &rwrl::LegDef::prediction_multiplier)
        .def_readwrite("check_neighbours", &rwrl::LegDef::check_neighbours)
        .def_readwrite("favour_opposite_side", &rwrl::LegDef::favour_opposite_side)
        .def_readwrite("hidden", &rwrl::LegDef::hidden)
        .def_readwrite("explode_on_death", &rwrl::LegDef::explode_on_death)
        .def_readwrite("move_speed", &rwrl::LegDef::move_speed)
        .def_readwrite("move_warmup_frames", &rwrl::LegDef::move_warmup_frames)
        .def_readwrite("rotate_speed", &rwrl::LegDef::rotate_speed)
        .def_readwrite("height_speed", &rwrl::LegDef::height_speed)
        .def_readwrite("hold_distance_min", &rwrl::LegDef::hold_distance_min)
        .def_readwrite("max_moving_legs", &rwrl::LegDef::max_moving_legs)
        .def_readwrite("move_only_if_furthest", &rwrl::LegDef::move_only_if_furthest)
        .def_readwrite("hold_distance_max", &rwrl::LegDef::hold_distance_max)
        .def_readwrite("hard_limit", &rwrl::LegDef::hard_limit)
        .def_readwrite("neighbours", &rwrl::LegDef::neighbours);

    py::class_<rwrl::LegState>(m, "LegState")
        .def_readonly("x", &rwrl::LegState::x)
        .def_readonly("y", &rwrl::LegState::y)
        .def_readonly("height", &rwrl::LegState::height)
        .def_readonly("moving", &rwrl::LegState::moving)
        .def_readonly("rotation", &rwrl::LegState::rotation)
        .def_readonly("warmup_frames", &rwrl::LegState::warmup_frames)
        .def_readonly("target_x", &rwrl::LegState::target_x)
        .def_readonly("target_y", &rwrl::LegState::target_y)
        .def_readonly("distance_sq", &rwrl::LegState::distance_sq);

    py::class_<rwrl::CustomAutoTriggerDef>(m, "CustomAutoTriggerDef")
        .def(py::init<>())
        .def_readwrite("action_id", &rwrl::CustomAutoTriggerDef::action_id)
        .def_readwrite("when_over_water", &rwrl::CustomAutoTriggerDef::when_over_water)
        .def_readwrite("check_interval_frames", &rwrl::CustomAutoTriggerDef::check_interval_frames);

    py::class_<rwrl::UnitDef>(m, "UnitDef")
        .def(py::init<>())
        .def_readwrite("name", &rwrl::UnitDef::name)
        .def_readwrite("cost", &rwrl::UnitDef::cost)
        .def_readwrite("build_tech_levels", &rwrl::UnitDef::build_tech_levels)
        .def_readwrite("has_declared_build_menu", &rwrl::UnitDef::has_declared_build_menu)
        .def_readwrite("factory_products", &rwrl::UnitDef::factory_products)
        .def_readwrite("construction_tech_costs", &rwrl::UnitDef::construction_tech_costs)
        .def_readwrite("hp", &rwrl::UnitDef::hp)
        .def_readwrite("speed", &rwrl::UnitDef::speed)
        .def_readwrite("turn_speed", &rwrl::UnitDef::turn_speed)
        .def_readwrite("turn_acceleration", &rwrl::UnitDef::turn_acceleration)
        .def_readwrite("move_acceleration", &rwrl::UnitDef::move_acceleration)
        .def_readwrite("move_deceleration", &rwrl::UnitDef::move_deceleration)
        .def_readwrite("strafes", &rwrl::UnitDef::strafes)
        .def_readwrite("move_ignores_body", &rwrl::UnitDef::move_ignores_body)
        .def_readwrite("joins_group_formations", &rwrl::UnitDef::joins_group_formations)
        .def_readwrite("reverse_speed_multiplier", &rwrl::UnitDef::reverse_speed_multiplier)
        .def_readwrite("behavior", &rwrl::UnitDef::behavior)
        .def_readwrite("armour", &rwrl::UnitDef::armour)
        .def_readwrite("armour_min_damage", &rwrl::UnitDef::armour_min_damage)
        .def_readwrite("shoot_delay_multiplier",
                      &rwrl::UnitDef::shoot_delay_multiplier)
        .def_readwrite("shoot_damage_multiplier",
                      &rwrl::UnitDef::shoot_damage_multiplier)
        .def_readwrite("nano_range", &rwrl::UnitDef::nano_range)
        .def_readwrite("nano_turret_index", &rwrl::UnitDef::nano_turret_index)
        .def_readwrite("nano_build_speed", &rwrl::UnitDef::nano_build_speed)
        .def_readwrite("self_regen_rate", &rwrl::UnitDef::self_regen_rate)
        .def_readwrite("target_height", &rwrl::UnitDef::target_height)
        .def_readwrite("nano_factory_speed", &rwrl::UnitDef::nano_factory_speed)
        .def_readwrite("target_height_drift", &rwrl::UnitDef::target_height_drift)
        .def_readwrite("height_change_rate", &rwrl::UnitDef::height_change_rate)
        .def_readwrite("falling_acceleration", &rwrl::UnitDef::falling_acceleration)
        .def_readwrite("land_on_ground", &rwrl::UnitDef::land_on_ground)
        .def_readwrite("land_on_ground_only_idle", &rwrl::UnitDef::land_on_ground_only_idle)
        .def_readwrite("base_nano_range", &rwrl::UnitDef::base_nano_range)
        .def_readwrite("is_builder", &rwrl::UnitDef::is_builder)
        .def_readwrite("can_repair", &rwrl::UnitDef::can_repair)
        .def_readwrite("can_repair_any", &rwrl::UnitDef::can_repair_any)
        .def_readwrite("auto_repair", &rwrl::UnitDef::auto_repair)
        .def_readwrite("nano_repair_speed", &rwrl::UnitDef::nano_repair_speed)
        .def_readwrite("nano_reclaim_speed", &rwrl::UnitDef::nano_reclaim_speed)
        .def_readwrite("resource_reclaim_multiplier", &rwrl::UnitDef::resource_reclaim_multiplier)
        .def_readwrite("nano_unbuild_speed", &rwrl::UnitDef::nano_unbuild_speed)
        .def_readwrite("can_reclaim", &rwrl::UnitDef::can_reclaim)
        .def_readwrite("can_repair_buildings", &rwrl::UnitDef::can_repair_buildings)
        .def_readwrite("can_reclaim_resources", &rwrl::UnitDef::can_reclaim_resources)
        .def_readwrite("can_be_repaired", &rwrl::UnitDef::can_be_repaired)
        .def_readwrite("place_only_on_res_pool", &rwrl::UnitDef::place_only_on_res_pool)
        .def_readwrite("extra_build_range_when_building_this",
                      &rwrl::UnitDef::extra_build_range_when_building_this)
        .def_readwrite("range", &rwrl::UnitDef::range)
        .def_readwrite("attack_follow_e_flag", &rwrl::UnitDef::attack_follow_e_flag)
        .def_readwrite("attack_follow_weapon_n", &rwrl::UnitDef::attack_follow_weapon_n)
        .def_readwrite("bomber_attack_movement", &rwrl::UnitDef::bomber_attack_movement)
        .def_readwrite("disable_passive_targeting", &rwrl::UnitDef::disable_passive_targeting)
        .def_readwrite("turret_multi_targeting", &rwrl::UnitDef::turret_multi_targeting)
        .def_readwrite("damage", &rwrl::UnitDef::damage)
        .def_readwrite("income", &rwrl::UnitDef::income)
        .def_readwrite("shield", &rwrl::UnitDef::shield)
        .def_readwrite("start_shield_at_zero", &rwrl::UnitDef::start_shield_at_zero)
        .def_readwrite("ignore_move_orders", &rwrl::UnitDef::ignore_move_orders)
        .def_readwrite("radius", &rwrl::UnitDef::radius)
        .def_readwrite("mass", &rwrl::UnitDef::mass)
        .def_readwrite("build_time", &rwrl::UnitDef::build_time)
        .def_readwrite("cooldown_max", &rwrl::UnitDef::cooldown_max)
        .def_readwrite("ammo_reload_ticks", &rwrl::UnitDef::ammo_reload_ticks)
        .def_readwrite("build_speed", &rwrl::UnitDef::build_speed)
        .def_readwrite("is_building", &rwrl::UnitDef::is_building)
        .def_readwrite("weapon_type", &rwrl::UnitDef::weapon_type)
        .def_readwrite("is_melee", &rwrl::UnitDef::is_melee)
        .def_readwrite("is_beam", &rwrl::UnitDef::is_beam)
        .def_readwrite("chain_count", &rwrl::UnitDef::chain_count)
        .def_readwrite("chain_range", &rwrl::UnitDef::chain_range)
        .def_readwrite("is_flame", &rwrl::UnitDef::is_flame)
        .def_readwrite("flame_expand_rate", &rwrl::UnitDef::flame_expand_rate)
        .def_readwrite("built_from", &rwrl::UnitDef::built_from)
        .def_readwrite("built_from_factories", &rwrl::UnitDef::built_from_factories)
        .def_readwrite("override_and_replace", &rwrl::UnitDef::override_and_replace)
        .def_readwrite("tech_level", &rwrl::UnitDef::tech_level)
        .def_readwrite("splash_radius", &rwrl::UnitDef::splash_radius)
        .def_readwrite("splash_damage", &rwrl::UnitDef::splash_damage)
        .def_readwrite("projectile_speed", &rwrl::UnitDef::projectile_speed)
        .def_readwrite("projectile_lifetime", &rwrl::UnitDef::projectile_lifetime)
        .def_readwrite("projectile_tracking", &rwrl::UnitDef::projectile_tracking)
        .def_readwrite("projectile_hitscan", &rwrl::UnitDef::projectile_hitscan)
        .def_readwrite("can_move", &rwrl::UnitDef::can_move)
        .def_readwrite("can_attack", &rwrl::UnitDef::can_attack)
        .def_readwrite("airborne", &rwrl::UnitDef::airborne)
        .def_readwrite("move_type", &rwrl::UnitDef::move_type)
        .def_readwrite("sight_range", &rwrl::UnitDef::sight_range)
        .def_readwrite("ammo_max", &rwrl::UnitDef::ammo_max)
        .def_readwrite("initial_ammo", &rwrl::UnitDef::initial_ammo)
        .def_readwrite("targets_air", &rwrl::UnitDef::targets_air)
        .def_readwrite("targets_ground", &rwrl::UnitDef::targets_ground)
        .def_readwrite("ground_attack_disabled_while_flying",
                       &rwrl::UnitDef::ground_attack_disabled_while_flying)
        .def_readwrite("targets_underwater", &rwrl::UnitDef::targets_underwater)
        .def_readwrite("targets_not_touching_water", &rwrl::UnitDef::targets_not_touching_water)
        .def_readwrite("max_energy", &rwrl::UnitDef::max_energy)
        .def_readwrite("initial_energy", &rwrl::UnitDef::initial_energy)
        .def_readwrite("energy_regen_rate", &rwrl::UnitDef::energy_regen_rate)
        .def_readwrite("energy_regen_per_frame", &rwrl::UnitDef::energy_regen_per_frame)
        .def_readwrite("energy_recharge_regen_per_frame",
                       &rwrl::UnitDef::energy_recharge_regen_per_frame)
        .def_readwrite("energy_needs_full_recharge", &rwrl::UnitDef::energy_needs_full_recharge)
        .def_readwrite("start_energy_at_zero", &rwrl::UnitDef::start_energy_at_zero)
        .def_readwrite("die_on_zero_energy", &rwrl::UnitDef::die_on_zero_energy)
        .def_readwrite("shield_mult", &rwrl::UnitDef::shield_mult)
        .def_readwrite("shield_absorb", &rwrl::UnitDef::shield_absorb)
        .def_readwrite("hp_mult", &rwrl::UnitDef::hp_mult)
        .def_readwrite("air_damage_mult", &rwrl::UnitDef::air_damage_mult)
        .def_readwrite("experimental_damage_mult", &rwrl::UnitDef::experimental_damage_mult)
        .def_readwrite("is_experimental", &rwrl::UnitDef::is_experimental)
        .def_readwrite("shield_regen_rate", &rwrl::UnitDef::shield_regen_rate)
        .def_property("shield_regen_per_frame",
            [](const rwrl::UnitDef& definition) {
                return definition.shield_regen_rate ==
                        definition.shield_regen_per_frame * rwrl::EngineConstants::FRAME_RATE
                    ? definition.shield_regen_per_frame
                    : definition.shield_regen_rate / rwrl::EngineConstants::FRAME_RATE;
            },
            [](rwrl::UnitDef& definition, rwrl::f32 value) {
                definition.shield_regen_per_frame = value;
                definition.shield_regen_rate = value * rwrl::EngineConstants::FRAME_RATE;
            })
        .def_readwrite("upgrade_cost", &rwrl::UnitDef::upgrade_cost)
        .def_readwrite("upgrade_to", &rwrl::UnitDef::upgrade_to)
        .def_readwrite("cargo_capacity", &rwrl::UnitDef::cargo_capacity)
        .def_readwrite("transport_unload_condition", &rwrl::UnitDef::transport_unload_condition)
        .def_readwrite("transport_add_unload_option", &rwrl::UnitDef::transport_add_unload_option)
        .def_readwrite("transport_unload_delay", &rwrl::UnitDef::transport_unload_delay)
        .def_readwrite("hovercraft_unload_style",
                      &rwrl::UnitDef::hovercraft_unload_style)
        .def_readwrite("cargo_weight", &rwrl::UnitDef::cargo_weight)
        .def_readwrite("transportable", &rwrl::UnitDef::transportable)
        .def_readwrite("building_damage_mult", &rwrl::UnitDef::building_damage_mult)
        .def_readwrite("is_nuke", &rwrl::UnitDef::is_nuke)
        .def_readwrite("is_antinuke", &rwrl::UnitDef::is_antinuke)
        .def_readwrite("nuke_range", &rwrl::UnitDef::nuke_range)
        .def_readonly("path_min_clearance", &rwrl::UnitDef::path_min_clearance)
        .def_readwrite("collision_footprint",
                      &rwrl::UnitDef::collision_footprint)
        .def_readwrite("placement_footprint",
                       &rwrl::UnitDef::placement_footprint)
        .def_readwrite("has_factory_exit_config",
                       &rwrl::UnitDef::has_factory_exit_config)
        .def_readwrite("factory_exit_x", &rwrl::UnitDef::factory_exit_x)
        .def_readwrite("factory_exit_y", &rwrl::UnitDef::factory_exit_y)
        .def_readwrite("factory_exit_move_away_amount",
                       &rwrl::UnitDef::factory_exit_move_away_amount)
        .def_readwrite("factory_exit_has_dir_offset",
                       &rwrl::UnitDef::factory_exit_has_dir_offset)
        .def_readwrite("factory_exit_dir_offset",
                       &rwrl::UnitDef::factory_exit_dir_offset)
        .def_readwrite("factory_exit_height_offset",
                       &rwrl::UnitDef::factory_exit_height_offset)
        .def_readwrite("factory_exit_height_ignore_parent",
                       &rwrl::UnitDef::factory_exit_height_ignore_parent)
        .def_readwrite("factory_keep_built_units",
                       &rwrl::UnitDef::factory_keep_built_units)
        .def_readwrite("building_to_footprint_offset_x",
                       &rwrl::UnitDef::building_to_footprint_offset_x)
        .def_readwrite("building_to_footprint_offset_y",
                       &rwrl::UnitDef::building_to_footprint_offset_y)
        .def_readwrite("weapons", &rwrl::UnitDef::weapons)
        .def_readwrite("can_build", &rwrl::UnitDef::can_build)
        .def_readwrite("income_interval", &rwrl::UnitDef::income_interval)
        .def_readwrite("auto_triggers", &rwrl::UnitDef::auto_triggers)
        .def_readwrite("auto_trigger_cooldown_frames", &rwrl::UnitDef::auto_trigger_cooldown_frames)
        .def_readwrite("soft_collision_on_all",
                       &rwrl::UnitDef::soft_collision_on_all)
        .def_readwrite("slow_death_fall", &rwrl::UnitDef::slow_death_fall)
        .def_readwrite("explode_on_death_ground_collision",
                       &rwrl::UnitDef::explode_on_death_ground_collision)
        .def_readwrite("hide_scorch_mark",
                       &rwrl::UnitDef::hide_scorch_mark)
        .def_readwrite("death_effect_uid_count",
                       &rwrl::UnitDef::death_effect_uid_count)
        .def_readwrite("has_wreck_image", &rwrl::UnitDef::has_wreck_image)
        .def_readwrite("default_ground_death_effect",
                       &rwrl::UnitDef::default_ground_death_effect)
        .def_readwrite("fire_turret_on_death", &rwrl::UnitDef::fire_turret_on_death)
        .def_readwrite("nuke_on_death", &rwrl::UnitDef::nuke_on_death)
        .def_readwrite("nuke_on_death_range", &rwrl::UnitDef::nuke_on_death_range)
        .def_readwrite("nuke_on_death_damage", &rwrl::UnitDef::nuke_on_death_damage)
        .def_readwrite("nuke_on_death_disable_when_no_nuke", &rwrl::UnitDef::nuke_on_death_disable_when_no_nuke)
        .def_readwrite("legs", &rwrl::UnitDef::legs)
        .def_readwrite("leg_rotation_slot", &rwrl::UnitDef::leg_rotation_slot)
        .def_readwrite("leg_death_scorch", &rwrl::UnitDef::leg_death_scorch);

    py::class_<rwrl::WeaponSlot>(m, "WeaponSlot")
        .def_readonly("target_uid", &rwrl::WeaponSlot::target_uid)
        .def_readonly("target_team_id", &rwrl::WeaponSlot::target_team_id)
        .def_readonly("damage", &rwrl::WeaponSlot::damage)
        .def_readonly("splash_damage", &rwrl::WeaponSlot::splash_damage)
        .def_readonly("splash_radius", &rwrl::WeaponSlot::splash_radius)
        .def_readonly("projectile_speed", &rwrl::WeaponSlot::projectile_speed)
        .def_readonly("projectile_speed_spread", &rwrl::WeaponSlot::projectile_speed_spread)
        .def_readonly("projectile_wobble_amplitude", &rwrl::WeaponSlot::projectile_wobble_amplitude)
        .def_readonly("projectile_wobble_frequency_frames", &rwrl::WeaponSlot::projectile_wobble_frequency_frames)
        .def_readonly("projectile_target_speed", &rwrl::WeaponSlot::projectile_target_speed)
        .def_readonly("projectile_acceleration", &rwrl::WeaponSlot::projectile_acceleration)
        .def_readonly("projectile_lifetime", &rwrl::WeaponSlot::projectile_lifetime)
        .def_readonly("projectile_unguided_speed_height", &rwrl::WeaponSlot::projectile_unguided_speed_height)
        .def_readonly("projectile_unguided_speed_x", &rwrl::WeaponSlot::projectile_unguided_speed_x)
        .def_readonly("projectile_unguided_speed_y", &rwrl::WeaponSlot::projectile_unguided_speed_y)
        .def_readonly("projectile_gravity", &rwrl::WeaponSlot::projectile_gravity)
        .def_readonly("projectile_true_gravity", &rwrl::WeaponSlot::projectile_true_gravity)
        .def_readonly("projectile_retarget_on_death", &rwrl::WeaponSlot::projectile_retarget_on_death)
        .def_readonly("projectile_deflection_power", &rwrl::WeaponSlot::projectile_deflection_power)
        .def_readonly("projectile_tags", &rwrl::WeaponSlot::projectile_tags)
        .def_readonly("projectile_should_reveal_fog", &rwrl::WeaponSlot::projectile_should_reveal_fog)
        .def_readonly("projectile_retarget_range", &rwrl::WeaponSlot::projectile_retarget_range)
        .def_readonly("projectile_retarget_lead", &rwrl::WeaponSlot::projectile_retarget_lead)
        .def_readonly("cooldown_ticks", &rwrl::WeaponSlot::cooldown_ticks)
        .def_readonly("cooldown_max", &rwrl::WeaponSlot::cooldown_max)
        .def_readonly("cooldown_remainder", &rwrl::WeaponSlot::cooldown_remainder)
        .def_readonly("fire_delay_frames",
                      &rwrl::WeaponSlot::fire_delay_frames)
        .def_readonly("energy_usage", &rwrl::WeaponSlot::energy_usage)
        .def_readonly("tracking", &rwrl::WeaponSlot::tracking)
        .def_readonly("fixed_target", &rwrl::WeaponSlot::fixed_target)
        .def_readonly("fixed_target_include_height",
                      &rwrl::WeaponSlot::fixed_target_include_height)
        .def_readonly("disable_lead_targeting",
                      &rwrl::WeaponSlot::disable_lead_targeting)
        .def_readonly("lead_targeting_speed", &rwrl::WeaponSlot::lead_targeting_speed)
        .def_readonly("ground_target_height_offset",
                      &rwrl::WeaponSlot::ground_target_height_offset)
        .def_readonly("ground_target_spread", &rwrl::WeaponSlot::ground_target_spread)
        .def_readonly("hitscan", &rwrl::WeaponSlot::hitscan)
        .def_readonly("persist_after_hit", &rwrl::WeaponSlot::persist_after_hit)
        .def_readonly("instant_reuse_last", &rwrl::WeaponSlot::instant_reuse_last)
        .def_readonly("instant_reuse_last_change_turret_aim", &rwrl::WeaponSlot::instant_reuse_last_change_turret_aim)
        .def_readonly("instant_reuse_last_keep_area_damage_list", &rwrl::WeaponSlot::instant_reuse_last_keep_area_damage_list)
        .def_readonly("move_with_parent", &rwrl::WeaponSlot::move_with_parent)
        .def_readonly("sweep_offset", &rwrl::WeaponSlot::sweep_offset)
        .def_readonly("sweep_offset_from_target_radius", &rwrl::WeaponSlot::sweep_offset_from_target_radius)
        .def_readonly("projectile_flame_weapon", &rwrl::WeaponSlot::projectile_flame_weapon)
        .def_readonly("laser_defence_excluded", &rwrl::WeaponSlot::laser_defence_excluded)
        .def_readonly("laser_defence_enabled", &rwrl::WeaponSlot::laser_defence_enabled)
        .def_readonly("laser_defence_energy_usage", &rwrl::WeaponSlot::laser_defence_energy_usage)
        .def_readonly("intercept_projectile_tags", &rwrl::WeaponSlot::intercept_projectile_tags)
        .def_readonly("intercept_ground_target_distance", &rwrl::WeaponSlot::intercept_ground_target_distance)
        .def_readonly("intercept_projectile_distance", &rwrl::WeaponSlot::intercept_projectile_distance)
        .def_readonly("intercept_projectile_height", &rwrl::WeaponSlot::intercept_projectile_height)
        .def_readonly("intercept_ammo_usage", &rwrl::WeaponSlot::intercept_ammo_usage)
        .def_readonly("intercept_required_flags", &rwrl::WeaponSlot::intercept_required_flags)
        .def_readonly("intercept_missing_flags", &rwrl::WeaponSlot::intercept_missing_flags)
        .def_readonly("limiting_range", &rwrl::WeaponSlot::limiting_range)
        .def_readonly("limiting_angle", &rwrl::WeaponSlot::limiting_angle)
        .def_readonly("splash_same_layer", &rwrl::WeaponSlot::splash_same_layer)
        .def_readonly("targets_air", &rwrl::WeaponSlot::targets_air)
        .def_readonly("targets_ground", &rwrl::WeaponSlot::targets_ground)
        .def_readonly("targets_underwater", &rwrl::WeaponSlot::targets_underwater)
        .def_readonly("targets_not_touching_water", &rwrl::WeaponSlot::targets_not_touching_water)
        .def_readonly("shield_mult", &rwrl::WeaponSlot::shield_mult)
        .def_readonly("shield_absorb", &rwrl::WeaponSlot::shield_absorb)
        .def_readonly("hp_mult", &rwrl::WeaponSlot::hp_mult)
        .def_readonly("building_damage_mult", &rwrl::WeaponSlot::building_damage_mult)
        .def_readonly("air_damage_mult", &rwrl::WeaponSlot::air_damage_mult)
        .def_readonly("experimental_damage_mult", &rwrl::WeaponSlot::experimental_damage_mult)
        .def_readonly("warmup_frames", &rwrl::WeaponSlot::warmup_frames)
        .def_readonly("warmup_progress", &rwrl::WeaponSlot::warmup_progress)
        .def_readonly("aim_ready", &rwrl::WeaponSlot::aim_ready)
        .def_readonly("warmup_hold_this_frame",
                      &rwrl::WeaponSlot::warmup_hold_this_frame)
        .def_readonly("warmup_no_reset", &rwrl::WeaponSlot::warmup_no_reset)
        .def_readonly("warmup_call_down_rate",
                      &rwrl::WeaponSlot::warmup_call_down_rate)
        .def_readonly("warmup_shoot_delay_transfer",
                      &rwrl::WeaponSlot::warmup_shoot_delay_transfer)
        .def_readonly("fire_enabled", &rwrl::WeaponSlot::fire_enabled)
        .def_readonly("linked_slot", &rwrl::WeaponSlot::linked_slot)
        .def_readonly("attached_to_slot", &rwrl::WeaponSlot::attached_to_slot)
        .def_readonly("delay_link_slot", &rwrl::WeaponSlot::delay_link_slot)
        .def_readonly("turret_angle", &rwrl::WeaponSlot::turret_angle)
        .def_readonly("turret_previous_angle", &rwrl::WeaponSlot::turret_previous_angle)
        .def_readonly("turret_turn_velocity", &rwrl::WeaponSlot::turret_turn_velocity)
        .def_readonly("turret_idle_timer", &rwrl::WeaponSlot::turret_idle_timer)
        .def_readonly("turret_sweep_timer",
                      &rwrl::WeaponSlot::turret_sweep_timer)
        .def_readonly("turret_sweep_direction",
                      &rwrl::WeaponSlot::turret_sweep_direction)
        .def_readonly("turret_turn_speed", &rwrl::WeaponSlot::turret_turn_speed)
        .def_readonly("turret_turn_acceleration", &rwrl::WeaponSlot::turret_turn_acceleration)
        .def_readonly("turret_turn_deceleration", &rwrl::WeaponSlot::turret_turn_deceleration)
        .def_readonly("aim_tolerance", &rwrl::WeaponSlot::aim_tolerance)
        .def_readonly("aim_offset_x", &rwrl::WeaponSlot::aim_offset_x)
        .def_readonly("aim_offset_y", &rwrl::WeaponSlot::aim_offset_y)
        .def_readonly("muzzle_offset", &rwrl::WeaponSlot::muzzle_offset)
        .def_readonly("muzzle_side", &rwrl::WeaponSlot::muzzle_side)
        .def_readonly("muzzle_side_odd", &rwrl::WeaponSlot::muzzle_side_odd)
        .def_readonly("odd_shot", &rwrl::WeaponSlot::odd_shot)
        .def_readonly("mount_forward", &rwrl::WeaponSlot::mount_forward)
        .def_readonly("mount_side", &rwrl::WeaponSlot::mount_side)
        .def_readonly("projectile_uses_muzzle", &rwrl::WeaponSlot::projectile_uses_muzzle)
        .def_readonly("height_guided", &rwrl::WeaponSlot::height_guided)
        .def_readonly("nuke_weapon", &rwrl::WeaponSlot::nuke_weapon)
        .def_readonly("launch_height", &rwrl::WeaponSlot::launch_height)
        .def_readonly("cruise_height", &rwrl::WeaponSlot::cruise_height)
        .def_readonly("vertical_speed", &rwrl::WeaponSlot::vertical_speed)
        .def_readonly("fixed_turret", &rwrl::WeaponSlot::fixed_turret)
        .def_readonly("turret_rotate_with_body",
                      &rwrl::WeaponSlot::turret_rotate_with_body);

    py::class_<rwrl::UnitOrder>(m, "UnitOrder")
        .def(py::init<>())
        .def_readwrite("formation_group_id", &rwrl::UnitOrder::formation_group_id)
        .def_readwrite("type", &rwrl::UnitOrder::type)
        .def_readwrite("x", &rwrl::UnitOrder::x)
        .def_readwrite("y", &rwrl::UnitOrder::y)
        .def_readwrite("target_uid", &rwrl::UnitOrder::target_uid)
        .def_readwrite("target_team_id", &rwrl::UnitOrder::target_team_id)
        .def_readwrite("unit_type", &rwrl::UnitOrder::unit_type)
        .def_readwrite("build_tech_level", &rwrl::UnitOrder::build_tech_level)
        .def_readwrite("action_id", &rwrl::UnitOrder::action_id)
        .def_readwrite("relaxed_formation", &rwrl::UnitOrder::relaxed_formation)
        .def_readwrite("formation_controller_relaxed_formation",
                      &rwrl::UnitOrder::formation_controller_relaxed_formation)
        .def_readwrite("max_time_frames", &rwrl::UnitOrder::max_time_frames)
        .def_readwrite("service_chase_range_extra", &rwrl::UnitOrder::service_chase_range_extra)
        .def_readwrite("automatic", &rwrl::UnitOrder::automatic)
        .def_readwrite("prefer_clearance", &rwrl::UnitOrder::prefer_clearance);

    py::class_<rwrl::Unit>(m, "Unit")
        .def("restore_runtime", &restore_unit_runtime)
        .def_property_readonly("main_target_uid", &rwrl::Unit::main_target_uid)
        .def_property_readonly("main_target_team_id", &rwrl::Unit::main_target_team_id)
        .def_readonly("uid", &rwrl::Unit::uid)
        .def_readonly("unit_type", &rwrl::Unit::unit_type)
        .def_readonly("team_id", &rwrl::Unit::team_id)
        .def_readonly("custom_unit", &rwrl::Unit::custom_unit)
        .def_readonly("custom_flags", &rwrl::Unit::custom_flags)
        .def_readwrite("x", &rwrl::Unit::x)
        .def_readwrite("y", &rwrl::Unit::y)
        .def_readonly("legs", &rwrl::Unit::legs)
        .def_readwrite("vel_x", &rwrl::Unit::vel_x)
        .def_readwrite("vel_y", &rwrl::Unit::vel_y)
        .def_readonly("collision_enabled", &rwrl::Unit::collision_enabled)
        .def_readonly("collision_impulse_x", &rwrl::Unit::collision_impulse_x)
        .def_readonly("collision_impulse_y", &rwrl::Unit::collision_impulse_y)
        .def_readonly("collision_moved", &rwrl::Unit::collision_moved)
        .def_readonly("collision_repeat", &rwrl::Unit::collision_repeat)
        .def_readonly("collision_next_tick", &rwrl::Unit::collision_next_tick)
        .def_readonly("collision_candidates", &rwrl::Unit::collision_candidates)
        .def_readonly("wall_bump_frames", &rwrl::Unit::wall_bump_frames)
        .def_readonly("wall_clear_streak", &rwrl::Unit::wall_clear_streak)
        .def_readonly("waypoint_stall_frames",
                      &rwrl::Unit::waypoint_stall_frames)
        .def_readonly("last_waypoint_stall_frames",
                      &rwrl::Unit::last_waypoint_stall_frames)
        .def_readonly("order_point_dwell_frames",
                      &rwrl::Unit::order_point_dwell_frames)
        .def_readonly("path_incomplete", &rwrl::Unit::path_incomplete)
        .def_readwrite("movement_prediction_enabled",
                       &rwrl::Unit::movement_prediction_enabled)
        .def_readonly("formation_leader_uid",
                      &rwrl::Unit::formation_leader_uid)
        .def_readonly("formation_controller_id",
                      &rwrl::Unit::formation_controller_id)
        .def_readonly("formation_is_leader",
                      &rwrl::Unit::formation_is_leader)
        .def_readonly("formation_preferred",
                      &rwrl::Unit::formation_preferred)
        .def_readonly("formation_group_size",
                      &rwrl::Unit::formation_group_size)
        .def_readonly("formation_offset_x",
                      &rwrl::Unit::formation_offset_x)
        .def_readonly("formation_offset_y",
                      &rwrl::Unit::formation_offset_y)
        .def_readonly("formation_heading",
                      &rwrl::Unit::formation_heading)
        .def_readonly("formation_distance_sq",
                      &rwrl::Unit::formation_distance_sq)
        .def_readonly("formation_path_fallback_frames",
                      &rwrl::Unit::formation_path_fallback_frames)
        .def_readonly("formation_path_failure_frames",
                      &rwrl::Unit::formation_path_failure_frames)
        .def_readonly("formation_idle_leader_frames",
                      &rwrl::Unit::formation_idle_leader_frames)
        .def_readonly("formation_projection_allowed",
                      &rwrl::Unit::formation_projection_allowed)
        .def_readonly("formation_projection_disabled_frames",
                      &rwrl::Unit::formation_projection_disabled_frames)
        .def_readonly("using_path", &rwrl::Unit::using_path)
        .def_readonly("has_move_target", &rwrl::Unit::has_move_target)
        .def_readonly("arrived", &rwrl::Unit::arrived)
        .def_readwrite("hp", &rwrl::Unit::hp)
        .def_readwrite("max_hp", &rwrl::Unit::max_hp)
        .def_readwrite("shield", &rwrl::Unit::shield)
        .def_readwrite("max_shield", &rwrl::Unit::max_shield)
        .def_readwrite("shield_regen_rate", &rwrl::Unit::shield_regen_rate)
        .def_property("shield_regen_per_frame",
            [](const rwrl::Unit& unit) {
                return unit.shield_regen_rate ==
                        unit.shield_regen_per_frame * rwrl::EngineConstants::FRAME_RATE
                    ? unit.shield_regen_per_frame
                    : unit.shield_regen_rate / rwrl::EngineConstants::FRAME_RATE;
            },
            [](rwrl::Unit& unit, rwrl::f32 value) {
                unit.shield_regen_per_frame = value;
                unit.shield_regen_rate = value * rwrl::EngineConstants::FRAME_RATE;
            })
        .def_readwrite("shield_disabled", &rwrl::Unit::shield_disabled)
        .def_readwrite("build_progress", &rwrl::Unit::build_progress)
        .def_readwrite("build_progress_secondary",
                       &rwrl::Unit::build_progress_secondary)
        .def_readonly("is_building", &rwrl::Unit::is_building)
        .def_readonly("collision_footprint",
                      &rwrl::Unit::collision_footprint)
        .def_readonly("placement_footprint",
                      &rwrl::Unit::placement_footprint)
        .def_readonly("is_alive", &rwrl::Unit::is_alive)
        .def_readonly("is_dead", &rwrl::Unit::is_dead)
        .def_readonly("cleanup_done", &rwrl::Unit::cleanup_done)
        .def_readonly("native_unit_list_member", &rwrl::Unit::native_unit_list_member)
        .def_readonly("object_creation_order", &rwrl::Unit::object_creation_order)
        .def_readonly("death_persistent", &rwrl::Unit::death_persistent)
        .def_readonly("death_clock_ms", &rwrl::Unit::death_clock_ms)
        .def_readonly("list_removal_tick", &rwrl::Unit::list_removal_tick)
        .def_readonly("crash_fall_velocity", &rwrl::Unit::crash_fall_velocity)
        .def_readonly("crash_impacted", &rwrl::Unit::crash_impacted)
        .def_readonly("crash_over_water", &rwrl::Unit::crash_over_water)
        .def_readonly("income_rate", &rwrl::Unit::income_rate)
        .def_readonly("income_timer", &rwrl::Unit::income_timer)
        .def_readonly("spawn_delay_frames", &rwrl::Unit::spawn_delay_frames)
        .def_readonly("custom_action_movement_blocked",
                      &rwrl::Unit::custom_action_movement_blocked)
        .def_readonly("action_cooldown_expiry_ticks",
                      &rwrl::Unit::action_cooldown_expiry_ticks)
        .def_readonly("damage", &rwrl::Unit::damage)
        .def_readonly("shoot_delay_multiplier",
                      &rwrl::Unit::shoot_delay_multiplier)
        .def_readonly("shoot_damage_multiplier",
                      &rwrl::Unit::shoot_damage_multiplier)
        .def_readonly("weapon_range", &rwrl::Unit::weapon_range)
        .def_readonly("cooldown_ticks", &rwrl::Unit::cooldown_ticks)
        .def_readonly("cooldown_max", &rwrl::Unit::cooldown_max)
        .def_readonly("move_speed", &rwrl::Unit::move_speed)
        .def_readonly("terrain_speed_multiplier", &rwrl::Unit::terrain_speed_multiplier)
        .def_readonly("auto_trigger_cooldown_frames", &rwrl::Unit::auto_trigger_cooldown_frames)
        .def_property_readonly("move_speed_per_frame", &rwrl::Unit::move_speed_per_frame)
        .def_readonly("turn_speed", &rwrl::Unit::turn_speed)
        .def_readonly("turn_acceleration", &rwrl::Unit::turn_acceleration)
        .def_readonly("move_acceleration", &rwrl::Unit::move_acceleration)
        .def_readonly("move_deceleration", &rwrl::Unit::move_deceleration)
        .def_readonly("strafes", &rwrl::Unit::strafes)
        .def_readonly("move_ignores_body", &rwrl::Unit::move_ignores_body)
        .def_readonly("joins_group_formations", &rwrl::Unit::joins_group_formations)
        .def_readonly("reverse_speed_multiplier", &rwrl::Unit::reverse_speed_multiplier)
        .def_readwrite("move_speed_factor", &rwrl::Unit::move_speed_factor)
        .def_readonly("reversing", &rwrl::Unit::reversing)
        .def_readonly("turn_velocity", &rwrl::Unit::turn_velocity)
        .def_readonly("movement_activation_delay_frames",
                      &rwrl::Unit::movement_activation_delay_frames)
        .def_readonly("path_request_pending",
                      &rwrl::Unit::path_request_pending)
        .def_readonly("path_request_target_size",
                      &rwrl::Unit::path_request_target_size)
        .def_readonly("path_request_solver_target_size",
                      &rwrl::Unit::path_request_solver_target_size)
        .def_readonly("path_request_preserves_active_path",
                      &rwrl::Unit::path_request_preserves_active_path)
        .def_readonly("path_request_delay_frames",
                      &rwrl::Unit::path_request_delay_frames)
        .def_readonly("last_request_x", &rwrl::Unit::last_request_x)
        .def_readonly("last_request_y", &rwrl::Unit::last_request_y)
        .def_readonly("repath_cooldown_frames",
                      &rwrl::Unit::repath_cooldown_frames)
        .def_readonly("has_deferred_navigation_target",
                      &rwrl::Unit::has_deferred_navigation_target)
        .def_readonly("clear_exposed_move_next_frame",
                      &rwrl::Unit::clear_exposed_move_next_frame)
        .def_readonly("deferred_navigation_target_x",
                      &rwrl::Unit::deferred_navigation_target_x)
        .def_readonly("deferred_navigation_target_y",
                      &rwrl::Unit::deferred_navigation_target_y)
        .def_readonly("path_target_override_active",
                      &rwrl::Unit::path_target_override_active)
        .def_readonly("path_target_override_x",
                      &rwrl::Unit::path_target_override_x)
        .def_readonly("path_target_override_y",
                      &rwrl::Unit::path_target_override_y)
        .def_readonly("single_node_path_counter",
                       &rwrl::Unit::single_node_path_counter)
        .def_readonly("move_type", &rwrl::Unit::move_type)
        .def_readonly("weapon_type", &rwrl::Unit::weapon_type)
        .def_readonly("cost", &rwrl::Unit::cost)
        .def_readwrite("behavior", &rwrl::Unit::behavior)
        .def_readwrite("rotation", &rwrl::Unit::rotation)
        .def_readonly("radius", &rwrl::Unit::radius)
        .def_readonly("mass", &rwrl::Unit::mass)
        .def_readonly("armour", &rwrl::Unit::armour)
        .def_readonly("armour_min_damage", &rwrl::Unit::armour_min_damage)
        .def_readonly("splash_radius", &rwrl::Unit::splash_radius)
        .def_readonly("splash_damage", &rwrl::Unit::splash_damage)
        .def_readonly("projectile_speed", &rwrl::Unit::projectile_speed)
        .def_readonly("projectile_lifetime", &rwrl::Unit::projectile_lifetime)
        .def_readonly("projectile_tracking", &rwrl::Unit::projectile_tracking)
        .def_readonly("projectile_hitscan", &rwrl::Unit::projectile_hitscan)
        .def_readonly("can_move", &rwrl::Unit::can_move)
        .def_readonly("can_attack", &rwrl::Unit::can_attack)
        .def_readonly("airborne", &rwrl::Unit::airborne)
        .def_property_readonly("submerged", &rwrl::Unit::is_submerged)
        .def_readonly("submerge_requested", &rwrl::Unit::submerge_requested)
        .def_readwrite("height", &rwrl::Unit::height)
        .def_readonly("vertical_speed", &rwrl::Unit::vertical_speed)
        .def_readonly("flight_phase", &rwrl::Unit::flight_phase)
        .def_readonly("custom_idle_frames", &rwrl::Unit::custom_idle_frames)
        .def_readonly("wing_fold_progress", &rwrl::Unit::wing_fold_progress)
        .def_readonly("total_shield_absorbed", &rwrl::Unit::total_shield_absorbed)
        .def_readonly("total_hp_lost", &rwrl::Unit::total_hp_lost)
        .def_readonly("damage_smoothing", &rwrl::Unit::damage_smoothing)
        .def_readonly("damage_smoothing_velocity",
                      &rwrl::Unit::damage_smoothing_velocity)
        .def_readonly("ammo", &rwrl::Unit::ammo)
        .def_readonly("ammo_max", &rwrl::Unit::ammo_max)
        .def_readonly("targets_air", &rwrl::Unit::targets_air)
        .def_readonly("targets_ground", &rwrl::Unit::targets_ground)
        .def_readonly("ground_attack_disabled_while_flying",
                      &rwrl::Unit::ground_attack_disabled_while_flying)
        .def_readonly("targets_underwater", &rwrl::Unit::targets_underwater)
        .def_readonly("targets_not_touching_water", &rwrl::Unit::targets_not_touching_water)
        .def_readonly("shield_mult", &rwrl::Unit::shield_mult)
        .def_readonly("shield_absorb", &rwrl::Unit::shield_absorb)
        .def_readonly("hp_mult", &rwrl::Unit::hp_mult)
        .def_readonly("building_damage_mult", &rwrl::Unit::building_damage_mult)
        .def_readonly("air_damage_mult", &rwrl::Unit::air_damage_mult)
        .def_readonly("experimental_damage_mult", &rwrl::Unit::experimental_damage_mult)
        .def_readonly("is_experimental", &rwrl::Unit::is_experimental)
        .def_readonly("weapon_slots", &rwrl::Unit::weapon_slots)
        .def_readonly("deterministic_random_salt",
                      &rwrl::Unit::deterministic_random_salt)
        .def_readonly("weapon_shot_counter", &rwrl::Unit::weapon_shot_counter)
        .def_readonly("active_beam_projectile_uid",
                      &rwrl::Unit::active_beam_projectile_uid)
        .def_readonly("sight_range", &rwrl::Unit::sight_range)
        .def_readonly("nano_range", &rwrl::Unit::nano_range)
        .def_readonly("nano_turret_index", &rwrl::Unit::nano_turret_index)
        .def_readonly("nano_build_speed", &rwrl::Unit::nano_build_speed)
        .def_readonly("self_regen_rate", &rwrl::Unit::self_regen_rate)
        .def_readonly("self_regen_per_frame", &rwrl::Unit::self_regen_per_frame)
        .def_readonly("target_height", &rwrl::Unit::target_height)
        .def_readonly("nano_factory_speed", &rwrl::Unit::nano_factory_speed)
        .def_readonly("target_height_drift", &rwrl::Unit::target_height_drift)
        .def_readonly("height_change_rate", &rwrl::Unit::height_change_rate)
        .def_readonly("falling_acceleration", &rwrl::Unit::falling_acceleration)
        .def_readonly("land_on_ground", &rwrl::Unit::land_on_ground)
        .def_readonly("land_on_ground_only_idle", &rwrl::Unit::land_on_ground_only_idle)
        .def_readonly("base_nano_range", &rwrl::Unit::base_nano_range)
        .def_readonly("is_builder", &rwrl::Unit::is_builder)
        .def_readonly("can_repair", &rwrl::Unit::can_repair)
        .def_readonly("can_repair_any", &rwrl::Unit::can_repair_any)
        .def_readonly("auto_repair", &rwrl::Unit::auto_repair)
        .def_readonly("nano_repair_speed", &rwrl::Unit::nano_repair_speed)
        .def_readonly("nano_reclaim_speed", &rwrl::Unit::nano_reclaim_speed)
        .def_readonly("resource_reclaim_multiplier", &rwrl::Unit::resource_reclaim_multiplier)
        .def_readonly("nano_unbuild_speed", &rwrl::Unit::nano_unbuild_speed)
        .def_readonly("auto_repair_timer_frames", &rwrl::Unit::auto_repair_timer_frames)
        .def_readonly("can_reclaim", &rwrl::Unit::can_reclaim)
        .def_readonly("can_repair_buildings", &rwrl::Unit::can_repair_buildings)
        .def_readonly("can_reclaim_resources", &rwrl::Unit::can_reclaim_resources)
        .def_readonly("can_be_repaired", &rwrl::Unit::can_be_repaired)
        .def_readwrite("energy", &rwrl::Unit::energy)
        .def_readonly("max_energy", &rwrl::Unit::max_energy)
        .def_readonly("bomber_attack_movement", &rwrl::Unit::bomber_attack_movement)
        .def_readonly("disable_passive_targeting", &rwrl::Unit::disable_passive_targeting)
        .def_readonly("turret_multi_targeting", &rwrl::Unit::turret_multi_targeting)
        .def_readonly("bomber_attack_heading", &rwrl::Unit::bomber_attack_heading)
        .def_readonly("bomber_retreating", &rwrl::Unit::bomber_retreating)
        .def_readonly("bomber_target_search_done", &rwrl::Unit::bomber_target_search_done)
        .def_readonly("energy_per_shot", &rwrl::Unit::energy_per_shot)
        .def_readonly("energy_regen_rate", &rwrl::Unit::energy_regen_rate)
        .def_readonly("energy_regen_per_frame", &rwrl::Unit::energy_regen_per_frame)
        .def_readonly("energy_recharge_regen_per_frame",
                      &rwrl::Unit::energy_recharge_regen_per_frame)
        .def_readonly("energy_needs_full_recharge", &rwrl::Unit::energy_needs_full_recharge)
        .def_readonly("energy_disabled", &rwrl::Unit::energy_disabled)
        .def_readonly("die_on_zero_energy", &rwrl::Unit::die_on_zero_energy)
        .def_readonly("is_nuke", &rwrl::Unit::is_nuke)
        .def_readonly("is_antinuke", &rwrl::Unit::is_antinuke)
        .def_readonly("nuke_range", &rwrl::Unit::nuke_range)
        .def_readonly("tech_level", &rwrl::Unit::tech_level)
        .def_readonly("upgrade_to", &rwrl::Unit::upgrade_to)
        .def_readonly("cargo_capacity", &rwrl::Unit::cargo_capacity)
        .def_readonly("hovercraft_unload_style",
                      &rwrl::Unit::hovercraft_unload_style)
        .def_readonly("cargo_weight", &rwrl::Unit::cargo_weight)
        .def_readonly("transportable", &rwrl::Unit::transportable)
        .def_readonly("cargo_units", &rwrl::Unit::cargo_units)
        .def_readonly("unloading_all", &rwrl::Unit::unloading_all)
        .def_readonly("activate_order_next_frame", &rwrl::Unit::activate_order_next_frame)
        .def_readonly("unload_timer_frames", &rwrl::Unit::unload_timer_frames)
        .def_readonly("lifetime_frames", &rwrl::Unit::lifetime_frames)
        .def_readonly("border_current_radius", &rwrl::Unit::border_current_radius)
        .def_readonly("border_target_radius", &rwrl::Unit::border_target_radius)
        .def_readonly("border_target_x", &rwrl::Unit::border_target_x)
        .def_readonly("border_target_y", &rwrl::Unit::border_target_y)
        .def_readonly("border_damage_timer", &rwrl::Unit::border_damage_timer)
        .def_readonly("fire_intensity", &rwrl::Unit::fire_intensity)
        .def_readonly("has_patrol", &rwrl::Unit::has_patrol)
        .def_readonly("patrol_logical_target_active",
                      &rwrl::Unit::patrol_logical_target_active)
        .def_readonly("patrol_logical_target_x",
                      &rwrl::Unit::patrol_logical_target_x)
        .def_readonly("patrol_logical_target_y",
                      &rwrl::Unit::patrol_logical_target_y)
        .def_readonly("patrol_points", &rwrl::Unit::patrol_points)
        .def_readonly("has_guard_pos", &rwrl::Unit::has_guard_pos)
        .def_readonly("guard_x", &rwrl::Unit::guard_x)
        .def_readonly("guard_y", &rwrl::Unit::guard_y)
        .def_readonly("guard_target_uid", &rwrl::Unit::guard_target_uid)
        .def_readonly("guard_target_team_id", &rwrl::Unit::guard_target_team_id)
        .def_readonly("follow_target_uid", &rwrl::Unit::follow_target_uid)
        .def_readonly("follow_target_team_id", &rwrl::Unit::follow_target_team_id)
        .def_readonly("was_attacked", &rwrl::Unit::was_attacked)
        .def_readonly("last_attacker_uid", &rwrl::Unit::last_attacker_uid)
        .def_readonly("target_uid", &rwrl::Unit::target_uid)
        .def_readonly("target_team_id", &rwrl::Unit::target_team_id)
        .def_readwrite("target_refresh_timer_frames",
                       &rwrl::Unit::target_refresh_timer_frames)
        .def_readwrite("target_scan_timer_frames",
                       &rwrl::Unit::target_scan_timer_frames)
        .def_readonly("service_warmup_progress",
                      &rwrl::Unit::service_warmup_progress)
        .def_readonly("service_aim_target_uid",
                      &rwrl::Unit::service_aim_target_uid)
        .def_readonly("service_aim_ready",
                      &rwrl::Unit::service_aim_ready)
        .def_readonly("parent_uid", &rwrl::Unit::parent_uid)
        .def_readwrite("collision_ignore_uid", &rwrl::Unit::collision_ignore_uid)
        .def_readwrite("collision_ignore_frames",
                      &rwrl::Unit::collision_ignore_frames)
        .def_readonly("has_move_target", &rwrl::Unit::has_move_target)
        .def_readonly("target_x", &rwrl::Unit::target_x)
        .def_readonly("target_y", &rwrl::Unit::target_y)
        .def_readonly("attack_order_target_x",
                      &rwrl::Unit::attack_order_target_x)
        .def_readonly("attack_order_target_y",
                      &rwrl::Unit::attack_order_target_y)
        .def_readonly("service_order_target_x", &rwrl::Unit::service_order_target_x)
        .def_readonly("service_order_target_y", &rwrl::Unit::service_order_target_y)
        .def_readonly("transport_order_target_x",
                      &rwrl::Unit::transport_order_target_x)
        .def_readonly("formation_tick", &rwrl::Unit::formation_tick)
        .def_readonly("active_order_elapsed_frames",
                      &rwrl::Unit::active_order_elapsed_frames)
        .def_readonly("transport_order_target_y",
                      &rwrl::Unit::transport_order_target_y)
        .def_readonly("attack_move_combat_target_valid",
                      &rwrl::Unit::attack_move_combat_target_valid)
        .def_readonly("attack_move_combat_target_x",
                      &rwrl::Unit::attack_move_combat_target_x)
        .def_readonly("attack_move_combat_target_y",
                      &rwrl::Unit::attack_move_combat_target_y)
        .def_readonly("guard_movement_target_valid",
                      &rwrl::Unit::guard_movement_target_valid)
        .def_readonly("guard_combat_chase_outer",
                      &rwrl::Unit::guard_combat_chase_outer)
        .def_readonly("guard_movement_chases_main_target",
                      &rwrl::Unit::guard_movement_chases_main_target)
        .def_readonly("guard_movement_target_x",
                      &rwrl::Unit::guard_movement_target_x)
        .def_readonly("guard_movement_target_y",
                      &rwrl::Unit::guard_movement_target_y)
        .def_readonly("arrived", &rwrl::Unit::arrived)
        .def_readonly("using_path", &rwrl::Unit::using_path)
        .def_readonly("current_waypoint", &rwrl::Unit::current_waypoint)
        .def_readonly("order_queue", &rwrl::Unit::order_queue)
        .def_readonly("waypoints", &rwrl::Unit::waypoints)
        .def("is_complete", &rwrl::Unit::is_complete)
        .def("is_mobile", &rwrl::Unit::is_mobile)
        .def("hp_ratio", &rwrl::Unit::hp_ratio)
        .def("distance_to", py::overload_cast<const rwrl::Unit&>(&rwrl::Unit::distance_to, py::const_))
        .def("distance_to", py::overload_cast<rwrl::f32, rwrl::f32>(&rwrl::Unit::distance_to, py::const_));

    py::class_<rwrl::UnitRegistry>(m, "UnitRegistry")
        .def("get", [](const rwrl::UnitRegistry& r, const std::string& name) -> py::object {
            auto* d = r.get(name);
            return d ? py::cast(d) : py::none();
        }, py::return_value_policy::reference)
        .def("all_names", &rwrl::UnitRegistry::all_names)
        .def("register_unit", &rwrl::UnitRegistry::register_unit,
             py::arg("definition"), py::arg("custom") = false)
        .def("register_custom_unit_from",
             &rwrl::UnitRegistry::register_custom_unit_from,
             py::return_value_policy::reference_internal)
        .def("register_custom_action", &rwrl::UnitRegistry::register_custom_action)
        .def("get_custom_action", [](
                const rwrl::UnitRegistry& registry,
                const std::string& unit_name,
                const std::string& action_id) -> py::object {
            const auto* action = registry.get_custom_action(unit_name, action_id);
            return action ? py::cast(action, py::return_value_policy::reference)
                          : py::none();
        })
        .def("is_custom", &rwrl::UnitRegistry::is_custom)
        .def("is_building", &rwrl::UnitRegistry::is_building)
        .def("get_cost", &rwrl::UnitRegistry::get_cost)
        .def("get_build_cost", &rwrl::UnitRegistry::get_build_cost)
        .def("get_income", &rwrl::UnitRegistry::get_income)
        .def("get_build_speed", &rwrl::UnitRegistry::get_build_speed)
        .def("get_can_build", &rwrl::UnitRegistry::get_can_build,
             py::return_value_policy::reference_internal)
        .def("has_upgrade", &rwrl::UnitRegistry::has_upgrade)
        .def("get_upgrade_target", &rwrl::UnitRegistry::get_upgrade_target);

    py::class_<rwrl::WorldState>(m, "WorldState")
        .def_readonly("team_id", &rwrl::WorldState::team_id)
        .def_readonly("tick", &rwrl::WorldState::tick)
        .def_readonly("credits", &rwrl::WorldState::credits)
        .def_readonly("total_income_rate", &rwrl::WorldState::total_income_rate)
        .def_readonly("income_per_sec", &rwrl::WorldState::income_per_sec)
        .def_readonly("total_income", &rwrl::WorldState::total_income)
        .def_readonly("total_spent", &rwrl::WorldState::total_spent)
        .def_readonly("active_units", &rwrl::WorldState::active_units)
        .def_readonly("total_units", &rwrl::WorldState::total_units)
        .def_readonly("buildings", &rwrl::WorldState::buildings)
        .def_readonly("pending_builds", &rwrl::WorldState::pending_builds)
        .def_readonly("projectiles", &rwrl::WorldState::projectiles)
        .def_readonly("kills", &rwrl::WorldState::kills)
        .def_readonly("losses", &rwrl::WorldState::losses)
        .def_readonly("units_built", &rwrl::WorldState::units_built)
        .def_readonly("buildings_completed", &rwrl::WorldState::buildings_completed)
        .def_readonly("unit_cap", &rwrl::WorldState::unit_cap)
        .def_readonly("factory_queue_size", &rwrl::WorldState::factory_queue_size);

    py::class_<rwrl::CustomActionContext>(m, "CustomActionContext")
        .def(py::init<>())
        .def_readwrite("has_position", &rwrl::CustomActionContext::has_position)
        .def_readwrite("x", &rwrl::CustomActionContext::x)
        .def_readwrite("y", &rwrl::CustomActionContext::y)
        .def_readwrite("target_uid", &rwrl::CustomActionContext::target_uid)
        .def_readwrite("target_team_id", &rwrl::CustomActionContext::target_team_id);

    py::class_<rwrl::FactoryQueueItemState>(m, "FactoryQueueItemState")
        .def(py::init<>())
        .def_readwrite("progress", &rwrl::FactoryQueueItemState::progress)
        .def_readwrite("resource_progress", &rwrl::FactoryQueueItemState::resource_progress)
        .def_readwrite("ammo_cost", &rwrl::FactoryQueueItemState::ammo_cost)
        .def_readwrite("high_priority", &rwrl::FactoryQueueItemState::high_priority);

    py::class_<rwrl::FactoryQueue>(m, "FactoryQueue")
        .def(py::init<>())
        .def_readwrite("upfront_costs", &rwrl::FactoryQueue::upfront_costs)
        .def_readwrite("action_contexts", &rwrl::FactoryQueue::action_contexts)
        .def_readwrite("item_states", &rwrl::FactoryQueue::item_states)
        .def_readwrite("factory_uid", &rwrl::FactoryQueue::factory_uid)
        .def_readwrite("queue", &rwrl::FactoryQueue::queue)
        .def_readwrite("progress", &rwrl::FactoryQueue::progress)
        .def_readwrite("resource_progress", &rwrl::FactoryQueue::resource_progress)
        .def_readwrite("build_speed", &rwrl::FactoryQueue::build_speed)
        .def_readwrite("rally_x", &rwrl::FactoryQueue::rally_x)
        .def_readwrite("rally_y", &rwrl::FactoryQueue::rally_y)
        .def_readwrite("has_rally", &rwrl::FactoryQueue::has_rally);

    py::class_<rwrl::Projectile>(m, "Projectile")
        .def(py::init<>())
        .def_readwrite("team_id", &rwrl::Projectile::team_id)
        .def_readwrite("target_team_id", &rwrl::Projectile::target_team_id)
        .def_readwrite("target_offset_x", &rwrl::Projectile::target_offset_x)
        .def_readwrite("target_offset_y", &rwrl::Projectile::target_offset_y)
        .def_readwrite("target_radius", &rwrl::Projectile::target_radius)
        .def_readwrite("target_shield", &rwrl::Projectile::target_shield)
        .def_readwrite("has_target_snapshot", &rwrl::Projectile::has_target_snapshot)
        .def_readwrite("target_is_stock_building", &rwrl::Projectile::target_is_stock_building)
        .def_readwrite("active", &rwrl::Projectile::active)
        .def_readwrite("is_nuke", &rwrl::Projectile::is_nuke)
        .def_readwrite("should_reveal_fog", &rwrl::Projectile::should_reveal_fog)
        .def_readwrite("is_antinuke", &rwrl::Projectile::is_antinuke)
        .def_readwrite("tracking", &rwrl::Projectile::tracking)
        .def_readwrite("impact_at_target", &rwrl::Projectile::impact_at_target)
        .def_readwrite("target_projectile_uid", &rwrl::Projectile::target_projectile_uid)
        .def_readwrite("tags", &rwrl::Projectile::tags)
        .def_readwrite("intercept_hp", &rwrl::Projectile::intercept_hp)
        .def_readwrite("laser_defence_immune", &rwrl::Projectile::laser_defence_immune)
        .def_readwrite("retarget_on_death", &rwrl::Projectile::retarget_on_death)
        .def_readwrite("retarget_range", &rwrl::Projectile::retarget_range)
        .def_readwrite("retarget_lead", &rwrl::Projectile::retarget_lead)
        .def_readwrite("shield_mult", &rwrl::Projectile::shield_mult)
        .def_readwrite("shield_absorb", &rwrl::Projectile::shield_absorb)
        .def_readwrite("hp_mult", &rwrl::Projectile::hp_mult)
        .def_readwrite("building_damage_mult", &rwrl::Projectile::building_damage_mult)
        .def_readwrite("air_damage_mult", &rwrl::Projectile::air_damage_mult)
        .def_readwrite("experimental_damage_mult", &rwrl::Projectile::experimental_damage_mult)
        .def_readwrite("splash_same_layer", &rwrl::Projectile::splash_same_layer)
        .def_readwrite("splash_friendly_fire", &rwrl::Projectile::splash_friendly_fire)
        .def_readwrite("splash_hit_units", &rwrl::Projectile::splash_hit_units)
        .def_readwrite("uid", &rwrl::Projectile::uid)
        .def_readwrite("owner_uid", &rwrl::Projectile::owner_uid)
        .def_readwrite("x", &rwrl::Projectile::x)
        .def_readwrite("y", &rwrl::Projectile::y)
        .def_readwrite("heading", &rwrl::Projectile::heading)
        .def_readwrite("height", &rwrl::Projectile::height)
        .def_readwrite("unguided_speed_height", &rwrl::Projectile::unguided_speed_height)
        .def_readwrite("unguided_speed_x", &rwrl::Projectile::unguided_speed_x)
        .def_readwrite("unguided_speed_y", &rwrl::Projectile::unguided_speed_y)
        .def_readwrite("gravity", &rwrl::Projectile::gravity)
        .def_readwrite("true_gravity", &rwrl::Projectile::true_gravity)
        .def_readwrite("target_x", &rwrl::Projectile::target_x)
        .def_readwrite("target_y", &rwrl::Projectile::target_y)
        .def_readwrite("target_height", &rwrl::Projectile::target_height)
        .def_readwrite("target_uid", &rwrl::Projectile::target_uid)
        .def_readwrite("fixed_target", &rwrl::Projectile::fixed_target)
        .def_readwrite("instant_hit", &rwrl::Projectile::instant_hit)
        .def_readwrite("persist_after_hit", &rwrl::Projectile::persist_after_hit)
        .def_readwrite("move_with_parent", &rwrl::Projectile::move_with_parent)
        .def_readwrite("parent_turret_slot", &rwrl::Projectile::parent_turret_slot)
        .def_readwrite("parent_anchor_x", &rwrl::Projectile::parent_anchor_x)
        .def_readwrite("parent_anchor_y", &rwrl::Projectile::parent_anchor_y)
        .def_readwrite("parent_anchor_height", &rwrl::Projectile::parent_anchor_height)
        .def_readwrite("sweep_offset", &rwrl::Projectile::sweep_offset)
        .def_readwrite("sweep_offset_from_target_radius", &rwrl::Projectile::sweep_offset_from_target_radius)
        .def_readwrite("continuous_beam", &rwrl::Projectile::continuous_beam)
        .def_readwrite("flame_weapon", &rwrl::Projectile::flame_weapon)
        .def_readwrite("has_hit", &rwrl::Projectile::has_hit)
        .def_readwrite("beam_charge_frames", &rwrl::Projectile::beam_charge_frames)
        .def_readwrite("firing_slot", &rwrl::Projectile::firing_slot)
        .def_readwrite("damage", &rwrl::Projectile::damage)
        .def_readwrite("splash_damage", &rwrl::Projectile::splash_damage)
        .def_readwrite("splash_radius", &rwrl::Projectile::splash_radius)
        .def_readwrite("expanding_splash", &rwrl::Projectile::expanding_splash)
        .def_readwrite("splash_impact_started",
                      &rwrl::Projectile::splash_impact_started)
        .def_readwrite("splash_expand_remaining_frames",
                      &rwrl::Projectile::splash_expand_remaining_frames)
        .def_readwrite("splash_expand_duration_frames",
                      &rwrl::Projectile::splash_expand_duration_frames)
        .def_readwrite("speed", &rwrl::Projectile::speed)
        .def_readwrite("speed_per_frame", &rwrl::Projectile::speed_per_frame)
        .def_readwrite("target_speed", &rwrl::Projectile::target_speed)
        .def_readwrite("speed_acceleration", &rwrl::Projectile::speed_acceleration)
        .def_readwrite("height_guided", &rwrl::Projectile::height_guided)
        .def_readwrite("launch_height", &rwrl::Projectile::launch_height)
        .def_readwrite("cruise_height", &rwrl::Projectile::cruise_height)
        .def_readwrite("vertical_speed", &rwrl::Projectile::vertical_speed)
        .def_readwrite("laser_defence_excluded", &rwrl::Projectile::laser_defence_excluded)
        .def_readwrite("age_frames", &rwrl::Projectile::age_frames)
        .def_readwrite("random_phase", &rwrl::Projectile::random_phase)
        .def_readwrite("wobble_amplitude", &rwrl::Projectile::wobble_amplitude)
        .def_readwrite("wobble_frequency_frames", &rwrl::Projectile::wobble_frequency_frames)
        .def_readwrite("arc_descending", &rwrl::Projectile::arc_descending)
        .def_readwrite("activation_delay_frames",
                      &rwrl::Projectile::activation_delay_frames)
        .def_readwrite("lifetime", &rwrl::Projectile::lifetime);

    py::class_<rwrl::ReplayPrepath>(m, "ReplayPrepath")
        .def(py::init<>())
        .def_readwrite("unit_uid", &rwrl::ReplayPrepath::unit_uid)
        .def_readwrite("start_x", &rwrl::ReplayPrepath::start_x)
        .def_readwrite("start_y", &rwrl::ReplayPrepath::start_y)
        .def_readwrite("target_x", &rwrl::ReplayPrepath::target_x)
        .def_readwrite("target_y", &rwrl::ReplayPrepath::target_y)
        .def_readwrite("frame", &rwrl::ReplayPrepath::frame)
        .def_readwrite("move_type", &rwrl::ReplayPrepath::move_type)
        .def_readwrite("has_path", &rwrl::ReplayPrepath::has_path)
        .def_readwrite("tiles", &rwrl::ReplayPrepath::tiles);

    py::class_<rwrl::GameCommand>(m, "GameCommand")
        .def(py::init<>())
        .def_readwrite("action", &rwrl::GameCommand::action)
        .def_readwrite("unit_uid", &rwrl::GameCommand::unit_uid)
        .def_readwrite("target_x", &rwrl::GameCommand::target_x)
        .def_readwrite("target_y", &rwrl::GameCommand::target_y)
        .def_readwrite("target_uid", &rwrl::GameCommand::target_uid)
        .def_readwrite("unit_type", &rwrl::GameCommand::unit_type)
        .def_readwrite("build_tech_level", &rwrl::GameCommand::build_tech_level)
        .def_readwrite("action_id", &rwrl::GameCommand::action_id)
        .def_readwrite("direct_action", &rwrl::GameCommand::direct_action)
        .def_readwrite("has_action_position", &rwrl::GameCommand::has_action_position)
        .def_readwrite("native_selection", &rwrl::GameCommand::native_selection)
        .def_readwrite("control_player_team_id", &rwrl::GameCommand::control_player_team_id)
        .def_readwrite("shared_control_mask", &rwrl::GameCommand::shared_control_mask)
        .def_readwrite("queued", &rwrl::GameCommand::queued)
        .def_readwrite("action_alt", &rwrl::GameCommand::action_alt)
        .def_readwrite("clear_orders", &rwrl::GameCommand::clear_orders)
        .def_readwrite("clear_non_build_repair",
                       &rwrl::GameCommand::clear_non_build_repair)
        .def_readwrite("merge_duplicate", &rwrl::GameCommand::merge_duplicate)
        .def_readwrite("relaxed_formation", &rwrl::GameCommand::relaxed_formation)
        .def_readwrite("group_size", &rwrl::GameCommand::group_size)
        .def_readwrite("group_index", &rwrl::GameCommand::group_index)
        .def_readwrite("group_uids", &rwrl::GameCommand::group_uids)
        .def_readwrite("replay_prepaths_provided",
                       &rwrl::GameCommand::replay_prepaths_provided)
        .def_readwrite("defer_waypoint_activation",
                       &rwrl::GameCommand::defer_waypoint_activation)
        .def_readwrite("has_replay_prepath",
                       &rwrl::GameCommand::has_replay_prepath)
        .def_readwrite("replay_prepath_frame",
                       &rwrl::GameCommand::replay_prepath_frame)
        .def_readwrite("replay_prepath_target_x",
                       &rwrl::GameCommand::replay_prepath_target_x)
        .def_readwrite("replay_prepath_target_y",
                       &rwrl::GameCommand::replay_prepath_target_y)
        .def_readwrite("replay_prepath_move_type",
                       &rwrl::GameCommand::replay_prepath_move_type)
        .def_readwrite("replay_prepath_tiles",
                       &rwrl::GameCommand::replay_prepath_tiles)
        .def_readwrite("replay_prepaths",
                       &rwrl::GameCommand::replay_prepaths);

    py::enum_<rwrl::GameCommand::Action>(m, "GameCommandAction")
        .value("MOVE", rwrl::GameCommand::MOVE)
        .value("ATTACK", rwrl::GameCommand::ATTACK)
        .value("BUILD", rwrl::GameCommand::BUILD)
        .value("REPAIR", rwrl::GameCommand::REPAIR)
        .value("LOAD_INTO", rwrl::GameCommand::LOAD_INTO)
        .value("UNLOAD_AT", rwrl::GameCommand::UNLOAD_AT)
        .value("RECLAIM", rwrl::GameCommand::RECLAIM)
        .value("ATTACK_MOVE", rwrl::GameCommand::ATTACK_MOVE)
        .value("LOAD_UP", rwrl::GameCommand::LOAD_UP)
        .value("PATROL", rwrl::GameCommand::PATROL)
        .value("GUARD", rwrl::GameCommand::GUARD)
        .value("GUARD_AT", rwrl::GameCommand::GUARD_AT)
        .value("TOUCH_TARGET", rwrl::GameCommand::TOUCH_TARGET)
        .value("FOLLOW", rwrl::GameCommand::FOLLOW)
        .value("TRIGGER_ACTION", rwrl::GameCommand::TRIGGER_ACTION)
        .value("TRIGGER_ACTION_WHEN_IN_RANGE",
               rwrl::GameCommand::TRIGGER_ACTION_WHEN_IN_RANGE)
        .value("SET_PASSIVE_TARGET", rwrl::GameCommand::SET_PASSIVE_TARGET)
        .value("STOP", rwrl::GameCommand::STOP)
        .value("UPGRADE", rwrl::GameCommand::UPGRADE)
        .value("LAUNCH_NUKE", rwrl::GameCommand::LAUNCH_NUKE)
        .value("BUILD_NUKE", rwrl::GameCommand::BUILD_NUKE)
        .value("BUILD_ANTINUKE", rwrl::GameCommand::BUILD_ANTINUKE)
        .value("UNLOAD_ALL", rwrl::GameCommand::UNLOAD_ALL)
        .value("CANCEL_UNLOAD", rwrl::GameCommand::CANCEL_UNLOAD)
        .value("SUBMERGE", rwrl::GameCommand::SUBMERGE)
        .value("SURFACE", rwrl::GameCommand::SURFACE)
        .value("CANCEL_QUEUE", rwrl::GameCommand::CANCEL_QUEUE)
        .value("SET_RALLY", rwrl::GameCommand::SET_RALLY);

    // World observations, save restoration and scenario administration.
    // Gameplay orders use Engine.execute_command() for team coordination.
    py::class_<rwrl::World>(m, "World")
        // ── observation (safe for RL) ──
        .def("map_width", &rwrl::World::map_width)
        .def("map_height", &rwrl::World::map_height)
        .def("tile_size", &rwrl::World::tile_size)
        .def("terrain", &rwrl::World::terrain)
        .def("is_resource_pool", &rwrl::World::is_resource_pool)
        .def("blocks_buildings", &rwrl::World::blocks_buildings)
        .def("path_cost", &rwrl::World::path_cost)
        .def("get_state", &rwrl::World::get_state)
        .def("unit_ids", [](const rwrl::World& world) {
            rwrl::Vec<rwrl::u32> ids;
            ids.reserve(world.units().size());
            for (const auto& [uid, unit] : world.units()) {
                if (!unit.pending_global_commit) ids.push_back(uid);
            }
            return ids;
        })
        .def("check_victory", &rwrl::World::check_victory)
        .def("is_cc_alive", &rwrl::World::is_cc_alive)
        .def("credits", &rwrl::World::credits)
        .def("is_ai_player", &rwrl::World::is_ai_player)
        .def("ai_difficulty", &rwrl::World::ai_difficulty)
        .def("ai_income_multiplier", &rwrl::World::ai_income_multiplier)
        .def("team_id", &rwrl::World::team_id)
        .def("alliance_id", &rwrl::World::alliance_id)
        .def("tick", &rwrl::World::tick)
        .def("simulation_clock_ms", &rwrl::World::simulation_clock_ms)
        .def("unit_cap", &rwrl::World::unit_cap)
        .def("count_active_units", &rwrl::World::count_active_units)
        .def("is_paused", &rwrl::World::is_paused)
        .def("units", [](rwrl::World& w) {
            py::dict d;
            for (auto& [uid, u] : w.units()) {
                if (u.pending_global_commit) continue;
                d[py::cast(uid)] = py::cast(
                    &u, py::return_value_policy::reference);
            }
            return d;
        })
        .def("projectiles", [](rwrl::World& w) {
            py::dict d;
            for (auto& [uid, p] : w.projectiles()) {
                d[py::cast(uid)] = py::cast(&p, py::return_value_policy::reference);
            }
            return d;
        })
        .def("cc_uid", &rwrl::World::cc_uid)
        .def("fog_mode", &rwrl::World::fog_mode)
        .def("can_see", &rwrl::World::can_see)
        .def("is_explored", &rwrl::World::is_explored)
        .def("get_unit", py::overload_cast<rwrl::u32>(&rwrl::World::get_unit),
             py::return_value_policy::reference)
        .def("restore_factory_queue", &rwrl::World::restore_factory_queue)
        .def("custom_action_cooldown_remaining",
             &rwrl::World::custom_action_cooldown_remaining)
        .def("can_repair_target", &rwrl::World::can_repair_target)
        .def("can_reclaim_target", &rwrl::World::can_reclaim_target)
        .def("nukes_enabled", &rwrl::World::nukes_enabled)

        // Scenario setup and administrator commands.
        .def("spawn_unit", &rwrl::World::spawn_unit,
             py::arg("name"), py::arg("x"), py::arg("y"), py::arg("start_built") = false,
             py::return_value_policy::reference)
        .def("start_build", &rwrl::World::start_build,
             py::return_value_policy::reference)
        .def("move_unit_to",
             py::overload_cast<rwrl::Unit&, rwrl::f32, rwrl::f32>(
                 &rwrl::World::move_unit_to))
        .def("spend_credits", &rwrl::World::spend_credits)
        .def("add_credits", &rwrl::World::add_credits)
        .def("surrender", &rwrl::World::surrender)
        .def("set_fog_enabled", &rwrl::World::set_fog_enabled)
        .def("set_fog_mode", &rwrl::World::set_fog_mode)
        .def("set_nukes_enabled", &rwrl::World::set_nukes_enabled)
        .def("set_ai_player", &rwrl::World::set_ai_player,
             py::arg("value"), py::arg("difficulty") = 0)
        .def("set_unit_cap", &rwrl::World::set_unit_cap)
        .def("set_paused", &rwrl::World::set_paused)
        .def("set_cc_uid", &rwrl::World::set_cc_uid);

    py::class_<rwrl::EngineState>(m, "EngineState")
        .def_readonly("global_tick", &rwrl::EngineState::global_tick)
        .def_readonly("game_time", &rwrl::EngineState::game_time)
        .def_readonly("speed_mult", &rwrl::EngineState::speed_mult)
        .def_readonly("income_mult", &rwrl::EngineState::income_mult)
        .def_readonly("teams", &rwrl::EngineState::teams)
        .def_readonly("paused", &rwrl::EngineState::paused)
        .def_readonly("game_over", &rwrl::EngineState::game_over)
        .def_readonly("winner_team", &rwrl::EngineState::winner_team);

    py::class_<rwrl::Engine>(m, "Engine",
        "Mutable simulation instance. Separate instances may run concurrently; "
        "serialize access to each instance and its World/Unit references.")
        .def(py::init<>())
        .def("create_world", &rwrl::Engine::create_world, py::return_value_policy::reference)
        .def("setup_teams", &rwrl::Engine::setup_teams,
             py::arg("team_a") = 0, py::arg("team_b") = 1)
        .def("setup_teams_vec", &rwrl::Engine::setup_teams_vec)
        .def("load_map", [](rwrl::Engine& engine, const py::object& path,
                            bool spawn_map_units, const rwrl::Vec<rwrl::i32>& active_teams) {
            const py::object resolved = py::module_::import("resource_paths")
                .attr("resolve_map_path")(path);
            engine.load_map(py::str(resolved).cast<rwrl::Str>(), spawn_map_units, active_teams);
        },
             py::arg("path"), py::arg("spawn_map_units") = true,
             py::arg("active_teams") = rwrl::Vec<rwrl::i32>{})
        .def("restore_unit", &rwrl::Engine::restore_unit,
             py::return_value_policy::reference_internal)
        .def("finish_unit_restore", &rwrl::Engine::finish_unit_restore,
             py::arg("next_uid"), py::arg("formation_next_id") = 1)
        .def("restore_auxiliary_object", &rwrl::Engine::restore_auxiliary_object,
             py::arg("x"), py::arg("y"), py::arg("scorch_mark"),
             py::arg("nuclear_scorch") = false)
        .def("restore_projectile", &rwrl::Engine::restore_projectile)
        .def("restore_timeline", &rwrl::Engine::restore_timeline,
             py::arg("frame"), py::arg("clock_ms"), py::arg("game_time"),
             py::arg("clock_already_advanced") = false)
        .def("queue_quick_resync", &rwrl::Engine::queue_quick_resync)
        .def("flush_quick_resync", &rwrl::Engine::flush_quick_resync)
        .def("tick", &rwrl::Engine::tick,
             py::arg("dt") = 1.0f / rwrl::EngineConstants::FRAME_RATE)
        .def("run_for", &rwrl::Engine::run_for,
             py::arg("seconds"), py::arg("dt") = 1.0f / rwrl::EngineConstants::FRAME_RATE)
        .def("get_state", &rwrl::Engine::get_state)
        .def("team_ids", [](const rwrl::Engine& engine) {
            rwrl::Vec<rwrl::i32> ids;
            ids.reserve(engine.worlds().size());
            for (const auto& [team, _] : engine.worlds()) ids.push_back(team);
            return ids;
        })
        .def("get_world", py::overload_cast<rwrl::i32>(&rwrl::Engine::get_world),
             py::return_value_policy::reference)
        .def("registry", py::overload_cast<>(&rwrl::Engine::registry),
             py::return_value_policy::reference)
        .def("global_tick", &rwrl::Engine::global_tick)
        .def("game_time", &rwrl::Engine::game_time)
        .def("speed_mult", &rwrl::Engine::speed_mult)
        .def("set_speed_mult", &rwrl::Engine::set_speed_mult)
        .def("income_mult", &rwrl::Engine::income_mult)
        .def("set_income_mult", &rwrl::Engine::set_income_mult)
        .def("set_paused", &rwrl::Engine::set_paused)
        .def("set_disable_ai", &rwrl::Engine::set_disable_ai)
        .def("disable_ai", &rwrl::Engine::disable_ai)
        .def("set_raw_commands", &rwrl::Engine::set_raw_commands)
        .def("raw_commands", &rwrl::Engine::raw_commands)
        .def("set_singleplayer_mode", &rwrl::Engine::set_singleplayer_mode)
        .def("singleplayer_mode", &rwrl::Engine::singleplayer_mode)
        .def("set_nukes_enabled", &rwrl::Engine::set_nukes_enabled)
        .def("nukes_enabled", &rwrl::Engine::nukes_enabled)
        .def("set_fog_enabled", &rwrl::Engine::set_fog_enabled)
        .def("set_fog_mode", &rwrl::Engine::set_fog_mode)
        .def("fog_mode", &rwrl::Engine::fog_mode)
        .def("is_paused", &rwrl::Engine::is_paused)
        .def("has_map", &rwrl::Engine::has_map)
        .def("map_width", &rwrl::Engine::map_width)
        .def("map_height", &rwrl::Engine::map_height)
        .def("tile_size", &rwrl::Engine::tile_size)
        .def("set_unit_cap", &rwrl::Engine::set_unit_cap)
        .def("unit_cap", &rwrl::Engine::unit_cap)
        .def("set_team_alliance", &rwrl::Engine::set_team_alliance)
        .def("team_alliance", &rwrl::Engine::team_alliance)
        .def("are_allied", &rwrl::Engine::are_allied)
        .def("execute_command", &rwrl::Engine::execute_command);

    m.def("set_global_random_seed", &rwrl::set_global_random_seed,
          "Install l.bJ, the map's global random seed, read by the host from "
          "the save/replay gameSetup block.");
    m.def("global_random_seed", &rwrl::global_random_seed);

    m.attr("FRAME_RATE") = rwrl::EngineConstants::FRAME_RATE;    m.attr("STARTING_CREDITS") = rwrl::EngineConstants::STARTING_CREDITS;
    m.attr("DEFAULT_SPEED") = rwrl::EngineConstants::DEFAULT_SPEED;
    m.attr("DEFAULT_UNIT_CAP") = rwrl::EngineConstants::DEFAULT_UNIT_CAP;
}
