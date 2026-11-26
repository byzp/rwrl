#pragma once
#include <rwrl/common.h>
#include <rwrl/enums.h>
#include <rwrl/constants.h>
#include <limits>

namespace rwrl {

enum class CustomStat : i32 {
    MASS,
    MAX_ENERGY,
    ENERGY,
    MAX_HP,
    HP,
    MAX_SHIELD,
    SHIELD,
    SHIELD_REGEN,
    ARMOUR,
    MAX_ATTACK_RANGE,
    SHOOT_DELAY_MULTIPLIER,
    SHOOT_DAMAGE_MULTIPLIER,
    MOVE_SPEED,
    MAX_TURN_SPEED,
    FOG_OF_WAR_SIGHT_RANGE,
    NANO_RANGE,
    SELF_REGEN_RATE,
    TARGET_HEIGHT,
    NANO_FACTORY_SPEED,
};

enum class CustomStatOperator : i32 {
    SET,
    ADD,
    SUBTRACT,
};

struct CustomStatAssignment {
    CustomStat stat = CustomStat::MASS;
    f32 value = 0.0f;
    CustomStatOperator op = CustomStatOperator::SET;
};

struct CustomResourceDelta {
    i32 credits = 0;
    f32 energy = 0.0f;
    f32 hp = 0.0f;
    f32 shield = 0.0f;
    i32 ammo = 0;
    i32 set_flags = 0;
    i32 unset_flags = 0;
};

enum class CustomResource : i32 {
    NONE,
    CREDITS,
    AMMO,
    ENERGY,
    HP,
    SHIELD,
};

struct CustomResourceAmountEffect {
    CustomResource resource = CustomResource::NONE;
    bool has_set_value = false;
    f64 set_value = 0.0;
    CustomResource add_other_resource = CustomResource::NONE;
    f32 multiply_by = 1.0f;
};

struct CustomResourceConversionEffect {
    CustomResource source = CustomResource::NONE;
    CustomResource target = CustomResource::NONE;
    f64 min_amount = 0.0;
    f64 max_amount = 0.0;
    f32 multiply_amount_by = 1.0f;
};

struct CustomWaypointEffect {
    bool enabled = false;
    WeaponType type = WeaponType::MOVE;
    bool prepend = false;
    bool position_from_action = false;
    bool has_position_offset_from_self = false;
    f32 position_offset_from_self_x = 0.0f;
    f32 position_offset_from_self_y = 0.0f;
    bool has_position_relative_offset_from_self = false;
    f32 position_relative_offset_from_self_x = 0.0f;
    f32 position_relative_offset_from_self_y = 0.0f;
    // JAR seconds are represented as simulation frames by the structured API.
    // -1 has no timeout; zero creates a transient waypoint and does not queue it.
    f32 max_time = -1.0f;
};

struct CustomUnitSpawn {
    Str unit_type;
    i32 count = 1;
    f32 spawn_chance = 1.0f;
    // Shared successful-spawn count across the containing list is compared
    // against this entry limit, matching custom.bp.
    i32 max_spawn_limit = std::numeric_limits<i32>::max();
    bool grid_align = false;
    bool skip_if_overlapping = false;
    bool falling = false;
    i32 tech_level = -1;
    bool always_start_dir_at_zero = false;
    f32 offset_x = 0.0f;
    f32 offset_y = 0.0f;
    f32 offset_height = 0.0f;
    f32 offset_dir = 0.0f;
    f32 offset_random_x = 0.0f;
    f32 offset_random_y = 0.0f;
    f32 offset_random_dir = 0.0f;
    CustomResourceDelta add_resources;
    i32 transported_units_to_transfer = 0;
    // The structured API currently exposes the canonical self-reference
    // branch. Arbitrary LogicBoolean UnitReferences need a separate model.
    bool copy_waypoints_from_self = false;
};

enum class CustomTakeResourcesTeam : i32 {
    OWN,
    ALLY,
    ALLY_NOT_OWN,
    ENEMY,
    NEUTRAL,
    ANY,
    NOT_OWN,
};

struct CustomTakeResourcesEffect {
    bool enabled = false;
    CustomResourceDelta resources;
    bool include_units_in_transport = false;
    bool include_parent = false;
    // custom.a.a.m only runs its spatial query for values greater than zero.
    // Zero is a valid configured value but produces no range candidates.
    f32 include_units_within_range = -1.0f;
    CustomTakeResourcesTeam range_team = CustomTakeResourcesTeam::OWN;
    bool exclude_units_without_all_resources = true;
    bool has_exclude_units_with_resources = false;
    CustomResourceDelta exclude_units_with_resources;
    i32 max_units = 1;
    bool discard_collected = false;
    bool keep_resources_on_target = false;
    Str trigger_action_if_any_collected;
    Str trigger_action_if_none_collected;
    Str trigger_action_for_each;
    // The JAR rejects built-in resources with this mode. The structured C++
    // API currently has no arbitrary custom-resource registry, so registration
    // rejects it as well instead of silently using different semantics.
    bool direct_transfer_stopping_at_zero = false;
};

struct CustomActionDef {
    Str id;
    // custom.a.g actions submitted directly use d.k's production queue.
    // triggerAction waypoints still execute their effects immediately.
    bool queueable = false;
    i32 price = 0;
    i32 price_ammo = 0;
    bool high_priority_queue = false;
    i32 fire_turret_index = -1;
    i32 fire_turret_count = 1;
    f32 build_speed = 0.01f;
    bool can_player_cancel = true;
    bool allow_multiple_in_queue = true;
    // Constant lessThan argument of self.ammoIncludingQueued(). Negative
    // leaves the action unrestricted; custom.j.by() sums queued addResources.
    f32 ammo_including_queued_limit = -1.0f;
    bool ignore_factory_speed_modifiers = false;
    bool when_building_cannot_move = false;
    bool has_when_building_rotate_to = false;
    f32 when_building_rotate_to = 0.0f;
    bool when_building_rotate_to_or_backwards = false;
    // Action-level addResources runs before the ordered effect list.
    CustomResourceDelta add_resources;
    // Implemented custom.a.a.o transport operations. Tag-filtered deletion
    // and LogicBoolean transport targets require their own parsers.
    i32 delete_num_units_from_transport = 0;
    bool start_unloading_transport = false;
    bool force_unload_transport_now = false;
    i32 force_unload_transport_now_only_on_slot = -1;
    // First supported branches of custom.a.a.b. clearAll wins when both are
    // true, matching the effect's if/else-if execution.
    bool clear_all_waypoints = false;
    bool clear_active_waypoint = false;
    CustomWaypointEffect add_waypoint;
    // custom.a.a.k: factory-style products and direct in-place spawns.
    Vec<CustomUnitSpawn> produce_units;
    Vec<CustomUnitSpawn> spawn_units;
    // custom.a.a.a is registered before the stats/deleteSelf effect loader.
    CustomResourceAmountEffect resource_amount;
    // custom.a.a.g is registered after resourceAmount and before custom.a.a.j.
    CustomResourceConversionEffect resource_conversion;
    // custom.a.a.m is registered after convertResource and before the shared
    // stats/deleteSelf effect.
    CustomTakeResourcesEffect take_resources;
    bool reset_unit_stats = false;
    // VariableScope.CachedWriter preserves the textual assignment order.
    Vec<CustomStatAssignment> set_unit_stats;
    bool delete_self = false;
    // Constant LogicBoolean number paths for custom.a.a.j.
    bool has_set_body_rotation = false;
    f32 set_body_rotation = 0.0f;
    bool has_set_height = false;
    f32 set_height = 0.0f;
    bool clear_all_action_cooldowns = false;
    // Already-parsed JAR l.by tick values. INI seconds are multiplied by
    // ACTION_COOLDOWN_TICKS_PER_SECOND before reaching this effect.
    f32 add_all_action_cooldowns_time = 0.0f;
    f32 add_action_cooldown_time = 0.0f;
    // Empty applies add_action_cooldown_time to the action being executed.
    Vec<Str> add_action_cooldown_apply_to_actions;
    bool remove_all_queued_items_without_refund = false;
    bool refund_all_queued_items = false;
    f32 set_built = -1.0f;
    bool has_offset_self_absolute = false;
    f32 offset_self_absolute_x = 0.0f;
    f32 offset_self_absolute_y = 0.0f;
    f32 offset_self_absolute_height = 0.0f;
    // Action metadata is applied after all effects have run.
    Str convert_to;
};

struct WeaponDef {
    f32 damage = 0.0f;
    f32 splash_damage = 0.0f;
    f32 splash_radius = 0.0f;
    f32 projectile_speed = 0.0f;
    // custom.bh.aM: random initial speed offset, in units per game frame.
    f32 projectile_speed_spread = 0.0f;
    // game.g.am/an: lateral sinusoidal motion and its frame period.
    f32 projectile_wobble_amplitude = 0.0f;
    f32 projectile_wobble_frequency_frames = 5.0f;
    // game.f.r/s: optional target speed and the per-frame approach amount.
    // Speeds use the engine's world-units-per-second convention.
    f32 projectile_target_speed = -1.0f;
    f32 projectile_acceleration = 0.1f * EngineConstants::FRAME_RATE;
    f32 projectile_lifetime = 0.0f;
    f32 splash_expand_time = 0.0f;
    bool splash_friendly_fire = false;
    // custom.bh.D/E/F and game.g.G/H, in world units per simulation frame.
    f32 projectile_unguided_speed_height = 0.0f;
    f32 projectile_unguided_speed_x = 0.0f;
    f32 projectile_unguided_speed_y = 0.0f;
    f32 projectile_gravity = 0.0f;
    f32 projectile_true_gravity = 0.0f;
    bool projectile_retarget_on_death = false;
    f32 projectile_retarget_range = 120.0f;
    f32 projectile_retarget_lead = 15.0f;
    // custom.bh.o: values below 0.5 make the projectile laser-immune;
    // other values initialize the native f.H interception durability.
    f32 projectile_deflection_power = 1.0f;
    Vec<Str> projectile_tags;
    bool projectile_should_reveal_fog = false;
    i32 cooldown_max = 60;
    // Turret-local bn.m delay; negative inherits [attack] shootDelay.
    f32 fire_delay_frames = -1.0f;
    // custom.bn.u is a turret-local cost shared by the unit's energy pool.
    f32 energy_usage = 0.0f;
    // custom.bn.aj is nullable; an explicit zero cost still enables defence.
    bool laser_defence_enabled = false;
    f32 laser_defence_energy_usage = 0.0f;
    // custom.bn.ak/al/am/an are independent of ordinary turret targeting,
    // aiming and reload. custom.b.k fires the last eligible tagged shot.
    Vec<Str> intercept_projectile_tags;
    f32 intercept_ground_target_distance = -1.0f;
    f32 intercept_projectile_distance = 2000.0f;
    f32 intercept_projectile_height = -999.0f;
    i32 intercept_ammo_usage = 0;
    i32 intercept_required_flags = 0;
    i32 intercept_missing_flags = 0;
    // custom.bn.ab uses the unit's range when it is at least 99999.
    f32 limiting_range = 99999.0f;
    f32 limiting_min_range = 0.0f;
    // custom.bn.ai: cone relative to attached parent's angle or body+idleDir.
    f32 limiting_angle = -1.0f;
    // Turret recoil sampled by y.G(slot). The distance is applied
    // along the current turret angle while the slot is reloading.
    f32 recoil_offset = 0.0f;
    f32 recoil_out_frames = 0.0f;
    f32 recoil_return_frames = 0.0f;
    bool tracking = false;
    bool hitscan = false;
    // game.f.B/M/g.X retain resolved laser, lightning, and image beams.
    bool persist_after_hit = false;
    bool instant_reuse_last = false;
    bool instant_reuse_last_change_turret_aim = false;
    bool instant_reuse_last_keep_area_damage_list = false;
    bool move_with_parent = false;
    f32 sweep_offset = 0.0f;
    f32 sweep_offset_from_target_radius = 0.0f;
    // custom.bh.aI / game.f.G emits a shared-ID object on impact.
    bool projectile_flame_weapon = false;
    bool laser_defence_excluded = false;
    bool splash_same_layer = true;
    bool targets_air = true;
    bool targets_ground = true;
    // custom.bn's absent target conditions allow all layers independently
    // of the unit-level capabilities used to select the main target.
    bool targets_underwater = true;
    bool targets_not_touching_water = true;
    f32 shield_mult = 1.0f;
    f32 shield_absorb = 1.0f;
    f32 hp_mult = 1.0f;
    f32 building_damage_mult = 1.0f;
    f32 air_damage_mult = 1.0f;
    f32 experimental_damage_mult = 1.0f;
    f32 warmup_frames = 0.0f;
    // Custom.j keeps warm-up progress while this turret fires instead of
    // resetting it after every shot.
    bool warmup_no_reset = false;
    f32 warmup_call_down_rate = 4.0f;
    f32 warmup_shoot_delay_transfer = 0.0f;
    bool fire_enabled = true;
    i32 linked_slot = -1;
    // custom.bn.w: attachment does not imply slave aiming.
    i32 attached_to_slot = -1;
    // Index of the turret whose shot releases this slot's long delay.
    i32 delay_link_slot = -1;
    f32 turret_turn_speed = 999.0f;
    f32 turret_turn_acceleration = -1.0f;
    f32 turret_turn_deceleration = -1.0f;
    bool turret_resets_to_idle = true;
    f32 turret_idle_direction = 0.0f;
    f32 turret_idle_direction_reversing = 0.0f;
    // custom.j idle turret behavior (bn.ar..bn.ax).
    f32 turret_idle_spin = 0.0f;
    f32 turret_idle_sweep_angle = 0.0f;
    f32 turret_idle_sweep_delay = 10.0f;
    f32 turret_idle_sweep_speed = 0.0f;
    f32 turret_idle_sweep_random_delay = -1.0f;
    f32 turret_idle_sweep_random_angle = 0.0f;
    bool turret_idle_sweep_requires_stationary = false;
    f32 aim_tolerance = 5.0f;
    f32 aim_offset_spread = 0.6f;
    f32 muzzle_offset = 0.0f;
    // custom.bn Y/Z: turret-local barrelX and its alternate-shot offset.
    f32 muzzle_side = 0.0f;
    f32 muzzle_side_odd = 0.0f;
    // G(slot) is the turret base in unit-local forward/side coordinates.
    f32 mount_forward = 0.0f;
    f32 mount_side = 0.0f;
    bool projectile_uses_muzzle = true;
    bool height_guided = false;
    bool nuke_weapon = false;
    f32 launch_height = 0.0f;
    f32 cruise_height = 0.0f;
    f32 vertical_speed = 0.0f;
    bool fixed_turret = false;
    // custom.l.dL / [attack] turretRotateWithBody.
    bool turret_rotate_with_body = true;
    // custom.bh targetGround fixes the projectile's destination at launch;
    // it is independent of the turret's permitted target layers.
    bool fixed_target = false;
    bool fixed_target_include_height = false;
    bool disable_lead_targeting = false;
    f32 lead_targeting_speed = -1.0f;
    f32 ground_target_height_offset = 0.0f;
    f32 ground_target_spread = 0.0f;
    // custom.bn.a: names identify the state retained across convertTo, even
    // when the destination INI declares the same turrets in another order.
    Str turret_name;
};

// Inclusive tile offsets returned by am.cc()/am.cd() in game-lib.jar.
// The rectangle is relative to the unit's map-tile anchor.
struct TileRect {
    i32 left = 0;
    i32 top = 0;
    i32 right = 0;
    i32 bottom = 0;
};

// custom.ba's gameplay-relevant leg/arm metadata. Foot positions matter
// because death scorch marks share the gameplay object allocation stream.
struct LegDef {
    f32 x = 0.0f, y = 0.0f;
    f32 lifting_height = 2.0f;
    f32 target_height = 0.0f;
    bool height_relative = true;
    f32 end_direction = 0.0f;
    bool lock_movement = false;
    f32 prediction_multiplier = 1.0f;
    bool check_neighbours = true;
    bool favour_opposite_side = false;
    bool hidden = false;
    bool explode_on_death = true;
    f32 move_speed = 1.0f;
    f32 move_warmup_frames = 0.0f;
    f32 rotate_speed = 3.0f;
    f32 height_speed = 0.3f;
    f32 hold_distance_min = 7.0f;
    i32 max_moving_legs = 3;
    bool move_only_if_furthest = true;
    f32 hold_distance_max = 16.0f;
    f32 hard_limit = 50.0f;
    Vec<i32> neighbours;
};

struct CustomAutoTriggerDef {
    Str action_id;
    // Supported literal self.overWater()/not self.overWater() predicates.
    bool when_over_water = true;
    i32 check_interval_frames = 1;
};

struct UnitDef {
    Str name;
    i32 cost = 0;
    f32 hp = 100.0f;
    f32 speed = 0.0f;
    f32 range = 0.0f;
    f32 damage = 2.0f;
    f32 income = 0.0f;
    f32 shield = 0.0f;
    f32 radius = 30.0f;
    f32 mass = 1.0f;
    f32 build_time = 20.0f;
    i32 cooldown_max = 60;
    MoveType move_type = MoveType::LAND;
    WeaponType weapon_type = WeaponType::ATTACK;
    bool is_building = false;
    f32 splash_radius = 0.0f;

