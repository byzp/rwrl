#pragma once
#include <rwrl/common.h>
#include <rwrl/enums.h>
#include <rwrl/constants.h>
#include <rwrl/unit_def.h>
#include <rwrl/unit.h>
#include <rwrl/projectile.h>
#include <rwrl/spatial.h>
#include <rwrl/pathfinder.h>
#include <rwrl/command.h>
#include <rwrl/map_loader.h>

namespace rwrl {

struct ScorchMark {
    f32 x = 0.0f;
    f32 y = 0.0f;
    bool nuclear = false;
};

struct CustomActionContext {
    bool has_position = false;
    f32 x = 0.0f;
    f32 y = 0.0f;
    u32 target_uid = 0;
    i32 target_team_id = -1;
};

struct FactoryQueueItemState {
    f32 progress = 0.0f;
    f32 resource_progress = 0.0f;
    i32 ammo_cost = 0;
    bool high_priority = false;
};

struct FactoryQueue {
    u32 factory_uid = 0;
    Vec<Str> queue;
    // Ordinary action prices are consumed when each item is accepted.
    Vec<f32> upfront_costs;
    Vec<CustomActionContext> action_contexts;
    // Native d.j.m/n remain with an interrupted item while high-priority
    // actions temporarily replace it at the front of d.k.c.
    Vec<FactoryQueueItemState> item_states;
    bool head_changed = false;
    f32 progress = 0.0f;
    // Optional streaming resource bundles are consumed in 0.01 increments.
    f32 resource_progress = 0.0f;
    f32 build_speed = 0.001f;
    f32 rally_x = 0.0f, rally_y = 0.0f;
    bool has_rally = false;
    void remember_head() {
        if (!head_changed && !item_states.empty()) {
            item_states.front().progress = progress;
            item_states.front().resource_progress = resource_progress;
        }
    }
};

// start_build() is also used as a synthetic scene injection API. Commanded
// construction marks the task as requiring one or more nearby builders.
struct BuildTask {
    f32 progress = 0.0f;
    u32 target_uid = 0;
    bool requires_builder = false;
    bool completion_accounted = false;
};

struct WorldState {
    i32 team_id = 0;
    i32 tick = 0;
    f32 credits = 0.0f;
    f32 total_income_rate = 0.0f;
    f32 income_per_sec = 0.0f;
    f32 total_income = 0.0f;
    f32 total_spent = 0.0f;
    i32 active_units = 0;
    i32 total_units = 0;
    i32 buildings = 0;
    i32 pending_builds = 0;
    i32 projectiles = 0;
    i32 kills = 0;
    i32 losses = 0;
    i32 units_built = 0;
    i32 buildings_completed = 0;
    i32 factory_queue_size = 0;
    i32 unit_cap = 200;
};

class World {
public:
    World(i32 team_id, const UnitRegistry& registry,
          u32* global_next_uid = nullptr,
          u64* global_next_object_order = nullptr,
          i32* global_object_cost_refresh_frames = nullptr,
          Vec<i8>* global_object_cost_layers = nullptr,
          Vec<ScorchMark>* global_scorch_marks = nullptr,
          SP<PathClearanceState> global_clearance_state = nullptr);

    Unit* spawn_unit(const Str& name, f32 x, f32 y, bool start_built = false);
    Unit* spawn_map_unit(const Str& map_name, f32 x, f32 y);
    Unit* get_unit(u32 uid);
    const Unit* get_unit(u32 uid) const;
    void restore_factory_queue(const FactoryQueue& queue);
    void remove_unit(u32 uid);

    void add_credits(f32 amount);
    bool spend_credits(f32 amount);
    void refund_credits(f32 amount, f32 rate);
    f32 compute_income(f32 dt, f32 speed_mult, f32 income_mult);
    void update_unit_income(f32 dt, f32 speed_mult, f32 income_mult,
                            u32 unit_uid);
    void set_ai_player(bool value, i32 difficulty = 0) {
        ai_player_ = value;
        ai_difficulty_ = difficulty;
    }
    bool is_ai_player() const { return ai_player_; }
    i32 ai_difficulty() const { return ai_difficulty_; }
    f32 ai_income_multiplier() const;

