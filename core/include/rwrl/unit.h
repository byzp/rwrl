#pragma once
#include <rwrl/common.h>
#include <rwrl/enums.h>
#include <rwrl/constants.h>
#include <rwrl/unit_def.h>
#include <array>

namespace rwrl {

struct PathLayerSnapshot;

struct WeaponSlot {
    u32 target_uid = 0;
    i32 target_team_id = -1;
    f32 damage = 0.0f;
    f32 splash_damage = 0.0f;
    f32 splash_radius = 0.0f;
    f32 projectile_speed = 0.0f;
    f32 projectile_speed_spread = 0.0f;
    f32 projectile_wobble_amplitude = 0.0f;
    f32 projectile_wobble_frequency_frames = 5.0f;
    f32 projectile_target_speed = -1.0f;
    f32 projectile_acceleration = 0.1f * EngineConstants::FRAME_RATE;
    f32 projectile_lifetime = 0.0f;
    f32 splash_expand_time = 0.0f;
    bool splash_friendly_fire = false;
    f32 projectile_unguided_speed_height = 0.0f;
    f32 projectile_unguided_speed_x = 0.0f;
    f32 projectile_unguided_speed_y = 0.0f;
    f32 projectile_gravity = 0.0f;
    f32 projectile_true_gravity = 0.0f;
    bool projectile_retarget_on_death = false;
    f32 projectile_retarget_range = 120.0f;
    f32 projectile_retarget_lead = 15.0f;
    f32 projectile_deflection_power = 1.0f;
    Vec<Str> projectile_tags;
    bool projectile_should_reveal_fog = false;
    i32 cooldown_ticks = 0;
    i32 cooldown_max = 60;
    f32 fire_delay_frames = -1.0f;
    f32 energy_usage = 0.0f;
    bool laser_defence_enabled = false;
    f32 laser_defence_energy_usage = 0.0f;
    Vec<Str> intercept_projectile_tags;
    f32 intercept_ground_target_distance = -1.0f;
    f32 intercept_projectile_distance = 2000.0f;
    f32 intercept_projectile_height = -999.0f;
    i32 intercept_ammo_usage = 0;
    i32 intercept_required_flags = 0;
    i32 intercept_missing_flags = 0;
    f32 limiting_range = 99999.0f;
    f32 limiting_min_range = 0.0f;
    f32 limiting_angle = -1.0f;
    // y.G(slot) shifts the weapon origin during the reload recoil window.
    f32 recoil_offset = 0.0f;
    f32 recoil_out_frames = 0.0f;
    f32 recoil_return_frames = 0.0f;
    f32 cooldown_remainder = 0.0f;
    bool tracking = false;
    bool hitscan = false;
    bool persist_after_hit = false;
    bool instant_reuse_last = false;
    bool instant_reuse_last_change_turret_aim = false;
    bool instant_reuse_last_keep_area_damage_list = false;
    bool move_with_parent = false;
    f32 sweep_offset = 0.0f;
    f32 sweep_offset_from_target_radius = 0.0f;
    bool projectile_flame_weapon = false;
    bool laser_defence_excluded = false;
    bool splash_same_layer = true;
    bool targets_air = true;
    bool targets_ground = true;
    bool targets_underwater = true;
    bool targets_not_touching_water = true;
    f32 shield_mult = 1.0f;
    f32 shield_absorb = 1.0f;
    f32 hp_mult = 1.0f;
    f32 building_damage_mult = 1.0f;
    f32 air_damage_mult = 1.0f;
    f32 experimental_damage_mult = 1.0f;
    f32 warmup_frames = 0.0f;
    f32 warmup_progress = 0.0f;
    u32 warmup_target_uid = 0;
    // Per-frame equivalent of JAR ap.g. It is reset at the start of combat,
    // set only when this slot (or its linked parent) is aimed successfully,
    // and gates warmup accumulation for linked turrets.
    bool aim_ready = false;
    // y.b() computes one unit-level warmup hold from any aimed slot whose
    // reload was zero before firing. This is transient and is consumed by the
    // combat pass's post-slot warmup decay.
    bool warmup_hold_this_frame = false;
    bool warmup_no_reset = false;
    f32 warmup_call_down_rate = 4.0f;
    f32 warmup_shoot_delay_transfer = 0.0f;
    bool fire_enabled = true;
    i32 linked_slot = -1;
    i32 attached_to_slot = -1;
    i32 delay_link_slot = -1;
    f32 turret_angle = 0.0f;
    // ap.b snapshots the last independently aimed angle. ap.d delays the
    // automatic return to the weapon's idle direction.
    f32 turret_previous_angle = 0.0f;
    f32 turret_turn_velocity = 0.0f;
    f32 turret_idle_timer = 0.0f;
    f32 turret_turn_speed = 999.0f;
    f32 turret_turn_acceleration = -1.0f;
    f32 turret_turn_deceleration = -1.0f;
    bool turret_resets_to_idle = true;
    f32 turret_idle_direction = 0.0f;
    f32 turret_idle_direction_reversing = 0.0f;
    // custom.j idle turret configuration and ap.k/ap.l sweep state.
    f32 turret_idle_spin = 0.0f;
    f32 turret_idle_sweep_angle = 0.0f;
    f32 turret_idle_sweep_delay = 10.0f;
    f32 turret_idle_sweep_speed = 0.0f;
    f32 turret_idle_sweep_random_delay = -1.0f;
    f32 turret_idle_sweep_random_angle = 0.0f;
    bool turret_idle_sweep_requires_stationary = false;
    f32 turret_sweep_timer = 0.0f;
    f32 turret_sweep_direction = 0.0f;
    f32 aim_tolerance = 5.0f;
    f32 aim_offset_spread = 0.6f;
    f32 aim_offset_x = 0.0f;
    f32 aim_offset_y = 0.0f;
    f32 muzzle_offset = 0.0f;
    f32 muzzle_side = 0.0f;
    f32 muzzle_side_odd = 0.0f;
    // y.a(delta,target,slot) toggles ap.m after firing and refreshing spread.
    bool odd_shot = false;
    f32 mount_forward = 0.0f;
    f32 mount_side = 0.0f;
    bool projectile_uses_muzzle = true;
    bool height_guided = false;
    bool nuke_weapon = false;
    f32 launch_height = 0.0f;
    f32 cruise_height = 0.0f;
    f32 vertical_speed = 0.0f;
    bool fixed_turret = false;
    // custom.l.dL / [attack] turretRotateWithBody: y.i() carries the body
    // turn into turret angles only when this flag is enabled.
    bool turret_rotate_with_body = true;
    bool fixed_target = false;
    bool fixed_target_include_height = false;
    bool disable_lead_targeting = false;
    f32 lead_targeting_speed = -1.0f;
    f32 ground_target_height_offset = 0.0f;
    f32 ground_target_spread = 0.0f;
};

struct UnitOrderPrepath {
    u32 unit_uid = 0;
    f32 start_x = 0.0f;
    f32 start_y = 0.0f;
    f32 target_x = 0.0f;
    f32 target_y = 0.0f;
    i32 frame = -99999;
    MoveType move_type = MoveType::NONE;
    Vec<std::pair<f32, f32>> path;
};

struct UnitOrder {
    WeaponType type = WeaponType::MOVE;
    f32 x = 0.0f;
    f32 y = 0.0f;
    u32 target_uid = 0;
    i32 target_team_id = -1;
    Str unit_type;
    Str action_id;
    i32 build_tech_level = 1;
    // au.i points at the shared ab formation controller created for one
    // selected command. The controller can outlive its original leader: y.aD
    // finds it through a surviving follower and asks ab.c() to regroup.
    u64 formation_group_id = 0;
    // au.j is the command flag. Secondary ab controllers retain their own
    // default f=false when ab.c() creates them, even with au.j=true.
    bool relaxed_formation = false;
    bool formation_controller_relaxed_formation = false;
    // au.k: zero confines an automatic service order to nano range;
    // the ordinary player-issued waypoint uses -1 and can chase its target.
    f32 service_chase_range_extra = -1.0f;
    f32 max_time_frames = -1.0f;
    // au.m marks temporary service waypoints inserted by automatic repair.
    // A queued order takes priority after this waypoint's next service pulse.
    bool automatic = false;
    // gameFramework.e.b(): MOVE/ATTACK_MOVE/ATTACK commands pre-solve a
    // grid path with prefer-clearance weighting at command time. y.a()
    // adopts it while under 180 frames old with the target within 10 world
    // units and a matching move type (the per-order path cache ab.g).
    bool has_prepath = false;
    bool prepath_from_replay = false;
    // Native replay can expose an already-solved cache in the same object
    // turn only when the command replaced an empty order queue. Replacing an
    // active order, or adopting a same-frame cache, waits one update.
    bool replay_prepath_command_had_active_order = false;
    // Native replay needs the serialized selection size for a narrow
    // formation handoff; the current regrouped formation can be smaller.
    u8 replay_command_group_size = 0;
    u8 replay_command_group_index = 0;
    // AIR cannot adopt grid caches but still shares the selection's command
    // handoff. Preserve cache presence before per-movement-type filtering.
    bool replay_command_has_path_cache = false;
    // Large replay ATTACK_MOVE commands retain a pending request when they
    // carry a multi-node serialized cache.
    bool replay_command_has_long_prepath = false;
    // The native nine-node cache shape additionally suppresses the new
    // formation handoff path for the whole selected command.
    bool replay_command_has_handoff_cache = false;
    i32 prepath_frame = -99999;
    i32 prepath_attach_frame = -99999;
    f32 prepath_target_x = 0.0f;
    f32 prepath_target_y = 0.0f;
    MoveType prepath_move_type = MoveType::NONE;
    Vec<std::pair<f32, f32>> prepath;
    Vec<UnitOrderPrepath> replay_prepath_cache;
    // gameFramework.e.b: prefer_clearance is set when ae && ah>1
    // (formation with multiple units); the solver applies ×7 clearance
    // soft-cost weighting on both the prepath and runtime requests.
    bool prefer_clearance = false;
    // d.k's synthetic factory-exit MOVE advances to a queued rally point on
    // the following object update, unlike an ordinary completed MOVE.
    bool factory_exit = false;
};

struct PositionSnapshot {
    i32 tick = -1;
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 rotation = 0.0f;
};

// custom.b.i stores planted feet independently of the body's current pose.
struct LegState {
    f32 x = 0.0f, y = 0.0f;
    f32 height = 0.1f;
    f32 warmup_frames = 0.0f;
    f32 target_x = 0.0f, target_y = 0.0f;
    f32 distance_sq = 0.0f;
    f32 rotation = 0.0f;
    bool moving = false;
    bool over_liquid = false;
    bool reset = true;
    bool compressed_reset = false;
};

struct Unit {
    u32 uid = 0;
    // All JAR game objects share one creation-ordered update list. Keep a
    // separate order from the unit UID so units and projectiles can be
    // interleaved exactly like gameFramework.w.er.
    u64 object_creation_order = 0;
    // am.bC is a per-unit salt consumed by deterministic visual/gameplay
    // random helpers. It is independent of the owning team.
    i32 deterministic_random_salt = 0;
    Str unit_type;
    i32 team_id = 0;
    bool custom_unit = false;
    i32 custom_flags = 0;

