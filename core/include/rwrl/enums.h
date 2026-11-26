#pragma once
#include <rwrl/common.h>

namespace rwrl {

enum class MoveType : uint8_t {
    AIR = 0, LAND = 1, HOVER = 2, BUILDING = 3,
    NONE = 4, OVER_CLIFF = 5, OVER_CLIFF_WATER = 6, ROOT = 7,
    WATER = 8
};

enum class WeaponType : uint8_t {
    MOVE = 0, ATTACK = 1, BUILD = 2, REPAIR = 3,
    LOAD_INTO = 4, UNLOAD_AT = 5, RECLAIM = 6,
    ATTACK_MOVE = 7, LOAD_UP = 8, PATROL = 9, GUARD = 10,
    GUARD_AT = 11, TOUCH_TARGET = 12, FOLLOW = 13,
    TRIGGER_ACTION = 14, TRIGGER_WHEN_IN_RANGE = 15,
    SET_PASSIVE_TARGET = 16
};

enum class Behavior : uint8_t {
    OUT_OF_RANGE = 0, ONLY_IN_RANGE = 1, RETURN_FIRE = 2,
    HOLD_FIRE = 3, GUARD_AREA = 4, AGGRESSIVE = 5,
    MIXED = 6
};

// game.b.b map property "fog": none disables the team fog array, map keeps
// revealed tiles visible, and los periodically downgrades them to explored.
enum class FogMode : uint8_t {
    NONE = 0, MAP = 1, LOS = 2
};

enum class GameCommandAction : uint8_t {
    MOVE = 0, ATTACK = 1, BUILD = 2, REPAIR = 3,
    LOAD_INTO = 4, UNLOAD_AT = 5, RECLAIM = 6,
    ATTACK_MOVE = 7, LOAD_UP = 8, PATROL = 9, GUARD = 10, GUARD_AT = 11,
    TOUCH_TARGET = 12, FOLLOW = 13,
    TRIGGER_ACTION = 14, TRIGGER_ACTION_WHEN_IN_RANGE = 15,
    SET_PASSIVE_TARGET = 16,
    STOP = 17, UPGRADE = 18, LAUNCH_NUKE = 19,
    BUILD_NUKE = 20, BUILD_ANTINUKE = 21,
    UNLOAD_ALL = 22, CANCEL_UNLOAD = 23,
    SUBMERGE = 24, SURFACE = 25, CANCEL_QUEUE = 26, SET_RALLY = 27
};

enum class RoomState : uint8_t {
    WAITING = 0, LOADING = 1, PLAYING = 2, GAME_OVER = 3
};

enum class ConnectState : uint8_t {
    DISCONNECTED = 0, CONNECTING = 1, REGISTERED = 2, IN_GAME = 3
};

inline bool is_mobile(MoveType mt) {
    return mt != MoveType::BUILDING && mt != MoveType::NONE && mt != MoveType::ROOT;
}

inline bool has_weapon(WeaponType wt) {
    return wt == WeaponType::ATTACK || wt == WeaponType::ATTACK_MOVE ||
           wt == WeaponType::GUARD || wt == WeaponType::GUARD_AT ||
           wt == WeaponType::PATROL || wt == WeaponType::TOUCH_TARGET ||
           wt == WeaponType::LOAD_INTO || wt == WeaponType::LOAD_UP;
}

inline bool is_attack_weapon(WeaponType wt) {
    return wt == WeaponType::ATTACK || wt == WeaponType::ATTACK_MOVE ||
           wt == WeaponType::GUARD || wt == WeaponType::GUARD_AT ||
           wt == WeaponType::PATROL || wt == WeaponType::TOUCH_TARGET;
}

inline bool targets_air_only(WeaponType wt) {
    return wt == WeaponType::ATTACK;
}

}