    Unit* start_build(const Str& name, f32 x, f32 y);
    void update_builds(f32 dt, f32 speed_mult);
    void update_unit_builds(f32 dt, f32 speed_mult, u32 unit_uid);
    bool queue_unit(u32 factory_uid, const Str& unit_type);
    bool cancel_factory_queue(u32 factory_uid, const Str& unit_type = "");
    i32 custom_action_cooldown_remaining(u32 unit_uid,
                                         const Str& action_id) const;
    bool trigger_custom_action(Unit& unit, const Str& action_id);
    bool queue_nuke(u32 launcher_uid);
    bool queue_antinuke(u32 launcher_uid);
    bool launch_nuke(Unit& launcher, f32 x, f32 y, i32 target_team_id = -1);
    bool set_factory_rally(u32 factory_uid, f32 x, f32 y);
    void cancel_build(Unit& u);
    void reclaim_unit(Unit& u);

    bool unit_attack(Unit& attacker, Unit& target);
    void update_projectiles(f32 dt, World& enemy_world);
    void update_projectiles(f32 dt, const Vec<World*>& enemy_worlds);
    void update_projectile(f32 dt, u32 projectile_uid,
                           const Vec<World*>& enemy_worlds);
    void auto_combat(World& enemy_world);
    void auto_combat(const Vec<World*>& enemy_worlds);
    void update_unit_combat(u32 unit_uid, const Vec<World*>& enemy_worlds);
    void publish_single_attack_move_proposal_(
        u32 unit_uid, const Vec<World*>& enemy_worlds);

    void update_movement(f32 dt);
    void update_movement(f32 dt, const Vec<World*>& other_worlds);
    void update_unit_movement(f32 dt, u32 unit_uid,
                              const Vec<World*>& other_worlds);
    // target_size mirrors k.k.n and lets the solver stop within that
    // Chebyshev distance of the goal.
    void move_unit_to(Unit& u, f32 x, f32 y, i32 target_size);
    void move_unit_to(Unit& u, f32 x, f32 y);
    void attack_move_unit_to(Unit& u, f32 x, f32 y);
    // game.units.y.q(am): derive k.k.n from the attacker's effective range.
    i32 attack_path_target_size(const Unit& attacker,
                                const Unit* target) const;

    void patrol_unit_to(Unit& u, f32 x, f32 y);
    void guard_unit(Unit& u, Unit& target);
    void guard_unit(Unit& u, World& target_world, Unit& target);
    void guard_unit_at(Unit& u, f32 x, f32 y);
    void follow_unit(Unit& u, Unit& target);
    void follow_unit(Unit& u, World& target_world, Unit& target);
    void attack_unit(Unit& u, Unit& target);
    void touch_target(Unit& u, Unit& target);
    void repair_unit(Unit& healer, Unit& target, f32 delta_frames = 1.0f);
    void repair_unit(Unit& healer, World& target_world, Unit& target,
                     f32 delta_frames = 1.0f);
    void reclaim_target(Unit& u, Unit& target, f32 delta_frames = 1.0f);
    void reclaim_target(Unit& u, World& target_world, Unit& target,
                        f32 delta_frames = 1.0f);
    bool can_repair_target(const Unit& source, const Unit& target) const;
    bool can_reclaim_target(const Unit& source, const Unit& target) const;
    void load_into(Unit& transport, Unit& cargo);
    void unload_at(Unit& transport, f32 x, f32 y);
    void begin_unload_all(Unit& transport);
    void cancel_unload_all(Unit& transport);
    void set_submerged(Unit& unit, bool submerged);
    void upgrade_unit(Unit& u, const Str& requested_target = "");
    void stop_unit(Unit& u);

    // Apply the native replay's team surrender action. The team update phase
    // consumes this flag after the current object pass, matching game.n.f().
    void surrender();

    void execute_command(const GameCommand& cmd);
    void execute_command(const GameCommand& cmd,
                         const Vec<World*>& other_worlds);