    f32 x = 0.0f, y = 0.0f;
    f32 radius = 30.0f;
    f32 rotation = 0.0f;
    // Native replay path-cache construction runs six ticks ahead of the
    // command's visible order. Retain a short history for that read-ahead.
    Vec<PositionSnapshot> position_history;
    f32 mass = 1.0f;
    i32 soft_collision_on_all = 0;
    f32 armour = 0.0f;
    f32 armour_min_damage = 1.0f;

    f32 hp = 100.0f, max_hp = 100.0f;
    f32 shield = 0.0f, max_shield = 0.0f;
    f32 shield_regen_rate = 0.0f;
    f32 shield_regen_per_frame = 0.0f;
    f32 shield_regen_timer = 0.0f;
    // Equivalent to am.cz: while non-zero, incoming damage bypasses shields.
    f32 shield_disabled = 0.0f;
    f32 total_shield_absorbed = 0.0f;
    f32 total_hp_lost = 0.0f;
    // am.cC/cD: transient HP-loss overlay and its accelerating decay rate.
    f32 damage_smoothing = 0.0f;
    f32 damage_smoothing_velocity = 0.0f;

    f32 build_progress = 1.0f;
    // am.cn is written alongside am.cm by setBuilt.
    f32 build_progress_secondary = 1.0f;
    bool is_building = false;
    TileRect collision_footprint;
    TileRect placement_footprint;
    f32 building_to_footprint_offset_x = 10.0f;
    f32 building_to_footprint_offset_y = 10.0f;

