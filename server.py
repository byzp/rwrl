import threading
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import sys
import struct
import time
import math

sys.path.insert(0, str(Path(__file__).resolve().parent / "py"))

import rwrl_core
from rwrl_core import Engine, FRAME_RATE
from net import NetServer, Connection, PacketType
from net.packet import (pack_preregister_info, pack_server_info, pack_team_list,
                        pack_tick, pack_start, pack_reconnect_to, pack_game_over,
                        pack_chat, pack_heartbeat_request, pack_heartbeat_response,
                        parse_heartbeat_response, parse_preregister,
                        parse_register_player, parse_get_server_info, unpack_string,
                        DEFAULT_MAP, DEFAULT_MAP_PATH, pack_step_rate_command,
                        starting_credit_mode,
                        pack_kick, pack_disconnect, pack_get_server_info)
from net.packet_log import PacketCapture
from game import Room, Player
from command import CommandHandler
from event import EventBus, EventType, Event
from config import Config
from unit_config import load_unit_assets
from resource_paths import MAPS_DIR, resolve_map_path
from rl.native_commands import encode_model_command, prepare_client_command
from rl.replay_parser import ReplayAction, parse_game_command


class RwrlServer:
    def __init__(self, config: Config = None):
        self.config = config or Config()
        self.rl_mode = self.config.rl_enabled
        if not self.rl_mode:
            starting_credit_mode(self.config.starting_credits)
        self.engine = None
        self.room = None
        self.events = EventBus()
        self.cmd = CommandHandler(self)
        self._running = False
        self._state_lock = threading.RLock()
        self._player_conns: dict[int, Connection] = {}
        self._next_heartbeat_at = 0.0
        self._next_team_sync_at = 0.0
        self.map_name = DEFAULT_MAP
        self.map_path = DEFAULT_MAP_PATH
        self._pending_game_commands: list[tuple[bytes, ReplayAction]] = []
        self._game_tick_history: list[bytes] = []
        self._next_blocking_frame = 0
        self._game_ready = False
        self._game_start_time = 0.0
        self._fog = 2
        self._all_mute = False
        self._pending_summon = None
        self._pending_nuke = None
        self._pending_clone = None
        self._model_ai = None
        self._model_ai_players: set[int] = set()
        self._model_ai_loader = ThreadPoolExecutor(max_workers=1,
                                                    thread_name_prefix='model-ai-load')
        self._model_ai_future = None
        self._model_ai_load_error = None
        self._ai_tick_interval = self.config.ai_ticks_per_inference
        self.packet_capture = None
        if self.config.diagnostics_enabled:
            self.packet_capture = PacketCapture(self.config.diagnostics_log_file)
            self._log_diagnostic(
                'server_initialized', host=self.config.host,
                port=self.config.port, packet_capture=True,
            )

        if self.rl_mode:
            self._net_tick_rate = self.config.rl_tick_rate
            self.net = None
        else:
            self._net_tick_rate = self.config.tick_rate
            self.net = NetServer(self.config.host, self.config.port)
            self.net.on_connect = self._on_connect
            self.net.on_disconnect = self._on_disconnect
            self.net.on_packet = self._on_packet
            self.net.on_packet_error = self._on_packet_error
            if self.packet_capture is not None:
                self.net.packet_logger = self.packet_capture.packet

        self.events.on(EventType.GAME_START, self._on_game_start)
        self.events.on(EventType.GAME_OVER, self._on_game_over)

    def _on_connect(self, conn: Connection):
        print(f"[Server] Player connected: {conn}")
        self._log_diagnostic('client_connected', connection_id=conn.conn_id,
                             peer=conn.addr)

    def _on_disconnect(self, conn: Connection):
        self._log_diagnostic(
            'client_disconnected', connection_id=conn.conn_id, peer=conn.addr,
            reason=conn.disconnect_reason,
        )
        with self._state_lock:
            self._disconnect_player(conn)

    def _log_diagnostic(self, event_name, **fields):
        capture = getattr(self, 'packet_capture', None)
        if capture is not None:
            capture.event(event_name, **fields)

    def _disconnect_player(self, conn: Connection):
        if conn.player_id < 0 or not self.room:
            return
        if self._player_conns.get(conn.player_id) is not conn:
            return

        print(f"[Server] Player disconnecting: pid={conn.player_id} reason={conn.disconnect_reason}")

        self._player_conns.pop(conn.player_id, None)
        player = self.room.players.get(conn.player_id)
        if player:
            player.conn = None

        terminal = conn.disconnect_reason in {
            "exited", "gotoNextLevel", "kicked", "no free slots",
        }
        if self.room.state == "waiting" and terminal:
            self.room.remove_player(conn.player_id)

        if not self._player_conns:
            self._reset_room()
        else:
            if player and player.is_admin:
                player.is_admin = False
                for p in self.room.players.values():
                    if p.conn is not None and p.player_id != conn.player_id:
                        p.is_admin = True
                        self._broadcast_system_chat(f"{p.name} is now admin")
                        break
            if self._running:
                if player:
                    if self.room and self.room.state == "playing":
                        player.shared_control = True
                        self._broadcast_system_chat(
                            "[Sharing control due to disconnect]"
                        )
                    self._broadcast_system_chat(
                        f"{player.name} disconnected"
                    )
                self._sync_team_lists()

    def _on_packet(self, conn: Connection, pkt_type: int, body: bytes):
        with self._state_lock:
            self._dispatch_packet(conn, pkt_type, body)

    def _on_packet_error(self, conn: Connection, pkt_type: int, exc: Exception):
        print(f'[Server] Packet handler error from {conn}: type={pkt_type}: {exc}')
        self._log_diagnostic(
            'packet_handler_error', connection_id=conn.conn_id,
            peer=conn.addr, packet_type=int(pkt_type), error=repr(exc),
        )

    def _dispatch_packet(self, conn: Connection, pkt_type: int, body: bytes):
        if (conn.player_id >= 0 and
                self._player_conns.get(conn.player_id) is not conn):
            return
        if pkt_type == PacketType.PREREGISTER_INFO_RECEIVE:
            self._handle_preregister(conn, body)
        elif pkt_type == PacketType.REGISTER_PLAYER:
            self._handle_register(conn, body)
        elif pkt_type == PacketType.CHAT_RECEIVE:
            self._handle_chat(conn, body)
        elif pkt_type == PacketType.GAMECOMMAND_RECEIVE:
            self._handle_game_command(conn, body)
        elif pkt_type == PacketType.HEART_BEAT:
            conn.queue_send(pack_heartbeat_response(body))
        elif pkt_type == PacketType.HEART_BEAT_RESPONSE:
            self._handle_heartbeat_response(conn, body)
        elif pkt_type == PacketType.ACCEPT_START_GAME:
            if conn.reconnecting:
                if not conn.reconnect_activating:
                    conn.reconnect_activating = True
                    self._activate_reconnected_player(conn)
            else:
                if not conn.in_game:
                    for packet in self._game_tick_history:
                        conn.queue_send(packet)
                conn.in_game = True
        elif pkt_type == PacketType.DISCONNECT:
            try:
                reason, _ = unpack_string(body)
            except (ValueError, UnicodeDecodeError):
                reason = f"unreadable body ({len(body)} bytes)"
            conn.disconnect_reason = reason
            conn.close()
        elif pkt_type == PacketType.GET_SERVER_INFO_RECEIVE:
            self._handle_get_server_info(conn, body)

    def _handle_preregister(self, conn: Connection, body: bytes):
        info = parse_preregister(body)
        conn.client_version = info["client_version"]
        conn.preregistered = True
        conn.queue_send(pack_preregister_info())

    def _handle_register(self, conn: Connection, body: bytes):
        if conn.registered:
            return
        info = parse_register_player(body)
        name = info["name"]
        client_id = info["client_id"]
        conn.client_version = info["client_version"]
        conn.client_id = client_id

        existing = self._find_reconnect_player(client_id)
        if existing is not None:
            self._reconnect_player(conn, existing, name)
            return

        if self.room and self.room.state == "playing":
            self._reject_connection(conn, "A game has already been started on this server")
            return
        if not self.room:
            self.engine = Engine()
            load_unit_assets(self.engine)
            self.room = Room(0, self.engine, self.events,
                             speed=self.config.speed,
                             starting_credits=self.config.starting_credits,
                             map_width=self.config.map_width,
                             map_height=self.config.map_height)

        p = self.room.add_player(name, conn, client_id=client_id)
        if len(self._player_conns) == 0:
            p.is_admin = True
        print(f"[Server] Player registered: pid={p.player_id} name={name} admin={p.is_admin} room_state={self.room.state}")
        conn.player_id = p.player_id
        conn.team_id = p.team_id
        conn.slot_id = p.slot_id
        conn.registered = True
        self._player_conns[p.player_id] = conn
        self._log_diagnostic(
            'player_registered', connection_id=conn.conn_id,
            player_id=p.player_id, name=p.name, slot=p.slot_id,
            team=p.team_id,
        )

        self._finish_registration(conn)

    def add_model_ai(self) -> tuple[bool, str]:
        """Add one checkpoint-driven player to the waiting-room lobby."""
        with self._state_lock:
            if self.room is None or self.room.state != 'waiting':
                return False, 'Game is running'
            if len(self.room.players) >= self.config.max_players:
                return False, 'No free player slots'
            from resource_paths import project_path
            if not project_path(self.config.ai_model_path).is_file():
                return False, f'Model AI unavailable: {self.config.ai_model_path}'
            player = self.room.add_ai_player()
            if player is None:
                return False, 'No free player slots'
            self._model_ai_players.add(player.player_id)
            if self._model_ai is None and self._model_ai_future is None:
                self._model_ai_load_error = None
                self._model_ai_future = self._model_ai_loader.submit(
                    self._load_model_ai)
            self._sync_team_lists()
            return True, f'Added model AI at slot {player.slot_id + 1}'

    def _load_model_ai(self):
        from rl.server_ai import ModelAIController
        return ModelAIController(
            self.config.ai_model_path,
            self.config.ai_device,
            self.config.ai_deterministic,
        )

    def _poll_model_ai_loader(self):
        future = self._model_ai_future
        if future is None or not future.done():
            return
        self._model_ai_future = None
        try:
            self._model_ai = future.result()
            print(f'[Server] Model AI loaded: {self._model_ai.model_path} '
                  f'({self._model_ai.parameter_count:,} parameters)')
        except Exception as exc:
            self._model_ai_load_error = str(exc)
            print(f'[Server] Model AI unavailable: {exc}')

    def _find_reconnect_player(self, client_id: str):
        if not self.room or not client_id:
            return None
        if self.room.state != "playing":
            return None
        for player in self.room.players.values():
            if player.client_id != client_id:
                continue
            if player.conn is not None:
                return player
            if player.present_at_game_start:
                return player
        return None

    def _reconnect_player(self, conn: Connection, player: Player, name: str):
        old_conn = player.conn
        if old_conn is not None and old_conn is not conn:
            old_conn.registered = False
            self.net.connections.pop(old_conn.conn_id, None)
            old_conn.close()

        player.conn = conn
        player.name = name
        conn.player_id = player.player_id
        conn.team_id = player.team_id
        conn.slot_id = player.slot_id
        conn.registered = True
        conn.reconnecting = self.room.state == "playing"
        conn.reconnect_activating = False
        player.reconnect_packets = (
            list(self._game_tick_history) if conn.reconnecting else []
        )
        self._player_conns[player.player_id] = conn

        self._finish_reconnection(conn, player)

    def _finish_reconnection(self, conn: Connection, player: Player):
        self._sync_team_lists()
        if not self._is_current_player_connection(conn):
            return
        self._send_server_info(conn)
        if (self.room and self.room.state == "playing" and
                self._is_current_player_connection(conn)):
            player_names = [p.name for p in self.room.players.values()]
            conn.queue_send(pack_reconnect_to(player_names, spectator=False))
        else:
            conn.reconnecting = False
            conn.reconnect_activating = False

    def _activate_reconnected_player(self, conn: Connection):
        if not self.room:
            conn.reconnect_activating = False
            return
        player = self.room.players.get(conn.player_id)
        if not player or player.conn is not conn:
            conn.reconnect_activating = False
            return

        replayed = 0
        while player.reconnect_packets:
            packets = player.reconnect_packets
            player.reconnect_packets = []
            for packet in packets:
                conn.queue_send(packet)
            replayed += len(packets)

        conn.reconnecting = False
        self._broadcast_system_chat(
            f"'{player.name}' reconnected. (Team {player.team_id})"
        )
        player.shared_control = False
        conn.reconnect_activating = False
        conn.in_game = True

    def _reject_connection(self, conn: Connection, reason: str):
        try:
            conn.queue_send(pack_kick(reason))
            conn.queue_send(pack_disconnect("kicked"))
        finally:
            conn.disconnect_reason = "kicked"
            conn.close()

    def _finish_registration(self, conn: Connection):
        self._sync_team_lists()
        if self._is_current_player_connection(conn):
            self._send_server_info(conn)

    def _sync_team_lists(self):
        if not self.room:
            return
        players = list(self.room.players.values())
        for player in players:
            conn = player.conn
            if (conn is None or not conn.registered or
                    not self._is_current_player_connection(conn)):
                continue
            packet = pack_team_list(
                players,
                conn.slot_id,
                max_players=self.config.max_players,
                starting_credits=int(self.config.starting_credits),
                max_units=self.config.unit_cap,
                income_multiplier=self.config.income_mult,
                paused=bool(self.engine and self.engine.is_paused()),
                fog=self._fog,
                no_nukes=not getattr(self.config, 'nukes_enabled', True),
            )
            conn.queue_send(packet)

    def _broadcast_system_chat(self, message: str):
        packet = pack_chat(message, "SERVER", -1)
        for conn in list(self._player_conns.values()):
            if self._is_current_player_connection(conn):
                conn.queue_send(packet)

    def _is_current_player_connection(self, conn: Connection) -> bool:
        if not self.room:
            return False
        player = self.room.players.get(conn.player_id)
        return (
            player is not None and
            player.conn is conn and
            self._player_conns.get(conn.player_id) is conn
        )

    def _handle_chat(self, conn: Connection, body: bytes):
        msg, _ = unpack_string(body)
        if not msg:
            return

        # The 1.15 client prefixes chat payloads with the channel marker "c ".
        # Strip it before looking for the native . / - / _ command prefixes.
        if msg.startswith('c '):
            msg = msg[2:]

        command = msg
        if msg[0] in ".-_":
            payload = msg[1:]
            if payload[:2].lower() == "qc":
                payload = payload[2:].lstrip()
                if payload[:1] in ".-_/":
                    payload = payload[1:]
            command = f"/{payload}"

        if self.cmd.execute(conn.player_id, command):
            return

        p = self.room.players.get(conn.player_id) if self.room else None
        name = p.name if p else f"Player{conn.player_id}"
        pkt = pack_chat(msg, name, conn.team_id)
        self.net.broadcast_raw(pkt)
        self.events.emit(Event(EventType.PLAYER_CHAT, player_id=conn.player_id, message=msg))

    def _handle_heartbeat_response(self, conn: Connection, body: bytes):
        if not conn.registered:
            return
        info = parse_heartbeat_response(body)
        now_ms = time.time_ns() // 1_000_000
        timestamp_ms = info["timestamp_ms"]
        if timestamp_ms <= 0 or timestamp_ms > now_ms:
            return
        if timestamp_ms != conn.last_heartbeat_sent_ms:
            return

        conn.ping_ms = min(now_ms - timestamp_ms, 9_000_000)
        conn.last_heartbeat_ms = now_ms
        conn.fps = info["fps"]

    def _handle_game_command(self, conn: Connection, body: bytes):
        if not conn.registered or not conn.in_game:
            return
        if not self.room or self.room.state != "playing":
            return
        try:
            shared_mask = sum(1 << player.slot_id for player in self.room.players.values()
                              if player.shared_control and 0 <= player.slot_id < 16)
            body, action = prepare_client_command(body, conn.slot_id, shared_mask)
            team = action.player_index
            if team >= 128:
                team -= 256
            if self.engine.get_world(team) is None:
                return
        except ValueError:
            return
        self._pending_game_commands.append((body, action))

    def _send_server_info(self, conn: Connection):
        player = self.room.players.get(conn.player_id) if self.room else None
        is_admin = player.is_admin if player else False
        conn.queue_send(pack_server_info(
            map_name=self.map_name,
            max_units=self.config.unit_cap,
            income_multiplier=self.config.income_mult,
            is_admin=is_admin,
            fog=self._fog,
            starting_credits=int(self.config.starting_credits),
            no_nukes=not getattr(self.config, 'nukes_enabled', True),
        ))
        pkt = pack_get_server_info(
            player_size=len(self._player_conns),
            max_player=self.config.max_players,
            map_name=self.map_name,
            income=self.config.income_mult,
            credits=int(self.config.starting_credits),
            no_nukes=not getattr(self.config, 'nukes_enabled', True),
            init_unit=getattr(self.config, 'starting_units', 1),
            mist=self._fog,
            shared_control=False,
        )
        conn.queue_send(pkt)

    def _handle_get_server_info(self, conn: Connection, body: bytes):
        self._send_server_info(conn)

    def _reset_room(self):
        state = self.room.state if self.room else "none"
        print(f"[Server] Resetting empty room (state={state})")
        self.room = None
        self.engine = None
        self._player_conns.clear()
        self._pending_game_commands.clear()
        self._game_tick_history.clear()
        self._next_blocking_frame = 0
        self._game_ready = False
        if self._model_ai is not None:
            self._model_ai.close()
        if self._model_ai_future is not None:
            self._model_ai_future.cancel()
        self._model_ai = None
        self._model_ai_future = None
        self._model_ai_load_error = None
        self._model_ai_players.clear()

    def _on_game_start(self, event: Event):
        pkt = pack_start(map_name=self.map_path)
        if self.net is not None:
            self.net.broadcast_raw(pkt)

    def _on_game_over(self, event: Event):
        pkt = pack_game_over()
        if self.net is not None:
            self.net.broadcast_raw(pkt)
        if self.room:
            self.room.state = "waiting"
            self.room.winner = None
            self.room.game_time = 0.0
            self.engine = Engine()
            load_unit_assets(self.engine)
            self.room.engine = self.engine
            self.room.stepper = None
            self._pending_game_commands.clear()
            self._game_tick_history.clear()
            self._next_blocking_frame = 0
            self._game_ready = False
            self._apply_fog()
            for p in self.room.players.values():
                p.present_at_game_start = False
                p.reconnect_packets = []
                if p.conn is not None:
                    p.conn.in_game = False
            if self._model_ai is not None:
                self._model_ai_players = {
                    p.player_id for p in self.room.players.values() if p.is_ai
                }
            self._sync_team_lists()

    def send_chat(self, player_id: int, message: str):
        pkt = pack_chat(message)
        self.net.broadcast_raw(pkt)

    def kick_player(self, player_id: int):
        conn = self._player_conns.pop(player_id, None)
        if conn:
            conn.close()
        if self.room:
            self.room.remove_player(player_id)
        self._model_ai_players.discard(player_id)

    def move_lobby_player(
        self,
        target_position: int,
        destination_position: int,
        requested_team: int,
    ) -> bool:
        if not self.room or self.room.state != "waiting":
            return False

        target_slot = target_position - 1
        destination_slot = destination_position - 1
        if not 0 <= target_slot < self.config.max_players:
            return False
        if not 0 <= destination_slot < self.config.max_players:
            return False

        target = self._player_at_slot(target_slot)
        if target is None:
            return False
        other = self._player_at_slot(destination_slot)
        old_slot = target.slot_id
        old_team = target.team_id

        target.slot_id = destination_slot
        target.team_id = self._resolve_lobby_team(requested_team, destination_slot)
        self._sync_player_connection(target)
        if other is not None and other is not target:
            other.slot_id = old_slot
            other.team_id = old_team if other.team_id != -3 else -3
            self._sync_player_connection(other)

        self._sync_team_lists()
        return True

    def set_lobby_team(self, target_position: int, team_position: int) -> bool:
        if not self.room or self.room.state != "waiting":
            return False
        target = self._player_at_slot(target_position - 1)
        if target is None:
            return False
        target.team_id = team_position - 1
        self._sync_player_connection(target)
        self._sync_team_lists()
        return True

    def _player_at_slot(self, slot_id: int):
        if not self.room:
            return None
        return next(
            (player for player in self.room.players.values()
             if player.slot_id == slot_id),
            None,
        )

    @staticmethod
    def _resolve_lobby_team(requested_team: int, slot_id: int) -> int:
        if requested_team == -1:
            return slot_id % 2
        if requested_team == -4:
            return -3
        return requested_team

    @staticmethod
    def _sync_player_connection(player):
        if player.conn is None:
            return
        player.conn.slot_id = player.slot_id
        player.conn.team_id = player.team_id

    def select_map(self, requested_name: str) -> bool:
        normalized = self._normalize_map_name(requested_name)
        maps_dir = MAPS_DIR / "skirmish"
        for map_file in maps_dir.glob("*.tmx"):
            display_name = self._display_map_name(map_file.name)
            if normalized not in {
                self._normalize_map_name(map_file.stem),
                self._normalize_map_name(display_name),
            }:
                continue
            self.map_name = map_file.name
            self.map_path = f"maps/skirmish/{map_file.name}"
            self._broadcast_server_info()
            return True
        return False

    def start_game(self):
        with self._state_lock:
            self._start_game()

    def _start_game(self):
        if not self.room or self.room.state != "waiting":
            return
        if not any(p.team_id != -3 for p in self.room.players.values()):
            return
        rwrl_core.set_global_random_seed(0)
        self.engine.set_singleplayer_mode(False)
        self.engine.set_unit_cap(self.config.unit_cap)
        self.engine.set_income_mult(self.config.income_mult)
        self.engine.set_nukes_enabled(getattr(self.config, 'nukes_enabled', True))
        self.room.starting_credits = self.config.starting_credits
        map_loaded = self._load_game_map()
        if not map_loaded:
            self._broadcast_system_chat(f'Cannot load map: {self.map_path}')
            return
        self._pending_game_commands.clear()
        self._game_tick_history.clear()
        self._next_blocking_frame = 0
        self._game_ready = False
        self._game_start_time = time.monotonic()
        self.room.start_game(map_loaded=map_loaded)
        self._log_diagnostic(
            'game_started', map=self.map_path, speed=self.config.speed,
            players=[{'player_id': p.player_id, 'slot': p.slot_id,
                      'team': p.team_id, 'ai': p.is_ai}
                     for p in self.room.players.values()],
        )
        if self.config.speed != 1.0:
            self.set_game_speed(self.config.speed)
        self._apply_fog()

    def set_game_speed(self, speed: float) -> bool:
        if not math.isfinite(speed) or speed < 0.1:
            return False
        with self._state_lock:
            self.config._cp.set('server', 'speed', str(speed))
            if self.room and self.room.state == 'playing':
                body = pack_step_rate_command(speed)
                self._pending_game_commands.append((body, parse_game_command(body)))
        return True

    def set_paused(self, paused: bool):
        with self._state_lock:
            if self.engine is not None:
                self.engine.set_paused(paused)
                self._sync_team_lists()

    def _apply_fog(self):
        if not self.engine:
            return
        fog_map = {0: rwrl_core.FogMode.NONE, 1: rwrl_core.FogMode.MAP,
                   2: rwrl_core.FogMode.LOS}
        self.engine.set_fog_mode(fog_map.get(self._fog, rwrl_core.FogMode.LOS))

    def _broadcast_server_info(self):
        n_players = len(self._player_conns)
        pkt = pack_get_server_info(
            player_size=n_players,
            max_player=self.config.max_players,
            map_name=self.map_name,
            income=self.config.income_mult,
            credits=int(self.config.starting_credits),
            no_nukes=not getattr(self.config, 'nukes_enabled', True),
            init_unit=getattr(self.config, 'starting_units', 1),
            mist=self._fog,
            shared_control=False,
        )
        self.net.broadcast_raw(pkt)

    def _load_game_map(self) -> bool:
        candidate = resolve_map_path(self.map_path)
        if not candidate.is_file():
            return False
        try:
            active_slots = [p.slot_id for p in self.room.players.values() if p.team_id != -3]
            self.engine.load_map(str(candidate), active_teams=active_slots)
        except RuntimeError:
            return False
        return True

    @staticmethod
    def _display_map_name(filename: str) -> str:
        name = Path(filename).stem
        if name.startswith("[") and "]" in name:
            name = name.split("]", 1)[1]
        return name.replace("_", " ").strip()

    @staticmethod
    def _normalize_map_name(name: str) -> str:
        return "".join(
            char.lower()
            for char in name.strip(" '\"")
            if char not in " _-"
        )

    def _game_loop(self):
        dt = 1.0 / self._net_tick_rate if self._net_tick_rate > 0 else 1.0 / FRAME_RATE
        unlimited = self._net_tick_rate <= 0
        while self._running:
            with self._state_lock:
                if not self.rl_mode:
                    now = time.monotonic()
                    if now >= self._next_heartbeat_at:
                        self._send_heartbeats()
                        self._next_heartbeat_at = now + 1.0
                    if now >= self._next_team_sync_at:
                        self._sync_team_lists()
                        self._next_team_sync_at = now + 2.0
                self._tick_game()

            if unlimited:
                time.sleep(0)
            else:
                time.sleep(dt)

    def _tick_game(self):
        if not self.room or self.room.state != 'playing' or not self.engine:
            return
        if self.engine.is_paused():
            return
        self._poll_model_ai_loader()
        if not self._game_ready:
            waiting = any(conn.registered and not conn.in_game and not conn.reconnecting
                          for conn in self._player_conns.values())
            if waiting and time.monotonic() - self._game_start_time < 15.0:
                return
            self._game_ready = True

        frame = self.engine.global_tick()
        if self._model_ai is not None and self._model_ai_players:
            try:
                model_commands = self._model_ai.poll_commands()
                if model_commands:
                    print(f'[Server] Model AI emitted {len(model_commands)} '
                          f'commands at frame {frame}')
                for model_command in model_commands:
                    try:
                        body = encode_model_command(model_command)
                        action = parse_game_command(body, tick=frame)
                        # No path cache was captured at decision time; let the
                        # server engine build its path just as the client does.
                        action.path_cache_payload_present = False
                        self._pending_game_commands.append((body, action))
                        self._log_diagnostic(
                            'model_command_encoded', frame=frame,
                            player_slot=int(model_command.control_player_team_id),
                            unit_uid=int(model_command.unit_uid),
                            action=int(model_command.action),
                            action_id=str(model_command.action_id),
                            packet_command_bytes=len(body),
                        )
                    except (ValueError, OverflowError) as exc:
                        print(f'[Server] Model AI command cannot be broadcast: {exc}')
                        self._log_diagnostic(
                            'model_command_dropped', frame=frame,
                            action=int(model_command.action), error=str(exc),
                        )
            except Exception as exc:
                print(f'[Server] Model AI command error: {exc}')

        commands = []
        if frame == self._next_blocking_frame:
            pending = self._pending_game_commands
            self._pending_game_commands = []
            self._next_blocking_frame += 10
            packet = pack_tick(self._next_blocking_frame, [body for body, _ in pending])
            if not self.rl_mode:
                self._game_tick_history.append(packet)
                self._send_game_tick(packet)
            commands = [action for _, action in pending]
        elif frame > self._next_blocking_frame:
            raise RuntimeError('Simulation advanced past the network blocking frame')
        if (self._model_ai is not None and self._model_ai_players and
                frame % self._ai_tick_interval == 0):
            active_slots = [p.slot_id for p in self.room.players.values()
                            if p.team_id != -3]
            self._model_ai.bind(self.engine, active_slots)
            ai_slots = []
            for player_id in sorted(self._model_ai_players):
                player = self.room.players.get(player_id)
                if player is None or player.team_id == -3:
                    continue
                ai_slots.append(player.slot_id)
            self._model_ai.submit(ai_slots)
        self.room.tick(commands=commands)

    def _send_game_tick(self, packet: bytes):
        if not self.room:
            return
        for player in self.room.players.values():
            conn = player.conn
            if conn is not None and (conn.reconnecting or conn.reconnect_activating):
                player.reconnect_packets.append(packet)
                continue
            if (conn is not None and conn.registered and conn.in_game and
                    self._is_current_player_connection(conn)):
                conn.queue_send(packet)

    def _send_heartbeats(self):
        for conn in list(self.net.connections.values()):
            if (not conn.registered or
                    not self._is_current_player_connection(conn)):
                continue
            timestamp_ms = time.time_ns() // 1_000_000
            conn.last_heartbeat_sent_ms = timestamp_ms
            conn.queue_send(pack_heartbeat_request(timestamp_ms))

    def run(self):
        self._running = True
        if not self.rl_mode:
            self.net.start()
            print(f"[Server] RWRL Server running on {self.config.host}:{self.config.port}")
            if self.packet_capture is not None:
                print(f"[Server] Packet log: {self.packet_capture.path}")
        else:
            print(f"[Server] RWRL Server running in RL mode (unlimited TPS, no network)")
        self.events.emit(Event(EventType.SERVER_START))
        self._game_loop()

    def stop(self):
        self._running = False
        if self._model_ai is not None:
            self._model_ai.close()
        if self._model_ai_future is not None:
            self._model_ai_future.cancel()
        self._model_ai_loader.shutdown(wait=False, cancel_futures=True)
        if self.net is not None:
            self.net.stop()
        self._log_diagnostic('server_stopped')
        if self.packet_capture is not None:
            self.packet_capture.close()
            self.packet_capture = None
        self.events.emit(Event(EventType.SERVER_STOP))


def main():
    import argparse
    ap = argparse.ArgumentParser(description='RWRL Server')
    ap.add_argument('-c', '--config', default=None, help='Config file path')
    ap.add_argument('--host', default=None)
    ap.add_argument('--port', type=int, default=None)
    ap.add_argument('--log-file', default=None,
                    help='Enable packet and event logging to this JSONL file')
    args = ap.parse_args()

    cfg = Config(args.config)
    if args.host:
        cfg._cp.set('server', 'host', args.host)
    if args.port is not None:
        cfg._cp.set('server', 'port', str(args.port))
    if args.log_file:
        cfg._cp.set('diagnostics', 'enabled', 'true')
        cfg._cp.set('diagnostics', 'log_file', args.log_file)

    server = RwrlServer(cfg)
    try:
        server.run()
    except KeyboardInterrupt:
        server.stop()


if __name__ == '__main__':
    main()