    void update_shields(f32 dt);
    void update_unit_shields(f32 dt, u32 unit_uid);
    void update_energy(f32 dt);
    void update_unit_energy(f32 dt, u32 unit_uid,
                            const UnitDef* update_definition = nullptr);
    void update_laser_defence(f32 dt, const Vec<World*>& enemy_worlds);
    void update_unit_laser_defence(f32 dt, u32 unit_uid,
                                   const Vec<World*>& enemy_worlds);
    void update_ammo(f32 dt, f32 speed_mult);
    void update_unit_ammo(f32 dt, f32 speed_mult, u32 unit_uid);
    void update_behaviors(World& enemy_world);
    void update_command_behaviors(f32 delta_frames);
    void update_command_behaviors(f32 delta_frames, const Vec<World*>& other_worlds);
    void update_unit_command_behaviors(
        f32 delta_frames, u32 unit_uid, const Vec<World*>& other_worlds);
    void update_patrols();
    void update_builder_behavior();
    void update_repair_bay(f32 delta_frames, u32 unit_uid = 0);
    void update_repair_bay(f32 delta_frames, u32 unit_uid,
                           const Vec<World*>& other_worlds);
    void update_unit_auto_repair(f32 delta_frames, u32 unit_uid,
                                 const Vec<World*>& other_worlds);
    void update_transport_unloading(f32 dt, f32 speed_mult);
    void update_unit_transport_unloading(f32 dt, f32 speed_mult,
                                         u32 unit_uid);
    // Runs the portions of am.a(float) that belong to dead wrecks and
    // short-lived map objects. Returns false when the ordinary live-unit
    // update must be skipped for this object turn.
    bool update_unit_lifecycle(f32 delta_frames, u32 unit_uid);
    void update_flying_states(f32 dt, f32 speed_mult);
    void update_unit_flying_states(f32 dt, f32 speed_mult, u32 unit_uid);
    void update_submersible_states(f32 dt, f32 speed_mult);
    void update_unit_submersible_states(f32 dt, f32 speed_mult, u32 unit_uid);
    void update_factory_production(f32 dt, f32 speed_mult);
    void update_unit_factory_production(f32 dt, f32 speed_mult,
                                        u32 factory_uid,
                                        const Vec<World*>& other_worlds = {});
    void update_collision(f32 dt);
    void update_collision(World& other, f32 dt);
    void update_collision(f32 dt, const Vec<World*>& worlds);
    void update_antinuke_interception(f32 dt, const Vec<World*>& enemy_worlds);
    void update_unit_antinuke_interception(
        f32 dt, u32 unit_uid, const Vec<World*>& enemy_worlds);
    void update_unit_runtime_precombat(f32 dt, u32 unit_uid);
    void update_unit_legs(f32 delta_frames, u32 unit_uid);
    void apply_unit_collision_impulse(u32 unit_uid);
    void update_unit_passive_health(f32 dt, u32 unit_uid);
    void update_unit_cooldowns(f32 dt, u32 unit_uid);
    void update_unit_service_decay(f32 delta_frames, u32 unit_uid);
    void update_shield_generator();
    void update_dynamic_map_units(f32 delta_frames,
                                  const Vec<World*>& enemy_worlds,
                                  u32 unit_uid = 0);

    bool check_victory() const;
    bool is_cc_alive() const;

    void step(f32 dt, f32 speed_mult, f32 income_mult, World* enemy_world);
    void step(f32 dt, f32 speed_mult, f32 income_mult, const Vec<World*>& enemy_worlds);
    void begin_step(f32 dt, f32 speed_mult, f32 income_mult,
                    const Vec<World*>& enemy_worlds);
    void finish_step(f32 dt, f32 speed_mult,
                     const Vec<World*>& enemy_worlds);

    WorldState get_state() const;