    bool is_alive = true;
    bool is_dead = false;
    // Team surrender writes the native -1 HP sentinel before the next object
    // turn. Other engine paths also use -1 as a transient delete sentinel.
    bool surrender_death = false;
    // Set once World has detached the unit from income, pathing and spatial indexes.
    bool cleanup_done = false;
    // am.bE membership differs from the global object/wreck lifetime.
    // al.k() leaves a fallen tree in bE, while am.bu() and saved bV remove it.
    bool native_unit_list_member = true;
    // JAR am.bV objects whose e() returns true remain as wrecks; stock
    // buildings and map effects remove themselves during bv().
    bool death_persistent = true;
    i32 death_tick = 0;
    // am.bW records the native integer l.by clock for periodic wreck cleanup.
    i64 death_clock_ms = 0;
    // Tick on which am.a() queued this object for removal from the global
    // list. utility.o defers both adds and removes until the next bF() flush,
    // so a wreck cleared by y.br() is still observable for the rest of that
    // frame and only disappears from the following one.
    i32 list_removal_tick = -1;
    f32 death_age_frames = 0.0f;
    // b.b.h/i/k: persistent aircraft wrecks continue falling after bV is set.
    f32 crash_fall_velocity = 0.0f;
    bool crash_impacted = false;
    bool crash_over_water = false;
    // custom unit slowDeathFall keeps the sliding vector while the wreck
    // descends with the custom 0.017-per-frame acceleration.
    bool slow_death_fall = false;
    bool explode_on_death_ground_collision = true;
    bool hide_scorch_mark = false;
    i32 death_effect_uid_count = 0;
    bool has_wreck_image = false;
    bool default_ground_death_effect = false;
    Vec<LegDef> leg_definitions;
    Vec<LegState> legs;
    i32 leg_rotation_slot = -1;
    bool leg_death_scorch = true;
    f32 leg_previous_x = 0.0f, leg_previous_y = 0.0f;
    f32 leg_previous_height = 0.0f, leg_previous_rotation = 0.0f;
    i32 spawn_tick = 0;
    i32 spawn_clock_ms = 0;
    // al.d: palms (0) and the two ordinary tree sprite families (1/2).
    i32 tree_type = 1;
    // w.dK keeps objects created during the logic pass pending until the next
    // frame. They receive their pending-object update immediately, but are not
    // yet present in the committed unit list used by snapshots and collision.
    bool pending_global_commit = false;
    i32 last_damage_tick = 0;
    f32 last_damage_age_ms = -1.0f;
    u32 last_attacker_uid = 0;
    bool was_attacked = false;

    f32 income_rate = 0.0f;
    f32 income_timer = 0.0f;
    f32 income_interval = 10.0f;

    // Absolute l.by expiry ticks. "-1" is s.i, the wildcard action id.
    Map<Str, i32> action_cooldown_expiry_ticks;

    f32 damage = 2.0f;
    f32 shoot_delay_multiplier = 1.0f;
    f32 shoot_damage_multiplier = 1.0f;
    f32 weapon_range = 120.0f;
    // y.a(delta, target, ad, false) follow-radius adjustments. o(target) is
    // m() plus both radii when attack_range_adds_radii (y.aV()); the stop
    // radius keeps all allowances when attack_follow_e_flag (y.E()) is true;
    // otherwise it shrinks by 1, by 2 with radii, and by 3 when the slot-0
    // weapon e(i) exceeds 5.
    bool attack_range_adds_radii = false;
    bool attack_follow_e_flag = false;
    f32 attack_follow_weapon_n = 0.0f;
    bool bomber_attack_movement = false;
    i32 fire_turret_on_death = -1;
    bool nuke_on_death = false;
    f32 nuke_on_death_range = 250.0f;
    f32 nuke_on_death_damage = 5400.0f;
    bool nuke_on_death_disable_when_no_nuke = false;
    bool disable_passive_targeting = false;
    bool turret_multi_targeting = false;
    // Native y.aq/ar/as persist between bomber attack updates.
    f32 bomber_attack_heading = -999.0f;
    bool bomber_retreating = false;
    bool bomber_target_search_done = false;
    i32 cooldown_ticks = 0;
    i32 cooldown_max = 60;
    f32 cooldown_remainder = 0.0f;
    i32 ammo = 1;
    i32 ammo_max = 1;
    i32 ammo_reload_timer = 0;
    f32 ammo_reload_remainder = 0.0f;
    WeaponType weapon_type = WeaponType::ATTACK;
    u32 target_uid = 0;
    i32 target_team_id = -1;
    // Unarmed y objects also retain R across MOVE. Their default scheduling
    // type cannot identify that pointer, and service uses target_uid itself.
    u32 unarmed_main_target_uid = 0;
    i32 unarmed_main_target_team_id = -1;
    // y.S gates passive main-target scans after a target is lost or no
    // candidate is found. y.T gates independent multi-turret target filling.
    // Armed builders keep native y.R independently of the service waypoint
    // target stored by the construction/repair helpers.
    u32 service_combat_target_uid = 0;
    i32 service_combat_target_team_id = -1;
    // Native y.l/m are sampled during y.i(), before a later-created repair
    // target can move. Keep them separate from the physical path node.
    f32 service_order_target_x = 0.0f;
    f32 service_order_target_y = 0.0f;
    f32 target_scan_timer_frames = 0.0f;
    f32 target_refresh_timer_frames = 0.0f;
    f32 splash_radius = 0.0f;
    f32 splash_damage = 0.0f;
    f32 projectile_speed = 0.0f;
    f32 projectile_lifetime = 0.0f;
    bool projectile_tracking = false;
    bool projectile_hitscan = false;
    bool can_move = true;
    bool can_attack = true;
    bool ignore_move_orders = false;
    bool airborne = false;
    bool submerged = false;
    bool submerge_requested = false;
    f32 height = 0.0f;
    f32 frame_start_height = 0.0f;
    f32 vertical_speed = 0.0f;
    f32 flight_phase = 0.0f;
    // custom.j.s counts frames without am.cK, including retained orders.
    f32 custom_idle_frames = 0.0f;
    // Stock amphibiousJet.u folds the two weapon mounts toward the hull.
    f32 wing_fold_progress = 0.0f;
    bool targets_air = true;
    bool targets_ground = true;
    bool ground_attack_disabled_while_flying = false;
    bool targets_underwater = false;
    bool targets_not_touching_water = true;
    Vec<WeaponSlot> weapon_slots;
    // turret_flamethrower uses this deterministic 0..10 spread sequence.
    i32 weapon_shot_counter = 0;
    // experimentalHoverTank keeps and refreshes one game.f.E beam object.
    u32 active_beam_projectile_uid = 0;
    // custom.j.G holds one projectile reference per turret independently
    // of the current unit definition or the target selected on its next shot.
    std::array<u32, 31> reusable_projectile_uids{};

