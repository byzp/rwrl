#include "world_internal.h"

namespace rwrl {

void World::restore_factory_queue(const FactoryQueue& queue) {
    if (!get_unit(queue.factory_uid) ||
        queue.queue.size() != queue.upfront_costs.size() ||
        queue.queue.size() != queue.action_contexts.size()) {
        throw std::invalid_argument("invalid saved factory queue");
    }
    FactoryQueue restored = queue;
    if (restored.item_states.empty()) {
        restored.item_states.resize(restored.queue.size());
        restored.remember_head();
    } else if (restored.item_states.size() != restored.queue.size()) {
        throw std::invalid_argument("invalid saved factory item states");
    }
    factory_queues_[queue.factory_uid] = std::move(restored);
}

void World::add_credits(f32 amount) {
    credits_ += amount;
    total_income_ += amount;
}

bool World::spend_credits(f32 amount) {
    if (credits_ >= amount) {
        credits_ -= amount;
        total_spent_ += amount;
        return true;
    }
    return false;
}

void World::refund_credits(f32 amount, f32 rate) {
    // Player credits are double in the stock engine.  The reclaim path passes
    // a float rate (notably 0.8f), which Java promotes to double before the
    // multiplication; doing the product in float loses 0.000011920929 on a
    // 1000-credit building and changes the post-refund digest.
    const f64 refund = static_cast<f64>(amount) * static_cast<f64>(rate);
    credits_ += refund;
    total_spent_ -= static_cast<f32>(refund);
}

f32 World::ai_income_multiplier() const {
    // n.E(): replay/network games use the saved n.x difficulty. The n.w
    // flag controls this bonus independently of whether AI commands run.
    if (!ai_player_) return 1.0f;
    f32 multiplier = 1.0f + static_cast<f32>(ai_difficulty_) *
        (ai_difficulty_ > 0 ? 0.4f : 0.3f);
    if (ai_difficulty_ == 3) multiplier += 1.5f;
    return std::max(0.1f, multiplier);
}

f32 World::compute_income(f32 dt, f32 speed_mult, f32 income_mult) {
    f32 earned = 0.0f;
    Vec<u32> unit_uids;
    unit_uids.reserve(units_.size());
    for (const auto& [uid, _] : units_) unit_uids.push_back(uid);
    for (u32 uid : unit_uids) {
        const f32 before = credits_;
        update_unit_income(dt, speed_mult, income_mult, uid);
        earned += credits_ - before;
    }
    return earned;
}

void World::update_unit_income(f32 dt, f32 speed_mult, f32 income_mult,
                               u32 unit_uid) {
    Unit* unit = get_unit(unit_uid);
    if (!unit || !unit->is_alive || !unit->is_complete() ||
        unit->parent_uid != 0 || unit->income_rate == 0.0f) {
        return;
    }

    // Stock income uses 10-frame pulses. Shipped custom generators use their
    // generation_delay (40 frames for extractor/fabricator) and pay the full
    // configured resource bundle at that boundary.
    const f32 interval = std::max(0.1f, unit->income_interval);
    unit->income_timer += dt * EngineConstants::FRAME_RATE * speed_mult;
    if (unit->income_timer <= interval - 0.1f) {
        return;
    }
    unit->income_timer -= interval;
    const f32 pulse = unit->income_rate *
        (interval / EngineConstants::ECONOMY_WINDOW);
    // n.b(float) applies E() before c(float) applies the global multiplier;
    // each product is rounded to float before the double credit addition.
    const f32 ai_pulse = ai_income_multiplier() * pulse;
    const f32 earned = ai_pulse * income_mult;
    credits_ += earned;
    total_income_ += earned;
}

Unit* World::start_build(const Str& name, f32 x, f32 y) {
    return start_build_(name, x, y, false);
}

Unit* World::start_build_(const Str& name, f32 x, f32 y, bool requires_builder,
                          bool* unaffordable, i32 build_tech_level,
                          u32 placement_source_uid, bool* candidate_created) {
    if (unaffordable) *unaffordable = false;
    if (candidate_created) *candidate_created = false;
    const UnitDef* building_def = registry_.get(name);
    if (!building_def || (!building_def->is_building && !requires_builder)) return nullptr;
    const bool mobile_product = !building_def->is_building;
    // y.a(au,as,...) tests the unit cap before constructing its provisional
    // non-building unit. Incomplete mobile products consume the same cap.
    if (mobile_product && count_active_units() >= unit_cap()) return nullptr;
    if (candidate_created) *candidate_created = true;
    const auto reject_mobile_candidate = [&]() -> Unit* {
        if (mobile_product) {
            // The rejected provisional am still consumed w.eh, while its
            // direct deletion does not run the ordinary death effects.
            if (global_next_uid_) ++(*global_next_uid_);
            else ++next_uid_;
            (void)allocate_object_order_();
        }
        return nullptr;
    };
    if (requires_builder && building_def) {
        // y.a(au,as,int,float,float) snaps the provisional building before
        // checking placement. b.b() multiplies by the float reciprocal and
        // truncates toward zero; neither the raw waypoint nor floor is used.
        const f32 tile_size = pathfinder_.tile_size();
        x = jar_build_coordinate(x,
            building_def->building_to_footprint_offset_x, tile_size);
        y = jar_build_coordinate(y,
            building_def->building_to_footprint_offset_y, tile_size);
    }
    // d.d.a(y,as,ao,...) checks seaFactory against WATER even though its
    // ordinary movement type is NONE. LAND placement uses the full BUILDING
    // occupancy layer, including forceNano mobile products. Other declared
    // movement layers retain their native placement rules.
    MoveType placement_type = MoveType::BUILDING;
    if (name == "seaFactory") {
        placement_type = MoveType::WATER;
    } else if (building_def && building_def->move_type != MoveType::NONE &&
               building_def->move_type != MoveType::ROOT &&
               building_def->move_type != MoveType::LAND) {
        placement_type = building_def->move_type;
    }
    const bool resource_building = building_def->place_only_on_res_pool;
    bool extractor_resource = false;
    if (resource_building) {
        extractor_resource = map_loaded_ &&
            pathfinder_.is_resource_pool_world(x, y);
        if (!map_loaded_) {
            const f32 extractor_radius = building_def ? building_def->radius : 18.0f;
            for (const auto& [_, resource] : units_) {
                if (!resource.is_alive || resource.unit_type != "crystalResource") continue;
                if (dist(resource.x, resource.y, x, y) <=
                    resource.radius + extractor_radius) {
                    extractor_resource = true;
                    break;
                }
            }
        }
    }
    if (resource_building && extractor_resource) {
        // as.p() buildings use the resource-pool tile path and skip the cd()
        // BUILDING-layer traversal. y.a(am,n) still scans the global unit
        // list, including enemy buildings, with a strict nine-unit radius.
        const auto overlaps = [&](const World& world) {
            for (const auto& [_, existing] : world.units_) {
                if (!existing.is_alive || !existing.is_building ||
                    existing.unit_type == "crystalResource") {
                    continue;
                }
                if (dist2(x, y, existing.x, existing.y) < 81.0f) return true;
            }
            return false;
        };
        if (overlaps(*this)) return nullptr;
        for (const World* world : peer_worlds_) {
            if (world && world != this && overlaps(*world)) return nullptr;
        }
    } else if (resource_building && map_loaded_) {
        return nullptr;
    } else if (building_def && !pathfinder_.is_buildable_footprint(
                   x, y, building_def->placement_footprint, placement_type,
                   building_def->building_to_footprint_offset_x,
                   building_def->building_to_footprint_offset_y)) {
        return reject_mobile_candidate();
    }
    if (mobile_product) {
        // y.a(am,n) checks all living units for a mobile placement. Its r()
        // uses nine units against a building and the summed collision radii
        // otherwise. A non-colliding mobile builder is the explicit exception.
        const Unit* source = get_unit(placement_source_uid);
        const u32 ignored_source = source && !source->is_building &&
            !source->collision_enabled ? placement_source_uid : 0;
        const auto overlaps = [&](const World& world) {
            for (const auto& [uid, existing] : world.units_) {
                if (!existing.is_alive || uid == ignored_source) continue;
                const f32 radius = existing.is_building ? 9.0f
                    : std::max(11.0f, building_def->radius + existing.radius);
                if (dist2(x, y, existing.x, existing.y) < radius * radius) return true;
            }
            return false;
        };
        if (overlaps(*this)) return reject_mobile_candidate();
        for (const World* world : peer_worlds_) {
            if (world && world != this && overlaps(*world)) return reject_mobile_candidate();
        }
    }
    i32 cost = registry_.get_build_cost(name, build_tech_level);
    if (cost <= 0 || credits_ < static_cast<f32>(cost)) {
        // y.a(au,as,int,float,float) refuses on price after the provisional
        // building has already been constructed and destroyed again, and the
        // refusal is the only branch that ever sets z.c. Every other rejection
        // -- unbuildable, locked, unavailable, unit cap, missing type, blocked
        // placement -- returns z.c=false and y.f() drops the waypoint. The
        // caller decides whether z.c is actually set; see construction.cpp.
        if (unaffordable) {
            *unaffordable = true;
        }
        return reject_mobile_candidate();
    }
    // Stock build actions have an ordinary upfront price and no streaming
    // resource bundle. y.a(au, as, ...) consumes the full action price before
    // it creates the incomplete building.
    if (!spend_credits(static_cast<f32>(cost))) return nullptr;

    Unit* u = spawn_unit(name, x, y, false);
    if (!u) {
        refund_credits(static_cast<f32>(cost), 1.0f);
        return nullptr;
    }
    u->build_progress = 0.0f;
    u->build_progress_secondary = 0.0f;
    // Stock a(2) selects the factory's instance menu before construction
    // progresses, without changing its HP or other unit stats.
    if (!u->custom_unit && build_tech_level == 2 &&
        (name == "landFactory" || name == "airFactory" || name == "seaFactory")) {
        u->tech_level = 2;
    }
    u->cost = cost;
    u->reclaim_cost = static_cast<f32>(cost);
    pending_builds_.push_back({0.0f, u->uid, requires_builder, false});
    // y.br() only reaches a building placed by y.f(float,au,ad); the public
    // scene helper has no waypoint behind it and never fells anything.
    if (requires_builder && building_def) {
        clear_placement_area_(*u, *building_def);
    }
    return u;
}

bool World::queue_unit(u32 factory_uid, const Str& unit_type) {
    Unit* factory = get_unit(factory_uid);
    const UnitDef* product = registry_.get(unit_type);
    if (!factory || !product || !factory->is_alive || factory->parent_uid != 0 ||
        (!factory->is_building && !factory->custom_unit) ||
        product->is_building) return false;
    if (!can_factory_build_(*factory, unit_type)) return false;
    if (count_active_units() >= unit_cap()) return false;

    const f32 cost = static_cast<f32>(registry_.get_cost(unit_type));
    if (!spend_credits(cost)) return false;

    auto& queue = factory_queues_[factory_uid];
    if (queue.factory_uid == 0) queue.factory_uid = factory_uid;
    queue.queue.push_back(unit_type);
    queue.upfront_costs.push_back(cost);
    queue.action_contexts.emplace_back();
    queue.item_states.emplace_back();
    return true;
}

bool World::cancel_factory_queue(u32 factory_uid, const Str& unit_type) {
    auto it = factory_queues_.find(factory_uid);
    if (it == factory_queues_.end() || it->second.queue.empty()) return false;
    FactoryQueue& queue = it->second;
    queue.remember_head();

    i32 item_index = static_cast<i32>(queue.queue.size()) - 1;
    if (!unit_type.empty()) {
        const Str upgrade_prefix = "__upgrade__:";
        while (item_index >= 0) {
            const Str& item = queue.queue[item_index];
            const bool matches = item == unit_type ||
                (item.compare(0, upgrade_prefix.size(), upgrade_prefix) == 0 &&
                 item.substr(upgrade_prefix.size()) == unit_type);
            if (matches) break;
            item_index--;
        }
    }
    if (item_index < 0) return false;

    // Every ordinary action price was paid at enqueue time, including tail
    // items. Streaming resources, when present, only belong to the front.
    if (item_index < static_cast<i32>(queue.upfront_costs.size())) {
        refund_credits(queue.upfront_costs[item_index],
                       EngineConstants::CANCEL_REFUND);
    }
    if (Unit* factory = get_unit(factory_uid)) {
        factory->ammo += queue.item_states[item_index].ammo_cost;
    }
    if (item_index == 0 && queue.resource_progress > 0.0f) {
        Unit* factory = get_unit(factory_uid);
        if (factory) {
            refund_credits(
                static_cast<f32>(factory_queue_item_cost_(
                    *factory, queue.queue.front())) * queue.resource_progress,
                EngineConstants::CANCEL_REFUND);
        }
    }
    queue.queue.erase(queue.queue.begin() + item_index);
    queue.item_states.erase(queue.item_states.begin() + item_index);
    if (item_index < static_cast<i32>(queue.upfront_costs.size())) {
        queue.upfront_costs.erase(queue.upfront_costs.begin() + item_index);
    }
    if (item_index < static_cast<i32>(queue.action_contexts.size())) {
        queue.action_contexts.erase(queue.action_contexts.begin() + item_index);
    }
    if (item_index == 0) {
        queue.progress = 0.0f;
        queue.resource_progress = 0.0f;
        queue.head_changed = true;
    }
    return true;
}

bool World::can_factory_build_(const Unit& factory, const Str& unit_type) const {
    // v.i() resolves custom replacements after the stock factory chooses its
    // tier's build list. The replacement retains that stock availability,
    // while queue_unit() uses its own price and production definition.
    const UnitDef* product = registry_.get(unit_type);
    const UnitDef* factory_def = registry_.get(factory.unit_type);
    if (factory_def && factory_def->has_declared_build_menu &&
        std::find(factory_def->factory_products.begin(), factory_def->factory_products.end(),
                  unit_type) == factory_def->factory_products.end()) {
        return false;
    }
    if (product && std::find(product->built_from_factories.begin(),
                            product->built_from_factories.end(),
                            factory.unit_type) != product->built_from_factories.end()) {
        return factory.tech_level >= product->tech_level;
    }
    const Str& availability_type = product && !product->override_and_replace.empty()
        ? product->override_and_replace : unit_type;
    if (factory.unit_type == "landFactory" && factory.tech_level >= 2) {
        static const Vec<Str> tier_two = {
            "builder", "tank", "hoverTank", "artillery", "hovercraft",
            "heavyTank", "heavyHoverTank", "laserTank",
        };
        return std::find(tier_two.begin(), tier_two.end(), availability_type) !=
               tier_two.end();
    }
    if (factory.unit_type == "airFactory") {
        // A host-registered custom product declares its own factory and tech
        // level. Stock products stay on the tier table below: several of them
        // also carry builtFrom airFactory, so testing builtFrom alone would
        // expose gunShip at tier 1.
        if (registry_.is_custom(unit_type)) {
            const UnitDef* custom_product = product;
            if (custom_product && custom_product->built_from == "airFactory" &&
                custom_product->tech_level <= factory.tech_level) {
                return true;
            }
        }
        // The stock air factory exposes lightGunship at tier 1. Its
        // serialized action is the canonical lightGunship name, while the
        // higher-tier gunShip/amphibiousJet entries remain tech-gated.
        if (availability_type == "lightGunship") return true;
        if (factory.tech_level < 2) return false;
        static const Vec<Str> tier_two = {
            "dropship", "gunShip", "amphibiousJet",
        };
        return std::find(tier_two.begin(), tier_two.end(), availability_type) !=
               tier_two.end();
    }
    if (factory.unit_type == "experimentalLandFactory") {
        return availability_type == "experimentalTank" ||
               availability_type == "experimentalHoverTank";
    }
    return registry_.can_build_unit(factory.unit_type, availability_type);
}

i32 World::factory_queue_item_cost_(const Unit& factory, const Str& item) const {
    if (item == "__build_nuke__") return 11000;
    if (item == "__build_antinuke__") return 4000;

    const Str action_prefix = "__custom_action__:";
    if (item.compare(0, action_prefix.size(), action_prefix) == 0) {
        const CustomActionDef* action = registry_.get_custom_action(
            factory.unit_type, item.substr(action_prefix.size()));
        return action ? action->price : 0;
    }

    const Str upgrade_prefix = "__upgrade__:";
    const bool upgrade = item.compare(
        0, upgrade_prefix.size(), upgrade_prefix) == 0;
    if (upgrade || item == "__upgrade_tier__") {
        const Str target = upgrade ? item.substr(upgrade_prefix.size()) : item;
        i32 cost = 0;
        f32 build_speed = 0.0f;
        if (resolve_upgrade_action_(factory, target, cost, build_speed)) {
            return cost;
        }
    }
    return registry_.get_cost(item);
}

bool World::queue_nuke(u32 launcher_uid) {
    Unit* launcher = get_unit(launcher_uid);
    if (!launcher || !launcher->is_alive || !launcher->is_complete() || !launcher->is_nuke) {
        return false;
    }
    auto& queue = factory_queues_[launcher_uid];
    i32 queued = launcher->ammo;
    for (const Str& item : queue.queue) {
        if (item == "__build_nuke__") queued++;
    }
    if (queued >= launcher->ammo_max) return false;
    constexpr f32 cost = 11000.0f;
    if (!spend_credits(cost)) return false;
    queue.factory_uid = launcher_uid;
    queue.queue.push_back("__build_nuke__");
    queue.upfront_costs.push_back(cost);
    queue.action_contexts.emplace_back();
    queue.item_states.emplace_back();
    return true;
}

bool World::queue_antinuke(u32 launcher_uid) {
    Unit* launcher = get_unit(launcher_uid);
    if (!launcher || !launcher->is_alive || !launcher->is_complete() || !launcher->is_antinuke) {
        return false;
    }
    auto& queue = factory_queues_[launcher_uid];
    i32 queued = launcher->ammo;
    for (const Str& item : queue.queue) {
        if (item == "__build_antinuke__") queued++;
    }
    if (queued >= launcher->ammo_max) return false;
    constexpr f32 cost = 4000.0f;
    if (!spend_credits(cost)) return false;
    queue.factory_uid = launcher_uid;
    queue.queue.push_back("__build_antinuke__");
    queue.upfront_costs.push_back(cost);
    queue.action_contexts.emplace_back();
    queue.item_states.emplace_back();
    return true;
}

bool World::resolve_upgrade_action_(const Unit& u, const Str& target,
                                    i32& cost, f32& build_speed) const {
    const UnitDef* source = registry_.get(u.unit_type);
    if (!source) return false;

    if (target == "__upgrade_tier__") {
        if (u.unit_type == "extractor" && u.tech_level == 1) {
            cost = 1200;
            build_speed = 0.0006f;
            return true;
        }
        if (u.unit_type == "extractor" && u.tech_level == 2) {
            cost = 2500;
            build_speed = 0.0003f;
            return true;
        }
        if (u.unit_type == "fabricator" && u.tech_level == 1) {
            cost = 3000;
            build_speed = 0.0003f;
            return true;
        }
        if (u.unit_type == "fabricator" && u.tech_level == 2) {
            cost = 5000;
            build_speed = 0.0002f;
            return true;
        }
        if (u.unit_type == "laserDefence" && u.tech_level == 1) {
            cost = 2000;
            build_speed = 0.0003f;
            return true;
        }
        if (u.unit_type == "supplyDepot" && u.tech_level == 1) {
            cost = 1000;
            build_speed = 0.0004f;
            return true;
        }
        if (u.unit_type == "landFactory" && u.tech_level == 1) {
            cost = 2000;
            build_speed = 0.0004f;
            return true;
        }
        if (u.unit_type == "airFactory" && u.tech_level == 1) {
            cost = 1500;
            build_speed = 0.0004f;
            return true;
        }
        if (u.unit_type == "seaFactory" && u.tech_level == 1) {
            cost = 2000;
            build_speed = 0.0004f;
            return true;
        }
        return false;
    }

    const UnitDef* destination = registry_.get(target);
    if (!destination) return false;

    const bool default_upgrade = target == u.upgrade_to;
    if (!default_upgrade) {
        const bool turret_conversion =
            u.unit_type == "turret" &&
            (target == "turret_artillery" || target == "turret_flamethrower");
        if (!turret_conversion) return false;
    }

    cost = (default_upgrade && source->upgrade_cost > 0)
        ? source->upgrade_cost
        : destination->cost - u.cost;
    if (cost < 0) return false;

    if (u.unit_type == "turret" && target == "turretT2") build_speed = 0.001f;
    else if (u.unit_type == "turretT2" && target == "turretT3") build_speed = 0.0003f;
    else if (u.unit_type == "turret" && target == "turret_artillery") build_speed = 0.0004f;
    else if (u.unit_type == "turret" && target == "turret_flamethrower") build_speed = 0.002f;
    else if (u.unit_type == "antiAirTurret" && target == "antiAirTurretT2") build_speed = 0.001f;
    else return false;
    return true;
}

bool World::queue_upgrade_(Unit& u, const Str& requested_target) {
    // Factory action buttons remain queueable while their building is still
    // under construction.  The queue itself waits for completion before
    // progressing, matching the stock d.k/d.i behavior.
    if (!u.is_alive || !u.is_building || u.parent_uid != 0) return false;
    const Str target = requested_target.empty()
        ? (u.upgrade_to.empty() ? "__upgrade_tier__" : u.upgrade_to)
        : requested_target;
    i32 cost = 0;
    f32 build_speed = 0.0f;
    if (!resolve_upgrade_action_(u, target, cost, build_speed)) return false;

    auto& queue = factory_queues_[u.uid];
    const Str upgrade_prefix = "__upgrade__:";
    for (const Str& item : queue.queue) {
        if (item.compare(0, upgrade_prefix.size(), upgrade_prefix) == 0 ||
            item == "__upgrade_tier__") return false;
    }
    if (!spend_credits(static_cast<f32>(cost))) return false;
    queue.factory_uid = u.uid;
    queue.queue.push_back(target == "__upgrade_tier__" ? target : upgrade_prefix + target);
    queue.upfront_costs.push_back(static_cast<f32>(cost));
    queue.action_contexts.emplace_back();
    queue.item_states.emplace_back();
    return true;
}

bool World::queue_custom_action_(Unit& unit, const CustomActionDef& action,
                                 const CustomActionContext& context) {
    if (!unit.is_alive || unit.parent_uid != 0 || !action.queueable ||
        custom_action_cooldown_remaining_(unit, action.id) > 0) return false;
    if (unit.ammo < action.price_ammo) return false;
    if (context.has_position && action.fire_turret_index >= 0) {
        if (action.fire_turret_index >= static_cast<i32>(unit.weapon_slots.size())) return false;
        const WeaponSlot& turret = unit.weapon_slots[action.fire_turret_index];
        const f32 range = std::min(unit.weapon_range, turret.limiting_range);
        const f32 distance_sq = dist2(unit.x, unit.y, context.x, context.y);
        if (distance_sq > range * range ||
            distance_sq < turret.limiting_min_range * turret.limiting_min_range) return false;
    }
    if (action.ammo_including_queued_limit >= 0.0f) {
        i32 ammo = unit.ammo;
        const auto existing = factory_queues_.find(unit.uid);
        if (existing != factory_queues_.end()) {
            const Str prefix = "__custom_action__:";
            for (const Str& queued : existing->second.queue) {
                if (queued.compare(0, prefix.size(), prefix) != 0) continue;
                const CustomActionDef* queued_action = registry_.get_custom_action(
                    unit.unit_type, queued.substr(prefix.size()));
                if (queued_action) ammo += queued_action->add_resources.ammo;
            }
        }
        // g.a(am,false) evaluates the guard before d.k accepts and charges
        // the item. The current item is not part of the queued resource sum.
        if (static_cast<f32>(ammo) >= action.ammo_including_queued_limit) {
            return false;
        }
    }
    const Str item = "__custom_action__:" + action.id;
    auto& queue = factory_queues_[unit.uid];
    // custom.ag forces allowMultipleInQueue=false for convertTo actions.
    if (!action.allow_multiple_in_queue || !action.convert_to.empty()) {
        if (std::find(queue.queue.begin(), queue.queue.end(), item) !=
            queue.queue.end()) return false;
    }
    const f32 cost = static_cast<f32>(action.price);
    if (!spend_credits(cost)) return false;
    unit.ammo -= action.price_ammo;
    queue.factory_uid = unit.uid;
    queue.remember_head();
    std::size_t index = queue.queue.size();
    if (action.high_priority_queue) {
        index = 0;
        while (index < queue.item_states.size() &&
               queue.item_states[index].high_priority) ++index;
    }
    queue.queue.insert(queue.queue.begin() + index, item);
    queue.upfront_costs.insert(queue.upfront_costs.begin() + index, cost);
    queue.action_contexts.insert(queue.action_contexts.begin() + index, context);
    FactoryQueueItemState state;
    state.ammo_cost = action.price_ammo;
    state.high_priority = action.high_priority_queue;
    queue.item_states.insert(queue.item_states.begin() + index, state);
    if (index == 0) queue.head_changed = true;
    return true;
}

bool World::launch_nuke(Unit& launcher, f32 x, f32 y, i32 target_team_id) {
    if (!launcher.is_alive || !launcher.is_complete() || !launcher.is_nuke || launcher.ammo <= 0) {
        return false;
    }
    if (launcher.distance_to(x, y) < 450.0f) return false;

    Projectile p{};
    p.uid = global_next_uid_ ? (*global_next_uid_)++ : next_uid_++;
    p.creation_order = allocate_object_order_();
    p.owner_uid = launcher.uid;
    p.team_id = team_id_;
    p.target_team_id = target_team_id;
    // NukeLaucher.E(0) is three world units above the building center in the
    // JAR's screen/world Y convention.
    p.x = launcher.x;
    p.y = launcher.y - 3.0f;
    p.target_x = x;
    p.target_y = y;
    p.damage = 0.0f;
    p.splash_damage = 5400.0f;
    p.splash_radius = 250.0f;
    p.speed = 0.1f * EngineConstants::FRAME_RATE;
    p.speed_per_frame = 0.1f;
    p.target_speed = 2.7f * EngineConstants::FRAME_RATE;
    p.speed_acceleration = 0.1f * EngineConstants::FRAME_RATE;
    p.lifetime = 99999.0f;
    p.tracking = false;
    p.is_nuke = true;
    p.should_reveal_fog = true;
    p.impact_at_target = true;
    p.expanding_splash = true;
    p.splash_expand_remaining_frames = 75.0f;
    p.splash_expand_duration_frames = 75.0f;
    p.height_guided = true;
    p.launch_height = 80.0f;
    p.cruise_height = 100.0f;
    p.vertical_speed = 1.1f;
    p.activation_delay_frames = 5.0f;
    p.laser_defence_excluded = true;
    projectiles_[p.uid] = std::move(p);
    launcher.ammo--;
    return true;
}

bool World::set_factory_rally(u32 factory_uid, f32 x, f32 y) {
    Unit* factory = get_unit(factory_uid);
    if (!factory || !factory->is_alive || !factory->is_building ||
        (registry_.get_can_build(factory->unit_type).empty() &&
         factory->unit_type != "airFactory")) {
        return false;
    }
    auto it = factory_queues_.find(factory_uid);
    if (it == factory_queues_.end()) {
        FactoryQueue queue;
        queue.factory_uid = factory_uid;
        queue.rally_x = x;
        queue.rally_y = y;
        queue.has_rally = true;
        factory_queues_.emplace(factory_uid, std::move(queue));
        return true;
    }
    it->second.rally_x = x;
    it->second.rally_y = y;
    it->second.has_rally = true;
    return true;
}

} // namespace rwrl
