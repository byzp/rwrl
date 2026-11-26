from event import EventBus, EventType, Event
from rl.native_commands import NativeStepper


class Player:
    __slots__ = (
        'player_id', 'slot_id', 'name', 'team_id', 'conn', 'client_id',
        'is_admin', 'reconnect_packets', 'present_at_game_start',
        'shared_control',
    )

    def __init__(self, player_id: int, name: str, team_id: int, conn=None,
                 slot_id: int = None, client_id: str = None):
        self.player_id = player_id
        self.slot_id = player_id - 1 if slot_id is None else slot_id
        self.name = name
        self.team_id = team_id
        self.conn = conn
        self.client_id = client_id
        self.is_admin = False
        self.reconnect_packets = []
        self.present_at_game_start = False
        self.shared_control = False

    def __repr__(self):
        return f"Player({self.player_id}, {self.name}, T{self.team_id})"


class Room:
    def __init__(self, room_id: int, engine, events: EventBus,
                 speed=2.5, starting_credits=4000.0,
                 map_width=2000, map_height=2000):
        self.room_id = room_id
        self.engine = engine
        self.events = events
        self.speed = speed
        self.starting_credits = starting_credits
        self.map_width = map_width
        self.map_height = map_height

        self.players: dict[int, Player] = {}
        self.state = "waiting"
        self.game_time = 0.0
        self.winner = None
        self.stepper = None
        self._next_pid = 1

    def add_player(self, name: str, conn=None, client_id: str = None) -> Player:
        used_slots = {player.slot_id for player in self.players.values()}
        slot_id = 0
        while slot_id in used_slots:
            slot_id += 1
        p = Player(
            self._next_pid,
            name,
            slot_id % 2,
            conn,
            slot_id=slot_id,
            client_id=client_id,
        )
        self._next_pid += 1
        self.players[p.player_id] = p
        self.events.emit(Event(EventType.PLAYER_JOIN, player=p))
        return p

    def remove_player(self, player_id: int):
        p = self.players.pop(player_id, None)
        if p:
            self.events.emit(Event(EventType.PLAYER_LEAVE, player=p))

    def start_game(self, map_loaded: bool = False):
        if self.state != "waiting":
            return
        for player in self.players.values():
            player.present_at_game_start = (
                player.conn is not None and player.conn.registered
            )
        participants = [p for p in self.players.values() if p.team_id != -3]
        if not map_loaded:
            self.engine.setup_teams_vec([p.slot_id for p in participants])
        for player in participants:
            world = self.engine.create_world(player.slot_id)
            self.engine.set_team_alliance(player.slot_id, player.team_id)
            world.add_credits(self.starting_credits - world.credits())
        self.stepper = NativeStepper(self.engine)
        self.state = "playing"
        self.game_time = 0.0
        self.winner = None
        self.events.emit(Event(EventType.GAME_START, room=self))

    def end_game(self, winner_team: int):
        if self.state != "playing":
            return
        self.state = "game_over"
        self.winner = winner_team
        self.events.emit(Event(EventType.GAME_OVER, winner=winner_team, room=self))

    def tick(self, dt: float = None, *, commands=()):
        if self.state != "playing" or self.engine.is_paused():
            return
        if self.stepper is None or self.stepper.engine is not self.engine:
            self.stepper = NativeStepper(self.engine)
        self.stepper.advance(commands, delta=None if dt is None else dt * 60.0)
        self.game_time = self.engine.game_time()

        if self.engine.global_tick() % 60 == 0:
            self._check_victory()

    def _check_victory(self):
        if self.game_time < 5.0:
            return
        state = self.engine.get_state()
        if state.game_over:
            self.end_game(state.winner_team)

    def get_state(self):
        return {
            'room_id': self.room_id,
            'state': self.state,
            'game_time': self.game_time,
            'winner': self.winner,
            'players': len(self.players),
            'engine': self.engine.get_state(),
        }
