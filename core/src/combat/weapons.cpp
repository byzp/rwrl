#include "world_internal.h"
#include "combat/targeting_helpers.h"

namespace rwrl {
namespace {

bool is_fixed_ballistic_unit(const Unit& unit) {
    return unit.unit_type == "artillery" ||
           unit.unit_type == "turret_artillery" ||
           unit.unit_type == "turret_flamethrower";
}

// These stock classes call game.f.a(am,x,y,height,slot) from their
// per-weapon muzzle hook. That factory consumes the unit's bC salt once for
// every newly-created projectile. Other stock classes, including
// commandCenter, use the three-argument factory and leave bC unchanged.
bool uses_stock_projectile_factory(const Unit& unit) {
    return unit.unit_type == "airShip" ||
           unit.unit_type == "amphibiousJet" ||
           unit.unit_type == "dropship" ||
           unit.unit_type == "gunShip" ||
           unit.unit_type == "helicopter" ||
           unit.unit_type == "heavyHoverTank" ||
           unit.unit_type == "hoverTank" ||
           unit.unit_type == "battleShip" ||
           unit.unit_type == "missileShip" ||
           unit.unit_type == "attackSubmarine";
}

bool within_attack_range(const Unit& attacker, const Unit& target) {
    f32 range = attacker.weapon_range;
    // y.o(am) normally returns m() unchanged. Only aV() units (the custom
    // isMelee flag and matching stock melee units) add both collision radii.
    if (attacker.is_melee) {
        range += attacker.radius + target.radius;
    }
    return dist2(attacker.x, attacker.y, target.x, target.y) < range * range;
}

std::pair<f32, f32> predicted_impact_point(
    const Unit& target, f32 origin_x, f32 origin_y, f32 speed_per_frame,
    f32 lifetime_frames, f32 weapon_range) {
    const auto predict_position = [&target](f32 frames) {
        f32 displacement_x = 0.0f;
        f32 displacement_y = 0.0f;
        if (target.is_mobile() && target.wall_bump_frames == 0.0f) {
            if (target.strafes) {
                displacement_x = target.vel_x * frames;
                displacement_y = target.vel_y * frames;
            } else if (target.move_speed_factor != 0.0f) {
                // y.n(float) multiplies z()*cf*time before the direction.
                // Factoring out velocity changes the lead point's float
                // bits, and can later change a projectile's height slope.
                const f32 distance = target.move_speed_per_frame() *
                                     target.move_speed_factor * frames;
                displacement_x = jar_cosine_degrees(target.rotation) * distance;
                displacement_y = jar_sine_degrees(target.rotation) * distance;
            }
        }
        return std::pair<f32, f32>{target.x + displacement_x,
                                  target.y + displacement_y};
    };
    f32 lead_time = 0.0f;
    if (static_cast<f64>(speed_per_frame) > 0.1 &&
        target.movement_prediction_enabled) {
        const f32 inverse_speed = 1.0f / speed_per_frame;
        for (i32 i = 0; i < 3; ++i) {
            const auto predicted = predict_position(lead_time);
            lead_time = dist(origin_x, origin_y, predicted.first, predicted.second) *
                        inverse_speed;
        }
    }
    lead_time = std::min(lead_time, lifetime_frames);
    const auto predicted = predict_position(lead_time);
    f32 predicted_x = predicted.first;
    f32 predicted_y = predicted.second;
    if (weapon_range >= 0.0f &&
        dist2(origin_x, origin_y, predicted_x, predicted_y) >
            weapon_range * weapon_range) {
        const f32 heading = jar_direction_degrees(
            origin_x, origin_y, predicted_x, predicted_y);
        predicted_x = origin_x + jar_cosine_degrees(heading) * weapon_range;
        predicted_y = origin_y + jar_sine_degrees(heading) * weapon_range;
    }
    return {predicted_x, predicted_y};
}

}  // namespace

void World::aim_weapon_slot_(Unit& attacker, WeaponSlot& slot, Unit& target) {
    const i32 slot_index = static_cast<i32>(
        &slot - attacker.weapon_slots.data());
    if (slot_index == attacker.nano_turret_index) return;
    // y.b(engine, delta) aims every independent turret before its firing
    // pass. A slave may precede its parent in the INI section order.
    if (slot.linked_slot >= 0 &&
        slot.linked_slot < static_cast<i32>(attacker.weapon_slots.size())) {
        return;
    }
    if (!attacker.is_active_for_update(tick_) || !target.is_alive || target.parent_uid != 0 ||
        !turret_target_allowed(attacker, slot, target) ||
        !target_layer_allowed(*this, attacker, target,
                              slot.targets_air, slot.targets_ground,
                              slot.targets_underwater, slot.targets_not_touching_water)) {
        return;
    }
    if (slot.fixed_turret) {
        // y.b(engine,delta) leaves ap.g false while dispatch k is active,
        // unless bj() lets movement ignore the body. Service waypoints own
        // body steering even when they stop inside their working radius.
        if (attacker.has_move_target && !attacker.move_ignores_body) return;
        if (!attacker.order_queue.empty()) {
            const WeaponType order = attacker.order_queue.front().type;
            if (order == WeaponType::BUILD || order == WeaponType::REPAIR ||
                order == WeaponType::RECLAIM) return;
        }
    }
    const Projectile* beam = nullptr;
    if (attacker.unit_type == "experimentalHoverTank" &&
        attacker.active_beam_projectile_uid != 0) {
        auto found = projectiles_.find(attacker.active_beam_projectile_uid);
        if (found != projectiles_.end()) beam = &found->second;
        else {
            auto retired = retired_beam_projectiles_.find(
                attacker.active_beam_projectile_uid);
            if (retired != retired_beam_projectiles_.end()) {
                beam = &retired->second;
            }
        }
    } else if (attacker.custom_unit && slot.instant_reuse_last_change_turret_aim) {
        const i32 index = static_cast<i32>(&slot - attacker.weapon_slots.data());
        if (index >= 0 && index < static_cast<i32>(attacker.reusable_projectile_uids.size())) {
            auto found = projectiles_.find(attacker.reusable_projectile_uids[index]);
            if (found != projectiles_.end() && found->second.active) beam = &found->second;
        }
    }
    slot.aim_ready = aim_weapon_slot(
        attacker, slot, target, combat_delta_frames_, attacker.weapon_slots,
        beam);
}

bool World::fire_weapon_slot_(Unit& attacker, WeaponSlot& slot, Unit& target,
                              const Unit* main_target) {
    const i32 slot_index = static_cast<i32>(
        &slot - attacker.weapon_slots.data());
    if (!attacker.is_active_for_update(tick_) || !target.is_alive || target.parent_uid != 0) {
        return false;
    }
    if (!turret_target_allowed(attacker, slot, target)) return false;
    if (slot.target_uid != target.uid || slot.target_team_id != target.team_id) {
        attacker.weapon_target_acquired_this_frame = true;
    }
    if (!target_layer_allowed(*this, attacker, target,
                              slot.targets_air, slot.targets_ground,
                              slot.targets_underwater, slot.targets_not_touching_water)) {
        return false;
    }
    // Callers gate the entire pass on R's o(R) range, then validate each
    // independent target with a(i,target,false,true). That custom boundary
    // is inclusive and must not acquire a second strict o(target) check.
    const bool linked = slot.linked_slot >= 0 &&
        slot.linked_slot < static_cast<i32>(attacker.weapon_slots.size());
    const bool aimed = linked
        ? attacker.weapon_slots[slot.linked_slot].aim_ready
        : slot.aim_ready;
    const f32 energy_usage = attacker.custom_unit
        ? slot.energy_usage : attacker.energy_per_shot;
    const bool energy_ready = energy_usage <= 0.0f ||
        !(energy_usage > attacker.energy ||
          (attacker.custom_unit && attacker.energy_disabled));
    // custom.j.u(i) checks bn's resourceUsage even for canShoot=false
    // turrets. An empty anti-nuke slot must not hold every turret's warmup.
    const bool resources_ready = energy_ready &&
        (!attacker.custom_unit ||
         ((slot.intercept_ammo_usage <= 0 ||
           attacker.ammo >= slot.intercept_ammo_usage) &&
          (attacker.custom_flags & slot.intercept_required_flags) ==
              slot.intercept_required_flags &&
          (attacker.custom_flags & slot.intercept_missing_flags) == 0));
    // JAR y.b() tests u(i) and ap.e before a(f, target, i) can install a new
    // reload. Preserve that pre-fire condition for its unit-level warmup
    // decay decision.
    // u(i) includes the custom energy gate. A parent with canShoot=false
    // can still hold warmup decay when its target, aim and reload allow u(i).
    slot.warmup_hold_this_frame =
        aimed && resources_ready && slot.cooldown_remainder == 0.0f;
    // Linked children keep their own ap.g false. y.a(delta,target,slot)
    // copies the parent's angle only once u(slot) allows this firing pass.
    if (linked && aimed && resources_ready) {
        aim_weapon_slot(attacker, slot, target, 0.0f,
                        attacker.weapon_slots);
    }
    if (!slot.fire_enabled || !aimed || !resources_ready || !target.is_alive) {
        return false;
    }

    if (slot.warmup_target_uid != target.uid) {
        slot.warmup_target_uid = target.uid;
    }
    // y.a(float, target, slot) advances warmupNoReset turrets while their
    // target is valid even during the shoot cooldown. The accumulated value
    // is then used by t(i) to transfer part of the warmup into the next
    // reload. Ordinary warmup is advanced only when the cooldown is clear.
    if (slot.warmup_no_reset) {
        if (slot.warmup_progress < slot.warmup_frames) {
            slot.warmup_progress += std::max(0.0f, combat_delta_frames_);
        } else {
            // Native s(i) permits crossing e(i) on the incrementing update,
            // then clamps f back to e(i) on the following eligible call.
            slot.warmup_progress = slot.warmup_frames;
        }
    }
    if (slot.cooldown_remainder > 0.0f) return false;
    // In y.a(float,target,slot), ordinary warmup increments f and returns
    // while f was below e. Reaching e therefore does not fire until the next
    // call; warmupNoReset follows the immediate-fire branch above.
    if (!slot.warmup_no_reset && slot.warmup_progress < slot.warmup_frames) {
        slot.warmup_progress += std::max(0.0f, combat_delta_frames_);
        return false;
    }

    slot.target_uid = target.uid;
    slot.target_team_id = target.team_id;
    if (energy_usage > 0.0f) {
        attacker.energy -= energy_usage;
        if (attacker.custom_unit && attacker.energy_needs_full_recharge &&
            attacker.energy < energy_usage) {
            attacker.energy_disabled = true;
        }
    }

    // custom.j.a(target,slot) resets a non-shooting attached parent before
    // D(slot) samples its recoil origin for the new projectile. Sampling
    // first carries the parent's previous reload recoil into this launch.
    if (linked) {
        WeaponSlot& parent = attacker.weapon_slots[
            static_cast<std::size_t>(slot.linked_slot)];
        if (!parent.fire_enabled && parent.recoil_offset != 0.0f) {
            const f32 parent_warmup_transfer =
                parent.warmup_shoot_delay_transfer > 0.0f &&
                parent.warmup_frames > 0.0f
                ? parent.warmup_shoot_delay_transfer *
                      (parent.warmup_progress / parent.warmup_frames)
                : 0.0f;
            set_custom_reload_frames(
                parent.fire_delay_frames * attacker.shoot_delay_multiplier -
                    parent_warmup_transfer,
                parent.cooldown_ticks, parent.cooldown_remainder);
        }
    }

    const u32 fired_projectile_uid =
        create_weapon_projectile_(attacker, slot, target);
    const f32 warmup_transfer =
        slot.warmup_shoot_delay_transfer > 0.0f &&
        slot.warmup_frames > 0.0f
        ? slot.warmup_shoot_delay_transfer *
              (slot.warmup_progress / slot.warmup_frames)
        : 0.0f;
    // custom.j's t(i) is added after the unit's shoot-delay multiplier has
    // been applied: e = b(i) + t(i), where b(i) is delay*multiplier.
    set_custom_reload_frames(
        slot.fire_delay_frames * attacker.shoot_delay_multiplier -
            warmup_transfer,
        slot.cooldown_ticks, slot.cooldown_remainder);

    // linkDelayWithTurret is stored on the delayed child. The firing turret
    // releases that child's 9000-frame gate before the rest of this unit's
    // slots are visited, allowing the linked turrets to fire in one update.
    for (WeaponSlot& linked : attacker.weapon_slots) {
        if (linked.delay_link_slot != slot_index) continue;
        const f32 linked_remaining = std::max(0.0f, linked.cooldown_remainder);
        const f32 release_threshold = 9000.0f -
            (slot.fire_delay_frames * attacker.shoot_delay_multiplier * 0.5f -
             linked.warmup_frames);
        if (linked_remaining < release_threshold) {
            linked.cooldown_ticks = 0;
            linked.cooldown_remainder = 0.0f;
        }
    }
    if (!slot.warmup_no_reset) slot.warmup_progress = 0.0f;
    // y.M(slot) always derives spread from y.R, even when an independently
    // aimed weapon slot just fired at a different target.
    refresh_weapon_aim_offset(
        attacker, slot, main_target, tick_, slot_index);
    slot.odd_shot = !slot.odd_shot;
    return true;
}

u32 World::create_weapon_projectile_(Unit& attacker, WeaponSlot& slot,
                                      Unit& target, bool ground_point) {
    const i32 slot_index = static_cast<i32>(
        &slot - attacker.weapon_slots.data());
    // b.c.a(am,2) returns without creating game.f. Its independent base
    // turret still reloads, refreshes aim spread and toggles ap.m below.
    const bool stock_jet_base_turret =
        attacker.unit_type == "amphibiousJet" && slot_index == 2;
    bool reused_continuous_beam = false;
    Projectile* reused_custom_projectile = nullptr;
    u32 fired_projectile_uid = attacker.active_beam_projectile_uid;
    if (slot.hitscan && attacker.unit_type == "experimentalHoverTank" &&
        attacker.active_beam_projectile_uid != 0) {
        auto beam_it = projectiles_.find(attacker.active_beam_projectile_uid);
        if (beam_it != projectiles_.end() &&
            beam_it->second.continuous_beam &&
            beam_it->second.target_uid == target.uid &&
            beam_it->second.target_team_id == target.team_id) {
            beam_it->second.lifetime = slot.projectile_lifetime;
            reused_continuous_beam = true;
        }
    }
    if (attacker.custom_unit && slot.instant_reuse_last && slot_index >= 0 &&
        slot_index < static_cast<i32>(attacker.reusable_projectile_uids.size())) {
        auto found = projectiles_.find(attacker.reusable_projectile_uids[slot_index]);
        if (found != projectiles_.end() && found->second.active) {
            reused_custom_projectile = &found->second;
            fired_projectile_uid = found->first;
        }
    }
    // custom.j advances bC by eh+1 before the projectile factory samples I.
    // The five-argument factory then increments bC once; speed and ground
    // spread read that final salt without advancing it again.
    if (attacker.custom_unit) {
        attacker.deterministic_random_salt = static_cast<i32>(
            static_cast<u32>(attacker.deterministic_random_salt) + attacker.uid + 1u);
    }
    f32 projectile_random_phase = 0.0f;
    if (!reused_continuous_beam && !reused_custom_projectile && !stock_jet_base_turret &&
        (attacker.custom_unit || uses_stock_projectile_factory(attacker))) {
        projectile_random_phase = deterministic_unit_random(
            attacker, tick_, 0.0f, 1.0f, attacker.deterministic_random_salt);
        attacker.deterministic_random_salt = static_cast<i32>(
            static_cast<u32>(attacker.deterministic_random_salt) + 1u);
    }
    if (!reused_continuous_beam && !stock_jet_base_turret) {
        // game.f inherits w.eh, the global object UID shared with units. The
        // digest and replay alignment depend on projectiles consuming the same
        // counter as units, in creation order.
        const u32 pid = reused_custom_projectile ? reused_custom_projectile->uid
            : global_next_uid_ ? (*global_next_uid_)++ : next_uid_++;
        fired_projectile_uid = pid;
        Projectile p = reused_custom_projectile ? *reused_custom_projectile : Projectile{};
        p.uid = pid;
        p.tags = slot.projectile_tags;
        p.should_reveal_fog = slot.projectile_should_reveal_fog;
        if (!reused_custom_projectile) p.creation_order = allocate_object_order_();
        p.owner_uid = attacker.uid;
        p.team_id = team_id_;
        if (attacker.custom_unit || uses_stock_projectile_factory(attacker)) {
            p.firing_slot = slot_index;
        }
        if (!reused_custom_projectile) p.random_phase = projectile_random_phase;
        if (reused_custom_projectile) {
            // f.a(owner,x,y,height) restarts the hit while keeping J, I and
            // K/L. custom.j resets the area list unless the template keeps it.
            p.has_hit = false;
            p.splash_impact_started = false;
            if (!slot.instant_reuse_last_keep_area_damage_list) p.splash_hit_units.clear();
        }
        p.wobble_amplitude = slot.projectile_wobble_amplitude;
        p.wobble_frequency_frames = slot.projectile_wobble_frequency_frames;
        // custom.j.F(slot) resolves attachedTo independently of v(slot)'s
        // slave aiming link, including each mount's recoil before its offset.
        const auto mount = attacker.custom_unit
            ? custom_weapon_mount_origin(attacker, slot_index)
            : std::pair<f32, f32>{
                attacker.x + (jar_cosine_degrees(attacker.rotation) * slot.mount_forward -
                              jar_sine_degrees(attacker.rotation) * slot.mount_side),
                attacker.y + (jar_sine_degrees(attacker.rotation) * slot.mount_forward +
                              jar_cosine_degrees(attacker.rotation) * slot.mount_side)};
        const f32 mount_x = mount.first;
        const f32 mount_y = mount.second;
        const f32 muzzle_angle = slot.fixed_turret
            ? attacker.rotation : slot.turret_angle;
        // Stock gunShip overrides y.E(slot): its muzzle is projected from
        // the hull heading, while its turret still aims independently.
        const f32 projectile_spawn_angle = attacker.unit_type == "gunShip"
            ? attacker.rotation : muzzle_angle;
        const f32 muzzle_offset = slot.projectile_uses_muzzle
            ? slot.muzzle_offset : 0.0f;
        const bool missile_ship_underwater_shot =
            attacker.unit_type == "missileShip" && target.is_submerged();
        const bool submarine_underwater_shot =
            attacker.unit_type == "attackSubmarine" && attacker.submerged;
        // megaTank and heavyTank use a second air-target projectile branch in
        // their stock a(am,int): it starts at the hull origin (not E(slot)),
        // climbs to 15, and has a much slower, longer-lived missile.
        const bool heavy_ground_to_air_shot =
            target.airborne &&
            (attacker.unit_type == "megaTank" ||
             attacker.unit_type == "heavyTank");
        if (heavy_ground_to_air_shot) {
            p.x = attacker.x;
            p.y = attacker.y;
        } else if (attacker.custom_unit && slot.projectile_uses_muzzle) {
            const f32 muzzle_side = slot.muzzle_side +
                (slot.odd_shot ? slot.muzzle_side_odd : 0.0f);
            const f32 cosine = jar_cosine_degrees(projectile_spawn_angle);
            const f32 sine = jar_sine_degrees(projectile_spawn_angle);
            // custom.j.D() adds each rotated offset to the mount only after
            // both products have been combined, preserving JAR float order.
            p.x = mount_x + (cosine * muzzle_offset - sine * muzzle_side);
            p.y = mount_y + (sine * muzzle_offset + cosine * muzzle_side);
        } else {
            p.x = mount_x + jar_cosine_degrees(projectile_spawn_angle) * muzzle_offset;
            p.y = mount_y + jar_sine_degrees(projectile_spawn_angle) * muzzle_offset;
            // missileShip overrides E(slot) directly instead of g(slot), so
            // its six-unit muzzle extension is not part of the public turret
            // muzzle-offset property sampled into WeaponSlot.
            if (attacker.unit_type == "missileShip") {
                p.x += jar_cosine_degrees(attacker.rotation) * 6.0f;
                p.y += jar_sine_degrees(attacker.rotation) * 6.0f;
            }
        }
        if (attacker.unit_type == "amphibiousJet" && slot_index < 2 &&
            attacker.weapon_slots.size() > 2) {
            const auto origin = amphibious_jet_weapon_origin(
                attacker, attacker.weapon_slots[2].turret_angle, slot_index);
            p.x = origin.first;
            p.y = origin.second;
        }
        // missileShip has a separate h/d.a(am,int) branch for submerged
        // targets.  It launches a low water projectile (42 damage, 1.9
        // units/frame, 220-frame lifetime) instead of the normal guided
        // surface missile (62 damage, 2 units/frame, 190 frames).
        p.height = attacker.frame_start_height -
            (missile_ship_underwater_shot ? 1.0f : 0.0f);
        // e.m never copies K/L into either projectile branch. e.f copies
        // its turret spread only for ground shots; h.d copies it only for
        // surface missiles. The other branches keep the factory's offsets.
        const bool ignores_turret_aim_offset = !attacker.custom_unit &&
            (attacker.unit_type == "megaTank" || heavy_ground_to_air_shot ||
             missile_ship_underwater_shot);
        if (!reused_custom_projectile && slot.sweep_offset == 0.0f &&
            slot.sweep_offset_from_target_radius == 0.0f) {
            p.target_offset_x = ignores_turret_aim_offset ? 0.0f : slot.aim_offset_x;
            p.target_offset_y = ignores_turret_aim_offset ? 0.0f : slot.aim_offset_y;
        }
        p.target_x = target.x + p.target_offset_x;
        p.target_y = target.y + p.target_offset_y;
        p.target_height = target.height;
        p.target_uid = target.uid;
        p.target_team_id = target.team_id;
        const bool air_variant = heavy_ground_to_air_shot;
        p.height_guided = (missile_ship_underwater_shot ||
                           submarine_underwater_shot)
            ? false : (air_variant ? true : slot.height_guided);
        p.launch_height = air_variant ? 10.0f : slot.launch_height;
        p.cruise_height = air_variant ? 15.0f : slot.cruise_height;
        p.vertical_speed = air_variant ? 2.0f : slot.vertical_speed;
        p.unguided_speed_height = slot.projectile_unguided_speed_height;
        p.unguided_speed_x = slot.projectile_unguided_speed_x;
        p.unguided_speed_y = slot.projectile_unguided_speed_y;
        p.gravity = slot.projectile_gravity;
        p.true_gravity = slot.projectile_true_gravity;
        f32 projectile_damage = slot.damage;
        f32 projectile_speed = slot.projectile_speed;
        f32 projectile_lifetime = slot.projectile_lifetime;
        if (missile_ship_underwater_shot) {
            projectile_damage = 42.0f;
        } else if (submarine_underwater_shot) {
            projectile_speed = 1.3f * EngineConstants::FRAME_RATE;
            projectile_lifetime = 250.0f;
        } else if (air_variant) {
            projectile_damage = attacker.unit_type == "megaTank" ? 40.0f : 50.0f;
            projectile_speed = attacker.unit_type == "megaTank"
                ? 4.0f * EngineConstants::FRAME_RATE
                : 0.5f * EngineConstants::FRAME_RATE;
            projectile_lifetime = 190.0f;
        }
        const bool fixed_ballistic =
            ground_point || is_fixed_ballistic_unit(attacker) || slot.fixed_target;
        const i32 flame_shot = attacker.weapon_shot_counter;
        if (attacker.unit_type == "turret_flamethrower") {
            projectile_speed =
                (3.0f + static_cast<f32>((flame_shot * 13) % 2)) *
                EngineConstants::FRAME_RATE;
        }
        f32 projectile_speed_per_frame =
            projectile_speed / EngineConstants::FRAME_RATE;
        if (slot.projectile_speed_spread != 0.0f) {
            // custom.j configures f.t after advancing bC and creating the
            // projectile. Its seed-1 draw truncates each bound to hundredths
            // and is also the speed used for fixed-target lead prediction.
            const i32 minimum = static_cast<i32>(
                -slot.projectile_speed_spread * 100.0f);
            const i32 maximum = static_cast<i32>(
                slot.projectile_speed_spread * 100.0f);
            projectile_speed_per_frame += static_cast<f32>(
                deterministic_unit_random_int(
                    attacker, tick_, minimum, maximum, 1)) / 100.0f;
            projectile_speed =
                projectile_speed_per_frame * EngineConstants::FRAME_RATE;
        }
        if (fixed_ballistic) {
            f32 prediction_speed = projectile_speed_per_frame;
            if (slot.fixed_target && slot.projectile_target_speed >= 0.0f) {
                prediction_speed = slot.projectile_target_speed / EngineConstants::FRAME_RATE;
            }
            if (slot.fixed_target && slot.lead_targeting_speed >= 0.0f) {
                prediction_speed = slot.lead_targeting_speed;
            }
            const auto predicted = ground_point || (slot.fixed_target && slot.disable_lead_targeting)
                ? std::pair<f32, f32>{target.x, target.y}
                : predicted_impact_point(
                      target, p.x, p.y, prediction_speed,
                      projectile_lifetime, attacker.weapon_range);
            p.target_x = predicted.first;
            p.target_y = predicted.second;
            p.target_height = slot.fixed_target && slot.fixed_target_include_height
                ? target.height : 0.0f;
            if (slot.fixed_target) {
                p.target_height += slot.ground_target_height_offset;
                if (slot.ground_target_spread != 0.0f) {
                    const i32 spread = static_cast<i32>(slot.ground_target_spread * 100.0f);
                    p.target_x += static_cast<f32>(deterministic_unit_random_int(
                        attacker, tick_, -spread, spread, 2)) / 100.0f;
                    p.target_y += static_cast<f32>(deterministic_unit_random_int(
                        attacker, tick_, -spread, spread, 7)) / 100.0f;
                }
                // f uses n+K/o+L for the fixed destination. Turret aim
                // offsets remain independent of targetGroundSpread.
                p.target_x += p.target_offset_x;
                p.target_y += p.target_offset_y;
            }
            p.target_uid = 0;
            p.fixed_target = true;
            if (attacker.unit_type == "turret_flamethrower") {
                p.target_x += static_cast<f32>(-15 + (flame_shot * 13) % 30);
                p.target_y += static_cast<f32>(
                    -15 + (63 + flame_shot * 33) % 30);
                attacker.weapon_shot_counter++;
                if (attacker.weapon_shot_counter > 10) {
                    attacker.weapon_shot_counter = 0;
                }
            }
        }
        if (!p.fixed_target) {
            p.has_target_snapshot = true;
            p.target_radius = target.radius;
            p.target_shield = target.shield;
            p.target_is_stock_building =
                target.is_building && !target.custom_unit;
        }
        p.damage = projectile_damage * attacker.shoot_damage_multiplier;
        p.splash_radius = slot.splash_radius;
        p.splash_damage =
            slot.splash_damage * attacker.shoot_damage_multiplier;
        p.splash_same_layer = slot.splash_same_layer;
        p.splash_friendly_fire = slot.splash_friendly_fire;
        p.expanding_splash = slot.splash_expand_time > 0.0f;
        p.splash_expand_remaining_frames = slot.splash_expand_time;
        p.splash_expand_duration_frames = slot.splash_expand_time;
        p.speed = missile_ship_underwater_shot
            ? 1.9f * EngineConstants::FRAME_RATE : projectile_speed;
        // Keep the native rounded sum: multiplying into seconds and dividing
        // back can change the initial speed by one float ULP.
        p.speed_per_frame = missile_ship_underwater_shot
            ? p.speed / EngineConstants::FRAME_RATE : projectile_speed_per_frame;
        p.target_speed = slot.projectile_target_speed;
        if (air_variant && attacker.unit_type == "heavyTank") {
            p.target_speed = 5.0f * EngineConstants::FRAME_RATE;
        }
        p.speed_acceleration = slot.projectile_acceleration;
        p.lifetime = missile_ship_underwater_shot ? 220.0f
                                                   : projectile_lifetime;
        p.is_nuke = slot.nuke_weapon || (!attacker.custom_unit && attacker.is_nuke);
        p.impact_at_target = p.is_nuke || ground_point;
        // custom.j.a(projectile, ...) initializes az from cL[slot].a.
        // A launch target can die before the newborn projectile's turn,
        // so its first retarget search already needs that firing direction.
        if (attacker.custom_unit || ground_point) p.heading = slot.turret_angle;
        p.instant_hit = slot.hitscan;
        p.persist_after_hit = slot.persist_after_hit ||
            attacker.unit_type == "laserTank";
        p.continuous_beam =
            attacker.unit_type == "experimentalHoverTank";
        p.sweep_offset = slot.sweep_offset;
        p.sweep_offset_from_target_radius = slot.sweep_offset_from_target_radius;
        if (slot.move_with_parent && slot_index >= 0) {
            p.move_with_parent = true;
            p.parent_turret_slot = slot_index;
            p.parent_anchor_x = p.x;
            p.parent_anchor_y = p.y;
            p.parent_anchor_height = p.height;
        }
        p.flame_weapon = slot.projectile_flame_weapon;
        if (p.continuous_beam) p.beam_charge_frames = 70.0f;
        p.tracking = fixed_ballistic ? false : slot.tracking;
        p.retarget_on_death = attacker.custom_unit
            ? slot.projectile_retarget_on_death : !fixed_ballistic && (
            attacker.unit_type == "commandCenter" ||
            attacker.unit_type == "antiAirTurret" ||
            attacker.unit_type == "antiAirTurretT2" ||
            (attacker.unit_type == "experimentalTank" && slot.targets_air) ||
            (attacker.unit_type == "heavyTank" && target.airborne));
        p.retarget_range = slot.projectile_retarget_range;
        p.retarget_lead = slot.projectile_retarget_lead;
        p.laser_defence_excluded = slot.hitscan || slot.laser_defence_excluded;
        if (slot.projectile_deflection_power < 0.5f) {
            p.laser_defence_immune = true;
        } else {
            p.intercept_hp = slot.projectile_deflection_power;
        }
        p.shield_mult = slot.shield_mult;
        p.shield_absorb = slot.shield_absorb;
        p.hp_mult = slot.hp_mult;
        p.building_damage_mult = slot.building_damage_mult;
        p.air_damage_mult = slot.air_damage_mult;
        p.experimental_damage_mult = slot.experimental_damage_mult;
        p.chain_count = attacker.chain_count;
        p.chain_range = attacker.chain_range;
        p.is_flame = attacker.is_flame;
        p.flame_expand_rate = attacker.flame_expand_rate;
        projectiles_[pid] = std::move(p);
        if (attacker.unit_type == "experimentalHoverTank") {
            retired_beam_projectiles_.erase(attacker.active_beam_projectile_uid);
            attacker.active_beam_projectile_uid = pid;
        }
        if (attacker.custom_unit && slot.instant_reuse_last && slot_index >= 0 &&
            slot_index < static_cast<i32>(attacker.reusable_projectile_uids.size())) {
            attacker.reusable_projectile_uids[slot_index] = pid;
        }
    }
    return fired_projectile_uid;
}

bool World::unit_attack(Unit& attacker, Unit& target) {
    if (!attacker.can_attack) return false;
    if (!attacker.is_active_for_update(tick_) || !target.is_alive || target.parent_uid != 0) {
        return false;
    }
    if (!within_attack_range(attacker, target)) return false;
    if (attacker.ammo_max > 1 && attacker.ammo <= 0) return false;
    if (!can_target_(attacker, target)) return false;
    if ((!attacker.custom_unit || attacker.weapon_slots.empty()) &&
        attacker.energy_per_shot > 0 &&
        attacker.energy < attacker.energy_per_shot) return false;

    if (!attacker.weapon_slots.empty()) {
        bool fired = false;
        for (WeaponSlot& slot : attacker.weapon_slots) {
            slot.aim_ready = false;
            slot.warmup_hold_this_frame = false;
        }
        for (WeaponSlot& slot : attacker.weapon_slots) {
            aim_weapon_slot_(attacker, slot, target);
        }
        for (WeaponSlot& slot : attacker.weapon_slots) {
            fired = fire_weapon_slot_(attacker, slot, target, &target) || fired;
            if (target.is_dead) break;
        }
        if (fired) {
            attacker.cooldown_ticks = attacker.weapon_slots.front().cooldown_ticks;
            if (attacker.ammo_max > 1) attacker.ammo--;
        }
        return fired;
    }

    if (attacker.cooldown_ticks > 0) return false;

    if (attacker.is_beam || attacker.projectile_hitscan) {
        f32 dmg = attacker.damage * attacker.shoot_damage_multiplier *
                  damage_category_multiplier(
            target, attacker.air_damage_mult, attacker.experimental_damage_mult);
        if (target.is_building) dmg *= attacker.building_damage_mult;
        target.apply_damage(dmg, attacker.uid, tick_, attacker.shield_mult, attacker.shield_absorb, attacker.hp_mult);
        set_custom_reload(attacker.cooldown_max, attacker.shoot_delay_multiplier,
                          attacker.cooldown_ticks,
                          attacker.cooldown_remainder);
        if (attacker.ammo_max > 1) attacker.ammo--;
        if (attacker.energy_per_shot > 0) {
            attacker.energy -= attacker.energy_per_shot;
        }
        return true;
    }

    u32 pid = global_next_uid_ ? (*global_next_uid_)++ : next_uid_++;
    Projectile p{};
    p.uid = pid;
    p.creation_order = allocate_object_order_();
    p.owner_uid = attacker.uid;
    p.team_id = team_id_;
    p.x = attacker.x; p.y = attacker.y;
    p.height = attacker.frame_start_height;
    p.target_x = target.x; p.target_y = target.y;
    p.target_height = target.height;
    p.target_uid = target.uid;
    p.target_team_id = target.team_id;
    p.damage = attacker.damage * attacker.shoot_damage_multiplier;
    p.splash_radius = attacker.splash_radius;
    p.splash_damage =
        attacker.splash_damage * attacker.shoot_damage_multiplier;
    p.speed = attacker.projectile_speed;
    p.lifetime = attacker.projectile_lifetime;
    const bool fixed_ballistic = is_fixed_ballistic_unit(attacker);
    const i32 flame_shot = attacker.weapon_shot_counter;
    if (attacker.unit_type == "turret_flamethrower") {
        p.speed = (3.0f + static_cast<f32>((flame_shot * 13) % 2)) *
                  EngineConstants::FRAME_RATE;
    }
    p.speed_per_frame = p.speed / EngineConstants::FRAME_RATE;
    if (fixed_ballistic) {
        const auto predicted = predicted_impact_point(
            target, p.x, p.y, p.speed / EngineConstants::FRAME_RATE,
            p.lifetime, attacker.weapon_range);
        p.target_x = predicted.first;
        p.target_y = predicted.second;
        p.target_height = 0.0f;
        p.target_uid = 0;
        p.fixed_target = true;
        if (attacker.unit_type == "turret_flamethrower") {
            p.target_x += static_cast<f32>(-15 + (flame_shot * 13) % 30);
            p.target_y += static_cast<f32>(
                -15 + (63 + flame_shot * 33) % 30);
            attacker.weapon_shot_counter++;
            if (attacker.weapon_shot_counter > 10) {
                attacker.weapon_shot_counter = 0;
            }
        }
    }
    if (!p.fixed_target) {
        p.has_target_snapshot = true;
        p.target_radius = target.radius;
        p.target_shield = target.shield;
        p.target_is_stock_building =
            target.is_building && !target.custom_unit;
    }
    p.is_nuke = attacker.is_nuke;
    p.impact_at_target = p.is_nuke;
    p.tracking = fixed_ballistic ? false : attacker.projectile_tracking;
    p.laser_defence_excluded = attacker.projectile_hitscan || attacker.is_flame;
    p.shield_mult = attacker.shield_mult;
    p.shield_absorb = attacker.shield_absorb;
    p.hp_mult = attacker.hp_mult;
    p.air_damage_mult = attacker.air_damage_mult;
    p.experimental_damage_mult = attacker.experimental_damage_mult;
    p.building_damage_mult = attacker.building_damage_mult;
    p.chain_count = attacker.chain_count;
    p.chain_range = attacker.chain_range;
    p.is_flame = attacker.is_flame;
    p.flame_expand_rate = attacker.flame_expand_rate;
    p.flame_current_radius = 0.0f;
    projectiles_[pid] = std::move(p);

    set_custom_reload(attacker.cooldown_max, attacker.shoot_delay_multiplier,
                      attacker.cooldown_ticks, attacker.cooldown_remainder);
    if (attacker.ammo_max > 1) attacker.ammo--;
    if (attacker.energy_per_shot > 0) {
        attacker.energy -= attacker.energy_per_shot;
    }
    return true;
}

} // namespace rwrl
