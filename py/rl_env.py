import math
import sys
from pathlib import Path
from dataclasses import dataclass, field
from enum import Enum
from typing import Any

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import numpy as np
import rwrl_core
from config import Config
from unit_config import load_unit_assets

# Resolution of the static terrain grid attached to observations.
# Must match the spatial resolution expected by rl.model (MAP_RES).
STATIC_MAP_RES = 64

_T_WATER = int(rwrl_core.TerrainFlag.WATER)
_T_BRIDGE = int(rwrl_core.TerrainFlag.WATER_BRIDGE)
_T_LAVA = int(rwrl_core.TerrainFlag.LAVA)
_T_CLIFF = int(rwrl_core.TerrainFlag.CLIFF)
_T_LCLIFF = int(rwrl_core.TerrainFlag.LARGE_CLIFF)
_T_BLOCK = int(rwrl_core.TerrainFlag.BLOCK_LAND)


# ---------------------------------------------------------------------------
# Unit status categories for RL observation
# ---------------------------------------------------------------------------

class UnitStatus(str, Enum):
    """Categorization of a unit's simulation state for RL observation.

    These categories are mutually exclusive and cover every unit in
    ``World.units()``.  The categorization mirrors the C++ engine's
    ``Unit::is_active()``, ``is_alive``, ``is_dead``,
    ``death_persistent``, ``is_complete()``, ``parent_uid``, and
    ``is_building`` fields.

    Mapping rules (evaluated top-to-bottom, first match wins):

    * ``WRECK``       — ``is_dead`` or (``is_alive == False`` and
                        ``death_persistent == True``).  Persistent wrecks
                        remain in the units map but are not actionable.
    * ``TRANSPORTED`` — ``is_alive`` and ``parent_uid != 0`` and the unit
                        is *not* listed in any transport's ``cargo_units``.
                        This covers units that have been loaded into a
                        transport that was subsequently destroyed (orphaned
                        cargo awaiting death transition).
    * ``CARGO``       — ``is_alive`` and ``parent_uid != 0`` and the unit
                        *is* found in a transport's ``cargo_units`` list.
                        Cargo units are alive but removed from the spatial
                        index and normal update loop.
    * ``INCOMPLETE``  — ``is_alive`` and ``is_complete() == False`` and
                        ``parent_uid == 0``.  Buildings under construction
                        that have not yet reached ``build_progress >= 1.0``.
    * ``ACTIVE``      — ``is_alive`` and ``is_complete() == True`` and
                        ``parent_uid == 0``.  Fully operational units that
                        participate in combat, movement, and income.
    """
    ACTIVE = 'active'
    WRECK = 'wreck'
    TRANSPORTED = 'transported'
    INCOMPLETE = 'incomplete'
    CARGO = 'cargo'


def categorize_unit(u: Any, cargo_uids: set[int] | None = None) -> UnitStatus:
    """Return the :class:`UnitStatus` category for a C++ Unit object.

    Parameters
    ----------
    u :
        A ``rwrl_core.Unit`` instance from ``World.units()``.
    cargo_uids :
        Optional pre-built set of all UIDs that appear in any transport's
        ``cargo_units`` list.  When *None*, the function falls back to
        checking ``u.parent_uid != 0`` alone, which correctly identifies
        transported/cargo units but cannot distinguish CARGO from
        TRANSPORTED without the full set.
    """
    if u.is_dead:
        return UnitStatus.WRECK
    # A unit with is_alive=False but is_dead=False should not normally
    # exist, but guard against it by treating non-alive non-dead as wreck
    # if death_persistent is set.
    if not u.is_alive and u.death_persistent:
        return UnitStatus.WRECK
    if not u.is_alive:
        # Non-persistent dead units are cleaned up after one tick, but
        # may briefly appear in the units map.
        return UnitStatus.WRECK

    # Alive units from here on.
    if u.parent_uid != 0:
        if cargo_uids is not None and u.uid in cargo_uids:
            return UnitStatus.CARGO
        # If we don't have the cargo set, assume parent_uid != 0 means
        # cargo (the common case; orphaned cargo is a transient state).
        if cargo_uids is None:
            return UnitStatus.CARGO
        # parent_uid != 0 but not in any cargo_units: orphaned/transported
        return UnitStatus.TRANSPORTED

    if not u.is_complete():
        return UnitStatus.INCOMPLETE

    return UnitStatus.ACTIVE


