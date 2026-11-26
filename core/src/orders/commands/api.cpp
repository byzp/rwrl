#include "world_internal.h"

#include <cstdio>
#include <cstdlib>

namespace rwrl {

void World::execute_command(const GameCommand& cmd) {
    const Vec<World*> no_other_worlds;
    execute_command(cmd, no_other_worlds);
}

void World::collect_command_formation_(
        const GameCommand& cmd, const Vec<World*>& other_worlds) {
    if (cmd.group_index == 0 || command_formation_local_start_) {
        pending_formation_uids_.clear();
    }

    Unit* unit = get_unit(cmd.unit_uid);
    if (unit && !unit->order_queue.empty()) {
        const UnitOrder& active = unit->order_queue.front();
        // au.c() resolves an expired saved object UID to null before e.k()
        // appends it. ab.c() still forms these target waypoints before their
        // handlers drop them, leaving links used by later commands/collisions.
        const bool unresolved_target =
            (cmd.action == GameCommand::ATTACK ||
             cmd.action == GameCommand::LOAD_INTO ||
             cmd.action == GameCommand::GUARD ||
             cmd.action == GameCommand::FOLLOW) && cmd.target_uid != 0 &&
            active.target_uid == 0 &&
            active.formation_group_id == active_command_formation_group_id_ &&
            !find_order_target_world_(cmd.target_uid, other_worlds);
        const bool same_target = cmd.target_uid != 0
            ? active.target_uid == cmd.target_uid || unresolved_target
            : active.x == cmd.target_x && active.y == cmd.target_y;
        if (active.type == static_cast<WeaponType>(cmd.action) && same_target) {
            pending_formation_uids_.push_back(unit->uid);
        }
    }

    if (!command_formation_commit_deferred_ &&
        static_cast<i32>(cmd.group_index) + 1 >=
        std::max(1, static_cast<i32>(cmd.group_size))) {
        configure_command_formation_(cmd, other_worlds);
        pending_formation_uids_.clear();
        command_path_snapshot_active_ = false;
        command_path_prediction_snapshot_.clear();
        command_pending_path_uids_.clear();
    }
}

} // namespace rwrl