    f32 energy = 0.0f;
    f32 max_energy = 0.0f;
    f32 energy_per_shot = 0.0f;
    f32 energy_regen_rate = 0.0f;
    f32 energy_regen_per_frame = 0.0f;
    f32 energy_recharge_regen_per_frame = 0.0f;
    bool energy_needs_full_recharge = false;
    bool die_on_zero_energy = false;
    bool energy_disabled = false;
    f32 laser_scan_remainder = 0.0f;

    f32 move_speed = 0.0f;
    // custom.j.j is updated after movement, and retained while cK is false.
    f32 terrain_speed_multiplier = 1.0f;
    // custom.j.w and the constructor's one-update i flag.
    f32 auto_trigger_cooldown_frames = 0.0f;
    bool auto_trigger_first_update = true;
    f32 turn_speed = 0.0f;
    f32 turn_acceleration = -1.0f;
    f32 move_acceleration = 99.0f;
    f32 move_deceleration = 99.0f;
    bool strafes = false;
    bool move_ignores_body = false;
    bool joins_group_formations = true;
    f32 reverse_speed_multiplier = 0.6f;
    // y.cf/ci: signed forward-speed fraction and current reverse state.
    f32 move_speed_factor = 0.0f;
    bool reversing = false;
    // Angular velocity in degrees per simulation frame, matching y.ce.
    f32 turn_velocity = 0.0f;
    MoveType move_type = MoveType::LAND;
    f32 vel_x = 0.0f, vel_y = 0.0f;
    // y.bZ/ca: collision displacement queued after the global collision pass
    // and applied at the beginning of the unit's next object update.
    f32 collision_impulse_x = 0.0f, collision_impulse_y = 0.0f;
    // am.bT is serialized independently of the unit definition. Missing
    // placeholders can move while retaining a saved building's disabled
    // collision layer.
    bool collision_enabled = true;
    // y.ay/cb/aL/aI: the global collision pass refreshes a bounded neighbor
    // cache periodically, then resolves that cache whenever the unit moved.
    bool collision_moved = false;
    bool collision_repeat = false;
    bool collision_first_update = true;
    i32 collision_next_tick = -9999;
    i32 collision_spatial_cell_x = -1;
    i32 collision_spatial_cell_y = -1;
    i32 collision_spatial_entry_tick = -9999;
    Vec<std::pair<i32, u32>> collision_candidates;
    // y.aK is a persistent ten-float side array. JAR's insertion loop shifts
    // aJ references but deliberately leaves the corresponding ranks in place.
    Vec<f32> collision_candidate_ranks = Vec<f32>(10, 0.0f);
    // y.Z/aa: most recent overlapping unit and the logical tick of contact.
    // Movement can consume a waypoint covered by that unit for one tick.
    u32 collision_partner_uid = 0;
    i32 collision_partner_team_id = -1;
    i32 collision_partner_tick = -9999;
    f32 target_x = 0.0f, target_y = 0.0f;
    // y.l/y.m cache the live ATTACK target position during this unit's own
    // object update. The target can move later in the same global frame, so
    // resolving it again while taking a snapshot is observably one frame too
    // new for units earlier in creation order.
    f32 attack_order_target_x = 0.0f;
    f32 attack_order_target_y = 0.0f;
    // LOAD waypoints capture au.g()/h() before the target's object turn.
    f32 transport_order_target_x = 0.0f;
    f32 transport_order_target_y = 0.0f;
    // ATTACK_MOVE first proposes its positional waypoint, then y.a(delta,R,
    // ad,true) can replace y.l/y.m with the live passive-combat target for
    // this object update. Cache that proposal because the target can move
    // later in the same global frame.
    bool attack_move_combat_target_valid = false;
    // The live-target handler saw ad before the common controller removed
    // or rejected that leader. Recheck its private request in the same turn.
    bool attack_move_private_navigation_handoff = false;
    // y.j allows a chainless physical chase; a live k/l/m combat target
    // alone does not supply that proposal while a grid request is pending.
    bool attack_move_direct_chase_proposal = false;
    f32 attack_move_combat_target_x = 0.0f;
    f32 attack_move_combat_target_y = 0.0f;
    // y.j lets the short formation chase use live l/m without an av node,
    // even while an earlier aU request is still pending.
    bool direct_attack_chase_this_update = false;
    // GUARD can expose a straight path after aE() captured no old node.
    // That chain is not eligible for aJ() until the next object update.
    bool guard_path_created_without_node = false;
    // y.l/y.m keep the target chosen by GUARD's per-frame movement proposal.
    // This can differ from both the guarded unit and the main target selected
    // later in the same object update.
    bool guard_movement_target_valid = false;
    bool guard_movement_chases_main_target = false;
    // A GUARD recent-attacker proposal survives R clearing until the next
    // valid combat target or order replacement, matching JAR y.l/y.m.
    bool guard_movement_target_hold = false;
    // Set by a same-update GUARD replacement and consumed by the following
    // physical pass; this mirrors y.a()'s one-pass logical-distance tolerance.
    bool guard_logical_tolerance_pending = false;
    // An active GUARD replaced with a target already tracked by a weapon slot
    // keeps its movement controller live through the next order activation.
    bool guard_replacement_keeps_movement = false;
    // A non-queued GUARD command replaced a PATROL order before the next
    // object turn. Navigation consumes this once to replace the patrol path.
    bool guard_replacement_from_patrol = false;
    // A large replay GUARD replacement keeps its old path through the
    // activation frame, then replaces that path on the following object turn.
    bool guard_replacement_refresh_next_frame = false;
    u32 guard_movement_target_uid = 0;
    i32 guard_movement_target_team_id = -1;
    f32 guard_path_drift_threshold = 150.0f;
    f32 guard_movement_target_x = 0.0f;
    f32 guard_movement_target_y = 0.0f;
    bool has_move_target = false;
    // y.k() reads the preceding object's k; command-time az()/aH() leave it
    // unchanged. Keep that value before C++ command activation replaces the
    // proposal, including multiple commands before the same object turn.
    bool command_start_has_move_target = false;
    i32 movement_proposal_command_tick = -1;
    bool frame_start_has_move_target = false;
    bool arrived = true;
    // y.a(float) consumes a reached path node after selecting this frame's
    // movement request. The path count changes immediately, while physics
    // still uses the old node once before the replacement becomes active.
    bool has_deferred_navigation_target = false;
    // A due formation fallback refresh can overwrite av before aJ(). Keep
    // its proximity test in the physical pass, after that refresh runs.
    bool formation_fallback_consumption_pending = false;
    f32 deferred_navigation_target_x = 0.0f;
    f32 deferred_navigation_target_y = 0.0f;
    // An order can clear or change y.l/y.m while retaining its av[0] node.
    // Keep that physical node separate until the path is consumed or replaced.
    bool path_target_override_active = false;
    f32 path_target_override_x = 0.0f;
    f32 path_target_override_y = 0.0f;
    // y.a(float,ad,au,boolean) captures aw before aJ() can consume a path
    // node. Native movement is split into order and physical passes, so keep
    // that entry value until the latter applies endpoint slowdown.
    i32 movement_navigation_points = 0;
    // am.cK: whether the latest order update produced an active movement
    // request. Lead prediction deliberately uses this instead of velocity.
    bool movement_prediction_enabled = true;
    // Command-time path planning precedes the next native object update.
    // Its idle turret pass still reads cK from before that command.
    bool idle_prediction_before_command = false;
    i32 idle_prediction_command_tick = -1;

