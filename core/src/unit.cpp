#include <rwrl/unit.h>
#include <rwrl/constants.h>

namespace rwrl {

void Unit::sync_stock_laser_defence_specs() {
    if (custom_unit || unit_type != "laserDefence") return;
    const bool upgraded = tech_level >= 2;
    weapon_range = upgraded ? 210.0f : 160.0f;
    energy_per_shot = upgraded ? 0.05f : 0.11f;
    energy_regen_rate = upgraded
        ? 0.0006f * EngineConstants::FRAME_RATE : 0.024f;
}

f32 Unit::apply_damage(f32 incoming, u32 attacker_uid, i32 current_tick,
                       f32 shield_mult, f32 shield_absorb, f32 hp_mult) {
    if (unit_type == "commandCenter") {
        incoming = std::min(incoming, 2500.0f);
    } else if (unit_type == "AntiNukeLaucher") {
        incoming = std::min(incoming, 2600.0f);
    } else if (unit_type == "editorOrBuilder" || unit_type == "fogRevealer") {
        incoming = 0.0f;
    } else if (unit_type == "spreadingFire") {
        // ai.a(am,float,f) consumes damage as fire intensity and then calls
        // the ordinary damage chain with zero, leaving HP untouched.
        fire_intensity -= incoming / 100.0f;
        incoming = 0.0f;
    }
    f32 remaining = incoming;

    if (build_progress < 1.0f) {
        remaining *= EngineConstants::INCOMPLETE_DAMAGE_MULT;
    }

    // custom.j subtracts armour only above armourMinDamageToKeep, then keeps
    // at least that configured amount. Projectile armour piercing is not
    // represented by the standalone command API and therefore defaults to 0.
    if (armour > 0.0f && remaining > armour_min_damage) {
        remaining = std::max(armour_min_damage, remaining - armour);
    }

    // The JAR checks the shield-disable timer for exact zero. A partially
    // active timer still bypasses shields until it reaches zero.
    if (shield_disabled == 0.0f && shield > 0.0f) {
        f32 sd = remaining * shield_mult;
        if (shield < sd) {
            remaining -= shield * shield_absorb;
            total_shield_absorbed += shield;
            shield = 0.0f;
        } else {
            shield -= sd;
            total_shield_absorbed += sd;
            remaining -= remaining * shield_absorb;
        }
    }

    if (remaining > 0.0f) {
        f32 hd = remaining * hp_mult;
        if (hp < hd) {
            damage_smoothing += hp;
            total_hp_lost += hp;
            remaining -= hp;
            hp = 0.0f;
        } else {
            hp -= hd;
            damage_smoothing -= hd;
            total_hp_lost += hd;
            remaining -= hd;
        }
    }

    last_attacker_uid = attacker_uid;
    was_attacked = true;
    last_damage_tick = current_tick;
    last_damage_age_ms = 0.0f;

    if (hp <= 0.0f && !is_dead) {
        is_dead = true;
        is_alive = false;
    }

    return remaining;
}

void Unit::repair(f32 amount, f32 shield_repair_mult, f32 hp_repair_mult,
                  f32 shield_repair_cost_mult) {
    f32 remaining = amount;
    if (max_shield > 0.0f && shield < max_shield) {
        f32 shield_amt = remaining * shield_repair_mult;
        f32 space = max_shield - shield;
        if (space > shield_amt) {
            shield += shield_amt;
            remaining -= shield_amt * shield_repair_cost_mult;
        } else {
            shield = max_shield;
            remaining -= shield_amt * shield_repair_cost_mult;
        }
    }
    if (remaining > 0.0f && hp < max_hp) {
        f32 hp_amt = remaining * hp_repair_mult;
        hp = std::min(max_hp, hp + hp_amt);
    }
}

void Unit::update_shield(f32 dt) {
    if (max_shield <= 0.0f) return;
    // Native stats multiply their original float by deltaFrames. Scaling to
    // seconds and back can lose that float, especially at 0.08. A caller
    // overriding the public per-second field keeps its existing dt semantics.
    const f32 amount = shield_regen_rate ==
            shield_regen_per_frame * EngineConstants::FRAME_RATE
        ? shield_regen_per_frame * (dt * EngineConstants::FRAME_RATE)
        : shield_regen_rate * dt;
    if (custom_unit) {
        // custom.j adds as.h even at full shield, then clamps its two bounds.
        if (shield_regen_rate != 0.0f) {
            shield = std::min(max_shield, shield + amount);
        }
        shield = std::max(0.0f, shield);
    } else if (shield_regen_rate > 0.0f) {
        // e.c uses f.a(cx,cA,0.25*delta), approaching from either side.
        if (shield < max_shield) {
            shield = std::min(max_shield, shield + amount);
        } else if (shield > max_shield) {
            shield = std::max(max_shield, shield - amount);
        }
    }
}

void Unit::reload_ammo() {
    if (is_nuke || is_antinuke || ammo_max <= 1) return;
    if (ammo >= ammo_max) return;
    ammo_reload_timer--;
    if (ammo_reload_timer <= 0) {
        ammo = std::min(ammo + 1, ammo_max);
        ammo_reload_timer = EngineConstants::AMMO_RELOAD_TICKS;
    }
}

}