    f32 sight_range = 200.0f;
    i32 ammo_max = 1;
    i32 ammo_reload_ticks = 300;
    f32 energy_per_shot = 0.0f;
    bool targets_air = true;
    bool targets_ground = true;
    Str upgrade_to;
    i32 cargo_capacity = 0;
    bool is_nuke = false;
    bool is_antinuke = false;
    f32 nuke_range = 0.0f;

    f32 shield_mult = 1.0f;
    f32 shield_absorb = 1.0f;
    f32 hp_mult = 1.0f;
    f32 building_damage_mult = 1.0f;
    bool is_melee = false;
    bool is_beam = false;
    i32 chain_count = 0;
    f32 chain_range = 0.0f;
    bool is_flame = false;
    f32 flame_expand_rate = 0.0f;
    Vec<Str> can_build;
    Str built_from;
    i32 tech_level = 1;
    // Build progress per game frame, matching as.D() in game-lib.jar.
    f32 build_speed = 0.001f;
    // am.cE (the custom ammo resource) defaults to zero. Stock weapon fire
    // does not consume it when ammo_max is one; missile launchers override
    // both the capacity and explicit initial amount.
    i32 initial_ammo = -1;

    // Projectile values sampled from game-lib.jar's game.f instances.
    f32 splash_damage = 0.0f;
    f32 projectile_speed = -1.0f;      // world units per second; -1 uses engine default
    f32 projectile_lifetime = -1.0f;    // simulation frames; -1 uses engine default
    bool projectile_tracking = false;
    bool projectile_hitscan = false;
    bool can_move = true;
    bool airborne = false;

