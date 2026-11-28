"""Run the trained policy as a connectionless server player."""

from pathlib import Path
from concurrent.futures import Future, ThreadPoolExecutor
import threading

import numpy as np
import torch

from resource_paths import project_path
from rl.model import (
    ObsBuilder, PolicyValueNet, batch_to_torch, decode_team_commands,
    sample_to_numpy,
)
from rl_env import RLEnv


class ModelAIController:
    """Shared model and observation state for one or more AI lobby slots."""

    def __init__(self, model_path: str, device: str = 'auto',
                 deterministic: bool = True):
        self.model_path = str(project_path(model_path))
        if not Path(self.model_path).is_file():
            raise FileNotFoundError(f'Model checkpoint not found: {self.model_path}')
        checkpoint = torch.load(self.model_path, map_location='cpu', weights_only=True)
        saved_config = checkpoint.get('config', {})
        model_cfg = saved_config.get('model', {})
        vocabulary = checkpoint.get('vocabulary', {})
        if vocabulary.get('architecture') != PolicyValueNet.ARCHITECTURE:
            raise ValueError(f'checkpoint is not a {PolicyValueNet.ARCHITECTURE} model')
        self.device = torch.device(
            'cuda' if device == 'auto' and torch.cuda.is_available()
            else ('cpu' if device == 'auto' else device))
        self.net = PolicyValueNet(
            d_model=model_cfg.get('d_model', 384),
            n_heads=model_cfg.get('n_heads', 8),
            n_layers=model_cfg.get('n_layers', 6),
            d_ff=model_cfg.get('d_ff', 1536),
            dropout=0.0,
        ).to(self.device)
        self.net.load_state_dict(checkpoint['model'], strict=True)
        self.net.eval()
        self.deterministic = bool(deterministic)
        self.builder = ObsBuilder()
        self.env = RLEnv(tick_rate=0, speed=1.0)
        self._engine = None
        self._team_ids = []
        self._lock = threading.Lock()
        self._executor = ThreadPoolExecutor(max_workers=1,
                                            thread_name_prefix='model-ai')
        self._future: Future | None = None
        self._future_team_id: int | None = None

    @property
    def parameter_count(self):
        return sum(parameter.numel() for parameter in self.net.parameters())

    def bind(self, engine, team_ids):
        """Bind observation access to the server's live engine."""
        with self._lock:
            changed = engine is not self._engine or list(team_ids) != self._team_ids
            self._engine = engine
            self._team_ids = list(team_ids)
            self.env.engine = engine
            self.env.worlds = [engine.get_world(team) for team in self._team_ids
                               if engine.get_world(team) is not None]
            self.env._num_teams = len(self.env.worlds)
            if changed:
                self.env._static_map = None
                self.env._resource_pools = None
                self.builder.reset()

    def _snapshot(self, team_id: int):
        """Copy an observation while the server owns its engine lock."""
        if self._engine is None or team_id not in self._team_ids:
            return None
        # The model observes all participating teams. RLEnv presents them in
        # the same stable order as _team_ids, independent of native slots.
        self.env.worlds = [self._engine.get_world(team) for team in self._team_ids
                           if self._engine.get_world(team) is not None]
        obs = self.env.observe()
        team_index = self._team_ids.index(team_id)
        return obs, team_index, team_id

    def _infer_snapshot(self, obs, team_index, team_id):
        # Encoding and model inference happen off the simulation thread.  The
        # main thread only copied the C++ observation in _snapshot().
        features, meta = self.builder.encode(obs, team_index)
        batch = batch_to_torch([features], self.device)
        with torch.inference_mode():
            sample = self.net.act(batch, deterministic=self.deterministic)
        decoded = decode_team_commands(sample_to_numpy(sample), 0, meta)
        for command in decoded:
            command.native_selection = True
            command.control_player_team_id = team_id
            command.shared_control_mask = 0
            command.group_uids = [command.unit_uid]
            command.group_size = 1
            command.group_index = 0
        return decoded
    def _infer_snapshots(self, snapshots):
        commands = []
        for obs, team_index, team_id in snapshots:
            commands.extend(self._infer_snapshot(obs, team_index, team_id))
        return commands

    def submit(self, team_id: int | list[int]) -> bool:
        """Snapshot now and infer asynchronously; never waits for inference."""
        with self._lock:
            # A completed future still owns commands which have not been
            # delivered to the simulation thread.  Do not replace it from a
            # sampling tick before poll_commands() gets a chance to consume
            # it; otherwise a result that completes exactly on an inference
            # boundary is silently lost.
            if self._future is not None:
                return False
            team_ids = [team_id] if isinstance(team_id, int) else list(team_id)
            obs = self.env.observe()
            snapshots = []
            for native_team in team_ids:
                if self._engine is None or native_team not in self._team_ids:
                    continue
                snapshots.append((obs, self._team_ids.index(native_team), native_team))
            if not snapshots:
                return False
            self._future_team_id = team_ids[0]
            self._future = self._executor.submit(
                self._infer_snapshots, snapshots)
            return True

    def poll_commands(self):
        """Return completed commands without blocking the simulation thread."""
        with self._lock:
            if self._future is None or not self._future.done():
                return []
            future, self._future = self._future, None
            self._future_team_id = None
        return future.result()

    def commands_for(self, team_id: int):
        """Synchronous helper retained for offline tests and diagnostics."""
        snapshot = self._snapshot(team_id)
        if snapshot is None:
            return []
        obs, team_index, native_team = snapshot
        return self._infer_snapshot(obs, team_index, native_team)

    def close(self):
        self._executor.shutdown(wait=False, cancel_futures=True)

