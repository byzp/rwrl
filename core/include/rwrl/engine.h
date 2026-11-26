#pragma once
#include <rwrl/common.h>
#include <rwrl/enums.h>
#include <rwrl/constants.h>
#include <rwrl/unit_def.h>
#include <rwrl/world.h>
#include <rwrl/map_loader.h>

namespace rwrl {

struct EngineState {
    i32 global_tick = 0;
    f32 game_time = 0.0f;
    f32 speed_mult = EngineConstants::DEFAULT_SPEED;
    f32 income_mult = 1.0f;
    Map<i32, WorldState> teams;
    bool paused = false;
    bool game_over = false;
    i32 winner_team = -1;
};

class Engine {
public:
    Engine();

    World& create_world(i32 team_id);
    void setup_teams(i32 team_a = 0, i32 team_b = 1);
    void setup_teams_vec(const Vec<i32>& team_ids);
    void load_map(const Str& path, bool spawn_map_units = true);
    // Native saves own their object shells and IDs independently of TMX
    // spawns. Load the terrain first, restore shells in saved list order,
    // then restore the shared allocator and rebuild occupancy once.
    Unit* restore_unit(const Str& name, bool custom, i32 team_id, u32 uid,
                       f32 x, f32 y);
    void finish_unit_restore(u32 next_uid, u64 formation_next_id = 1);
    void restore_auxiliary_object(f32 x, f32 y, bool scorch_mark,
                                  bool nuclear_scorch = false);
    void restore_projectile(const Projectile& projectile);
    // Resync runs after the native clock advance but before object updates.
    // Retain elapsed game time and optionally supply that turn's final clock.
    void restore_timeline(i32 frame, i32 clock_ms, f32 game_time,
                          bool clock_already_advanced = false);
    void queue_quick_resync() {
        quick_resync_pending_ = true;
        quick_resync_queued_frame_ = global_tick_;
    }
    void flush_quick_resync();

    void tick(f32 dt = 1.0f / EngineConstants::FRAME_RATE);
    void run_for(f32 seconds, f32 dt = 1.0f / EngineConstants::FRAME_RATE);

    EngineState get_state() const;

    World* get_world(i32 team_id);
    const World* get_world(i32 team_id) const;
    const Map<i32, World>& worlds() const { return worlds_; }
    Map<i32, World>& worlds_mut() { return worlds_; }

    UnitRegistry& registry() { return registry_; }
    const UnitRegistry& registry() const { return registry_; }

    i32 global_tick() const { return global_tick_; }
    f32 game_time() const { return game_time_; }
    f32 speed_mult() const { return speed_mult_; }
    void set_speed_mult(f32 v) { speed_mult_ = v; }
    f32 income_mult() const { return income_mult_; }
    void set_income_mult(f32 v) { income_mult_ = v; }

    void set_paused(bool p);
    void set_fog_enabled(bool enabled);
    void set_fog_mode(FogMode mode);
    FogMode fog_mode() const { return fog_mode_; }
    bool is_paused() const { return paused_; }
    bool has_map() const { return map_data_.has_value(); }
    void set_disable_ai(bool v) { disable_ai_ = v; }
    bool disable_ai() const { return disable_ai_; }
    void set_raw_commands(bool v) { raw_commands_ = v; }
    bool raw_commands() const { return raw_commands_; }
    void set_singleplayer_mode(bool enabled);
    bool singleplayer_mode() const { return singleplayer_mode_; }
    void set_nukes_enabled(bool enabled);
    bool nukes_enabled() const { return nukes_enabled_; }
    i32 map_width() const {
        return map_data_ ? map_data_->width : EngineConstants::MAP_WIDTH;
    }
    i32 map_height() const {
        return map_data_ ? map_data_->height : EngineConstants::MAP_HEIGHT;
    }
    f32 tile_size() const {
        return map_data_ ? map_data_->tile_size : EngineConstants::TILE_SIZE;
    }

    void set_unit_cap(i32 cap);
    i32 unit_cap() const { return unit_cap_; }

    void set_team_alliance(i32 team_id, i32 alliance_id);
    i32 team_alliance(i32 team_id) const;
    bool are_allied(i32 team_a, i32 team_b) const;

    void execute_command(i32 team_id, const GameCommand& cmd);

private:
    UnitRegistry registry_;
    Map<i32, World> worlds_;
    // Saved native players can authorize shared control without owning a
    // simulated world (for example an idle multiplayer participant).
    Map<i32, i32> player_alliances_;
    u32 next_unit_uid_ = 1;
    u64 next_object_order_ = 1;
    Vec<ScorchMark> scorch_marks_;
    i32 object_cost_refresh_frames_[5] = {-99, -99, -99, -99, -99};
    Vec<i8> object_cost_layers_[5];
    SP<PathClearanceState> clearance_state_ = mk<PathClearanceState>();
    i32 global_tick_ = 0;
    i32 object_update_sweep_counter_ = 0;
    f32 game_time_ = 0.0f;
    f32 speed_mult_ = EngineConstants::DEFAULT_SPEED;
    f32 income_mult_ = 1.0f;
    bool paused_ = false;
    bool disable_ai_ = false;
    bool raw_commands_ = false;
    bool singleplayer_mode_ = true;
    bool nukes_enabled_ = true;
    FogMode fog_mode_ = FogMode::NONE;
    bool game_over_ = false;
    i32 winner_team_ = -1;
    i32 unit_cap_ = EngineConstants::DEFAULT_UNIT_CAP;
    bool quick_resync_pending_ = false;
    i32 quick_resync_queued_frame_ = -1;
    std::optional<MapData> map_data_;

    u64 combined_building_revision_() const;
    void sync_building_pathing_();
    void refresh_spatial_index_();
};

}