    // Units with multiple JAR weapon slots keep their per-slot behavior here.
    // Empty means the legacy single-weapon fields above are sufficient.
    Vec<WeaponDef> weapons;
    f32 max_energy = 0.0f;
    f32 initial_energy = 0.0f;
    f32 energy_regen_rate = 0.0f;
    // Retain the parsed frame rates: converting to seconds and back changes
    // the float bits used by custom.j's cB += rate * deltaFrames.
    f32 energy_regen_per_frame = 0.0f;
    f32 energy_recharge_regen_per_frame = 0.0f;
    bool energy_needs_full_recharge = false;
    bool start_energy_at_zero = false;
    bool die_on_zero_energy = false;

    // Explicit action price from game-lib.jar.  Most canonical upgrades cost
    // the difference between the two unit prices, but the stock anti-air
    // upgrade is priced independently (1200 vs. 1700-600=1100).
    i32 upgrade_cost = 0;

    // game.f target and damage modifiers, sampled from the projectile
    // produced by each stock JAR weapon.
    f32 air_damage_mult = 1.0f;
    // Kept for API compatibility; stock game.f has no experimental-target
    // multiplier.  The ai field is the building multiplier below.
    f32 experimental_damage_mult = 1.0f;
    bool is_experimental = false;
    // Shield points restored per real-time second at speed 1.0.
    f32 shield_regen_rate = 0.0f;
    // Preserve custom.as.h before scaling it to the public per-second rate.
    f32 shield_regen_per_frame = 0.0f;
    // custom.l.cM: creation keeps an empty shield; conversion preserves its
    // current points, clamping only to the new maximum instead of scaling.
    bool start_shield_at_zero = false;
    // game.units.y.bh(): minimum-clearance penalty threshold stored into
    // path request k.k.q. Zero for all canonical units except
    // experimentalTank (units.e.d.bh() == 1).
    i32 path_min_clearance = 0;
    // JAR cargo weight (cw()). Most units weigh one; experimental units weigh five.
    i32 cargo_weight = 1;
    // Result of the stock transport filter for canonical unit types.
    bool transportable = false;