    // y.ad/ae/af/ah/ak/al/am/ao: command formation state. The public logical
    // waypoint remains unchanged while followers use a private leader slot.
    u32 formation_leader_uid = 0;
    bool formation_is_leader = false;
    // y.ac: the shared formation controller identity survives clearing the
    // current waypoint, so the collision pass can still apply the JAR's
    // same-formation order cancellation to an idle member.
    u64 formation_controller_id = 0;
    // A synchronous command-time path is hidden from earlier-created
    // followers until this leader reaches its own object update.
    bool formation_path_activation_pending = false;
    bool formation_preferred = false;
    i16 formation_group_size = 0;
    f32 formation_offset_x = 0.0f;
    f32 formation_offset_y = 0.0f;
    f32 formation_heading = 0.0f;
    f32 formation_distance_sq = 0.0f;
    f32 formation_idle_leader_frames = 0.0f;
    i32 formation_tick = 0;
    bool formation_tick_pending = false;
    // y.aG() is evaluated before the physical helper consumes av or W/X.
    // Preserve that decision across the split order and physical passes.
    bool formation_stall_clock_refresh_pending = false;
    // JAR ad.d: per-object-update gate for using the leader's projected path.
    // A linked follower can keep formation speed/reverse behavior while this
    // gate makes it steer along its own path for the current update.
    bool formation_projection_allowed = true;
    // JAR y.ai: frames since an ATTACK follower stopped using ad.d. It is
    // reset while leader projection remains active and held at 500 without a
    // leader; newly linked units therefore differ from followers that have
    // just approached their target.
    // New units start with no ATTACK handoff age. An unlinked ATTACK update
    // changes this to the 500-frame sentinel; keeping the sentinel as the
    // constructor default makes a unit's first formation ATTACK advance from
    // 500 to 501 instead of matching JAR's 0-to-1 transition.
    f32 formation_projection_disabled_frames = 0.0f;
    // y.c: temporary fallback timer used when a formation path is invalid.
    f32 formation_path_fallback_frames = 0.0f;
    // y.k() reads cf before dispatch changes the movement proposal. Keep
    // that countdown gate across the separate order and physical passes.
    bool formation_fallback_countdown_active = false;
    // y.d: time spent with a stale formation or repeated tile collisions.
    // Crossing its thresholds temporarily switches the follower from the
    // leader's projected path to a private recovery path.
    f32 formation_path_failure_frames = 0.0f;
    // A consumed final av node clears c/d after the physical helper has
    // selected and used this turn's fallback movement context.
    bool formation_timers_clear_after_movement = false;
    // A late service completion can expose the follower's next waypoint
    // after its movement update. JAR drops the stale leader link on the
    // follower's next object turn.
    bool formation_rebuild_path_pending = false;
    // Captured before a replacement command clears the old path; promoted
    // only if that command actually enters a leader-removal rebuild.
    bool formation_rebuild_reverse_coast_candidate = false;
    // A leader-removal MOVE rebuild can retain the former reverse coast for
    // the follower's next physical pass, before normal formation rules apply.
    bool formation_rebuild_reverse_coast_pending = false;
    // A leader-removal rebuild defers ATTACK_MOVE's order-point handoff until
    // its retained combat target is invalidated in the combat phase.
    bool formation_rebuild_handoff_pending = false;
    bool clear_exposed_move_next_frame = false;