    const Map<u32, Unit>& units() const { return units_; }
    Map<u32, Unit>& units_mut() { return units_; }
    const Map<u32, Projectile>& projectiles() const { return projectiles_; }
    // The stock player stores credits in a double.  Keep the public value at
    // that precision so spend/income thresholds retain the same residuals;
    // WorldState still exposes the legacy float observation below.
    f64 credits() const { return credits_; }
    i32 team_id() const { return team_id_; }
    i32 alliance_id() const { return alliance_id_; }
    void set_alliance_id(i32 alliance_id) { alliance_id_ = alliance_id; }
    bool is_allied_with(const World& other) const {
        // game.n.i (team -1) is neutral: it is allied only with itself and is
        // neither allied nor hostile to ordinary/aggressive teams.
        if (team_id_ == -1 || other.team_id_ == -1) {
            return team_id_ == -1 && other.team_id_ == -1;
        }
        return alliance_id_ == other.alliance_id_;
    }
    bool is_enemy_with(const World& other) const {
        if (team_id_ == -1 || other.team_id_ == -1) return false;
        return alliance_id_ != other.alliance_id_;
    }
    i32 tick() const { return tick_; }
    i32 simulation_clock_ms() const { return simulation_clock_ms_; }
    u32 cc_uid() const { return cc_uid_; }
    void set_cc_uid(u32 uid) { cc_uid_ = uid; }
    f32 total_income_rate() const { return total_income_rate_; }
    Pathfinder& pathfinder() { return pathfinder_; }
    const Pathfinder& pathfinder() const { return pathfinder_; }
    void apply_map(const MapData& map);
    i32 map_width() const { return pathfinder_.width(); }
    i32 map_height() const { return pathfinder_.height(); }
    f32 tile_size() const { return pathfinder_.tile_size(); }
    u8 terrain(i32 tx, i32 ty) const { return pathfinder_.terrain(tx, ty); }
    bool is_resource_pool(i32 tx, i32 ty) const {
        return pathfinder_.is_resource_pool_tile(tx, ty);
    }
    bool blocks_buildings(i32 tx, i32 ty) const {
        return pathfinder_.blocks_buildings(tx, ty);
    }
    i8 path_cost(i32 tx, i32 ty) const { return pathfinder_.path_cost(tx, ty); }

    i32 unit_cap() const {
        if (unit_cap_ >= 0) return unit_cap_;
        if (total_income_rate_ > 0.0f)
            return static_cast<i32>(total_income_rate_);
        return 9999;
    }
    void set_unit_cap(i32 cap) {
        unit_cap_ = cap;
    }
    i32 count_active_units() const;

    void set_paused(bool p) { paused_ = p; }
    void set_disable_ai(bool v) { disable_ai_ = v; }
    void set_raw_commands(bool v) { raw_commands_ = v; }
    void set_singleplayer_mode(bool enabled) { singleplayer_mode_ = enabled; }
    void set_nukes_enabled(bool enabled) { nukes_enabled_ = enabled; }
    bool nukes_enabled() const { return nukes_enabled_; }
    bool is_paused() const { return paused_; }
    void set_fog_enabled(bool enabled);
    void set_fog_mode(FogMode mode);
    FogMode fog_mode() const { return fog_mode_; }
    bool fog_enabled() const { return fog_mode_ != FogMode::NONE; }
    bool can_see(const Unit& target) const;
    bool is_explored(f32 x, f32 y) const;

private:
    friend class Engine;

    void refresh_unit_spatial_index_(Unit& unit);
    void retain_collision_unit_(Unit& unit);
    Unit* waypoint_target_(u32 uid, bool retain_removed);

    i32 team_id_;
    // game.n.r is distinct from the player slot (game.n.k). Constructors
    // initialize it to the player id, so every team starts hostile by default.
    i32 alliance_id_;
    const UnitRegistry& registry_;
    Map<u32, Unit> units_;
    // Native aJ arrays keep the actual am object after it leaves the global
    // unit list. Cached pairs share its accumulated displacement until the
    // last live unit releases that reference on a later candidate refresh.
    // Bomber ATTACK waypoints can also retain a removed corpse while fleeing.
    // In-flight projectiles retain both their target and their shooter.
    Map<u32, Unit> retained_collision_units_;
    u32 next_uid_ = 1;
    u32* global_next_uid_ = nullptr;
    u64 next_object_order_ = 1;
    u64* global_next_object_order_ = nullptr;
    Vec<ScorchMark> scorch_marks_;
    Vec<ScorchMark>* global_scorch_marks_ = nullptr;
    f64 credits_ = EngineConstants::STARTING_CREDITS;
    f32 total_income_rate_ = 0.0f;
    u32 cc_uid_ = 0;
    bool surrendered_ = false;
    i32 surrender_start_clock_ms_ = -1;

    Vec<BuildTask> pending_builds_;
    Map<u32, FactoryQueue> factory_queues_;
    Map<u32, Projectile> projectiles_;
    // e.c.K(i) still reads a removed beam's sweep before the owner's next
    // active hover update clears its reference. It is outside w.er meanwhile.
    Map<u32, Projectile> retired_beam_projectiles_;
    u32 next_pid_ = 100000;

    u64 allocate_object_order_() {
        return global_next_object_order_
            ? (*global_next_object_order_)++
            : next_object_order_++;
    }

    SpatialGrid grid_;
    Pathfinder pathfinder_;

