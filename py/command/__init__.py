import shlex


class CommandHandler:
    def __init__(self, server):
        self.server = server
        self._commands = {}
        self._register_defaults()

    def register(self, name: str, handler, help_text: str = ""):
        self._commands[name] = (handler, help_text)

    def execute(self, player_id: int, text: str):
        if not text.startswith('/'):
            return False
        parts = text[1:].split(None, 1)
        if not parts:
            return False
        name = parts[0].lower()
        args_str = parts[1] if len(parts) > 1 else ""
        try:
            args = shlex.split(args_str) if args_str else []
        except ValueError:
            args = [args_str]

        entry = self._commands.get(name)
        if not entry:
            return False
        handler, _ = entry
        handler(player_id, args)
        return True

    def help_text(self) -> str:
        lines = []
        for name, (_, h) in sorted(self._commands.items()):
            if h == "HIDE":
                continue
            lines.append(f"  /{name} {h}")
        return '\n'.join(lines)

    def _is_admin(self, pid: int) -> bool:
        if not self.server.room:
            return True
        p = self.server.room.players.get(pid)
        if p and p.is_admin:
            return True
        self.server.send_chat(pid, "Admin only")
        return False

    def _sync(self):
        self.server._sync_team_lists()
        self.server._broadcast_server_info()

    def _register_defaults(self):
        # --- Game Core (HIDE, QC commands) ---
        self.register("credits", self._cmd_credits, "HIDE")
        self.register("nukes", self._cmd_nukes, "HIDE")
        self.register("addai", self._cmd_addai, "HIDE")
        self.register("fog", self._cmd_fog, "HIDE")
        self.register("sharedcontrol", self._cmd_sharedcontrol, "HIDE")
        self.register("startingunits", self._cmd_startingunits, "HIDE")
        self.register("income", self._cmd_income, "clientCommands.income")
        self.register("ai", self._cmd_ai, "clientCommands.ai")
        self.register("start", self._cmd_start, "clientCommands.start")
        self.register("kick", self._cmd_kick, "clientCommands.kick")
        self.register("move", self._cmd_move, "HIDE")
        self.register("team", self._cmd_team, "HIDE")
        self.register("self_team", self._cmd_self_team, "HIDE")
        self.register("self_move", self._cmd_self_move, "HIDE")
        self.register("am", self._cmd_am, "clientCommands.am")
        self.register("battleroom", self._cmd_battleroom, "clientCommands.battleroom")
        self.register("pause", self._cmd_pause, "clientCommands.pause")
        self.register("unpause", self._cmd_unpause, "clientCommands.unpause")

        # --- Game CommandX (visible) ---
        self.register("help", self._cmd_help, "clientCommands.help")
        self.register("map", self._cmd_map, "clientCommands.map")
        self.register("maps", self._cmd_maps, "clientCommands.maps")
        self.register("afk", self._cmd_afk, "clientCommands.afk")
        self.register("status", self._cmd_status, "clientCommands.status")
        self.register("sync", self._cmd_sync, "clientCommands.sync")
        self.register("vote", self._cmd_vote, "clientCommands.vote")
        self.register("setmaxplayer", self._cmd_setmaxplayer, "clientCommands.setmaxplayer")
        self.register("iunit", self._cmd_iunit, "clientCommands.iunit")

        # --- Game Custom ---
        self.register("summon", self._cmd_summon, "clientCommands.summon")
        self.register("addmoney", self._cmd_addmoney, "clientCommands.addmoney")
        self.register("nuke", self._cmd_nuke, "clientCommands.nuke")
        self.register("clone", self._cmd_clone, "clientCommands.clone")
        self.register("myid", self._cmd_myid, "clientCommands.myid")
        self.register("ban", self._cmd_ban, "clientCommands.ban")
        self.register("kickx", self._cmd_kickx, "clientCommands.kickx")
        self.register("allmute", self._cmd_allmute, "clientCommands.allmute")

        # --- Extra ---
        self.register("speed", self._cmd_speed, "[speed multiplier]")
        self.register("stop", self._cmd_stop, "[stop game]")
        self.register("build", self._cmd_build, "<team> <unit> [x] [y]")
        self.register("unitmove", self._cmd_unitmove, "<team> <x> <y>")
        self.register("units", self._cmd_units, "[list unit types]")

    # ------------------------------------------------------------------ #
    # Game Core Commands (QC, matching RW-HPS ClientCommands.kt)
    # ------------------------------------------------------------------ #

    def _cmd_credits(self, pid, args):
        if not self._is_admin(pid):
            return
        if not args:
            self.server.send_chat(pid, f"Credits: {self.server.config.starting_credits}")
            return
        try:
            val = int(args[0])
            self.server.config.starting_credits = float(val)
            self._sync()
        except ValueError:
            pass

    def _cmd_nukes(self, pid, args):
        if not self._is_admin(pid):
            return
        if not args:
            return
        enabled = args[0].lower() in ("on", "true", "1", "yes")
        self.server.config.nukes_enabled = enabled
        if self.server.room and self.server.room.engine:
            for tid in range(4):
                w = self.server.room.engine.get_world(tid)
                if w:
                    w.set_nukes_enabled(enabled)
        self._sync()

    def _cmd_addai(self, pid, args):
        self.server.send_chat(pid, "AI not supported in this server")

    def _cmd_fog(self, pid, args):
        if not self._is_admin(pid):
            return
        if not args:
            labels = {0: "off", 1: "basic", 2: "full"}
            self.server.send_chat(pid, f"Fog: {labels.get(self.server._fog, '?')}")
            return
        fog_str = args[0].lower()
        if fog_str == "off":
            val = 0
        elif fog_str == "basic":
            val = 1
        else:
            val = 2
        self.server._fog = val
        self.server._apply_fog()
        self._sync()

    def _cmd_sharedcontrol(self, pid, args):
        if not self._is_admin(pid):
            return
        if not args:
            return
        enabled = args[0].lower() in ("on", "true", "1", "yes")
        if self.server.room:
            for p in self.server.room.players.values():
                p.shared_control = enabled
        self._sync()

    def _cmd_startingunits(self, pid, args):
        if not self._is_admin(pid):
            return
        if not args:
            return
        try:
            val = int(args[0])
            self.server.config.starting_units = val
            self._sync()
        except ValueError:
            pass

    def _cmd_income(self, pid, args):
        if not self._is_admin(pid):
            return
        if not args:
            self.server.send_chat(pid, f"Income: {getattr(self.server.config, 'income_multiplier', 1.0)}")
            return
        try:
            val = float(args[0])
            self.server.config.income_multiplier = val
            if self.server.room and self.server.room.engine:
                self.server.room.engine.set_income_multiplier(val)
            self._sync()
        except ValueError:
            pass

    def _cmd_ai(self, pid, args):
        if not self._is_admin(pid):
            return
        self.server.send_chat(pid, "AI not supported in this server")

    def _cmd_start(self, pid, args):
        if not self._is_admin(pid):
            return
        if self.server.room and self.server.room.state == "waiting":
            self.server.start_game()

    def _cmd_kick(self, pid, args):
        if not self._is_admin(pid):
            return
        if self.server.room and self.server.room.state == "playing":
            self.server.send_chat(pid, "Game is running")
            return
        if not args:
            return
        p = self._find_player(args[0])
        if p:
            self.server.kick_player(p.player_id)

    def _cmd_move(self, pid, args):
        if not self._is_admin(pid):
            return
        if self.server.room and self.server.room.state == "playing":
            self.server.send_chat(pid, "Game is running")
            return
        if len(args) < 3:
            return
        try:
            self.server.move_lobby_player(int(args[0]), int(args[1]), int(args[2]))
            self._sync()
        except ValueError:
            pass

    def _cmd_team(self, pid, args):
        if not self._is_admin(pid):
            return
        if self.server.room and self.server.room.state == "playing":
            self.server.send_chat(pid, "Game is running")
            return
        if len(args) < 2:
            return
        try:
            self.server.set_lobby_team(int(args[0]), int(args[1]))
            self._sync()
        except ValueError:
            pass

    def _cmd_self_team(self, pid, args):
        if not args:
            return
        try:
            team = int(args[0])
        except ValueError:
            return
        if not self.server.room:
            return
        p = self.server.room.players.get(pid)
        if not p:
            return
        p.team_id = team
        if p.conn:
            p.conn.team_id = team
        self._sync()

    def _cmd_self_move(self, pid, args):
        if len(args) < 2:
            return
        try:
            position = int(args[0])
            team = int(args[1])
            self.server.move_lobby_player(position, position, team)
            self._sync()
        except ValueError:
            pass

    def _cmd_am(self, pid, args):
        if not self._is_admin(pid):
            return
        if self.server.room and self.server.room.state == "playing":
            self.server.send_chat(pid, "Game is running")
            return
        if not args:
            return
        enabled = args[0].lower() in ("on", "true", "1", "yes")
        if enabled and self.server.room:
            for p in self.server.room.players.values():
                p.team_id = p.slot_id % 2
        self._sync()
        self.server.send_chat(pid, f"Auto-mix: {'on' if enabled else 'off'}")

    def _cmd_battleroom(self, pid, args):
        if not self.server.room or self.server.room.state != "playing":
            self.server.send_chat(pid, "No game running")
            return
        if not self._is_admin(pid):
            return
        self.server.room.end_game(-1)
        self.server.send_chat(pid, "Returned to battle room")

    def _cmd_pause(self, pid, args):
        if not self.server.room or self.server.room.state != "playing":
            self.server.send_chat(pid, "No game running")
            return
        if not self._is_admin(pid):
            return
        self.server.room.engine.set_speed_mult(0.0)
        self.server.send_chat(pid, "Game paused")

    def _cmd_unpause(self, pid, args):
        if not self.server.room or self.server.room.state != "playing":
            self.server.send_chat(pid, "No game running")
            return
        if not self._is_admin(pid):
            return
        self.server.room.engine.set_speed_mult(self.server.config.speed)
        self.server.send_chat(pid, "Game unpaused")

    # ------------------------------------------------------------------ #
    # Game CommandX (visible help)
    # ------------------------------------------------------------------ #

    def _cmd_help(self, pid, args):
        self.server.send_chat(pid, self.help_text())

    def _cmd_map(self, pid, args):
        if not self._is_admin(pid):
            return
        if self.server.room and self.server.room.state == "playing":
            self.server.send_chat(pid, "Game is running")
            return
        if not args:
            return
        requested_name = " ".join(args)
        if not self.server.select_map(requested_name):
            self.server.send_chat(pid, f"Map not found: {requested_name}")

    def _cmd_maps(self, pid, args):
        if self.server.room and self.server.room.state == "playing":
            self.server.send_chat(pid, "Game is running")
            return
        from resource_paths import MAPS_DIR
        maps_dir = MAPS_DIR / "skirmish"
        if not maps_dir.is_dir():
            self.server.send_chat(pid, "No maps directory")
            return
        maps = sorted(maps_dir.glob("*.tmx"))
        for i, m in enumerate(maps):
            self.server.send_chat(pid, f"  [{i}] {m.stem}")

    def _cmd_afk(self, pid, args):
        if not self.server.room:
            return
        p = self.server.room.players.get(pid)
        if not p or p.is_admin:
            self.server.send_chat(pid, "Admin cannot AFK")
            return
        if self.server.room.state == "playing":
            self.server.send_chat(pid, "Game is running")
            return
        old_admin = None
        for op in self.server.room.players.values():
            if op.is_admin and op.player_id != pid:
                return
            if op.is_admin:
                old_admin = op
        if old_admin:
            old_admin.is_admin = False
        p.is_admin = True
        self.server._broadcast_system_chat(f"{p.name} is now admin (AFK transfer)")
        self._sync()

    def _cmd_status(self, pid, args):
        room = self.server.room
        if not room:
            self.server.send_chat(pid, "No room")
            return
        n = len(room.players)
        self.server.send_chat(pid, f"Players: {n}, State: {room.state}")

    def _cmd_sync(self, pid, args):
        if not self._is_admin(pid):
            return
        if not args:
            return
        enabled = args[0].lower() in ("on", "true", "1", "yes")
        self.server.send_chat(pid, f"Sync: {'on' if enabled else 'off'}")

    def _cmd_vote(self, pid, args):
        if not args:
            return
        if args[0].lower() == "gameover":
            if self.server.room and self.server.room.state == "playing":
                self.server.room.end_game(-1)
                self.server.send_chat(pid, "Vote: game over")

    def _cmd_setmaxplayer(self, pid, args):
        if not self._is_admin(pid):
            return
        if not args:
            return
        try:
            val = int(args[0])
            self.server.config.max_players = val
            self._sync()
        except ValueError:
            pass

    def _cmd_iunit(self, pid, args):
        if not self._is_admin(pid):
            return
        if self.server.room and self.server.room.state == "playing":
            self.server.send_chat(pid, "Game is running")
            return
        if len(args) < 2:
            return
        try:
            pos = int(args[0]) - 1
            unit_id = int(args[1])
            if self.server.room:
                for p in self.server.room.players.values():
                    if p.slot_id == pos:
                        p.start_unit = unit_id
                        break
        except ValueError:
            pass

    # ------------------------------------------------------------------ #
    # Game Custom Commands
    # ------------------------------------------------------------------ #

    def _cmd_summon(self, pid, args):
        if not self.server.room or self.server.room.state != "playing":
            self.server.send_chat(pid, "No game running")
            return
        if not self._is_admin(pid):
            return
        if not args:
            return
        name = args[0]
        self.server.send_chat(pid, f"Summon {name}: click on map to place")
        self.server._pending_summon = (pid, name)

    def _cmd_addmoney(self, pid, args):
        if not self.server.room or self.server.room.state != "playing":
            self.server.send_chat(pid, "No game running")
            return
        if not self._is_admin(pid):
            return
        if len(args) < 2:
            return
        p = self._find_player(args[0])
        if not p:
            self.server.send_chat(pid, "Player not found")
            return
        try:
            amount = int(args[1])
            w = self.server.room.engine.get_world(p.team_id)
            if w:
                w.add_credits(float(amount))
                self.server.send_chat(pid, f"Added {amount} credits to {p.name}")
        except ValueError:
            self.server.send_chat(pid, "Invalid amount")

    def _cmd_nuke(self, pid, args):
        if not self.server.room or self.server.room.state != "playing":
            self.server.send_chat(pid, "No game running")
            return
        if not self._is_admin(pid):
            return
        self.server.send_chat(pid, "Nuke: click on map to place")
        self.server._pending_nuke = pid

    def _cmd_clone(self, pid, args):
        if not self.server.room or self.server.room.state != "playing":
            self.server.send_chat(pid, "No game running")
            return
        if not self._is_admin(pid):
            return
        self.server.send_chat(pid, "Clone: click on map to place")
        self.server._pending_clone = pid

    def _cmd_myid(self, pid, args):
        p = self.server.room.players.get(pid) if self.server.room else None
        name = p.name if p else f"Player{pid}"
        self.server.send_chat(pid, f"Your ID: {pid}, Name: {name}")

    def _cmd_ban(self, pid, args):
        if not self._is_admin(pid):
            return
        if not args:
            return
        p = self._find_player(args[0])
        if p:
            self.server.kick_player(p.player_id)

    def _cmd_kickx(self, pid, args):
        if not self._is_admin(pid):
            return
        if not args:
            return
        p = self._find_player(args[0])
        if p:
            self.server.kick_player(p.player_id)

    def _cmd_allmute(self, pid, args):
        if not self._is_admin(pid):
            return
        if not args:
            return
        enabled = args[0].lower() in ("on", "true", "1", "yes")
        self.server._all_mute = enabled
        self.server.send_chat(pid, f"All mute: {'on' if enabled else 'off'}")

    # ------------------------------------------------------------------ #
    # Extra Commands
    # ------------------------------------------------------------------ #

    def _cmd_speed(self, pid, args):
        if not self._is_admin(pid):
            return
        if not args:
            spd = self.server.room.engine.speed_mult if self.server.room and self.server.room.engine else 0
            self.server.send_chat(pid, f"Speed: {spd}")
            return
        try:
            spd = float(args[0])
            self.server.room.engine.set_speed_mult(spd)
            self.server.send_chat(pid, f"Speed set to {spd}")
        except (ValueError, AttributeError):
            pass

    def _cmd_stop(self, pid, args):
        if not self._is_admin(pid):
            return
        if self.server.room:
            self.server.room.end_game(-1)
            self.server.send_chat(pid, "Game stopped")

    def _cmd_build(self, pid, args):
        if not self._is_admin(pid):
            return
        if not self.server.room or self.server.room.state != "playing":
            self.server.send_chat(pid, "No game running")
            return
        if len(args) < 2:
            return
        try:
            tid = int(args[0])
            name = args[1]
            x = float(args[2]) if len(args) > 2 else 500 + tid * 800
            y = float(args[3]) if len(args) > 3 else 600
            w = self.server.room.engine.get_world(tid)
            if not w:
                return
            cost = self.server.room.engine.registry().get_cost(name)
            if not cost:
                self.server.send_chat(pid, f"Unknown unit: {name}")
                return
            if self.server.room.engine.registry().is_building(name):
                w.start_build(name, x, y)
            else:
                if not w.spend_credits(cost):
                    self.server.send_chat(pid, "Insufficient credits")
                    return
                w.spawn_unit(name, x, y)
            self.server.send_chat(pid, f"Built {name} at ({x:.0f},{y:.0f})")
        except (ValueError, IndexError):
            pass

    def _cmd_unitmove(self, pid, args):
        if not self._is_admin(pid):
            return
        if not self.server.room or self.server.room.state != "playing":
            return
        if len(args) < 3:
            return
        try:
            tid = int(args[0])
            tx, ty = float(args[1]), float(args[2])
            w = self.server.room.engine.get_world(tid)
            if not w:
                return
            n = 0
            for uid, u in w.units().items():
                if u.is_alive and u.is_mobile():
                    w.move_unit_to(u, tx, ty)
                    n += 1
            self.server.send_chat(pid, f"Moved {n} units")
        except (ValueError, IndexError):
            pass

    def _cmd_units(self, pid, args):
        if not self.server.room or not self.server.room.engine:
            return
        names = self.server.room.engine.registry().all_names()
        for i in range(0, len(names), 10):
            self.server.send_chat(pid, ", ".join(names[i:i+10]))

    # ------------------------------------------------------------------ #
    # Helpers
    # ------------------------------------------------------------------ #

    def _find_player(self, target_str: str):
        room = self.server.room
        if not room:
            return None
        try:
            pos = int(target_str)
            for p in room.players.values():
                if p.slot_id + 1 == pos:
                    return p
        except ValueError:
            for p in room.players.values():
                if target_str.lower() in p.name.lower():
                    return p
        return None
