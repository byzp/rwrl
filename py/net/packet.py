import gzip
import struct
from enum import IntEnum


PROTOCOL_VERSION = 176
MAX_PACKET_SIZE = 50_000_000
GAME_ID = "com.corrodinggames.rts"
SERVER_ID = "net.rwrl.server"
SERVER_UUID = "rwrl-python-server"
DEFAULT_MAP = "[p2]Small_Island (2p).tmx"
DEFAULT_MAP_PATH = f"maps/skirmish/{DEFAULT_MAP}"
STARTING_CREDIT_VALUES = (4000, 0, 1000, 2000, 5000, 10000, 50000, 100000, 200000)


def starting_credit_mode(credits: int) -> int:
    try:
        return STARTING_CREDIT_VALUES.index(credits)
    except ValueError as exc:
        raise ValueError(f'Unsupported original 1.15 starting credits: {credits}') from exc


class PacketType(IntEnum):
    PACKET_DOWNLOAD_PENDING = 4
    TICK = 10
    GAMECOMMAND_RECEIVE = 20
    SYNC_CHECK = 30
    SYNCCHECKSUM_STATUS = 31
    SYNC = 35
    SERVER_INFO = 106
    HEART_BEAT = 108
    HEART_BEAT_RESPONSE = 109
    REGISTER_PLAYER = 110
    DISCONNECT = 111
    ACCEPT_START_GAME = 112
    PASSWD_ERROR = 113
    TEAM_LIST = 115
    START_GAME = 120
    RETURN_TO_BATTLEROOM = 122
    CHAT_RECEIVE = 140
    CHAT = 141
    KICK = 150
    PREREGISTER_INFO_RECEIVE = 160
    PREREGISTER_INFO = 161
    PACKET_RECONNECT_TO = 178
    GET_SERVER_INFO_RECEIVE = 3000
    GET_SERVER_INFO = 3001

    GAME_COMMAND = GAMECOMMAND_RECEIVE
    GAME_COMMAND_ONE = GAMECOMMAND_RECEIVE
    REGISTER = REGISTER_PLAYER
    START = START_GAME
    GAME_OVER = RETURN_TO_BATTLEROOM
    PING = HEART_BEAT


def pack_header(pkt_type: int, length: int) -> bytes:
    return struct.pack(">II", length, int(pkt_type))


def unpack_header(data: bytes, offset: int = 0):
    if len(data) - offset < 8:
        return None, 0
    length, pkt_type = struct.unpack_from(">II", data, offset)
    return pkt_type, length


def pack_packet(pkt_type: int, body: bytes = b"") -> bytes:
    return pack_header(pkt_type, len(body)) + body


def _encode_modified_utf8(value: str) -> bytes:
    encoded = bytearray()
    utf16 = value.encode("utf-16-be", errors="surrogatepass")
    for offset in range(0, len(utf16), 2):
        code = (utf16[offset] << 8) | utf16[offset + 1]
        if 0x0001 <= code <= 0x007F:
            encoded.append(code)
        elif code <= 0x07FF:
            encoded.extend((0xC0 | (code >> 6), 0x80 | (code & 0x3F)))
        else:
            encoded.extend((
                0xE0 | (code >> 12),
                0x80 | ((code >> 6) & 0x3F),
                0x80 | (code & 0x3F),
            ))
    return bytes(encoded)


def _decode_modified_utf8(data: bytes) -> str:
    utf16 = bytearray()
    offset = 0
    while offset < len(data):
        first = data[offset]
        if first & 0x80 == 0:
            code = first
            offset += 1
        elif first & 0xE0 == 0xC0:
            if offset + 1 >= len(data):
                raise ValueError("truncated modified UTF-8 sequence")
            code = ((first & 0x1F) << 6) | (data[offset + 1] & 0x3F)
            offset += 2
        elif first & 0xF0 == 0xE0:
            if offset + 2 >= len(data):
                raise ValueError("truncated modified UTF-8 sequence")
            code = (
                ((first & 0x0F) << 12)
                | ((data[offset + 1] & 0x3F) << 6)
                | (data[offset + 2] & 0x3F)
            )
            offset += 3
        else:
            raise ValueError("invalid modified UTF-8 sequence")
        utf16.extend(struct.pack(">H", code))
    return bytes(utf16).decode("utf-16-be", errors="surrogatepass")


