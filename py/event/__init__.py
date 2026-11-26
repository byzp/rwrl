from enum import Enum


class EventType(Enum):
    PLAYER_JOIN = "player_join"
    PLAYER_LEAVE = "player_leave"
    PLAYER_CHAT = "player_chat"
    GAME_START = "game_start"
    GAME_OVER = "game_over"
    UNIT_BUILT = "unit_built"
    UNIT_KILLED = "unit_killed"
    UNIT_LOST = "unit_lost"
    BUILDING_COMPLETE = "building_complete"
    ATTACK_COMMAND = "attack_command"
    MOVE_COMMAND = "move_command"
    BUILD_COMMAND = "build_command"
    SERVER_START = "server_start"
    SERVER_STOP = "server_stop"
    TICK = "tick"


class Event:
    __slots__ = ('type', 'data')

    def __init__(self, type_: EventType, **kwargs):
        self.type = type_
        self.data = kwargs

    def __repr__(self):
        return f"Event({self.type.value}, {self.data})"


class EventBus:
    def __init__(self):
        self._handlers = {}

    def on(self, event_type: EventType, handler):
        self._handlers.setdefault(event_type, []).append(handler)

    def off(self, event_type: EventType, handler):
        handlers = self._handlers.get(event_type, [])
        if handler in handlers:
            handlers.remove(handler)

    def emit(self, event: Event):
        for handler in self._handlers.get(event.type, []):
            try:
                handler(event)
            except Exception as e:
                print(f"[EventBus] handler error: {e}")

    def clear(self):
        self._handlers.clear()
