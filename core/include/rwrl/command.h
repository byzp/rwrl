#pragma once
#include <rwrl/common.h>
#include <rwrl/enums.h>

namespace rwrl {

struct ReplayPrepath {
    u32 unit_uid = 0;
    f32 start_x = 0.0f;
    f32 start_y = 0.0f;
    f32 target_x = 0.0f;
    f32 target_y = 0.0f;
    i32 frame = -99999;
    MoveType move_type = MoveType::NONE;
    bool has_path = false;
    Vec<std::pair<i16, i16>> tiles;
};

struct GameCommand {
    enum Action : u8 {
        MOVE = 0, ATTACK = 1, BUILD = 2, REPAIR = 3,
        LOAD_INTO = 4, UNLOAD_AT = 5, RECLAIM = 6,
        ATTACK_MOVE = 7, LOAD_UP = 8, PATROL = 9, GUARD = 10, GUARD_AT = 11,
        TOUCH_TARGET = 12, FOLLOW = 13,
        // These three values are the stock JAR waypoint protocol. Their
        // payload is custom-unit action data, which the standalone engine
        // does not model yet.
        TRIGGER_ACTION = 14, TRIGGER_ACTION_WHEN_IN_RANGE = 15,
        SET_PASSIVE_TARGET = 16,
        // Engine-only controls follow the JAR waypoint range.
        STOP = 17, UPGRADE = 18, LAUNCH_NUKE = 19,
        BUILD_NUKE = 20, BUILD_ANTINUKE = 21,
        UNLOAD_ALL = 22, CANCEL_UNLOAD = 23,
        SUBMERGE = 24, SURFACE = 25, CANCEL_QUEUE = 26, SET_RALLY = 27
    };

    Action action = MOVE;
    u32 unit_uid = 0;
    f32 target_x = 0.0f, target_y = 0.0f;
    u32 target_uid = 0;
    Str unit_type;
    // au.d is the serialized construction tier, independently of the type.
    i32 build_tech_level = 1;
    // game.units.a.c action id carried by triggerAction waypoints and by
    // direct unit-action commands. This is a distinct protocol field from
    // the build unit type.
    Str action_id;
    // Direct action buttons are distinct from triggerAction waypoints.
    bool direct_action = false;
    bool has_action_position = false;
    // Native e.p/e.q validate globally selected units independently of the
    // issuing e.i team. Ordinary API commands retain their owning-team route.
    bool native_selection = false;
    i32 control_player_team_id = -1;
    u16 shared_control_mask = 0;
    // gameFramework.e.e: append instead of replacing the current queue.
    bool queued = false;
    // gameFramework.e.o: alternate direct action, used by factory BUILD to
    // cancel the matching queued product and refund its upfront cost.
    bool action_alt = false;
    // gameFramework.e.o: clear all current orders before processing this command.
    bool clear_orders = false;
    // gameFramework.e.f: preserve BUILD/REPAIR entries while pruning the queue.
    bool clear_non_build_repair = false;
    // gameFramework.e.h: collapse a nearby tail MOVE/ATTACK_MOVE before append.
    bool merge_duplicate = false;
    // game.units.au.j: ab groups within 400 units regardless of speed/count.
    bool relaxed_formation = false;
    // gameFramework.e.b: ae && ah>1 sets prefer_clearance on the path request.
    // When >1, the solver applies ×7 clearance soft-cost weighting.
    u8 group_size = 1;
    // A selected JAR command is submitted through the Python API one unit at
    // a time. This index defers formation setup until the group is complete.
    u8 group_index = 0;
    // Selection order from the serialized command. The JAR visits selected
    // objects in this order after formation setup; replay alignment uses it
    // to reproduce the object-layer snapshot seen by shared path requests.
    Vec<u32> group_uids;
    // Native replay commands serialize the command-time path cache. When the
    // payload is present, use it verbatim instead of rebuilding it from the
    // later execution-time unit positions.
    bool replay_prepaths_provided = false;
    // A frame batch can replace this waypoint before any object update.
    // Apply its queue/formation changes without activating a runtime path.
    bool defer_waypoint_activation = false;
    bool has_replay_prepath = false;
    i32 replay_prepath_frame = -99999;
    f32 replay_prepath_target_x = 0.0f;
    f32 replay_prepath_target_y = 0.0f;
    MoveType replay_prepath_move_type = MoveType::NONE;
    Vec<std::pair<i16, i16>> replay_prepath_tiles;
    // ab.g is shared by every waypoint created by a selected command. y.a()
    // searches all entries by target, age, move type, and start proximity;
    // the serialized unit uid does not restrict which unit can reuse one.
    Vec<ReplayPrepath> replay_prepaths;
};

}