def pack_string(value: str) -> bytes:
    encoded = _encode_modified_utf8(value)
    if len(encoded) > 0xFFFF:
        raise ValueError("Java UTF string exceeds 65535 encoded bytes")
    return struct.pack(">H", len(encoded)) + encoded


def unpack_string(data: bytes, offset: int = 0):
    reader = PacketReader(data, offset)
    value = reader.read_string()
    return value, reader.offset


def _pack_optional_string(value: str | None) -> bytes:
    if value is None:
        return b"\x00"
    return b"\x01" + pack_string(value)


def _pack_optional_int(value: int | None) -> bytes:
    if value is None:
        return b"\x00"
    return b"\x01" + struct.pack(">i", value)


class PacketReader:
    def __init__(self, data: bytes, offset: int = 0):
        self.data = data
        self.offset = offset

    @property
    def remaining(self):
        return len(self.data) - self.offset

    def read(self, size: int) -> bytes:
        end = self.offset + size
        if size < 0 or end > len(self.data):
            raise ValueError(
                f"packet ended at offset {self.offset}; needed {size} more bytes"
            )
        value = self.data[self.offset:end]
        self.offset = end
        return value

    def read_byte(self) -> int:
        v = struct.unpack_from(">b", self.data, self.offset)[0]; self.offset += 1; return v

    def read_bool(self) -> bool:
        v = self.data[self.offset] != 0; self.offset += 1; return v

    def read_int(self) -> int:
        v = struct.unpack_from(">i", self.data, self.offset)[0]; self.offset += 4; return v

    def read_long(self) -> int:
        v = struct.unpack_from(">q", self.data, self.offset)[0]; self.offset += 8; return v

    def read_string(self) -> str:
        length = struct.unpack_from(">H", self.data, self.offset)[0]
        self.offset += 2
        raw = self.data[self.offset:self.offset + length]
        self.offset += length
        return _decode_modified_utf8(raw)

    def read_optional_string(self) -> str | None:
        return self.read_string() if self.read_bool() else None


def parse_preregister(body: bytes) -> dict:
    reader = PacketReader(body)
    game_id = reader.read_string()
    packet_version = reader.read_int()
    client_version = reader.read_int()
    platform = reader.read_int() if packet_version >= 1 else 1
    query = reader.read_optional_string() if packet_version >= 2 else None
    player_name = reader.read_string() if packet_version >= 3 else None
    if packet_version >= 4:
        reader.read_string()
        reader.read_string()
    return {
        "game_id": game_id,
        "packet_version": packet_version,
        "client_version": client_version,
        "platform": platform,
        "query": query,
        "player_name": player_name,
    }


def parse_register_player(body: bytes) -> dict:
    reader = PacketReader(body)
    game_id = reader.read_string()
    packet_version = reader.read_int()
    client_version = reader.read_int()
    game_version = reader.read_int()
    name = reader.read_string()
    password = reader.read_optional_string()
    package_name = reader.read_string() if packet_version >= 1 else None
    client_id = reader.read_string() if packet_version >= 2 else None
    unit_checksum = reader.read_int() if packet_version >= 3 else None
    integrity = reader.read_string() if packet_version >= 4 else None
    extra = reader.read_string() if packet_version >= 5 else None
    return {
        "game_id": game_id,
        "packet_version": packet_version,
        "client_version": client_version,
        "game_version": game_version,
        "name": name,
        "password": password,
        "package_name": package_name,
        "client_id": client_id,
        "unit_checksum": unit_checksum,
        "integrity": integrity,
        "extra": extra,
    }


def pack_preregister_info() -> bytes:
    body = bytearray()
    body += pack_string(GAME_ID)
    body += struct.pack(">iii", 2, PROTOCOL_VERSION, PROTOCOL_VERSION)
    body += pack_string(SERVER_ID)
    body += pack_string(SERVER_UUID)
    body += struct.pack(">iii", 0, 0, 0)
    return pack_packet(PacketType.PREREGISTER_INFO, bytes(body))


