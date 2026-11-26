#include "internal.h"

namespace rwrl {

bool World::execute_custom_action_(
        Unit& unit, const Str& action_id, const Vec<World*>& other_worlds,
        bool* active_waypoint_consumed, const CustomActionContext* context,
        i32 recursion_depth) {
    if (recursion_depth > 10) return false;
    if (active_waypoint_consumed) *active_waypoint_consumed = false;
    CustomActionContext action_context;
    if (context) {
        action_context = *context;
    } else if (!unit.order_queue.empty()) {
        action_context.has_position = true;
        action_context.x = unit.order_queue.front().x;
        action_context.y = unit.order_queue.front().y;
        action_context.target_uid = unit.order_queue.front().target_uid;
        action_context.target_team_id =
            unit.order_queue.front().target_team_id;
    }
    const bool has_action_position = action_context.has_position;
    const f32 action_position_x = action_context.x;
    const f32 action_position_y = action_context.y;
    const CustomActionDef* action =
        registry_.get_custom_action(unit.unit_type, action_id);
    if (!action) return false;

    // custom.j executes the action's direct resource bundle before ac, the
    // ordered custom-effect list.
    const CustomResourceDelta& resources = action->add_resources;
    if (custom_actions::has_resource_delta(resources)) {
        credits_ += static_cast<f32>(resources.credits);
        unit.energy += resources.energy;
        unit.hp += resources.hp;
        unit.shield += resources.shield;
        unit.ammo += resources.ammo;
        // custom.d.b.e(am) unsets first, then sets overlapping flag bits.
        unit.custom_flags &= ~resources.unset_flags;
        unit.custom_flags |= resources.set_flags;
        clamp_custom_resources_(unit);
    }

    // custom.j runs fireTurretXAtGround before the ordered effect list. It
    // invokes the real projectile factory without energy/reload checks and
    // uses the position captured when the paid queue item was submitted.
    if (has_action_position && action->fire_turret_index >= 0 &&
        action->fire_turret_index < static_cast<i32>(unit.weapon_slots.size())) {
        Unit point{};
        point.x = action_position_x;
        point.y = action_position_y;
        for (i32 shot = 0; shot < action->fire_turret_count; ++shot) {
            create_weapon_projectile_(
                unit, unit.weapon_slots[action->fire_turret_index], point, true);
        }
    }

    // custom.a.a.o is the first registered ordered effect. Deletion scans
    // from the end once per requested unit and destroys the removed cargo.
    for (i32 deleted = 0;
         deleted < action->delete_num_units_from_transport &&
         !unit.cargo_units.empty();
         ++deleted) {
        const u32 cargo_uid = unit.cargo_units.back();
        unit.cargo_units.pop_back();
        remove_unit(cargo_uid);
    }

    if (action->start_unloading_transport && unit.cargo_capacity != 0) {
        // j.L() does not require a non-empty cargo list.
        unit.unloading_all = true;
        unit.unload_timer_frames = 30.0f;
    }

    if (action->force_unload_transport_now) {
        auto force_unload_slot = [&](std::size_t slot, bool alternate_side) {
            if (slot >= unit.cargo_units.size()) return;
            const u32 cargo_uid = unit.cargo_units[slot];
            Unit* cargo = get_unit(cargo_uid);
            unit.cargo_units.erase(unit.cargo_units.begin() +
                                   static_cast<std::ptrdiff_t>(slot));
            if (!cargo) return;

            // Defaults from custom.j.b(am,true,side): direction offset 180,
            // side offset 7, and move-away distance 70. The force flag skips
            // every terrain-placement test.
            const f32 cargo_rotation = unit.rotation + 180.0f;
            const f32 side = alternate_side ? -7.0f : 7.0f;
            cargo->x = unit.x +
                jar_cosine_degrees(unit.rotation + 90.0f) * side;
            cargo->y = unit.y +
                jar_sine_degrees(unit.rotation + 90.0f) * side;
            cargo->height = unit.height;
            cargo->rotation = normalize_angle(cargo_rotation);
            cargo->parent_uid = 0;
            cargo->target_uid = 0;
            cargo->target_team_id = -1;
            cargo->has_move_target = false;
            cargo->arrived = true;
            cargo->using_path = false;
            cargo->waypoints.clear();
            cargo->current_waypoint = 0;
            cargo->order_queue.clear();
            clear_navigation_orders(*cargo);
            restore_default_weapon_type(registry_, *cargo);
            cargo->collision_ignore_uid = unit.uid;
            cargo->collision_ignore_frames = 45.0f;
            grid_.add(*cargo);

            const f32 exit_angle =
                cargo_rotation + (alternate_side ? -7.0f : 7.0f);
            const f32 exit_x = cargo->x +
                jar_cosine_degrees(exit_angle) * 70.0f;
            const f32 exit_y = cargo->y +
                jar_sine_degrees(exit_angle) * 70.0f;
            UnitOrder exit_order;
            exit_order.type = WeaponType::MOVE;
            exit_order.x = exit_x;
            exit_order.y = exit_y;
            cargo->order_queue.push_back(exit_order);
            move_unit_to(*cargo, exit_x, exit_y);
        };

        for (std::size_t slot = unit.cargo_units.size(); slot-- > 0;) {
            if (action->force_unload_transport_now_only_on_slot != -1 &&
                action->force_unload_transport_now_only_on_slot !=
                    static_cast<i32>(slot)) {
                continue;
            }
            const bool alternate_side = unit.cargo_units.size() % 2 == 0;
            force_unload_slot(slot, alternate_side);
        }
    }

    // custom.a.a.b runs after the transport effect. It can consume the
    // trigger waypoint that is currently executing, so the caller must not
    // blindly erase the queue head a second time.
    if (action->clear_all_waypoints) {
        clear_current_order_state_(unit);
        unit.order_queue.clear();
        if (active_waypoint_consumed) *active_waypoint_consumed = true;
    } else if (action->clear_active_waypoint &&
               !unit.order_queue.empty()) {
        clear_current_order_state_(unit);
        unit.order_queue.erase(unit.order_queue.begin());
        while (!unit.order_queue.empty() &&
               !activate_order_(
                   unit, unit.order_queue.front(), other_worlds)) {
            unit.order_queue.erase(unit.order_queue.begin());
        }
        if (active_waypoint_consumed) *active_waypoint_consumed = true;
    }

    const CustomWaypointEffect& waypoint = action->add_waypoint;
    if (waypoint.enabled && waypoint.max_time != 0.0f &&
        unit.order_queue.size() < 29) {
        f32 target_x = unit.x;
        f32 target_y = unit.y;
        bool position_valid = true;
        if (waypoint.position_from_action) {
            if (has_action_position) {
                target_x = action_position_x;
                target_y = action_position_y;
            } else {
                position_valid = false;
            }
        } else {
            if (waypoint.has_position_offset_from_self) {
                target_x += waypoint.position_offset_from_self_x;
                target_y += waypoint.position_offset_from_self_y;
            }
            if (waypoint.has_position_relative_offset_from_self) {
                const f32 cosine = jar_cosine_degrees(unit.rotation);
                const f32 sine = jar_sine_degrees(unit.rotation);
                const f32 lateral =
                    waypoint.position_relative_offset_from_self_x;
                const f32 forward =
                    waypoint.position_relative_offset_from_self_y;
                target_x += cosine * forward - sine * lateral;
                target_y += sine * forward + cosine * lateral;
            }
        }

        if (position_valid) {
            UnitOrder order;
            order.type = waypoint.type;
            order.x = target_x;
            order.y = target_y;
            order.max_time_frames = waypoint.max_time;
            if (waypoint.prepend) {
                const bool remove_trigger_after_prepend =
                    !unit.order_queue.empty() &&
                    (unit.order_queue.front().type ==
                         WeaponType::TRIGGER_ACTION ||
                     unit.order_queue.front().type ==
                         WeaponType::TRIGGER_WHEN_IN_RANGE) &&
                    unit.order_queue.front().action_id == action_id;
                clear_current_order_state_(unit);
                unit.order_queue.insert(unit.order_queue.begin(), order);
                if (remove_trigger_after_prepend &&
                    unit.order_queue.size() > 1) {
                    unit.order_queue.erase(unit.order_queue.begin() + 1);
                }
                activate_order_(unit, unit.order_queue.front(), other_worlds);
                // The trigger waypoint that started this action has moved to
                // index one. The newly prepended waypoint is now active.
                if (active_waypoint_consumed) *active_waypoint_consumed = true;
            } else {
                const bool activate_now = unit.order_queue.empty();
                unit.order_queue.push_back(order);
                if (activate_now) {
                    activate_order_(unit, unit.order_queue.front(), other_worlds);
                }
            }
        }
    }

    // custom.a.a.k follows the waypoint effect. produceUnits first collects
    // all products and then runs the source unit's factory-exit E/F hooks;
    // spawnUnits leaves the bp positions unchanged.
    spawn_custom_unit_list_(
        unit, action->produce_units, true, other_worlds);
    spawn_custom_unit_list_(
        unit, action->spawn_units, false, other_worlds);

    // The resourceAmount loader runs before custom.a.a.j, which owns the
    // stats and deleteSelf effects. Its built-in setters do not clamp.
    const CustomResourceAmountEffect& resource_amount =
        action->resource_amount;
    if (resource_amount.resource != CustomResource::NONE) {
        f64 value = resource_amount.has_set_value
            ? resource_amount.set_value
            : read_custom_resource_(unit, resource_amount.resource);
        if (resource_amount.add_other_resource != CustomResource::NONE) {
            value += read_custom_resource_(
                unit, resource_amount.add_other_resource);
        }
        value *= static_cast<f64>(resource_amount.multiply_by);
        write_custom_resource_(unit, resource_amount.resource, value);
    }

    // convertResource uses each resource type's raw additive setter. These
    // setters do not clamp pools; ammo truncates toward zero after each add.
    const CustomResourceConversionEffect& conversion =
        action->resource_conversion;
    if (conversion.source != CustomResource::NONE &&
        conversion.target != CustomResource::NONE) {
        const f64 source_value =
            read_custom_resource_(unit, conversion.source);
        if (source_value > conversion.min_amount) {
            const f64 amount = std::min(
                source_value, conversion.max_amount);
            write_custom_resource_(
                unit, conversion.source,
                read_custom_resource_(unit, conversion.source) - amount);
            write_custom_resource_(
                unit, conversion.target,
                read_custom_resource_(unit, conversion.target) +
                    amount * static_cast<f64>(
                        conversion.multiply_amount_by));
        }
    }

    apply_take_resources_(
        unit, action->take_resources, other_worlds,
        action_context, recursion_depth);

    const UnitDef* base = registry_.get(unit.unit_type);
    if (action->reset_unit_stats && base) reset_custom_stats_(unit, *base);

    if (!action->set_unit_stats.empty()) {
        for (const CustomStatAssignment& assignment : action->set_unit_stats) {
            apply_custom_stat_(unit, assignment);
        }
        // VariableScope.CachedWriter is followed by d.b.d(am), once after
        // every ordered assignment has been evaluated.
        clamp_custom_resources_(unit);
    }

    // deleteSelf is a separate effect after reset/setUnitStats. The JAR keeps
    // iterating the effect list and still reaches convertTo afterwards.
    if (action->delete_self) remove_unit(unit.uid);

    if (action->has_set_body_rotation) {
        const f32 delta = shortest_angle_delta(
            unit.rotation, action->set_body_rotation);
        if (std::abs(delta) > 0.01f) {
            unit.rotation = normalize_angle(unit.rotation + delta);
        }
    }
    if (action->has_set_height) {
        unit.height = action->set_height;
    }

    // This final custom.a.a.j effect is registered after deleteSelf. Its
    // internal order is clear, queue removal/refund, cooldown additions,
    // setBuilt, then offsetSelfAbsolute.
    if (action->clear_all_action_cooldowns) {
        unit.action_cooldown_expiry_ticks.clear();
    }
    if (action->remove_all_queued_items_without_refund) {
        clear_factory_queue_(unit, false);
    }
    if (action->refund_all_queued_items) {
        clear_factory_queue_(unit, true);
    }
    add_custom_action_cooldown_(
        unit, "-1", action->add_all_action_cooldowns_time);
    if (action->add_action_cooldown_time > 0.0f) {
        if (action->add_action_cooldown_apply_to_actions.empty()) {
            add_custom_action_cooldown_(
                unit, action_id, action->add_action_cooldown_time);
        } else {
            for (const Str& target_action :
                    action->add_action_cooldown_apply_to_actions) {
                add_custom_action_cooldown_(
                    unit, target_action, action->add_action_cooldown_time);
            }
        }
    }
    if (action->set_built >= 0.0f) {
        set_custom_build_progress_(unit, action->set_built);
    }
    if (action->has_offset_self_absolute) {
        const bool indexed = unit.is_alive && !unit.cleanup_done;
        unit.x += action->offset_self_absolute_x;
        unit.y += action->offset_self_absolute_y;
        if (indexed) grid_.update(unit);
        unit.height += action->offset_self_absolute_height;
        if (indexed && unit.is_building) {
            rebuild_local_buildings_();
        }
    }

    if (!action->convert_to.empty()) {
        const UnitDef* target = registry_.get(action->convert_to);
        if (target) {
            const auto queue = factory_queues_.find(unit.uid);
            const i32 front_cost = queue != factory_queues_.end() &&
                !queue->second.queue.empty()
                ? factory_queue_item_cost_(unit, queue->second.queue.front()) : 0;
            convert_unit_definition_(unit, *target, true);
            if (unit.custom_unit) prune_custom_action_queue_(unit, front_cost);
        }
    }
    return true;
}

}  // namespace rwrl
