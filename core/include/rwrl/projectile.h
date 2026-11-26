#pragma once
#include <rwrl/common.h>
#include <rwrl/constants.h>

namespace rwrl {

struct Projectile {
    u32 uid = 0;
    u64 creation_order = 0;
    u32 owner_uid = 0;
    i32 team_id = 0;
    f32 x = 0.0f, y = 0.0f;
    // game.f.az is updated even while vertical guidance holds the shot in
    // place. Retargeting searches ahead along this last flight direction.
    f32 heading = 0.0f;
    f32 target_x = 0.0f, target_y = 0.0f;
    // game.f.K/L retain the firing slot's ap.h/i aim offset and add it to
    // the live target position on every projectile update.
    f32 target_offset_x = 0.0f, target_offset_y = 0.0f;
    f32 sweep_offset = 0.0f;
    f32 sweep_offset_from_target_radius = 0.0f;
    bool move_with_parent = false;
    i32 parent_turret_slot = -1;
    f32 parent_anchor_x = 0.0f, parent_anchor_y = 0.0f;
    f32 parent_anchor_height = 0.0f;
    f32 target_height = 0.0f;
    // Artillery-style stock shots set a predicted impact point once at fire
    // time and do not retain a live target pointer.
    bool fixed_target = false;
    f32 age_frames = 0.0f;
    // game.f.I is sampled by the five-argument projectile factory, before
    // its final bC increment. The wobble controller uses the old game.f.J.
    f32 random_phase = 0.0f;
    f32 wobble_amplitude = 0.0f;
    f32 wobble_frequency_frames = 5.0f;
    f32 activation_delay_frames = 0.0f;
    f32 height = 0.0f;
    // game.f.u/v/w and game.g.G/H use world units per simulation frame.
    // These velocities are added independently of target-directed flight.
    f32 unguided_speed_x = 0.0f;
    f32 unguided_speed_y = 0.0f;
    f32 unguided_speed_height = 0.0f;
    f32 gravity = 0.0f;
    f32 true_gravity = 0.0f;
    bool arc_descending = false;
    bool height_guided = false;
    f32 launch_height = 0.0f;
    f32 cruise_height = 0.0f;
    f32 vertical_speed = 0.0f;
    u32 target_uid = 0;
    i32 target_team_id = -1;
    // game.f retains its target object even after that object leaves the
    // global unit list. Keep the last collision geometry so dead targets use
    // the same impact threshold without remaining damageable.
    bool has_target_snapshot = false;
    f32 target_radius = 0.0f;
    f32 target_shield = 0.0f;
    // game.f uses target instanceof units.d.d here. Custom buildings inherit
    // custom.j instead, so the enlarged impact radius is stock-building-only.
    bool target_is_stock_building = false;
    f32 damage = 0.0f;
    f32 speed = EngineConstants::PROJECTILE_SPEED;
    // Authoritative game.f.t value. The JAR stores and accumulates projectile
    // speed in world units per simulation frame; speed remains the public
    // per-second mirror used by the rest of the C++ API.
    f32 speed_per_frame =
        EngineConstants::PROJECTILE_SPEED / EngineConstants::FRAME_RATE;
    f32 target_speed = -1.0f;
    f32 speed_acceleration = 0.1f * EngineConstants::FRAME_RATE;
    f32 lifetime = EngineConstants::PROJECTILE_LIFETIME;
    f32 max_lifetime = EngineConstants::PROJECTILE_TIMEOUT;
    f32 splash_radius = 0.0f;
    f32 splash_damage = 0.0f;
    bool splash_same_layer = false;
    bool splash_friendly_fire = false;
    // game.f.aS marks a laser-defence kill before the projectile's object
    // turn. It leaves the object in the current frame snapshot, so that turn
    // still runs once before removal is committed.
    bool active = true;
    bool is_nuke = false;
    // Native f.aY creates a temporary fogRevealer on descent or impact.
    bool should_reveal_fog = false;
    // game.f.aC keeps the area-effect center at the commanded target rather
    // than the projectile's current impact position (stock nuclear missiles).
    bool impact_at_target = false;
    // Stock nuclear damage expands from radius zero over W/X=75 simulation
    // frames after impact. game.f.ap prevents units being damaged twice.
    bool expanding_splash = false;
    bool splash_impact_started = false;
    f32 splash_expand_remaining_frames = 0.0f;
    f32 splash_expand_duration_frames = 0.0f;
    Set<u64> splash_hit_units;
    bool is_antinuke = false;
    bool tracking = false;
    // game.f.A resolves against its target without horizontal travel. B keeps
    // the resolved beam object alive, while E reapplies scaled beam damage.
    bool instant_hit = false;
    bool persist_after_hit = false;
    bool continuous_beam = false;
    bool flame_weapon = false;
    bool flame_impact_emitted = false;
    bool has_hit = false;
    f32 beam_charge_frames = 0.0f;
    // game.f.k records the five-argument factory's firing turret. Retargeting
    // checks that turret on the current owner, including after conversion.
    i32 firing_slot = -1;
    bool retarget_on_death = false;
    f32 retarget_range = 120.0f;
    f32 retarget_lead = 15.0f;
    u32 target_projectile_uid = 0;
    Vec<Str> tags;
    f32 intercept_hp = 1.0f;
    bool laser_defence_excluded = false;
    bool laser_defence_immune = false;

    f32 shield_mult = 1.0f;
    f32 shield_absorb = 1.0f;
    f32 hp_mult = 1.0f;
    f32 air_damage_mult = 1.0f;
    f32 experimental_damage_mult = 1.0f;
    f32 building_damage_mult = 1.0f;
    i32 chain_count = 0;
    f32 chain_range = 0.0f;
    bool is_flame = false;
    f32 flame_expand_rate = 0.0f;
    f32 flame_current_radius = 0.0f;
};

}