def pack_server_info(
    map_name: str = DEFAULT_MAP,
    max_units: int = 200,
    starting_units: int = 1,
    income_multiplier: float = 1.0,
    is_admin: bool = True,
    fog: int = 2,
    starting_credits: int = 4000,
    no_nukes: bool = False,
) -> bytes:
    body = bytearray()
    body += pack_string(GAME_ID)
    body += struct.pack(">ii", PROTOCOL_VERSION, 0)
    body += pack_string(map_name)
    body += struct.pack(">ii?i", starting_credit_mode(starting_credits), fog, True, 1)
    body += struct.pack(">b??", 8, is_admin, is_admin)
    body += struct.pack(">iiif", max_units, max_units, starting_units, income_multiplier)
    body += struct.pack(">???", no_nukes, False, False)
    body += struct.pack(">?????i", False, False, False, True, False, 0)
    return pack_packet(PacketType.SERVER_INFO, bytes(body))


def _pack_team_player(
    slot: int,
    name: str,
    team_id: int,
    credits: int,
    is_admin: bool = False,
    shared_control: bool = False,
) -> bytes:
    data = bytearray()
    data += struct.pack(">bii", slot, credits, team_id)
    data += _pack_optional_string(name)
    data += struct.pack(">?iq", False, -1, -1)
    data += struct.pack(">?ii", False, 0, 0)
    data += struct.pack(">b????i", 0, shared_control, False, False, False, -9999)
    data += _pack_optional_string(None)
    data += struct.pack(">i", 1 if is_admin else 0)
    for _ in range(4):
        data += _pack_optional_int(None)
    data += struct.pack(">i", -1)
    return bytes(data)


def pack_team_list(
    players,
    local_slot: int,
    max_players: int = 10,
    starting_credits: int = 4000,
    max_units: int = 200,
    starting_units: int = 1,
    income_multiplier: float = 1.0,
    paused: bool = False,
    fog: int = 2,
    no_nukes: bool = False,
) -> bytes:
    max_players = max(2, min(int(max_players), 100))
    team_data = bytearray()
    players_by_slot = {
        player.slot_id: player
        for player in players
        if 0 <= player.slot_id < max_players
    }
    for slot in range(max_players):
        player = players_by_slot.get(slot)
        team_data += struct.pack(">?", player is not None)
        if player is not None:
            team_data += struct.pack(">i", 0)
            team_data += _pack_team_player(
                slot,
                player.name,
                player.team_id,
                int(starting_credits),
                getattr(player, "is_admin", False),
                getattr(player, "shared_control", False),
            )

    compressed = gzip.compress(bytes(team_data), mtime=0)
    body = bytearray(struct.pack(">i?i", local_slot, False, max_players))
    body += pack_string("teams")
    body += struct.pack(">i", len(compressed))
    body += compressed
    body += struct.pack(">ii?i", fog, starting_credit_mode(starting_credits), True, 1)
    body += struct.pack(">bi", 5, max_units)
    body += struct.pack(">iif", max_units, starting_units, income_multiplier)
    body += struct.pack(">?????", no_nukes, False, False, False, paused)
    return pack_packet(PacketType.TEAM_LIST, bytes(body))


def pack_chat(message: str, sender: str | None = None, team_id: int = -1) -> bytes:
    body = bytearray(pack_string(message))
    body += struct.pack(">b", 3)
    body += _pack_optional_string(sender)
    body += struct.pack(">ii", team_id, team_id)
    return pack_packet(PacketType.CHAT, bytes(body))


def pack_heartbeat_response(body: bytes) -> bytes:
    reader = PacketReader(body)
    timestamp = reader.read_long()
    if reader.remaining:
        reader.read_byte()
    return pack_packet(
        PacketType.HEART_BEAT_RESPONSE,
        struct.pack(">qbb", timestamp, 1, 60),
    )


def pack_heartbeat_request(timestamp_ms: int) -> bytes:
    body = struct.pack(">qb", int(timestamp_ms), 0)
    return pack_packet(PacketType.HEART_BEAT, body)


def parse_heartbeat_response(body: bytes) -> dict:
    reader = PacketReader(body)
    timestamp_ms = reader.read_long()
    protocol = reader.read_byte() if reader.remaining else 0
    fps = reader.read_byte() if protocol >= 1 and reader.remaining else None
    return {
        "timestamp_ms": timestamp_ms,
        "protocol": protocol,
        "fps": fps,
    }