    i32 tick_ = 0;
    i32 object_update_sweep_counter_ = 0;
    i32 simulation_clock_ms_ = 0;
    std::optional<i32> next_simulation_clock_ms_;
    bool step_in_progress_ = false;
    i32 action_cooldown_tick_ = 0;
    f32 action_cooldown_tick_remainder_ = 0.0f;
    f32 speed_mult_ = EngineConstants::DEFAULT_SPEED;
    f32 income_mult_ = 1.0f;
    bool ai_player_ = false;
    i32 ai_difficulty_ = 0;
    f32 total_income_ = 0.0f;
    f32 total_spent_ = 0.0f;
    i32 kills_ = 0;
    i32 losses_ = 0;
    i32 units_built_ = 0;
    i32 buildings_completed_ = 0;
    i32 unit_cap_ = EngineConstants::DEFAULT_UNIT_CAP;
    bool paused_ = false;
    FogMode fog_mode_ = FogMode::NONE;
    // game.n.N stores 10 for unexplored, 5 for explored but outside the
    // current LOS, and 0..4 for currently visible map tiles.
    Vec<u8> fog_tiles_;
    f32 fog_refresh_frames_ = 0.0f;
    bool map_loaded_ = false;
    // Monotonic marker used by Engine to refresh the JAR-global building
    // cost layer after a building is created, destroyed, converted or moved.
    u64 building_revision_ = 0;
    // gameFramework.k.i.k is shared by all teams. Standalone World instances
    // use this local fallback; Engine worlds point at the engine-owned table.
    i32 object_cost_refresh_frame_[5] = {-99, -99, -99, -99, -99};
    i32* global_object_cost_refresh_frames_ = nullptr;
    Vec<i8>* global_object_cost_layers_ = nullptr;
    SP<PathClearanceState> global_clearance_state_;
    // Each World has its own Pathfinder storage and applies a shared refresh
    // version lazily before its next request.
    i32 object_cost_applied_frame_[5] = {-100, -100, -100, -100, -100};
    // Unit maps that fed the last cross-world pathing sync; used to rebuild
    // the object-cost layer lazily. World objects live in Engine's node-based
    // map, so the pointers stay valid between syncs.
    Vec<const Map<u32, Unit>*> object_cost_sources_;
    // The same sync in mutable form. am.bF() is one global object list, so
    // y.br() reaches wrecks that belong to another team.
    Vec<World*> peer_worlds_;

    f32 combat_delta_frames_ = 1.0f;
    f32 ai_timer_ = 0.0f;
    bool disable_ai_ = false;
    bool raw_commands_ = false;
    bool singleplayer_mode_ = true;
    bool nukes_enabled_ = true;
    Vec<u32> pending_formation_uids_;
    // Engine batches a shared-control selection across its owners. Each
    // owner starts its own command state; formation commits wait until the
    // last selected unit has received the waypoint.
    bool command_formation_local_start_ = false;
    bool command_formation_commit_deferred_ = false;
    bool command_formation_clear_prepared_ = false;
    u64 next_formation_group_id_ = 1;
    u64 next_formation_controller_id_ = 1;
    u64 active_command_formation_group_id_ = 0;
    bool formation_rebuild_active_ = false;
    // JAR builds every shared command prepath before activating any selected
    // unit. Sequential public commands retain the group's pre-command cK
    // values so an earlier member cannot disappear from a later cost layer.
    bool command_path_snapshot_active_ = false;
    bool command_path_request_active_ = false;
    // y's formation recovery helper submits its own path after the ATTACK
    // handler has chosen projection. That request bypasses the chase gate.
    bool formation_fallback_path_request_active_ = false;
    Map<u32, bool> command_path_prediction_snapshot_;
    // az()/aH()/ab.c() retain requests submitted before this command.
    // Eager command activation can also create new requests that a linked
    // follower must discard once its formation has been assigned.
    Set<u32> command_pending_path_uids_;
    // A formation-leader handoff is submitted in finish_step after the
    // former followers have already taken their object turns. The JAR path
    // layer still observes cK from the beginning of that frame.
    bool handoff_path_request_active_ = false;
    Map<u32, bool> step_start_prediction_snapshot_;
    Map<u32, bool> handoff_command_prediction_snapshot_;
    i32 handoff_command_snapshot_tick_ = -1;