    // Stock y movement/turning values, expressed per JAR simulation frame.
    // Kept after the legacy aggregate fields so old template initializers do
    // not shift when these runtime-only properties are added.
    f32 turn_speed = 0.0f;
    f32 turn_acceleration = -1.0f;
    f32 move_acceleration = 99.0f;
    f32 move_deceleration = 99.0f;
    bool strafes = false;
    // custom.l.dY / y.bj(): movement uses the requested heading rather than
    // the body rotation. This is independent of sliding-vector movement.
    bool move_ignores_body = false;
    // custom.l.ei / y.bg(): participate in command formation grouping.
    bool joins_group_formations = true;
    // y.bc(): maximum backward speed as a fraction of z().
    f32 reverse_speed_multiplier = 0.6f;
    // y.P's constructor default, with the stock ladybug override retained.
    Behavior behavior = Behavior::ONLY_IN_RANGE;

    // Writable custom-unit stats from custom.as. Values expressed as rates
    // use the JAR's per-simulation-frame convention and are converted when
    // copied into Unit's per-second runtime fields.
    f32 armour = 0.0f;
    f32 armour_min_damage = 1.0f;
    f32 shoot_delay_multiplier = 1.0f;
    f32 shoot_damage_multiplier = 1.0f;
    // custom.as.o is an optional runtime override. -1 delegates to the
    // unit class' y() implementation (85 for ordinary orderable units).
    i32 nano_range = -1;
    // custom.j.aT(): -1 uses the first turret for service aiming.
    i32 nano_turret_index = -1;
    // am.b(am) / custom.l.bf multiplies the target type's construction rate.
    f32 nano_build_speed = 1.0f;
    f32 self_regen_rate = 0.0f;
    f32 target_height = 0.0f;
    f32 nano_factory_speed = 1.0f;
    f32 target_height_drift = 0.0f;
    f32 height_change_rate = -1.0f;
    f32 falling_acceleration = 0.03f;
    // custom.l.dQ/dR: [movement] landOnGround=false/true/onlyIdle.
    bool land_on_ground = false;
    bool land_on_ground_only_idle = false;
    // custom.l.dP / [movement] ignoreMoveOrders. Buildings also reject the
    // MOVE-family waypoint during their next order dispatch.
    bool ignore_move_orders = false;
    i32 base_nano_range = 85;
    // am.ak(): stock type's builder role or custom.l.fp ([core] isBuilder).
    bool is_builder = false;
    bool can_repair = false;
    bool can_repair_any = false;
    // custom.l.aW installs the idle automatic repair plugin.
    bool auto_repair = false;
    // custom.j.c(am) reads custom.l.bb, in HP per game frame.
    f32 nano_repair_speed = 0.2f;
    bool can_reclaim = false;
    // custom.l.aU/aV/fk are independent. The true defaults retain the
    // stock builder's building repair and resource reclaim capabilities.
    bool can_repair_buildings = true;
    bool can_reclaim_resources = true;
    // am.q() / custom.l.aK excludes environmental and unrepairable targets.
    bool can_be_repaired = true;
    // custom.l.aJ/aL: as.p() and as.a(builder) affect the nano reach of
    // builders servicing this type, independently of its footprint.
    bool place_only_on_res_pool = false;
    f32 extra_build_range_when_building_this = 0.0f;
    bool hovercraft_unload_style = false;
    // custom.l.fc/eT/eN: automatic unload gate, menu visibility and cadence.
    Str transport_unload_condition = "not self.isoverliquid() and not self.ismoving()";
    bool transport_add_unload_option = true;
    f32 transport_unload_delay = 30.0f;
    TileRect collision_footprint;
    TileRect placement_footprint;
    // custom.j factory output placement, supplied by the host-side INI
    // loader. The core never reads asset files itself.
    bool has_factory_exit_config = false;
    f32 factory_exit_x = 0.0f;
    f32 factory_exit_y = 9.0f;
    f32 factory_exit_move_away_amount = 70.0f;
    bool factory_exit_has_dir_offset = false;
    f32 factory_exit_dir_offset = 0.0f;
    f32 factory_exit_height_offset = 0.0f;
    bool factory_exit_height_ignore_parent = false;
    bool factory_keep_built_units = false;
    // custom.j.cZ()/da() return custom.l.da/db, the INI
    // buildingToFootprintOffsetX/Y. Stock am.cZ()/da() return the half tile
    // b.b.p/q, which is this same default.
    f32 building_to_footprint_offset_x = 10.0f;
    f32 building_to_footprint_offset_y = 10.0f;
    Map<Str, CustomActionDef> custom_actions;
    Vec<CustomAutoTriggerDef> auto_triggers;
    f32 auto_trigger_cooldown_frames = 60.0f;
    f32 income_interval = 10.0f;