def pack_tick(tick: int, commands=None) -> bytes:
    commands = list(commands or ())
    body = bytearray(struct.pack(">ii", int(tick), len(commands)))
    for command in commands:
        body += command
    return pack_packet(PacketType.TICK, bytes(body))


def validate_game_command(body: bytes) -> None:
    reader = PacketReader(body)
    if reader.read_string() != "c":
        raise ValueError("game command block is not named 'c'")
    block_length = reader.read_int()
    if block_length < 0:
        raise ValueError("game command block has a negative length")
    reader.read(block_length)
    if reader.remaining:
        raise ValueError(f"game command has {reader.remaining} trailing bytes")


def pack_step_rate_command(rate: float) -> bytes:
    """Serialize the host's e.r/e.s command in the original 1.15 wire format."""
    payload = bytearray(struct.pack('>b???ii??i??q',
                                    -1, False, False, False, -1, -1,
                                    False, False, 0, False, False, -1))
    payload += pack_string('-1')
    payload += struct.pack('>?H?bffii?', False, 0, True, 0, rate, 0.0, 0, 0, False)
    return pack_string('c') + struct.pack('>i', len(payload)) + payload


def pack_start(
    *_unused,
    map_name: str = DEFAULT_MAP_PATH,
) -> bytes:
    body = (
        struct.pack(">bi", 0, 0)
        + pack_string(map_name)
    )
    return pack_packet(PacketType.START_GAME, body)


def pack_reconnect_to(
    player_names: list[str],
    spectator: bool = False,
) -> bytes:
    body = struct.pack(">bi?", 0, len(player_names), spectator)
    body += struct.pack(">i", len(player_names))
    for name in player_names:
        body += pack_string(name)
    return pack_packet(PacketType.PACKET_RECONNECT_TO, body)


def pack_kick(reason: str) -> bytes:
    return pack_packet(PacketType.KICK, pack_string(reason))


def pack_disconnect(reason: str) -> bytes:
    return pack_packet(PacketType.DISCONNECT, pack_string(reason))


def parse_get_server_info(body: bytes) -> dict:
    reader = PacketReader(body)
    server_id = reader.read_string()
    connect_index = reader.read_int()
    map_type = reader.read_int()
    map_name = reader.read_string()
    credits = reader.read_int()
    fog = reader.read_int()
    revealed_map = reader.read_bool()
    ai_difficulty = reader.read_int()
    return {
        "server_id": server_id,
        "connect_index": connect_index,
        "map_type": map_type,
        "map_name": map_name,
        "credits": credits,
        "fog": fog,
        "revealed_map": revealed_map,
        "ai_difficulty": ai_difficulty,
    }


def pack_get_server_info(
    player_size: int = 0,
    max_player: int = 10,
    map_name: str = "",
    income: float = 1.0,
    credits: int = 4000,
    no_nukes: bool = False,
    init_unit: int = 1,
    mist: int = 2,
    shared_control: bool = False,
) -> bytes:
    body = bytearray()
    body += pack_string("ServerNetType")
    body += pack_string("Server")
    body += pack_string("PlayerSize")
    body += struct.pack(">i", player_size)
    body += pack_string("MaxPlayer")
    body += struct.pack(">i", max_player)
    body += pack_string("MapName")
    body += pack_string(map_name)
    body += pack_string("Income")
    body += struct.pack(">f", income)
    body += pack_string("Credits")
    body += struct.pack(">i", credits)
    body += pack_string("NoNukes")
    body += struct.pack(">?", no_nukes)
    body += pack_string("InitUnit")
    body += struct.pack(">i", init_unit)
    body += pack_string("Mist")
    body += struct.pack(">i", mist)
    body += pack_string("SharedControl")
    body += struct.pack(">?", shared_control)
    return pack_packet(PacketType.GET_SERVER_INFO, bytes(body))


def pack_game_command_one(*_args, **_kwargs) -> bytes:
    raise NotImplementedError("native Rusted Warfare command encoding is not implemented")


def pack_register_response(*_args, **_kwargs) -> bytes:
    return pack_preregister_info()


def pack_game_over(*_args, **_kwargs) -> bytes:
    return pack_packet(PacketType.RETURN_TO_BATTLEROOM, b"\x00")