def _collect_cargo_uids(w: rwrl_core.World) -> set[int]:
    """Collect the set of all UIDs carried inside transports."""
    cargo: set[int] = set()
    for uid, u in w.units().items():
        if u.cargo_capacity > 0 and u.cargo_units:
            cargo.update(u.cargo_units)
    return cargo


# ---------------------------------------------------------------------------
# WorldState field semantics documentation
# ---------------------------------------------------------------------------
# The C++ ``WorldState`` struct (core/include/rwrl/world.h) is populated
# by ``World::get_state()`` (core/src/world_step.cpp).  Key fields:
#
# ``active_units`` — count of units where ``!u.is_dead``.  This includes
#   alive buildings, incomplete buildings, and transported/cargo units.
#   It does NOT include dead units (even persistent wrecks).  Despite the
#   name, this is NOT the same as ``count_active_units()``.
#
# ``total_units`` — ``units_.size()``, which includes persistent wrecks
#   (``death_persistent == True``) that have not yet been erased.  Non-
#   persistent dead units are erased after one tick.
#
# ``buildings`` — count of units where ``!u.is_dead && u.is_building``.
#
# ``count_active_units()`` (method, not a WorldState field) — counts
#   non-dead non-building units PLUS queued non-building factory products.
#   This is the unit-cap counter.  It differs from ``active_units`` in
#   that it excludes buildings and includes queued items.
#
# ``factory_queue_size`` — total items across all factory queues (includes
#   unit productions, upgrades, nuke/antinuke stock).
#
# ``pending_builds`` — count of active BuildTask entries (buildings under
#   construction that have not yet completed).
#
# ``unit_cap`` — effective unit cap (from ``unit_cap()`` method).
# ---------------------------------------------------------------------------


@dataclass
class TeamObservation:
    """Structured observation for one team's world state.

    All counters are snapshot values at observation time.  Delta rewards
    are computed by differencing consecutive snapshots inside :class:`RLEnv`.
    """
    team_id: int = 0
    alliance_id: int = 0
    credits: float = 0.0
    unit_cap: int = 200

    # Unit counts by status category
    num_active: int = 0
    num_wreck: int = 0
    num_transported: int = 0
    num_incomplete: int = 0
    num_cargo: int = 0

    # C++ WorldState fields (verified semantics)
    ws_active_units: int = 0       # get_state().active_units: non-dead units
    ws_total_units: int = 0        # get_state().total_units: units_.size() incl. wrecks
    ws_buildings: int = 0          # get_state().buildings: non-dead buildings
    ws_pending_builds: int = 0     # get_state().pending_builds
    ws_projectiles: int = 0        # get_state().projectiles
    ws_kills: int = 0              # get_state().kills
    ws_losses: int = 0             # get_state().losses
    ws_units_built: int = 0        # get_state().units_built
    ws_buildings_completed: int = 0  # get_state().buildings_completed
    ws_factory_queue_size: int = 0   # get_state().factory_queue_size
    ws_income_per_sec: float = 0.0   # get_state().income_per_sec

    count_active_units: int = 0    # w.count_active_units(): cap counter
    is_cc_alive: bool = False

    # Per-unit detail lists
    units: list[dict[str, Any]] = field(default_factory=list)
    projectiles: list[tuple[float, float]] = field(default_factory=list)


@dataclass
class RewardConfig:
    """Weights for per-step delta rewards and terminal bonuses."""
    kill_reward: float = 1.0
    loss_penalty: float = -1.0
    build_reward: float = 0.5
    income_reward: float = 0.001     # per credit earned
    win_bonus: float = 10.0
    loss_bonus: float = -10.0
    draw_bonus: float = 0.0