    // JAR y.g stores the active waypoint at index 0 followed by up to 28
    // queued typed waypoints. Navigation waypoints below are path nodes only.
    Vec<UnitOrder> order_queue;
    // Engine.tick may run a zero-delta command phase before physical movement.
    i32 order_phase_tick = -1;
    f32 active_order_elapsed_frames = 0.0f;
    // y.Y: the frames the unit has spent within 41 units of the current
    // waypoint's own point. The MOVE/attackMove/patrol dispatch widens its
    // arrival tolerance from 7 to 16 at 240 frames and to 36 at 340.
    f32 order_point_dwell_frames = 0.0f;
    // A newly activated positional waypoint is visible immediately, but
    // y.a(float) does not apply its physical turn/movement until next frame.
    f32 movement_activation_delay_frames = 0.0f;
    // BUILD activation can submit its generic request before the first
    // object dispatch. Preserve that request's charge once; loaded objects
    // and later dispatches apply y.f()'s travelling cooldown cap.
    bool build_path_request_awaits_dispatch = false;
    // Grid paths requested while following a live target are solved through
    // k.l's delayed request queue. The request captures its start state; y
    // exposes the logical target while av/aw remain empty until completion.
    bool path_request_pending = false;
    // y.t is cleared by k() and set only when PATROL publishes its ordered
    // point. A recent attacker can publish j while a nearby patrol stays
    // stopped; that request keeps the normal worker budget.
    bool path_request_low_priority = false;
    // Some linked ATTACK target-size requests expose k.l's result for one
    // object update before y.b(l) adopts the av/aw chain.
    bool path_request_solver_ready = false;
    // A formation promotion's compensated countdown reaches zero before
    // k.l publishes the result. Retain one worker-to-object handoff;
    // ordinary request budgets already include that wait.
    bool path_request_needs_worker_handoff = false;
    // A formation leader removed after its former follower's object turn can
    // submit the replacement ATTACK request at finish_step. Keep cK false
    // while that delayed request has no adopted path yet.
    bool path_request_prediction_suppressed = false;
    // The JAR formation handoff submits a target-sized grid request even when
    // the unweighted direct-line probe would otherwise succeed.
    bool force_grid_path_request = false;
    // A command-time solver request can finish before the order consumes it.
    // The cached result is adopted through the same delayed path hand-off as
    // JAR k.l/y.b instead of becoming visible in the submission call.
    bool path_request_uses_prepath = false;
    // aU owns the chosen command-cache result even when az() replaces au.
    // Keep it apart from the current waypoint's mutable cache selection.
    Vec<std::pair<f32, f32>> path_request_prepath;
    bool path_request_prepath_from_replay = false;
    // y.b(l) reads aU before y.a(FFIZZ) can select a new cached result.
    // A cache selected during this turn waits for the next object update,
    // independently of the update's simulation-frame delta.
    i32 path_request_prepath_submission_tick = -1;
    f32 path_request_delay_frames = 0.0f;
    f32 path_request_start_x = 0.0f;
    f32 path_request_start_y = 0.0f;
    f32 path_request_goal_x = 0.0f;
    f32 path_request_goal_y = 0.0f;
    f32 path_request_rotation = 0.0f;
    // k.k.k captures bb() || ci when the request is submitted. A later
    // steering change cannot replace the solver's initial turn-cost table.
    bool path_request_flexible_reverse = false;
    // y.n is the current controller's allowance, including turns that only
    // project a formation. A submitted k.n stays fixed when y.n changes.
    i32 path_request_target_size = 0;
    i32 path_request_solver_target_size = 0;
    bool path_request_prefer_clearance = false;
    // k.k.e() captures the current path-layer arrays when a delayed request
    // is submitted. Later layer refreshes replace those arrays, so an
    // in-flight solver must keep using the submission-time snapshot.
    SP<const PathLayerSnapshot> path_request_cost_snapshot;
    // Runtime GUARD refresh requests (y.c chase turns feeding y.a's grid
    // branch) leave au/aw untouched: the adopted path stays active and
    // movement continues while k.l solves the request. Command-time
    // deferred requests instead coast with no active path until adoption.
    bool path_request_preserves_active_path = false;
    // Native command submission clears the old order before the next object
    // update. Keep whether an AIR ATTACK replacement had an active av/aw
    // chain so the follower can replace that chain instead of taking the
    // projection-only shortcut.
    bool attack_order_had_active_air_path = false;
    // An AIR ATTACK replacement with an existing weapon main target keeps a
    // private path even when the old order had no path nodes.
    bool attack_order_had_main_target = false;
    // JAR y.o/y.p: destination of the last movement request. The runtime
    // refresh trigger compares the current proposal against this point.
    // y.r defaults to 150 each update; live-follow handlers replace it with
    // 18 before the generic controller caps y.s at 30 and resubmits at zero.
    f32 last_request_x = 0.0f;
    f32 last_request_y = 0.0f;
    Vec<std::pair<f32, f32>> waypoints;
    i32 current_waypoint = 0;
    bool using_path = false;
    bool has_patrol = false;
    // JAR keeps y.l/y.m at the patrol waypoint until passive combat actually
    // enters the out-of-range chase branch; target_uid is set earlier.
    bool patrol_logical_target_active = false;
    f32 patrol_logical_target_x = 0.0f;
    f32 patrol_logical_target_y = 0.0f;
    Vec<std::pair<f32, f32>> patrol_points;
    i32 patrol_index = 0;
    f32 guard_x = 0.0f, guard_y = 0.0f;
    bool has_guard_pos = false;
    u32 guard_target_uid = 0;
    i32 guard_target_team_id = -1;
    // y.w latches GUARD's return state and gates its own q(2) fallback.
    bool guard_combat_chase_outer = false;
    u32 follow_target_uid = 0;
    i32 follow_target_team_id = -1;
    bool recent_order_chase = false;

