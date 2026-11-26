#include <rwrl/unit_def.h>

#include <cctype>

namespace rwrl {

const UnitDef* UnitRegistry::get(const Str& name) const {
    auto it = defs_.find(name);
    if (it != defs_.end()) return &it->second;
    auto custom_it = custom_defs_.find(name);
    return custom_it != custom_defs_.end() ? &custom_it->second : nullptr;
}

bool UnitRegistry::register_unit(const UnitDef& definition, bool custom) {
    if (definition.name.empty()) return false;
    if (custom) {
        if (defs_.find(definition.name) != defs_.end() ||
            map_defs_.find(definition.name) != map_defs_.end()) {
            return false;
        }
        custom_defs_[definition.name] = definition;
        return true;
    }
    if (custom_defs_.find(definition.name) != custom_defs_.end()) return false;
    defs_[definition.name] = definition;
    return true;
}

UnitDef* UnitRegistry::register_custom_unit_from(const Str& name,
                                                 const Str& base_name) {
    if (name.empty() || defs_.find(name) != defs_.end() ||
        map_defs_.find(name) != map_defs_.end()) {
        return nullptr;
    }
    const UnitDef* base = get(base_name);
    if (!base) return nullptr;
    UnitDef definition = *base;
    definition.name = name;
    // A replacement INI creates custom.j, whose inherited y.bh() returns
    // zero even when the replaced stock experimentalTank overrides it.
    definition.path_min_clearance = 0;
    custom_defs_[name] = std::move(definition);
    return &custom_defs_.at(name);
}

bool UnitRegistry::register_custom_action(const Str& unit_name,
                                          const CustomActionDef& action) {
    if (!is_custom(unit_name) || action.id.empty()) return false;
    UnitDef* unit = nullptr;
    auto unit_it = custom_defs_.find(unit_name);
    if (unit_it != custom_defs_.end()) unit = &unit_it->second;
    else {
        auto builtin_it = defs_.find(unit_name);
        if (builtin_it != defs_.end()) unit = &builtin_it->second;
    }
    if (!unit) return false;
    const CustomResourceConversionEffect& conversion =
        action.resource_conversion;
    const bool has_conversion_source =
        conversion.source != CustomResource::NONE;
    const bool has_conversion_target =
        conversion.target != CustomResource::NONE;
    const CustomWaypointEffect& waypoint = action.add_waypoint;
    const CustomTakeResourcesEffect& take = action.take_resources;
    const bool waypoint_position_type =
        waypoint.type == WeaponType::MOVE ||
        waypoint.type == WeaponType::ATTACK_MOVE;
    const bool has_waypoint_position =
        waypoint.position_from_action ||
        waypoint.has_position_offset_from_self ||
        waypoint.has_position_relative_offset_from_self;
    auto valid_spawn_list = [&](const Vec<CustomUnitSpawn>& entries) {
        for (const CustomUnitSpawn& entry : entries) {
            if (entry.unit_type.empty() || !get(entry.unit_type)) return false;
        }
        return true;
    };
    if (action.price < 0 || action.build_speed <= 0.0f ||
        action.set_built > 1.0f ||
        (action.remove_all_queued_items_without_refund &&
         action.refund_all_queued_items) ||
        (!action.add_action_cooldown_apply_to_actions.empty() &&
         action.add_action_cooldown_time <= 0.0f) ||
        (action.force_unload_transport_now_only_on_slot != -1 &&
         !action.force_unload_transport_now) ||
        has_conversion_source != has_conversion_target ||
        (waypoint.enabled &&
         (!waypoint_position_type || !has_waypoint_position ||
          waypoint.max_time < -1.0f)) ||
        !valid_spawn_list(action.produce_units) ||
        !valid_spawn_list(action.spawn_units) ||
        (take.enabled &&
         (take.max_units <= 0 ||
          take.include_units_within_range < -1.0f ||
          take.direct_transfer_stopping_at_zero ||
          (!take.include_units_in_transport && !take.include_parent &&
           take.include_units_within_range < 0.0f))) ||
        (has_conversion_source &&
         (conversion.min_amount < 0.0 || conversion.max_amount < 0.0 ||
          conversion.max_amount < conversion.min_amount))) {
        return false;
    }
    unit->custom_actions[action.id] = action;
    return true;
}

const CustomActionDef* UnitRegistry::get_custom_action(
        const Str& unit_name, const Str& action_id) const {
    if (!is_custom(unit_name)) return nullptr;
    const UnitDef* unit = get(unit_name);
    if (!unit) return nullptr;
    auto action_it = unit->custom_actions.find(action_id);
    return action_it != unit->custom_actions.end()
        ? &action_it->second : nullptr;
}

bool UnitRegistry::is_custom(const Str& name) const {
    // These shipped replacements are custom.j instances in the JAR even
    // though they remain in defs_ to expose their canonical production names.
    // The runtime class matters for behavior such as game.f's stock-building
    // impact-radius check (target instanceof units.d.d).
    return name == "scout" || name == "extractor" || name == "fabricator" ||
           custom_defs_.find(name) != custom_defs_.end();
}

const UnitDef* UnitRegistry::get_map_unit(const Str& name) const {
    if (const UnitDef* canonical = get(name)) return canonical;
    for (const auto& [candidate, definition] : defs_) {
        if (candidate.size() != name.size()) continue;
        bool matches = true;
        for (std::size_t i = 0; i < name.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(candidate[i])) !=
                std::tolower(static_cast<unsigned char>(name[i]))) {
                matches = false;
                break;
            }
        }
        if (matches) return &definition;
    }
    auto it = map_defs_.find(name);
    if (it != map_defs_.end()) return &it->second;
    return nullptr;
}