class RLEnv:
    def __init__(self, config: Config = None, reward_config: RewardConfig = None,
                 **overrides):
        cfg = config or Config()
        for key, value in overrides.items():
            section = 'game' if key in ('tick_rate', 'sync_interval') else 'server'
            cfg._cp.set(section, key, str(value))
        self._tick_rate = cfg.tick_rate
        self._dt = 1.0 / self._tick_rate if self._tick_rate > 0 else 1.0 / rwrl_core.FRAME_RATE
        self._unlimited_tps = self._tick_rate <= 0
        self._speed_mult = cfg.speed
        self._starting_credits = cfg.starting_credits
        self._income_mult = cfg.income_mult
        self._map_width = cfg.map_width
        self._map_height = cfg.map_height
        self.engine: rwrl_core.Engine | None = None
        self.worlds: list[rwrl_core.World] = []
        self._num_teams = 0
        self._static_map: np.ndarray | None = None
        self._reward_config = reward_config or RewardConfig()
        self._prev_obs: dict[int, TeamObservation] = {}

    @property
    def worlds(self) -> list[rwrl_core.World]:
        # Quick resync replaces the C++ World objects. Resolve their stable
        # team IDs on access instead of retaining pointers to freed worlds.
        if self.engine is None:
            return []
        return [world for team in self._world_team_ids
                if (world := self.engine.get_world(team)) is not None]

    @worlds.setter
    def worlds(self, worlds):
        self._world_team_ids = [world.team_id() for world in worlds]

    def reset(self, num_teams: int = 2, map_path: str | None = None) -> dict:
        self.engine = rwrl_core.Engine()
        load_unit_assets(self.engine)
        self._num_teams = num_teams
        self.engine.set_speed_mult(self._speed_mult)
        if self._income_mult != 1.0:
            self.engine.set_income_mult(self._income_mult)
        if map_path:
            self.engine.load_map(map_path)
        elif num_teams == 2:
            self.engine.setup_teams(0, 1)
        else:
            # FFA: spread command centers on a circle so any team count fits
            # the map (setup_teams_vec walks off the map for 4+ teams).
            w_world = self.engine.map_width() * self.engine.tile_size()
            h_world = self.engine.map_height() * self.engine.tile_size()
            cx, cy = w_world * 0.5, h_world * 0.5
            rad = 0.35 * min(w_world, h_world)
            for i in range(num_teams):
                w = self.engine.create_world(i)
                ang = 2.0 * math.pi * i / num_teams - math.pi / 2.0
                cc = w.spawn_unit('commandCenter',
                                  cx + rad * math.cos(ang),
                                  cy + rad * math.sin(ang), True)
                if cc:
                    w.set_cc_uid(cc.uid)
        worlds = []
        for tid in range(num_teams):
            w = self.engine.get_world(tid)
            if w is not None:
                worlds.append(w)
                w.add_credits(max(0, self._starting_credits - w.credits()))
        self.worlds = worlds
        self._static_map = None
        self._prev_obs = {}
        self.engine.run_for(0.0, self._dt)
        obs = self.observe()
        for tid, team_obs in enumerate(obs.get('teams', [])):
            self._prev_obs[tid] = team_obs if isinstance(team_obs, TeamObservation) else self._dict_to_team_obs(team_obs)
        return obs

    def step(self, actions: dict[int, list[rwrl_core.GameCommand]] | None = None,
             sim_seconds: float | None = None) -> dict:
        if self.engine is None:
            raise RuntimeError("call reset() before step()")
        if actions:
            for team_id, commands in actions.items():
                for cmd in commands:
                    self.engine.execute_command(team_id, cmd)
        dt = sim_seconds if sim_seconds is not None else self._dt
        if self._unlimited_tps:
            self.engine.run_for(dt, 1.0 / rwrl_core.FRAME_RATE)
        else:
            self.engine.run_for(dt, self._dt)
        obs = self.observe()
        rewards = self._compute_rewards(obs)
        obs['rewards'] = rewards
        for tid, team_obs in enumerate(obs.get('teams', [])):
            self._prev_obs[tid] = team_obs if isinstance(team_obs, TeamObservation) else self._dict_to_team_obs(team_obs)
        return obs

    def step_ticks(self, num_ticks: int, actions: dict[int, list[rwrl_core.GameCommand]] | None = None) -> dict:
        if self.engine is None:
            raise RuntimeError("call reset() before step_ticks()")
        if actions:
            for team_id, commands in actions.items():
                for cmd in commands:
                    self.engine.execute_command(team_id, cmd)
        tick_dt = 1.0 / rwrl_core.FRAME_RATE
        for _ in range(num_ticks):
            self.engine.tick(tick_dt)
        obs = self.observe()
        rewards = self._compute_rewards(obs)
        obs['rewards'] = rewards
        for tid, team_obs in enumerate(obs.get('teams', [])):
            self._prev_obs[tid] = team_obs if isinstance(team_obs, TeamObservation) else self._dict_to_team_obs(team_obs)
        return obs

    def observe(self, team_id: int | None = None) -> dict:
        if self.engine is None:
            return {}
        obs: dict = {
            'tick': self.engine.global_tick(),
            'game_time': self.engine.game_time(),
            'map_width': self.engine.map_width(),
            'map_height': self.engine.map_height(),
            'tile_size': self.engine.tile_size(),
            'static_map': self._get_static_map(),
            'teams': [],
        }
        for tid, w in enumerate(self.worlds):
            if team_id is not None and tid != team_id:
                continue
            obs['teams'].append(self._observe_world(w, tid))
        return obs

    def _get_static_map(self) -> np.ndarray:
        """[3, RES, RES] float32: water(non-bridge), blocked (cliff/blockland), resource pool."""
        if self._static_map is not None:
            return self._static_map
        res = STATIC_MAP_RES
        grid = np.zeros((3, res, res), dtype=np.float32)
        if self.worlds:
            w = self.worlds[0]
            tw = max(self.engine.map_width(), 1)
            th = max(self.engine.map_height(), 1)
            for gy in range(res):
                ty = min(int((gy + 0.5) / res * th), th - 1)
                for gx in range(res):
                    tx = min(int((gx + 0.5) / res * tw), tw - 1)
                    f = w.terrain(tx, ty)
                    if (f & (_T_WATER | _T_LAVA)) and not (f & _T_BRIDGE):
                        grid[0, gy, gx] = 1.0
                    if f & (_T_CLIFF | _T_LCLIFF | _T_BLOCK):
                        grid[1, gy, gx] = 1.0
                    if w.is_resource_pool(tx, ty):
                        grid[2, gy, gx] = 1.0
        self._static_map = grid
        return grid

    def _observe_world(self, w: rwrl_core.World, team_id: int) -> dict:
        cargo_uids = _collect_cargo_uids(w)
        units_data = []
        counts = {s: 0 for s in UnitStatus}
        for uid, u in w.units().items():
            status = categorize_unit(u, cargo_uids)
            counts[status] += 1
            oq = u.order_queue
            units_data.append({
                'uid': u.uid,
                'type': u.unit_type,
                'x': u.x,
                'y': u.y,
                'vel_x': u.vel_x,
                'vel_y': u.vel_y,
                'rotation': u.rotation,
                'hp': u.hp,
                'max_hp': u.max_hp,
                'shield': u.shield,
                'max_shield': u.max_shield,
                'alive': u.is_alive,
                'complete': u.is_complete(),
                'build_progress': u.build_progress,
                'team_id': u.team_id,
                'is_building': u.is_building,
                'can_move': u.can_move,
                'weapon_range': u.weapon_range,
                'cooldown_frac': u.cooldown_ticks / max(u.cooldown_max, 1),
                'ammo': u.ammo,
                'ammo_max': u.ammo_max,
                'energy': u.energy,
                'max_energy': u.max_energy,
                'submerged': u.submerged,
                'cargo_count': len(u.cargo_units),
                'cargo_capacity': u.cargo_capacity,
                'was_attacked': u.was_attacked,
                'tech_level': u.tech_level,
                'income_rate': u.income_rate,
                'cost': u.cost,
                'has_move_target': u.has_move_target,
                'move_target_x': u.target_x,
                'move_target_y': u.target_y,
                'attack_target_uid': u.target_uid,
                'order_queue_len': len(oq),
                'first_order_type': int(oq[0].type) if oq else -1,
                'status': status.value,
                'is_dead': u.is_dead,
                'death_persistent': u.death_persistent,
                'parent_uid': u.parent_uid,
            })
        st = w.get_state()
        return {
            'team_id': team_id,
            'alliance_id': w.alliance_id(),
            'credits': w.credits(),
            'unit_cap': w.unit_cap(),
            'count_active_units': w.count_active_units(),
            'is_cc_alive': w.is_cc_alive(),
            'income_per_sec': st.income_per_sec,
            'kills': st.kills,
            'losses': st.losses,
            'factory_queue_size': st.factory_queue_size,
            'pending_builds': st.pending_builds,
            'units': units_data,
            'projectiles': [(p.x, p.y) for p in w.projectiles().values()],
            'unit_counts': {s.value: counts[s] for s in UnitStatus},
            'ws_active_units': st.active_units,
            'ws_total_units': st.total_units,
            'ws_buildings': st.buildings,
            'ws_pending_builds': st.pending_builds,
            'ws_projectiles': st.projectiles,
            'ws_kills': st.kills,
            'ws_losses': st.losses,
            'ws_units_built': st.units_built,
            'ws_buildings_completed': st.buildings_completed,
            'ws_factory_queue_size': st.factory_queue_size,
            'ws_income_per_sec': st.income_per_sec,
        }

    def game_over(self) -> dict[str, Any]:
        """Check whether the game has ended and return terminal info.

        Returns a dict with:
          ``over`` (bool): whether the game has ended.
          ``winner`` (int | None): winning team id, or None if draw/ongoing.
          ``winners`` (list[int]): all team ids on the winning alliance(s).
          ``reason`` (str): short reason string.

        The engine's ``Engine.tick()`` already checks victory by alliance
        id: when ≤1 alliance has surviving victory-key units, the game
        ends.  This method mirrors that logic at the Python level and
        handles alliance, neutral team, FFA, shared winners, and the
        no-legal-winner scenario.
        """
        if self.engine is None:
            return {'over': False, 'winner': None, 'winners': [], 'reason': 'no_engine'}

        surviving_alliances: set[int] = set()
        alliance_members: dict[int, list[int]] = {}
        for tid, w in enumerate(self.worlds):
            if w.team_id() < 0:
                continue
            if w.check_victory():
                aid = w.alliance_id()
                surviving_alliances.add(aid)
                alliance_members.setdefault(aid, []).append(tid)

        if len(surviving_alliances) > 1:
            return {'over': False, 'winner': None, 'winners': [], 'reason': 'ongoing'}

        if not surviving_alliances:
            return {'over': True, 'winner': -1, 'winners': [],
                    'reason': 'no_legal_winner'}

        winning_alliance = next(iter(surviving_alliances))
        winners = alliance_members.get(winning_alliance, [])
        winner = min(winners) if winners else -1
        return {'over': True, 'winner': winner, 'winners': winners,
                'reason': 'last_alliance_standing'}

    def spawn_unit(self, team_id: int, unit_type: str, x: float, y: float,
                   completed: bool = True) -> rwrl_core.Unit | None:
        if team_id >= len(self.worlds):
            return None
        return self.worlds[team_id].spawn_unit(unit_type, x, y, completed)

    def get_world(self, team_id: int) -> rwrl_core.World | None:
        if team_id < len(self.worlds):
            return self.worlds[team_id]
        return None

    def command(self, action: rwrl_core.GameCommandAction,
                team_id: int = 0, **kwargs) -> rwrl_core.GameCommand:
        cmd = rwrl_core.GameCommand()
        cmd.action = action
        if 'unit_uid' in kwargs:
            cmd.unit_uid = kwargs['unit_uid']
        if 'target_uid' in kwargs:
            cmd.target_uid = kwargs['target_uid']
        if 'target_x' in kwargs:
            cmd.target_x = kwargs['target_x']
        if 'target_y' in kwargs:
            cmd.target_y = kwargs['target_y']
        if 'unit_type' in kwargs:
            cmd.unit_type = kwargs['unit_type']
        if 'queued' in kwargs:
            cmd.queued = kwargs['queued']
        if 'action_id' in kwargs:
            cmd.action_id = kwargs['action_id']
        self.engine.execute_command(team_id, cmd)
        return cmd

    def _dict_to_team_obs(self, d: dict) -> TeamObservation:
        """Convert a raw observation dict for one team into a TeamObservation."""
        uc = d.get('unit_counts', {})
        return TeamObservation(
            team_id=d.get('team_id', 0),
            alliance_id=d.get('alliance_id', 0),
            credits=d.get('credits', 0.0),
            unit_cap=d.get('unit_cap', 200),
            num_active=uc.get('active', 0),
            num_wreck=uc.get('wreck', 0),
            num_transported=uc.get('transported', 0),
            num_incomplete=uc.get('incomplete', 0),
            num_cargo=uc.get('cargo', 0),
            ws_active_units=d.get('ws_active_units', 0),
            ws_total_units=d.get('ws_total_units', 0),
            ws_buildings=d.get('ws_buildings', 0),
            ws_pending_builds=d.get('ws_pending_builds', 0),
            ws_projectiles=d.get('ws_projectiles', 0),
            ws_kills=d.get('kills', 0),
            ws_losses=d.get('losses', 0),
            ws_units_built=d.get('ws_units_built', 0),
            ws_buildings_completed=d.get('ws_buildings_completed', 0),
            ws_factory_queue_size=d.get('ws_factory_queue_size', 0),
            ws_income_per_sec=d.get('ws_income_per_sec', 0.0),
            count_active_units=d.get('count_active_units', 0),
            is_cc_alive=d.get('is_cc_alive', False),
        )

    def _compute_rewards(self, obs: dict) -> dict[int, float]:
        """Compute per-team delta rewards from consecutive observations.

        Reward components per step:
          * kills delta  × kill_reward
          * losses delta × loss_penalty  (negative weight × positive delta = negative reward)
          * units_built delta × build_reward
          * income delta × income_reward
          * terminal win/loss/draw bonus
        """
        rc = self._reward_config
        rewards: dict[int, float] = {}
        terminal = self.game_over()

        for tid, team_dict in enumerate(obs.get('teams', [])):
            cur = team_dict if isinstance(team_dict, TeamObservation) else self._dict_to_team_obs(team_dict)
            prev = self._prev_obs.get(tid)
            if prev is None:
                rewards[tid] = 0.0
                continue

            delta_kills = cur.ws_kills - prev.ws_kills
            delta_losses = cur.ws_losses - prev.ws_losses
            delta_built = cur.ws_units_built - prev.ws_units_built
            delta_income = cur.credits - prev.credits
            # Income can be negative (spending), so use the income-per-sec
            # delta as a smoother signal.  But for accuracy, use the actual
            # credit delta from income only (not spending).  Since we can't
            # separate income from spending in the current API, use
            # total_income from WorldState if available, otherwise 0.
            # For now, use kills/losses/built as the primary signals.

            r = 0.0
            r += delta_kills * rc.kill_reward
            r += delta_losses * rc.loss_penalty
            r += delta_built * rc.build_reward

            # Income reward: positive credit change from income (not spending).
            # We approximate by only rewarding when credits increased, which
            # happens when income exceeds spending in a step.
            if delta_income > 0:
                r += delta_income * rc.income_reward

            # Terminal bonus
            if terminal['over']:
                if tid in terminal.get('winners', []):
                    r += rc.win_bonus
                elif terminal['winner'] == -1:
                    r += rc.draw_bonus
                else:
                    r += rc.loss_bonus

            rewards[tid] = r

        return rewards