    void update_visibility_(f32 delta_frames,
                            const Vec<World*>& other_worlds);
    void update_aircraft_wrecks_(f32 delta_frames, u32 only_unit_uid = 0);
    bool try_create_scorch_mark_(f32 x, f32 y, bool nuclear = false);
    void reveal_visibility_(const Unit& observer);
    void decay_visibility_();
    void rebuild_local_buildings_();
    void rebuild_pathing_from_worlds_(const Vec<World*>& worlds);
    void clear_placement_area_(const Unit& placed, const UnitDef& definition);
    static void flush_unit_list_(const Vec<World*>& worlds);
    void queue_wreck_removal_(Unit& unit);
    void fell_tree_(Unit& tree, bool damage_death = false);
    void update_surrender_();
    static void sweep_wrecks_(const Vec<World*>& worlds);

    void resolve_collision_pair_(Unit& a, World& other_world, Unit& b,
                                 f32 delta_frames);
    void apply_projectile_damage_(Projectile& p, const Vec<World*>& enemy_worlds);
    void update_projectiles_(f32 dt, const Vec<World*>& enemy_worlds,
                             u32 only_projectile_uid);
    void auto_combat_(const Vec<World*>& enemy_worlds, u32 only_unit_uid);
    void update_laser_defence_(f32 dt, const Vec<World*>& enemy_worlds,
                               u32 only_unit_uid);
    void intercept_tagged_projectile_(Unit& unit, WeaponSlot& slot,
                                      const Vec<World*>& projectile_worlds);
    void update_antinuke_interception_(
        f32 dt, const Vec<World*>& enemy_worlds, u32 only_unit_uid);
    void aim_weapon_slot_(Unit& attacker, WeaponSlot& slot, Unit& target);
    bool fire_weapon_slot_(Unit& attacker, WeaponSlot& slot, Unit& target,
                           const Unit* main_target);
    u32 create_weapon_projectile_(Unit& attacker, WeaponSlot& slot,
                                  Unit& target, bool ground_point = false);
    bool can_target_(const Unit& attacker, const Unit& target) const;
    f32 engagement_range_(const Unit& unit, bool extended = false) const;
    Unit* select_passive_target_(
        const Unit& unit, const Vec<World*>& enemy_worlds, f32 scan_range,
        World*& selected_world);
    World* order_target_world_(i32 target_team_id,
                               const Vec<World*>& other_worlds);
    World* find_order_target_world_(u32 target_uid,
                                    const Vec<World*>& other_worlds);
    Unit* formation_unit_(u32 uid);
    bool submit_order_(Unit& unit, const GameCommand& cmd,
                       const Vec<World*>& other_worlds);
    void collect_command_formation_(const GameCommand& cmd,
                                    const Vec<World*>& other_worlds);
    void configure_command_formation_(const GameCommand& cmd,
                                      const Vec<World*>& other_worlds,
                                      u64 inherited_controller_id = 0,
                                      bool combine_owners = false);
    void rebuild_formation_after_leader_removed_(
        Unit& leader, const Set<u32>* excluded_uids = nullptr);
    bool activate_order_(Unit& unit, const UnitOrder& order,
                         const Vec<World*>& other_worlds);
    void clear_current_order_state_(Unit& unit,
                                    bool preserve_movement_proposal = false,
                                    bool preserve_formation_timers = false);
    void move_unit_to_(Unit& u, f32 x, f32 y, i32 target_size,
                       bool defer_grid_path,
                       bool preserve_active_path = false);
    void move_unit_to_async_(Unit& u, f32 x, f32 y,
                             i32 target_size = 0);
    // Native y.a() treats the first frame of a service chase (repair, reclaim
    // or construction) as a handoff rather than an ordinary movement request:
    // the path is exposed immediately, but s starts at 10 and the physical
    // pass coasts once without prediction or steering.
    void begin_service_chase_(Unit& healer, const Unit& target);
    f32 service_range_to_(const Unit& source, const Str& target_type) const;
    f32 service_build_speed_(const Unit& source, const Unit& target) const;
    bool service_target_in_range_(const Unit& source, const Unit& target) const;
    i32 service_path_target_size_(const Unit& source, const Str& target_type) const;
    // y.b(l): shared consumption of a solved path list (120-node cap,
    // truncation flag, single-node and empty handling).
    void consume_solver_path_(Unit& u, const Vec<std::pair<f32, f32>>& path);
    // game.units.y.a(f2, l, x, y): clamps the frame's movement displacement
    // against the blocked-tile layer, sliding along tile edges; accumulates
    // the wall-bump counter y.b.
    void apply_movement_tile_clamp_(Unit& u, f32 old_x, f32 old_y,
                                    f32 delta_frames);
    bool update_unit_order_navigation_(
        Unit& unit, f32 delta_frames, const Vec<World*>& other_worlds);
    bool can_load_cargo_(const Unit& transport, const Unit& cargo) const;
    void release_transport_cargo_on_death_(Unit& transport);
    void apply_mobile_factory_exit_(Unit& factory, Unit& product);
    void update_transport_order_(Unit& unit, const Vec<World*>& other_worlds);
    void complete_formation_followers_(Unit& leader,
                                       const Vec<World*>& other_worlds);
    void complete_current_order_(Unit& unit,
                                 const Vec<World*>& other_worlds,
                                 bool cycle_to_back = false,
                                 bool defer_next_order_activation = false);
    void advance_finished_order_(Unit& unit,
                                 const Vec<World*>& other_worlds);
    bool execute_custom_action_(
        Unit& unit, const Str& action_id, const Vec<World*>& other_worlds,
        bool* active_waypoint_consumed = nullptr,
        const CustomActionContext* context = nullptr,
        i32 recursion_depth = 0);
    i32 custom_action_cooldown_remaining_(const Unit& unit,
                                          const Str& action_id) const;
    void add_custom_action_cooldown_(Unit& unit, const Str& action_id,
                                     f32 duration);
    void clear_factory_queue_(Unit& unit, bool refund);
    void prune_custom_action_queue_(Unit& unit, i32 original_front_cost);
    void set_custom_build_progress_(Unit& unit, f32 progress);
    void reset_custom_stats_(Unit& unit, const UnitDef& base);
    f32 read_custom_stat_(const Unit& unit, CustomStat stat) const;
    void apply_custom_stat_(Unit& unit,
                            const CustomStatAssignment& assignment);
    void spawn_custom_unit_list_(
        Unit& source, const Vec<CustomUnitSpawn>& entries, bool produced,
        const Vec<World*>& other_worlds);
    bool apply_take_resources_(
        Unit& source, const CustomTakeResourcesEffect& effect,
        const Vec<World*>& other_worlds,
        const CustomActionContext& context, i32 recursion_depth);
    void clamp_custom_resources_(Unit& unit);
    f64 read_custom_resource_(const Unit& unit, CustomResource resource) const;
    void write_custom_resource_(Unit& unit, CustomResource resource, f64 value);
    void convert_unit_definition_(Unit& unit, const UnitDef& definition,
                                  bool preserve_pool_ratios);
    bool can_factory_build_(const Unit& factory, const Str& unit_type) const;
    i32 factory_queue_item_cost_(const Unit& factory, const Str& item) const;
    bool queue_upgrade_(Unit& u, const Str& requested_target);
    bool queue_custom_action_(Unit& unit, const CustomActionDef& action,
                              const CustomActionContext& context);
    void update_unit_action_building_state_(f32 delta_frames, u32 unit_uid);
    void update_unit_auto_actions_(f32 delta_frames, u32 unit_uid,
                                   const Vec<World*>& other_worlds);
    void initialize_legs_(Unit& unit);
    void update_legs_(Unit& unit, f32 delta_frames);
    bool resolve_upgrade_action_(const Unit& u, const Str& target,
                                 i32& cost, f32& build_speed) const;
    void upgrade_unit_now_(Unit& u, const Str& requested_target, bool charge_cost);
    // y.a(au,as,...) checks the cap before constructing its provisional
    // product, then snaps it and checks placement and price. The caller
    // retains only young waypoints refused on price; candidate_created also
    // distinguishes a cap refusal from a rejected provisional object.
    Unit* start_build_(const Str& name, f32 x, f32 y, bool requires_builder,
                       bool* unaffordable = nullptr, i32 build_tech_level = 1,
                       u32 placement_source_uid = 0,
                       bool* candidate_created = nullptr);
    Unit* spawn_unit_from_def_(const UnitDef& def, const Str& name,
                              f32 x, f32 y, bool start_built);
};

}