Vec<Str> UnitRegistry::all_names() const {
    Vec<Str> names;
    names.reserve(defs_.size());
    for (auto& [k, _] : defs_) names.push_back(k);
    return names;
}

bool UnitRegistry::is_building(const Str& name) const {
    auto* d = get(name);
    return d ? d->is_building : false;
}

i32 UnitRegistry::get_cost(const Str& name) const {
    auto* d = get(name);
    return d ? d->cost : 0;
}

i32 UnitRegistry::get_build_cost(const Str& name, i32 tech_level) const {
    const UnitDef* definition = get(name);
    if (!definition) return 0;
    // Custom metadata's intrinsic techLevel does not request a stock upgrade.
    if (!is_custom(name)) {
        const auto price = definition->construction_tech_costs.find(tech_level);
        if (price != definition->construction_tech_costs.end()) return price->second;
    }
    return definition->cost;
}

f32 UnitRegistry::get_income(const Str& name) const {
    auto* d = get(name);
    return d ? d->income : 0.0f;
}

f32 UnitRegistry::get_build_speed(const Str& name) const {
    auto* d = get(name);
    return d ? d->build_speed : 0.0f;
}

bool UnitRegistry::has_upgrade(const Str& name) const {
    auto* d = get(name);
    return d && !d->upgrade_to.empty();
}

Str UnitRegistry::get_upgrade_target(const Str& name) const {
    auto* d = get(name);
    return d ? d->upgrade_to : "";
}

bool UnitRegistry::can_build_unit(const Str& factory_name, const Str& unit_name,
                                  i32 build_tech_level) const {
    auto* d = get(factory_name);
    if (!d) return false;
    if (d->can_build.empty()) return false;
    for (auto& n : d->can_build) {
        if (n != unit_name) continue;
        if (build_tech_level == -1) return true;
        const auto levels = d->build_tech_levels.find(unit_name);
        if (levels == d->build_tech_levels.end()) {
            return !d->has_declared_build_menu &&
                d->build_tech_levels.empty() && build_tech_level == 1;
        }
        return std::find(levels->second.begin(), levels->second.end(),
                         build_tech_level) != levels->second.end();
    }
    return false;
}

const Vec<Str>& UnitRegistry::get_can_build(const Str& name) const {
    static const Vec<Str> empty;
    auto* d = get(name);
    return d ? d->can_build : empty;
}

} // namespace rwrl