    u32 parent_uid = 0;
    // y.bR/bS: recently unloaded units ignore their transport for 45 frames.
    u32 collision_ignore_uid = 0;
    // am.bQ: this turn's LOAD or recently unloaded transport exclusion;
    // retained for the collision pass even when bS expires and clears bR.
    u32 collision_approach_target_uid = 0;
    f32 collision_ignore_frames = 0.0f;
    Vec<u32> cargo_units;
    i32 cargo_capacity = 0;
    Str transport_unload_condition = "not self.isoverliquid() and not self.ismoving()";
    bool transport_add_unload_option = true;
    f32 transport_unload_delay = 30.0f;
    bool hovercraft_unload_style = false;
    i32 cargo_weight = 1;
    // y.bh() minimum-clearance threshold copied into path requests (k.k.q).
    i32 path_min_clearance = 0;
    // y.u: the current path was truncated (escape move capped at 60 units,
    // or a future >120-node truncation). Consuming its last waypoint
    // re-requests a path toward the ordered destination instead of
    // completing the order.
    bool path_incomplete = false;
    // A truncated final node is removed one update before y.a() requests the
    // continuation path from the unit's new position.
    bool path_retry_next_frame = false;
    bool movement_delay_keeps_prediction = false;
    // A service or new-order handoff can retain its previous reverse state
    // while coasting. The physical C()/D() rate is chosen from the speed sign.
    bool movement_delay_uses_acceleration = false;
    // A service path's final node remains the physical target for one pass;
    // the next object update clears it before requesting the next path.
    bool service_final_waypoint_coast = false;
    // A single-node PATROL dispatch can clear k/l/m without consuming av/aw.
    // Keep the old speed factor for the physical pass so it decelerates once.
    bool patrol_soft_stop_coast = false;
    // A final PATROL node keeps its passive-target proposal for the current
    // object turn, then coasts one more physical pass with prediction off.
    bool patrol_final_node_coast_pending = false;
    // A final ATTACK node keeps its transient movement proposal through the
    // consuming update. The following object turn clears it while the chase
    // request remains in cooldown.
    bool attack_final_node_coast_pending = false;
    // A short formation ATTACK_MOVE leader keeps its live combat proposal
    // after consuming the final route node, then coasts on the next object
    // turns until its chase cooldown permits a replacement path.
    bool attack_move_final_node_coast_pending = false;
    // y.i() republishes k/l/m for the service point on every object update.
    // While the unit is still steering off a formation leader that proposal
    // is all it has: no aU request was made for it, so it must not stand in
    // for one when the projection later ends.
    bool service_projection_proposal = false;
    // y.W / y.X: frames spent trying to move since the last normal waypoint
    // consumption, and the previous value of that counter.
    f32 waypoint_stall_frames = 0.0f;
    f32 last_waypoint_stall_frames = 0.0f;
    // Tick on which the order pass took the next chain node. y.aJ() runs in
    // the JAR's physical pass, after its waypoint dispatch, so a dispatch
    // that suspends the proposal has to be able to put the node back.
    i32 waypoint_consumed_tick = -1;
    // y.b / y.a: frames spent clamped against blocked tiles, reset after
    // three consecutive unclamped moving frames.
    f32 wall_bump_frames = 0.0f;
    i32 wall_clear_streak = 0;
    // y.s: repath cooldown, counted down toward zero each frame. Issuing a
    // request sets 500; chase followers clamp to 90 (30 on target drift
    // beyond 18 world units per axis) and only re-request at zero.
    f32 repath_cooldown_frames = 0.0f;
    // The C++ object update is split into zero-delta order and physical
    // passes. Preserve the order-pass entry value so a cooldown written by
    // that pass is not immediately decremented by the second half.
    f32 repath_cooldown_order_start = 0.0f;
    // y.q: consecutive single-node path counter. y.b(l) increments q when
    // the solver returns aw==1; build-follow gives up (ay()) when q > 3.
    i32 single_node_path_counter = 0;
    bool transportable = false;
    bool unloading_all = false;
    f32 unload_timer_frames = 0.0f;
    f32 auto_repair_timer_frames = 0.0f;
    // The stock nano turret shares one warm-up accumulator across build,
    // repair, and reclaim orders. It charges only while aimed at an in-range
    // service target and decays while idle or moving back into range.
    f32 service_warmup_progress = 0.0f;
    f32 service_warmup_max = 0.0f;
    f32 service_warmup_decay = 0.0f;
    u32 service_aim_target_uid = 0;
    bool service_aim_ready = false;
    bool service_active_this_frame = false;
    // y.h retains the previous order handler's aimed service state and adds
    // five units to the next REPAIR/RECLAIM distance check.
    bool service_range_extended = false;
    // y.aN is written by the nano-beam update after y.k(). Warm-up decay
    // therefore requires an active service beam in both object updates.
    bool service_active_previous_frame = false;
    // The build pass runs before the service dispatch inside one object
    // update. y.i(...) ends the waypoint in the same update that saturates
    // its target's HP, so record the completion for that dispatch to consume.
    bool service_completed_in_build_pass = false;
    // y.a(au, as, ...) refuses a build action the team cannot pay for before
    // it constructs anything. The builder then holds its whole placement pass
    // — turn included — until the action becomes ready again 0.2s later.
    i32 build_price_last_refusal_clock_ms = -9999;
    // Set when combat installs a new weapon-slot target before movement. A
    // lock_body_rotation_with_main_turret unit synchronizes its body turn to
    // the turret only on that acquisition frame.
    bool weapon_target_acquired_this_frame = false;
    // y.ab accumulates sub-credit payouts while reclaiming resource units.
    f32 resource_reclaim_credit_fraction = 0.0f;
    // custom.j.r alternates the one-unit lateral factory-exit nudge.
    bool custom_factory_exit_side = false;
    // custom.j.g is selected after the queue update and gates I() on the
    // following object turn, preserving the action's first movement frame.
    bool custom_action_movement_blocked = false;
    // Copied from UnitDef for custom.j's factory output placement.
    bool has_factory_exit_config = false;
    f32 factory_exit_x = 0.0f;
    f32 factory_exit_y = 9.0f;
    f32 factory_exit_move_away_amount = 70.0f;
    bool factory_exit_has_dir_offset = false;
    f32 factory_exit_dir_offset = 0.0f;
    f32 factory_exit_height_offset = 0.0f;
    bool factory_exit_height_ignore_parent = false;
    bool factory_keep_built_units = false;
    // Temporary canonical map units (fogRevealer) remove themselves after a
    // fixed number of JAR simulation frames.
    f32 lifetime_frames = -1.0f;
    // d.k assigns a short post-production movement delay (am.cl). The
    // object remains active for combat/collision, but cannot physically
    // advance its movement request until this reaches zero.
    f32 spawn_delay_frames = 0.0f;
    // d.k installs the raw factory-exit destination immediately, but y.a()
    // does not build its navigation path until am.cl reaches zero.
    bool factory_exit_path_pending = false;
    // y.ay() leaves the rally waypoint queued for one object update after
    // consuming d.k's synthetic factory-exit MOVE.
    bool activate_order_next_frame = false;
    // Dynamic map-unit state used by the stock damagingBorder/zoneMarker and
    // spreadingFire prototypes. These are deliberately kept on Unit so the
    // objects retain their UID and lifecycle like ordinary JAR units.
    f32 border_current_radius = 0.0f;
    f32 border_shrink_speed = 0.0f;
    f32 border_damage_timer = 0.0f;
    f32 border_target_radius = 0.0f;
    f32 border_target_x = 0.0f;
    f32 border_target_y = 0.0f;
    f32 border_damage_multiplier = 1.0f;
    bool border_initialized = true;
    bool border_moving = false;
    bool zone_marker = false;

