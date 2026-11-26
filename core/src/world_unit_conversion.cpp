#include "world_internal.h"

#include <cctype>

namespace rwrl {
namespace {

WeaponSlot make_weapon_slot(const WeaponDef& weapon, f32 unit_rotation,
                            bool custom_unit) {
    WeaponSlot slot{};
    slot.damage = weapon.damage;
    slot.splash_damage = weapon.splash_damage;
    slot.splash_radius = weapon.splash_radius;
    slot.projectile_speed = weapon.projectile_speed;
    slot.projectile_speed_spread = weapon.projectile_speed_spread;
    slot.projectile_wobble_amplitude = weapon.projectile_wobble_amplitude;
    slot.projectile_wobble_frequency_frames = weapon.projectile_wobble_frequency_frames;
    slot.projectile_target_speed = weapon.projectile_target_speed;
    slot.projectile_acceleration = weapon.projectile_acceleration;
    slot.projectile_lifetime = weapon.projectile_lifetime;
    slot.splash_expand_time = weapon.splash_expand_time;
    slot.splash_friendly_fire = weapon.splash_friendly_fire;
    slot.projectile_retarget_on_death = weapon.projectile_retarget_on_death;
    slot.projectile_retarget_range = weapon.projectile_retarget_range;
    slot.projectile_retarget_lead = weapon.projectile_retarget_lead;
    slot.projectile_deflection_power = weapon.projectile_deflection_power;
    slot.projectile_tags = weapon.projectile_tags;
    slot.projectile_should_reveal_fog = weapon.projectile_should_reveal_fog;
    slot.cooldown_max = weapon.cooldown_max;
    slot.energy_usage = weapon.energy_usage;
    slot.laser_defence_enabled = weapon.laser_defence_enabled;
    slot.laser_defence_energy_usage = weapon.laser_defence_energy_usage;
    slot.intercept_projectile_tags = weapon.intercept_projectile_tags;
    slot.intercept_ground_target_distance = weapon.intercept_ground_target_distance;
    slot.intercept_projectile_distance = weapon.intercept_projectile_distance;
    slot.intercept_projectile_height = weapon.intercept_projectile_height;
    slot.intercept_ammo_usage = weapon.intercept_ammo_usage;
    slot.intercept_required_flags = weapon.intercept_required_flags;
    slot.intercept_missing_flags = weapon.intercept_missing_flags;
    slot.limiting_range = weapon.limiting_range;
    slot.limiting_min_range = weapon.limiting_min_range;
    slot.limiting_angle = weapon.limiting_angle;
    slot.fire_delay_frames = weapon.fire_delay_frames > 0.0f
        ? weapon.fire_delay_frames
        : static_cast<f32>(weapon.cooldown_max);
    slot.recoil_offset = weapon.recoil_offset;
    slot.recoil_out_frames = weapon.recoil_out_frames;
    slot.recoil_return_frames = weapon.recoil_return_frames;
    slot.tracking = weapon.tracking;
    slot.fixed_target = weapon.fixed_target;
    slot.fixed_target_include_height = weapon.fixed_target_include_height;
    slot.disable_lead_targeting = weapon.disable_lead_targeting;
    slot.lead_targeting_speed = weapon.lead_targeting_speed;
    slot.ground_target_height_offset = weapon.ground_target_height_offset;
    slot.ground_target_spread = weapon.ground_target_spread;
    slot.hitscan = weapon.hitscan;
    slot.persist_after_hit = weapon.persist_after_hit;
    slot.instant_reuse_last = weapon.instant_reuse_last;
    slot.instant_reuse_last_change_turret_aim = weapon.instant_reuse_last_change_turret_aim;
    slot.instant_reuse_last_keep_area_damage_list = weapon.instant_reuse_last_keep_area_damage_list;
    slot.move_with_parent = weapon.move_with_parent;
    slot.sweep_offset = weapon.sweep_offset;
    slot.sweep_offset_from_target_radius = weapon.sweep_offset_from_target_radius;
    slot.projectile_flame_weapon = weapon.projectile_flame_weapon;
    slot.laser_defence_excluded = weapon.laser_defence_excluded;
    slot.splash_same_layer = weapon.splash_same_layer;
    slot.targets_air = weapon.targets_air;
    slot.targets_ground = weapon.targets_ground;
    slot.targets_underwater = weapon.targets_underwater;
    slot.targets_not_touching_water = weapon.targets_not_touching_water;
    slot.shield_mult = weapon.shield_mult;
    slot.shield_absorb = weapon.shield_absorb;
    slot.hp_mult = weapon.hp_mult;
    slot.building_damage_mult = weapon.building_damage_mult;
    slot.air_damage_mult = weapon.air_damage_mult;
    slot.experimental_damage_mult = weapon.experimental_damage_mult;
    slot.warmup_frames = weapon.warmup_frames;
    slot.warmup_no_reset = weapon.warmup_no_reset;
    slot.warmup_call_down_rate = weapon.warmup_call_down_rate;
    slot.warmup_shoot_delay_transfer = weapon.warmup_shoot_delay_transfer;
    slot.fire_enabled = weapon.fire_enabled;
    slot.linked_slot = weapon.linked_slot;
    slot.attached_to_slot = weapon.attached_to_slot;
    slot.delay_link_slot = weapon.delay_link_slot;
    const f32 initial_rotation = custom_unit ? 0.0f : unit_rotation;
    slot.turret_angle = initial_rotation + weapon.turret_idle_direction;
    slot.turret_previous_angle = slot.turret_angle;
    slot.turret_turn_speed = weapon.turret_turn_speed;
    slot.turret_turn_acceleration = weapon.turret_turn_acceleration;
    slot.turret_turn_deceleration = weapon.turret_turn_deceleration;
    slot.turret_resets_to_idle = weapon.turret_resets_to_idle;
    slot.turret_idle_direction = weapon.turret_idle_direction;
    slot.turret_idle_direction_reversing =
        weapon.turret_idle_direction_reversing;
    slot.turret_idle_spin = weapon.turret_idle_spin;
    slot.turret_idle_sweep_angle = weapon.turret_idle_sweep_angle;
    slot.turret_idle_sweep_delay = weapon.turret_idle_sweep_delay;
    slot.turret_idle_sweep_speed = weapon.turret_idle_sweep_speed;
    slot.turret_idle_sweep_random_delay =
        weapon.turret_idle_sweep_random_delay;
    slot.turret_idle_sweep_random_angle =
        weapon.turret_idle_sweep_random_angle;
    slot.turret_idle_sweep_requires_stationary =
        weapon.turret_idle_sweep_requires_stationary;
    slot.aim_tolerance = weapon.aim_tolerance;
    slot.aim_offset_spread = weapon.aim_offset_spread;
    slot.muzzle_offset = weapon.muzzle_offset;
    slot.muzzle_side = weapon.muzzle_side;
    slot.muzzle_side_odd = weapon.muzzle_side_odd;
    slot.mount_forward = weapon.mount_forward;
    slot.mount_side = weapon.mount_side;
    slot.projectile_uses_muzzle = weapon.projectile_uses_muzzle;
    slot.height_guided = weapon.height_guided;
    slot.nuke_weapon = weapon.nuke_weapon;
    slot.launch_height = weapon.launch_height;
    slot.cruise_height = weapon.cruise_height;
    slot.vertical_speed = weapon.vertical_speed;
    slot.projectile_unguided_speed_height = weapon.projectile_unguided_speed_height;
    slot.projectile_unguided_speed_x = weapon.projectile_unguided_speed_x;
    slot.projectile_unguided_speed_y = weapon.projectile_unguided_speed_y;
    slot.projectile_gravity = weapon.projectile_gravity;
    slot.projectile_true_gravity = weapon.projectile_true_gravity;
    slot.fixed_turret = weapon.fixed_turret;
    slot.turret_rotate_with_body = weapon.turret_rotate_with_body;
    return slot;
}

f32 preserve_ratio(f32 current, f32 old_max, f32 new_max) {
    return old_max == 0.0f ? new_max : current / old_max * new_max;
}

void preserve_weapon_state(WeaponSlot& slot, const WeaponSlot& old) {
    slot.target_uid = old.target_uid;
    slot.target_team_id = old.target_team_id;
    slot.cooldown_remainder = std::min(
        old.cooldown_remainder, slot.fire_delay_frames);
    slot.cooldown_ticks = static_cast<i32>(std::ceil(slot.cooldown_remainder));
    slot.warmup_progress = std::min(old.warmup_progress, slot.warmup_frames);
    slot.warmup_target_uid = old.warmup_target_uid;
    slot.aim_ready = old.aim_ready;
    slot.turret_angle = old.turret_angle;
    slot.turret_previous_angle = old.turret_previous_angle;
    slot.turret_turn_velocity = old.turret_turn_velocity;
    slot.turret_idle_timer = old.turret_idle_timer;
    slot.turret_sweep_timer = old.turret_sweep_timer;
    slot.turret_sweep_direction = old.turret_sweep_direction;
    slot.aim_offset_x = old.aim_offset_x;
    slot.aim_offset_y = old.aim_offset_y;
    slot.odd_shot = old.odd_shot;
}

bool same_turret_name(const Str& first, const Str& second) {
    return first.size() == second.size() &&
        std::equal(first.begin(), first.end(), second.begin(),
            [](unsigned char a, unsigned char b) {
                return std::tolower(a) == std::tolower(b);
            });
}

}  // namespace

void World::convert_unit_definition_(Unit& unit, const UnitDef& definition,
                                     bool preserve_pool_ratios) {
    const bool was_indexed = unit.is_alive && !unit.cleanup_done;
    const bool was_building = unit.is_building;
    const bool was_custom_unit = unit.custom_unit;
    const UnitDef* old_definition = registry_.get(unit.unit_type);
    const MoveType old_move_type = unit.move_type;
    const f32 old_income = was_indexed && unit.is_complete()
        ? unit.income_rate : 0.0f;
    const f32 old_max_hp = unit.max_hp;
    const f32 old_max_shield = unit.max_shield;
    const f32 old_max_energy = unit.max_energy;
    const f32 old_hp = unit.hp;
    const f32 old_shield = unit.shield;
    const f32 old_energy = unit.energy;

    if (was_indexed) grid_.remove(unit);

    unit.unit_type = definition.name;
    unit.custom_unit = registry_.is_custom(definition.name);
    unit.can_attack = definition.can_attack;
    unit.collision_enabled = !definition.is_building;
    unit.custom_action_movement_blocked = false;
    unit.radius = definition.radius;
    unit.mass = definition.mass;
    unit.soft_collision_on_all = definition.soft_collision_on_all;
    unit.armour = definition.armour;
    unit.armour_min_damage = definition.armour_min_damage;
    unit.max_hp = definition.hp;
    if (preserve_pool_ratios) {
        unit.hp = preserve_ratio(old_hp, old_max_hp, unit.max_hp);
    } else {
        const f32 hp_delta = std::max(0.0f, unit.max_hp - old_max_hp);
        unit.hp = std::min(unit.max_hp, std::max(0.0f, old_hp + hp_delta));
    }
    unit.max_shield = definition.shield;
    unit.shield = preserve_pool_ratios && !definition.start_shield_at_zero
        ? preserve_ratio(old_shield, old_max_shield, unit.max_shield)
        : std::min(unit.max_shield, std::max(0.0f, old_shield));
    unit.shield_regen_rate = definition.shield_regen_rate;
    unit.shield_regen_per_frame = definition.shield_regen_per_frame;
    unit.damage = definition.damage;
    unit.shoot_delay_multiplier = definition.shoot_delay_multiplier;
    unit.shoot_damage_multiplier = definition.shoot_damage_multiplier;
    unit.weapon_range = definition.range;
    unit.attack_range_adds_radii = definition.attack_range_adds_radii;
    unit.attack_follow_e_flag = definition.attack_follow_e_flag;
    unit.attack_follow_weapon_n = definition.attack_follow_weapon_n;
    unit.bomber_attack_movement = definition.bomber_attack_movement;
    unit.disable_passive_targeting = definition.disable_passive_targeting;
    unit.turret_multi_targeting = definition.turret_multi_targeting;
    unit.move_speed = definition.speed > 0.0f
        ? definition.speed * EngineConstants::TICK_RATE : 0.0f;
    // custom.j.a(l,...) resets j even when both definitions use the same layer.
    unit.terrain_speed_multiplier = 1.0f;
    unit.turn_speed = definition.turn_speed;
    unit.turn_acceleration = definition.turn_acceleration;
    unit.move_acceleration = definition.move_acceleration;
    unit.move_deceleration = definition.move_deceleration;
    unit.strafes = definition.strafes;
    unit.move_ignores_body = definition.move_ignores_body;
    unit.joins_group_formations = definition.joins_group_formations;
    unit.reverse_speed_multiplier = definition.reverse_speed_multiplier;
    unit.move_type = definition.move_type;
    if (old_move_type != unit.move_type) {
        // custom.j changes the movement layer through y.aH(): clear the
        // adopted path and its recovery counters, retaining orders, the
        // logical move proposal, and any outstanding solver request.
        unit.using_path = false;
        unit.waypoints.clear();
        unit.current_waypoint = 0;
        unit.path_incomplete = false;
        unit.repath_cooldown_frames = 0.0f;
        unit.waypoint_stall_frames = 0.0f;
        unit.last_waypoint_stall_frames = 0.0f;
        unit.single_node_path_counter = 0;
        unit.path_request_preserves_active_path = false;
    }
    // custom.j replaces the definition without reactivating the existing
    // waypoint. weapon_type also represents C++'s active order behavior, so
    // keep that behavior until the normal order handler drops the waypoint.
    if (unit.order_queue.empty()) unit.weapon_type = definition.weapon_type;
    unit.slow_death_fall = definition.slow_death_fall;
    unit.explode_on_death_ground_collision =
        definition.explode_on_death_ground_collision;
    unit.hide_scorch_mark = definition.hide_scorch_mark;
    unit.death_effect_uid_count = definition.death_effect_uid_count;
    unit.has_wreck_image = definition.has_wreck_image;
    unit.default_ground_death_effect = definition.default_ground_death_effect;
    unit.fire_turret_on_death = definition.fire_turret_on_death;
    unit.nuke_on_death = definition.nuke_on_death;
    unit.nuke_on_death_range = definition.nuke_on_death_range;
    unit.nuke_on_death_damage = definition.nuke_on_death_damage;
    unit.nuke_on_death_disable_when_no_nuke = definition.nuke_on_death_disable_when_no_nuke;
    unit.leg_definitions = definition.legs;
    unit.leg_rotation_slot = definition.leg_rotation_slot;
    unit.leg_death_scorch = definition.leg_death_scorch;
    // custom.j.a(l,...) preserves y.P, including holdFire, during conversion.
    unit.is_building = definition.is_building;
    unit.collision_footprint = definition.collision_footprint;
    unit.placement_footprint = definition.placement_footprint;
    unit.building_to_footprint_offset_x = definition.building_to_footprint_offset_x;
    unit.building_to_footprint_offset_y = definition.building_to_footprint_offset_y;
    unit.has_factory_exit_config = definition.has_factory_exit_config;
    unit.factory_exit_x = definition.factory_exit_x;
    unit.factory_exit_y = definition.factory_exit_y;
    unit.factory_exit_move_away_amount =
        definition.factory_exit_move_away_amount;
    unit.factory_exit_has_dir_offset = definition.factory_exit_has_dir_offset;
    unit.factory_exit_dir_offset = definition.factory_exit_dir_offset;
    unit.factory_exit_height_offset = definition.factory_exit_height_offset;
    unit.factory_exit_height_ignore_parent =
        definition.factory_exit_height_ignore_parent;
    unit.factory_keep_built_units = definition.factory_keep_built_units;
    unit.income_rate = definition.income;
    unit.income_interval = definition.income_interval;
    if (preserve_pool_ratios && old_definition &&
        old_definition->cost != definition.cost) {
        // custom.j.a(l,false) clears bx/by price overrides when the source
        // and destination prices differ. Reclaim then uses the destination's
        // full cM() price. Equal-price conversions retain the old override;
        // stock upgrades account for their paid price in their own caller.
        unit.reclaim_cost = static_cast<f32>(definition.cost);
    }
    unit.cost = definition.cost;
    unit.build_time = definition.build_time;
    unit.cooldown_max = definition.cooldown_max;
    unit.splash_radius = definition.splash_radius;
    unit.splash_damage = definition.splash_damage;
    unit.projectile_speed = definition.projectile_speed >= 0.0f
        ? definition.projectile_speed : EngineConstants::PROJECTILE_SPEED;
    unit.projectile_lifetime = definition.projectile_lifetime >= 0.0f
        ? definition.projectile_lifetime : EngineConstants::PROJECTILE_LIFETIME;
    unit.projectile_tracking = definition.projectile_tracking;
    unit.projectile_hitscan = definition.projectile_hitscan;
    unit.can_move = definition.can_move;
    unit.is_builder = definition.is_builder;
    unit.ignore_move_orders = definition.ignore_move_orders;
    unit.airborne = definition.airborne;
    unit.sight_range = definition.sight_range;
    unit.nano_range = definition.nano_range;
    unit.nano_turret_index = definition.nano_turret_index;
    unit.nano_build_speed = definition.nano_build_speed;
    unit.self_regen_per_frame = definition.self_regen_rate;
    unit.self_regen_rate =
        unit.self_regen_per_frame * EngineConstants::FRAME_RATE;
    unit.target_height = definition.target_height;
    unit.nano_factory_speed = definition.nano_factory_speed;
    unit.target_height_drift = definition.target_height_drift;
    unit.height_change_rate = definition.height_change_rate;
    unit.falling_acceleration = definition.falling_acceleration;
    unit.land_on_ground = definition.land_on_ground;
    unit.land_on_ground_only_idle = definition.land_on_ground_only_idle;
    unit.base_nano_range = definition.base_nano_range;
    unit.can_repair = definition.can_repair;
    unit.can_repair_any = definition.can_repair_any;
    unit.auto_repair = definition.auto_repair;
    unit.nano_repair_speed = definition.nano_repair_speed;
    unit.nano_reclaim_speed = definition.nano_reclaim_speed;
    unit.resource_reclaim_multiplier = definition.resource_reclaim_multiplier;
    unit.nano_unbuild_speed = definition.nano_unbuild_speed;
    unit.can_reclaim = definition.can_reclaim;
    unit.can_repair_buildings = definition.can_repair_buildings;
    unit.can_reclaim_resources = definition.can_reclaim_resources;
    unit.can_be_repaired = definition.can_be_repaired;
    unit.targets_air = definition.targets_air;
    unit.targets_ground = definition.targets_ground;
    unit.ground_attack_disabled_while_flying = definition.ground_attack_disabled_while_flying;
    unit.targets_underwater = definition.targets_underwater;
    unit.targets_not_touching_water = definition.targets_not_touching_water;
    unit.upgrade_to = definition.upgrade_to;
    unit.cargo_capacity = definition.cargo_capacity;
    unit.transport_unload_condition = definition.transport_unload_condition;
    unit.transport_add_unload_option = definition.transport_add_unload_option;
    unit.transport_unload_delay = definition.transport_unload_delay;
    unit.hovercraft_unload_style = definition.hovercraft_unload_style;
    unit.cargo_weight = definition.cargo_weight;
    unit.path_min_clearance = definition.path_min_clearance;
    unit.transportable = definition.transportable;
    unit.is_nuke = definition.is_nuke;
    unit.is_antinuke = definition.is_antinuke;
    unit.nuke_range = definition.nuke_range;
    unit.shield_mult = definition.shield_mult;
    unit.shield_absorb = definition.shield_absorb;
    unit.hp_mult = definition.hp_mult;
    unit.air_damage_mult = definition.air_damage_mult;
    unit.experimental_damage_mult = definition.experimental_damage_mult;
    unit.is_experimental = definition.is_experimental;
    unit.building_damage_mult = definition.building_damage_mult;
    unit.is_melee = definition.is_melee;
    unit.is_beam = definition.is_beam;
    unit.chain_count = definition.chain_count;
    unit.chain_range = definition.chain_range;
    unit.is_flame = definition.is_flame;
    unit.flame_expand_rate = definition.flame_expand_rate;
    unit.can_build = definition.can_build;
    unit.built_from = definition.built_from;
    unit.tech_level = definition.tech_level;
    unit.ammo_max = definition.ammo_max;
    // custom.j.applyMetadata leaves the native am.cE stockpile unchanged.
    // Shipped conversions share that pool even when ammo_max is only 1.
    if (!unit.custom_unit) unit.ammo = std::min(unit.ammo, unit.ammo_max);
    unit.ammo_reload_timer = definition.ammo_reload_ticks;
    unit.energy_per_shot = definition.energy_per_shot;
    unit.max_energy = unit.custom_unit || definition.max_energy > 0.0f
        ? definition.max_energy
        : (definition.energy_per_shot > 0.0f ? 100.0f : 0.0f);
    unit.energy = preserve_pool_ratios &&
            !(unit.custom_unit && definition.start_energy_at_zero)
        ? preserve_ratio(old_energy, old_max_energy, unit.max_energy)
        : std::min(old_energy, unit.max_energy);
    unit.energy_regen_rate = definition.max_energy > 0.0f
        ? definition.energy_regen_rate
        : (definition.energy_per_shot > 0.0f ? 1.0f : 0.0f);
    unit.energy_regen_per_frame = definition.energy_regen_per_frame;
    unit.energy_recharge_regen_per_frame = definition.energy_recharge_regen_per_frame;
    unit.energy_needs_full_recharge = definition.energy_needs_full_recharge;
    unit.die_on_zero_energy = definition.die_on_zero_energy;

    Vec<WeaponSlot> old_slots = std::move(unit.weapon_slots);
    unit.weapon_slots.clear();
    unit.weapon_slots.reserve(definition.weapons.size());
    const bool preserve_turrets = preserve_pool_ratios &&
        was_custom_unit && unit.custom_unit && old_definition;
    bool same_turret_layout = preserve_turrets &&
        (definition.weapons.size() <= 1 ||
         definition.weapons.size() == old_definition->weapons.size());
    if (same_turret_layout && definition.weapons.size() > 1) {
        for (std::size_t i = 0; i < definition.weapons.size(); ++i) {
            if (!same_turret_name(definition.weapons[i].turret_name,
                                 old_definition->weapons[i].turret_name)) {
                same_turret_layout = false;
                break;
            }
        }
    }
    for (std::size_t i = 0; i < definition.weapons.size(); ++i) {
        const WeaponDef& weapon = definition.weapons[i];
        WeaponSlot slot = make_weapon_slot(
            weapon, unit.rotation, unit.custom_unit);
        if (preserve_turrets) {
            std::size_t old_index = old_slots.size();
            if (same_turret_layout) {
                old_index = i;
            } else if (!weapon.turret_name.empty()) {
                for (std::size_t j = 0;
                     j < old_definition->weapons.size() && j < old_slots.size(); ++j) {
                    if (same_turret_name(old_definition->weapons[j].turret_name,
                                         weapon.turret_name)) {
                        old_index = j;
                        break;
                    }
                }
            }
            if (old_index < old_slots.size()) {
                preserve_weapon_state(slot, old_slots[old_index]);
            } else {
                // custom.j resets an unmatched ap to the body's direction.
                slot.turret_angle = unit.rotation;
                slot.turret_previous_angle = unit.rotation;
            }
        }
        unit.weapon_slots.push_back(slot);
    }
    if (preserve_turrets && !unit.weapon_slots.empty()) {
        unit.cooldown_ticks = unit.weapon_slots.front().cooldown_ticks;
    }
    configure_service_warmup(unit);
    if (unit.order_queue.empty() && definition.range > 0.0f &&
        (definition.damage > 0.0f || definition.splash_damage > 0.0f) &&
        !unit.is_nuke && !unit.is_antinuke) {
        unit.weapon_type = unit.is_mobile()
            ? WeaponType::ATTACK : WeaponType::GUARD;
    }

    initialize_legs_(unit);
    if (old_income > 0.0f) total_income_rate_ -= old_income;
    if (was_indexed && unit.is_complete() && unit.income_rate > 0.0f) {
        total_income_rate_ += unit.income_rate;
    }
    if (was_indexed) grid_.add(unit);
    if (was_building || unit.is_building) {
        rebuild_local_buildings_();
    }
}

}  // namespace rwrl
