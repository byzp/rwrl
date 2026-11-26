#pragma once
#include <rwrl/common.h>

namespace rwrl {

struct EngineConstants {
    static constexpr f32 FRAME_RATE = 60.0f;
    // game-lib.jar advances simulation in 60 game frames per second.
    static constexpr f32 TICK_RATE = FRAME_RATE;
    static constexpr f32 SIM_DT = 1.0f / FRAME_RATE;
    // utility.ab.c parses cooldown seconds into l.by ticks at this rate.
    static constexpr f32 ACTION_COOLDOWN_TICKS_PER_SECOND = 16.666666f;
    static constexpr f32 ECONOMY_WINDOW = 40.0f;
    static constexpr f32 INCOME_INTERVAL = 10.0f;
    static constexpr f32 DEFAULT_SPEED = 2.5f;
    static constexpr f32 STARTING_CREDITS = 4000.0f;
    static constexpr f32 CC_INCOME_CY = 18.0f;

    static constexpr f32 INCOMPLETE_DAMAGE_MULT = 1.75f;
    static constexpr f32 INACTIVE_FACTORY_REGRESSION = -8.0f;

    static constexpr f32 RECLAIM_REFUND = 0.80f;
    static constexpr f32 CANCEL_REFUND = 1.00f;
    // y.a(au,as,int,float,float) only marks the refusal retryable while
    // y.V < 1000, so a waypoint that has been current for 1000 frames is
    // dropped by the same price refusal that a younger one survives.
    static constexpr f32 BUILD_PRICE_RETRY_MAX_ORDER_FRAMES = 1000.0f;

    static constexpr f32 SHIELD_MULTIPLIER = 1.0f;
    static constexpr f32 SHIELD_ABSORB = 1.0f;
    static constexpr f32 HP_MULTIPLIER = 1.0f;
    static constexpr f32 SHIELD_REGEN_DELAY = 10.0f;
    static constexpr f32 SHIELD_REGEN_RATE = 0.01f;
    static constexpr f32 SHIELD_REGEN_HP_THRESHOLD = 0.33f;
    static constexpr f32 MAX_HP_OVERCHARGE = 2.0f;
    static constexpr f32 MIN_DAMAGE = 0.005f;

    static constexpr f32 PROJECTILE_LIFETIME = 3600.0f;
    static constexpr f32 PROJECTILE_TIMEOUT = 14400.0f;
    static constexpr f32 PROJECTILE_SPEED = 80.0f;
    static constexpr f32 PROJECTILE_ACTIVE_TIMER = 6000.0f;
    static constexpr f32 PROJECTILE_SELF_DESTRUCT = 11000.0f;

    static constexpr i32 SPATIAL_GRID_SIZE = 32;
    static constexpr f32 SPATIAL_CELL_SIZE = 50.0f;
    static constexpr f32 ATTACK_DETECT_RANGE = 360.0f;

    static constexpr f32 IMPACT_THRESHOLD = 8.0f;
    static constexpr f32 SPLASH_RATIO = 0.4f;
    static constexpr f32 ARRIVAL_THRESHOLD = 20.0f;
    static constexpr f32 FRICTION_FACTOR = 3.0f;
    static constexpr f32 VEL_DEAD_ZONE = 0.01f;
    static constexpr i32 DEATH_CLEANUP_TICKS = 300;

    static constexpr f32 BUILDER_SEARCH_RANGE = 100.0f;
    // Service orders use the source unit's y()+target-type offset from the
    // JAR. Canonical builder and repairbay ranges are 85 and 230 frames.
    static constexpr f32 BUILDER_REPAIR_RANGE = 85.0f;
    static constexpr f32 REPAIRBAY_RANGE = 230.0f;
    static constexpr f32 REPAIR_RATE = 0.20f;
    static constexpr f32 RECLAIM_RATE = 1.02f;
    static constexpr f32 RECLAIM_BUILD_RATE = 0.0051f;
    static constexpr f32 RECLAIM_STARTUP_FRAMES = 100.0f;
    static constexpr f32 REPAIRBAY_SCAN_INTERVAL = 40.0f;
    static constexpr i32 AMMO_RELOAD_TICKS = 300;

    static constexpr f32 EXPLOSION_RADIUS_MOBILE = 800.0f;
    static constexpr f32 EXPLOSION_RADIUS_DEPLOYED = 540.0f;
    static constexpr f32 COLLISION_MARGIN = 120.0f;

    static constexpr i32 MAP_WIDTH = 120;
    static constexpr i32 MAP_HEIGHT = 120;
    static constexpr f32 TILE_SIZE = 20.0f;

    static constexpr i32 MAX_PLAYERS = 10;
    static constexpr i32 DEFAULT_UNIT_CAP = -1;

    static constexpr f32 AI_UPDATE_MS = 250.0f;
    static constexpr f32 PROJECTILE_UPDATE_MS = 25.0f;

    static constexpr f32 SHOT_SPREAD_ANGLE = 0.6f;
    static constexpr f32 SHOT_SPREAD_MIN_ANGLE = 720.0f;
    static constexpr f32 SHOT_SPREAD_OFFSET = 110.0f;
    static constexpr f32 SHOT_SPREAD_RANGE = 450.0f;
};

}