    f32 fire_intensity = 0.0f;
    f32 fire_growth = 0.0f;
    f32 fire_max_intensity = 1.0f;
    f32 fire_spread_threshold = 0.3f;
    f32 fire_spread_timer = 0.0f;
    f32 fire_animation_timer = 0.0f;
    bool fire_initialized = false;
    bool fire_spread_exhausted = false;

    // y.P defaults to game.units.a.onlyInRange for every orderable unit.
    Behavior behavior = Behavior::ONLY_IN_RANGE;
    f32 sight_range = 200.0f;
    i32 nano_range = -1;
    i32 nano_turret_index = -1;
    f32 nano_build_speed = 1.0f;
    f32 self_regen_rate = 0.0f;
    // custom.as.p is a per-frame float. The seconds projection above is
    // retained for callers, but cannot recover all native rounding bits.
    f32 self_regen_per_frame = 0.0f;
    f32 target_height = 0.0f;
    f32 nano_factory_speed = 1.0f;
    f32 target_height_drift = 0.0f;
    f32 height_change_rate = -1.0f;
    f32 falling_acceleration = 0.03f;
    bool land_on_ground = false;
    bool land_on_ground_only_idle = false;
    i32 base_nano_range = 85;
    bool is_builder = false;
    bool can_repair = false;
    bool can_repair_any = false;
    bool auto_repair = false;
    f32 nano_repair_speed = 0.2f;
    f32 nano_reclaim_speed = 1.02f;
    f32 resource_reclaim_multiplier = 1.0f;
    f32 nano_unbuild_speed = 0.0051f;
    bool can_reclaim = false;
    bool can_repair_buildings = true;
    bool can_reclaim_resources = true;
    bool can_be_repaired = true;

    i32 cost = 0;
    // Native reclaim uses the full current build price or a surviving bx
    // override. Stock upgrades add their paid price; custom conversions
    // discard the old override when the destination's price differs.
    f32 reclaim_cost = 0.0f;
    f32 build_time = 20.0f;

    Str upgrade_to;
    bool is_nuke = false;
    bool is_antinuke = false;
    f32 nuke_range = 0.0f;
    f32 antinuke_scan_timer = 0.0f;

    f32 shield_mult = 1.0f;
    // game.f defaults to full shield absorption. Individual weapons can
    // override this value (amphibiousJet, for example, uses 0.1).
    f32 shield_absorb = 1.0f;
    f32 hp_mult = 1.0f;
    f32 air_damage_mult = 1.0f;
    f32 experimental_damage_mult = 1.0f;
    bool is_experimental = false;
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

    // Stock p.d is serialized; these laser properties are derived from it.
    void sync_stock_laser_defence_specs();

    bool is_complete() const { return build_progress >= 1.0f; }
    // JAR am.bT() gates orderable updates on completion and transport state;
    // bV (death) is checked by the caller and remains a separate lifetime bit.
    bool is_active() const {
        return is_alive && is_complete() && parent_uid == 0;
    }
    // y.a(float) checks am.bT() before k(float). A LOAD_INTO that attaches
    // during this unit's order phase still finishes combat and movement.
    bool is_active_for_update(i32 frame) const {
        return is_alive && is_complete() &&
               (parent_uid == 0 || order_phase_tick == frame);
    }
    bool is_mobile() const { return can_move && rwrl::is_mobile(move_type); }
    bool is_submerged() const {
        // custom.j.Q() reads eq directly, with an inclusive -1 boundary.
        // Stock submarines retain their separate strict-height state.
        return custom_unit ? height <= -1.0f : submerged;
    }
    bool has_armed_service_order() const {
        if (!can_attack || weapon_slots.empty() || order_queue.empty()) return false;
        const WeaponType type = order_queue.front().type;
        return type == WeaponType::BUILD || type == WeaponType::REPAIR ||
               type == WeaponType::RECLAIM;
    }
    u32 main_target_uid() const {
        return has_armed_service_order() ? service_combat_target_uid
             : (has_weapon(weapon_type) ? target_uid : unarmed_main_target_uid);
    }
    i32 main_target_team_id() const {
        return has_armed_service_order() ? service_combat_target_team_id
             : (has_weapon(weapon_type) ? target_team_id : unarmed_main_target_team_id);
    }
    void restore_main_target(u32 uid, i32 team_id) {
        if (has_armed_service_order()) {
            service_combat_target_uid = uid;
            service_combat_target_team_id = uid != 0 ? team_id : -1;
        } else {
            target_uid = uid;
            target_team_id = uid != 0 ? team_id : -1;
            if (!can_attack) {
                unarmed_main_target_uid = uid;
                unarmed_main_target_team_id = uid != 0 ? team_id : -1;
            }
        }
    }
    f32 move_speed_per_frame() const {
        // b.e.z() reads the current height at each call. Formation commands
        // run between object updates, before this aircraft's next height step.
        if (!custom_unit && unit_type == "gunShip") {
            return height < 15.0f ? 0.0f : 1.4f;
        }
        return move_speed / EngineConstants::FRAME_RATE * terrain_speed_multiplier;
    }
    f32 hp_ratio() const { return max_hp > 0 ? hp / max_hp : 0.0f; }

    f32 distance_to(const Unit& o) const { return dist(x, y, o.x, o.y); }
    f32 distance_to(f32 ox, f32 oy) const { return dist(x, y, ox, oy); }

    f32 apply_damage(f32 incoming, u32 attacker_uid, i32 current_tick = 0,
                     f32 shield_mult = EngineConstants::SHIELD_MULTIPLIER,
                     f32 shield_absorb = EngineConstants::SHIELD_ABSORB,
                     f32 hp_mult = EngineConstants::HP_MULTIPLIER);
    void repair(f32 amount, f32 shield_repair_mult = 1.0f,
                f32 hp_repair_mult = 1.0f, f32 shield_repair_cost_mult = 1.0f);
    void update_shield(f32 dt);
    void reload_ammo();
};

}
