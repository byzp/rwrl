#include "../../world_internal.h"

namespace rwrl {

i32 World::custom_action_cooldown_remaining_(
        const Unit& unit, const Str& action_id) const {
    i32 latest_expiry = action_cooldown_tick_;
    bool found = false;
    auto include = [&](const Str& id) {
        auto it = unit.action_cooldown_expiry_ticks.find(id);
        if (it != unit.action_cooldown_expiry_ticks.end() &&
            it->second > action_cooldown_tick_) {
            latest_expiry = std::max(latest_expiry, it->second);
            found = true;
        }
    };
    include("-1");
    include(action_id);
    return found ? latest_expiry - action_cooldown_tick_ : 0;
}

i32 World::custom_action_cooldown_remaining(
        u32 unit_uid, const Str& action_id) const {
    const Unit* unit = get_unit(unit_uid);
    return unit ? custom_action_cooldown_remaining_(*unit, action_id) : 0;
}

bool World::trigger_custom_action(Unit& unit, const Str& action_id) {
    const Vec<World*> no_other_worlds;
    return execute_custom_action_(unit, action_id, no_other_worlds);
}

void World::add_custom_action_cooldown_(
        Unit& unit, const Str& action_id, f32 duration) {
    if (duration <= 0.0f) return;
    const i32 expiry = action_cooldown_tick_ + static_cast<i32>(duration);
    auto [it, inserted] =
        unit.action_cooldown_expiry_ticks.emplace(action_id, expiry);
    if (!inserted) it->second = std::max(it->second, expiry);
}

void World::clear_factory_queue_(Unit& unit, bool refund) {
    auto it = factory_queues_.find(unit.uid);
    if (it == factory_queues_.end()) return;
    FactoryQueue& queue = it->second;
    queue.remember_head();

    if (refund) {
        for (const FactoryQueueItemState& state : queue.item_states) unit.ammo += state.ammo_cost;
        for (f32 cost : queue.upfront_costs) {
            refund_credits(cost, 1.0f);
        }
        if (queue.resource_progress > 0.0f && !queue.queue.empty()) {
            refund_credits(
                static_cast<f32>(factory_queue_item_cost_(
                    unit, queue.queue.front())) * queue.resource_progress,
                1.0f);
        }
    }

    queue.queue.clear();
    queue.upfront_costs.clear();
    queue.action_contexts.clear();
    queue.item_states.clear();
    queue.head_changed = false;
    queue.progress = 0.0f;
    queue.resource_progress = 0.0f;
}

void World::prune_custom_action_queue_(Unit& unit, i32 original_front_cost) {
    auto it = factory_queues_.find(unit.uid);
    if (it == factory_queues_.end()) return;
    FactoryQueue& queue = it->second;
    queue.remember_head();
    const Str prefix = "__custom_action__:";
    for (std::size_t index = 0; index < queue.queue.size();) {
        const Str& item = queue.queue[index];
        if (item.compare(0, prefix.size(), prefix) != 0 ||
            registry_.get_custom_action(unit.unit_type, item.substr(prefix.size()))) {
            ++index;
            continue;
        }
        // custom.j calls d.k.e() immediately after convertTo. Refund every
        // removed action in queue order, retaining actions the new type has.
        if (index < queue.upfront_costs.size()) {
            refund_credits(queue.upfront_costs[index], 1.0f);
            queue.upfront_costs.erase(queue.upfront_costs.begin() + index);
        }
        if (index == 0) {
            if (queue.resource_progress > 0.0f) {
                refund_credits(static_cast<f32>(original_front_cost) *
                    queue.resource_progress, 1.0f);
            }
            queue.progress = 0.0f;
            queue.resource_progress = 0.0f;
            queue.head_changed = true;
        }
        unit.ammo += queue.item_states[index].ammo_cost;
        queue.item_states.erase(queue.item_states.begin() + index);
        queue.queue.erase(queue.queue.begin() + index);
        if (index < queue.action_contexts.size()) {
            queue.action_contexts.erase(queue.action_contexts.begin() + index);
        }
    }
}

void World::set_custom_build_progress_(Unit& unit, f32 progress) {
    const bool was_complete = unit.is_complete();
    const bool indexed = unit.is_alive && !unit.cleanup_done;
    unit.build_progress = progress;
    unit.build_progress_secondary = progress;
    const bool is_complete = unit.is_complete();

    if (indexed && was_complete != is_complete && unit.income_rate > 0.0f) {
        if (is_complete) {
            total_income_rate_ += unit.income_rate;
        } else {
            total_income_rate_ =
                std::max(0.0f, total_income_rate_ - unit.income_rate);
        }
    }

    for (auto it = pending_builds_.begin(); it != pending_builds_.end(); ++it) {
        if (it->target_uid != unit.uid) continue;
        if (is_complete) {
            pending_builds_.erase(it);
        } else {
            it->progress = progress;
        }
        break;
    }
}

void World::clamp_custom_resources_(Unit& unit) {
    unit.energy = clamp(unit.energy, 0.0f, unit.max_energy);
    unit.shield = clamp(unit.shield, 0.0f, unit.max_shield);
    unit.hp = std::min(unit.hp, unit.max_hp);
    unit.ammo = std::max(0, unit.ammo);
}

f64 World::read_custom_resource_(const Unit& unit,
                                 CustomResource resource) const {
    switch (resource) {
    case CustomResource::CREDITS: return credits_;
    case CustomResource::AMMO: return unit.ammo;
    case CustomResource::ENERGY: return unit.energy;
    case CustomResource::HP: return unit.hp;
    case CustomResource::SHIELD: return unit.shield;
    case CustomResource::NONE: return 0.0;
    }
    return 0.0;
}

void World::write_custom_resource_(Unit& unit, CustomResource resource,
                                   f64 value) {
    switch (resource) {
    case CustomResource::CREDITS:
        credits_ = value;
        break;
    case CustomResource::AMMO:
        unit.ammo = static_cast<i32>(value);
        break;
    case CustomResource::ENERGY:
        unit.energy = static_cast<f32>(value);
        break;
    case CustomResource::HP:
        unit.hp = static_cast<f32>(value);
        break;
    case CustomResource::SHIELD:
        unit.shield = static_cast<f32>(value);
        break;
    case CustomResource::NONE:
        break;
    }
}

void World::reset_custom_stats_(Unit& unit, const UnitDef& base) {
    unit.mass = base.mass;
    unit.soft_collision_on_all = base.soft_collision_on_all;
    unit.max_energy = base.max_energy;
    unit.max_hp = base.hp;
    unit.max_shield = base.shield;
    unit.shield_regen_rate = base.shield_regen_rate;
    unit.shield_regen_per_frame = base.shield_regen_per_frame;
    unit.armour = base.armour;
    unit.weapon_range = base.range;
    unit.shoot_delay_multiplier = base.shoot_delay_multiplier;
    unit.shoot_damage_multiplier = base.shoot_damage_multiplier;
    unit.move_speed = base.speed * EngineConstants::FRAME_RATE;
    unit.turn_speed = base.turn_speed;
    unit.sight_range = base.sight_range;
    unit.nano_range = base.nano_range;
    unit.self_regen_per_frame = base.self_regen_rate;
    unit.self_regen_rate =
        unit.self_regen_per_frame * EngineConstants::FRAME_RATE;
    unit.target_height = base.target_height;
    unit.nano_factory_speed = base.nano_factory_speed;

    // custom.a.a.j only trims the two current pools explicitly after
    // restoring the shared base stat object.
    if (unit.hp > unit.max_hp) unit.hp = unit.max_hp;
    if (unit.shield > unit.max_shield) unit.shield = unit.max_shield;
}

f32 World::read_custom_stat_(const Unit& unit, CustomStat stat) const {
    switch (stat) {
    case CustomStat::MASS: return unit.mass;
    case CustomStat::MAX_ENERGY: return unit.max_energy;
    case CustomStat::ENERGY: return unit.energy;
    // custom.as stores these base fields as ints even though custom.j keeps
    // the corresponding live limits as floats.
    case CustomStat::MAX_HP: return static_cast<f32>(static_cast<i32>(unit.max_hp));
    case CustomStat::HP: return unit.hp;
    case CustomStat::MAX_SHIELD:
        return static_cast<f32>(static_cast<i32>(unit.max_shield));
    case CustomStat::SHIELD: return unit.shield;
    case CustomStat::SHIELD_REGEN:
        if (unit.shield_regen_rate ==
            unit.shield_regen_per_frame * EngineConstants::FRAME_RATE) {
            return unit.shield_regen_per_frame;
        }
        return unit.shield_regen_rate / EngineConstants::FRAME_RATE;
    case CustomStat::ARMOUR: return unit.armour;
    case CustomStat::MAX_ATTACK_RANGE: return unit.weapon_range;
    case CustomStat::SHOOT_DELAY_MULTIPLIER:
        return unit.shoot_delay_multiplier;
    case CustomStat::SHOOT_DAMAGE_MULTIPLIER:
        return unit.shoot_damage_multiplier;
    case CustomStat::MOVE_SPEED:
        return unit.move_speed / EngineConstants::FRAME_RATE;
    case CustomStat::MAX_TURN_SPEED: return unit.turn_speed;
    case CustomStat::FOG_OF_WAR_SIGHT_RANGE: return unit.sight_range;
    case CustomStat::NANO_RANGE: return static_cast<f32>(unit.nano_range);
    case CustomStat::SELF_REGEN_RATE:
        return unit.self_regen_per_frame;
    case CustomStat::TARGET_HEIGHT: return unit.target_height;
    case CustomStat::NANO_FACTORY_SPEED: return unit.nano_factory_speed;
    }
    return 0.0f;
}

void World::apply_custom_stat_(
        Unit& unit, const CustomStatAssignment& assignment) {
    f32 value = assignment.value;
    if (assignment.op == CustomStatOperator::ADD) {
        value = read_custom_stat_(unit, assignment.stat) + value;
    } else if (assignment.op == CustomStatOperator::SUBTRACT) {
        value = read_custom_stat_(unit, assignment.stat) - value;
    }
    switch (assignment.stat) {
    case CustomStat::MASS:
        unit.mass = value;
        break;
    case CustomStat::MAX_ENERGY:
        unit.max_energy = value;
        break;
    case CustomStat::ENERGY:
        unit.energy = value;
        break;
    case CustomStat::MAX_HP:
        unit.max_hp = value;
        break;
    case CustomStat::HP:
        unit.hp = value;
        break;
    case CustomStat::MAX_SHIELD:
        unit.max_shield = value;
        break;
    case CustomStat::SHIELD:
        unit.shield = value;
        break;
    case CustomStat::SHIELD_REGEN:
        unit.shield_regen_per_frame = value;
        unit.shield_regen_rate = value * EngineConstants::FRAME_RATE;
        break;
    case CustomStat::ARMOUR:
        unit.armour = value;
        break;
    case CustomStat::MAX_ATTACK_RANGE:
        unit.weapon_range = value;
        break;
    case CustomStat::SHOOT_DELAY_MULTIPLIER:
        unit.shoot_delay_multiplier = value;
        break;
    case CustomStat::SHOOT_DAMAGE_MULTIPLIER:
        unit.shoot_damage_multiplier = value;
        break;
    case CustomStat::MOVE_SPEED:
        unit.move_speed = value * EngineConstants::FRAME_RATE;
        break;
    case CustomStat::MAX_TURN_SPEED:
        unit.turn_speed = value;
        break;
    case CustomStat::FOG_OF_WAR_SIGHT_RANGE:
        unit.sight_range = static_cast<f32>(static_cast<i32>(value));
        break;
    case CustomStat::NANO_RANGE:
        unit.nano_range = static_cast<i32>(value);
        break;
    case CustomStat::SELF_REGEN_RATE:
        unit.self_regen_per_frame = value;
        unit.self_regen_rate = value * EngineConstants::FRAME_RATE;
        break;
    case CustomStat::TARGET_HEIGHT:
        unit.target_height = value;
        break;
    case CustomStat::NANO_FACTORY_SPEED:
        unit.nano_factory_speed = value;
        break;
    }
}

}  // namespace rwrl