    // y.o(target) and the ATTACK-follow radius: with
    // attack_range_adds_radii (custom.l.eF / y.aV()) o() adds both collision
    // radii; otherwise o() is the plain weapon range. attack_follow_e_flag
    // (custom.l.ey / y.E()) is isFixedFiring and suppresses all follow-radius
    // adjustments. Otherwise attack_follow_weapon_n (custom.bn.n / y.e(0))
    // adds a -3 shrink when above 5. Stock weapons supply their warmup too.
    // Trailing fields keep the positional reg_() initializers above intact.
    bool attack_range_adds_radii = false;
    bool attack_follow_e_flag = false;
    f32 attack_follow_weapon_n = 0.0f;
    // custom.l.ec / y.be(): bombers fly through the stopping radius and
    // preserve an approach heading while retreating to recharge.
    bool bomber_attack_movement = false;
    // custom.l.ef / custom.j.bf(): suppress new passive main-target scans.
    // Retained main targets and independent turret refreshes remain active.
    bool disable_passive_targeting = false;
    // custom.l.eh defaults to false: ordinary custom turrets share y.R.
    bool turret_multi_targeting = false;
    // CustomUnitMetadata softCollisionOnAll. Zero keeps the stock immediate
    // separation rate; positive values spread overlap correction over time.
    i32 soft_collision_on_all = 0;
    // custom unit slowDeathFall: retain horizontal sliding while descending.
    // Keep this trailing so existing positional UnitDef initializers remain
    // source-compatible.
    bool slow_death_fall = false;
    // Death-effect flags supplied by the host-side asset loader.
    bool explode_on_death_ground_collision = true;
    bool hide_scorch_mark = false;
    // Number of game-object IDs consumed by the configured effectOnDeath.
    // The host asset layer resolves effect names; the core only receives the
    // resulting deterministic allocation count.
    i32 death_effect_uid_count = 0;
    // custom.j.e() keeps a completed custom wreck when graphics:image_wreak
    // is configured. The core receives only this metadata; it never loads the
    // image asset itself.
    bool has_wreck_image = false;
    // Host-resolved custom replacement used to preserve stock factory tier
    // availability while producing the custom definition and charging it.
    Str override_and_replace;
    // custom.ag.a(as) appends every builtFrom entry to factory menus from
    // the product's tech level onward, independently of replacement slots.
    Vec<Str> built_from_factories;
    // Host-resolved custom.j.e()/y.a(ab,true) smoke and scorch branch.
    // Applies to completed units dying on land, independently of a wreck image.
    bool default_ground_death_effect = false;
    Vec<LegDef> legs;
    i32 leg_rotation_slot = -1;
    bool leg_death_scorch = true;
    // custom.l.em ([attack] canAttack) is independent of active waypoints.
    bool can_attack = true;
    // custom.l.bB names the turret fired at a completed unit's death point.
    i32 fire_turret_on_death = -1;
    bool nuke_on_death = false;
    f32 nuke_on_death_range = 250.0f;
    f32 nuke_on_death_damage = 5400.0f;
    bool nuke_on_death_disable_when_no_nuke = false;
    bool targets_underwater = false;
    // Supported attack.canAttackLandUnits: if not self.flying predicate.
    bool ground_attack_disabled_while_flying = false;
    bool targets_not_touching_water = true;
    // Positional build actions select a product and its construction tier.
    Map<Str, Vec<i32>> build_tech_levels;
    // Loaded custom menus distinguish factory buttons from forceNano
    // waypoints, including mobile products which can appear in both menus.
    bool has_declared_build_menu = false;
    Vec<Str> factory_products;
    // ar.d(tier) includes a stock factory's upgrade in its build price.
    Map<i32, i32> construction_tech_costs;
    // custom.j.z(am)/f(am), resolved from custom.l.bc/bd/be.
    f32 nano_reclaim_speed = 1.02f;
    f32 resource_reclaim_multiplier = 1.0f;
    f32 nano_unbuild_speed = 0.0051f;
};

class UnitRegistry {
public:
    void register_builtin_units();
    const UnitDef* get(const Str& name) const;
    // Register a definition supplied by the host language.  The simulation
    // core deliberately does not read files; callers own parsing and can
    // pass either canonical or custom definitions here.
    bool register_unit(const UnitDef& definition, bool custom = false);
    UnitDef* register_custom_unit_from(const Str& name, const Str& base_name);
    bool register_custom_action(const Str& unit_name,
                                const CustomActionDef& action);
    const CustomActionDef* get_custom_action(const Str& unit_name,
                                             const Str& action_id) const;
    bool is_custom(const Str& name) const;
    Vec<Str> all_names() const;
    bool is_building(const Str& name) const;
    i32 get_cost(const Str& name) const;
    i32 get_build_cost(const Str& name, i32 tech_level) const;
    f32 get_income(const Str& name) const;
    f32 get_build_speed(const Str& name) const;
    bool has_upgrade(const Str& name) const;
    Str get_upgrade_target(const Str& name) const;
    bool can_build_unit(const Str& factory_name, const Str& unit_name,
                        i32 build_tech_level = -1) const;
    const Vec<Str>& get_can_build(const Str& name) const;
    // Map-only aliases and stock fallback objects are intentionally excluded
    // from all_names(), which remains the exact game.units.ar enum surface.
    const UnitDef* get_map_unit(const Str& name) const;

private:
    Map<Str, UnitDef> defs_;
    Map<Str, UnitDef> map_defs_;
    Map<Str, UnitDef> custom_defs_;
    void reg_(const UnitDef& d);
};

}
