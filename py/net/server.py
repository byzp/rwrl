import errno
import selectors
import socket
import struct
import threading
import time
from collections import deque

from .packet import PacketType, pack_header, unpack_header, MAX_PACKET_SIZE

_RECV_BUF_SIZE = 65536
_WRITE_BUF_HIGH = 4 * 1024 * 1024
_TICK_HISTORY_CAP = 6000


class Connection:
    __slots__ = (
        'conn_id', 'sock', 'addr', 'player_id', 'team_id', 'slot_id',
        'registered', 'preregistered', 'in_game', 'client_version',
        'ping_ms', 'last_heartbeat_sent_ms', 'last_heartbeat_ms',
        'fps', 'client_id', 'disconnect_reason', 'reconnecting',
        'reconnect_activating', '_recv_buf', '_write_buf',
        '_io_thread', '_closed', '_lock',
        '_packet_logger',
    )

    def __init__(self, sock: socket.socket, conn_id: int, addr: str,
                 packet_logger=None):
        self.conn_id = conn_id
        self.sock = sock
        self.addr = addr
        self.player_id = -1
        self.team_id = -1
        self.slot_id = -1
        self.registered = False
        self.preregistered = False
        self.in_game = False
        self.client_version = 0
        self.ping_ms = -1
        self.last_heartbeat_sent_ms = -1
        self.last_heartbeat_ms = -1
        self.fps = None
        self.client_id = None
        self.disconnect_reason = None
        self.reconnecting = False
        self.reconnect_activating = False
        self._recv_buf = bytearray()
        self._write_buf = bytearray()
        self._io_thread = None
        self._closed = False
        self._lock = threading.Lock()
        self._packet_logger = packet_logger

    def queue_send(self, data: bytes):
        with self._lock:
            if self._closed:
                return
            self._write_buf.extend(data)
            if len(self._write_buf) > _WRITE_BUF_HIGH:
                self._close_locked()
                return
        if self._packet_logger is not None:
            self._packet_logger('server_to_client', self, data)
        if self._io_thread is not None:
            self._io_thread.request_write(self)

    def queue_send_packet(self, pkt_type: int, body: bytes = b''):
        self.queue_send(pack_header(pkt_type, len(body)) + body)

    def _close_locked(self):
        if not self._closed:
            self._closed = True
            try:
                self.sock.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            try:
                self.sock.close()
            except OSError:
                pass

    def close(self):
        with self._lock:
            self._close_locked()

    @property
    def write_pending(self) -> bool:
        with self._lock:
            return len(self._write_buf) > 0 and not self._closed

    def __repr__(self):
        return f"Connection({self.conn_id}, {self.addr}, pid={self.player_id})"


class _IOThread:
    def __init__(self, thread_id: int, server: 'NetServer'):
        self.thread_id = thread_id
        self.server = server
        self._sel = selectors.EpollSelector()
        self._thread = None
        self._running = False
        self._new_conns: deque[Connection] = deque()
        self._new_conns_lock = threading.Lock()
        self._wake_r, self._wake_w = socket.socketpair()
        self._wake_r.setblocking(False)
        self._wake_w.setblocking(False)
        self._sel.register(self._wake_r, selectors.EVENT_READ)

    def start(self):
        self._running = True
        self._thread = threading.Thread(
            target=self._run, name=f"io-thread-{self.thread_id}", daemon=True)
        self._thread.start()

    def stop(self):
        self._running = False
        try:
            self._wake_w.send(b'\x00')
        except OSError:
            pass
        if self._thread is not None:
            self._thread.join(timeout=5.0)

    def add_connection(self, conn: Connection):
        with self._new_conns_lock:
            self._new_conns.append(conn)
        try:
            self._wake_w.send(b'\x00')
        except OSError:
            pass

    def request_write(self, conn: Connection):
        if not self._running:
            return
        try:
            fd = conn.sock.fileno()
            if fd >= 0:
                self._sel.modify(conn.sock, selectors.EVENT_READ | selectors.EVENT_WRITE, conn)
        except (OSError, KeyError, ValueError):
            pass

    def _drain_new_conns(self):
        with self._new_conns_lock:
            conns = list(self._new_conns)
            self._new_conns.clear()
        for conn in conns:
            if conn._closed:
                continue
            conn._io_thread = self
            try:
                self._sel.register(conn.sock, selectors.EVENT_READ, conn)
            except (KeyError, OSError, ValueError):
                conn.close()

    def _run(self):
        while self._running:
            try:
                events = self._sel.select(timeout=0.05)
            except (OSError, ValueError):
                time.sleep(0.01)
                continue

            for key, mask in events:
                if key.data is None:
                    try:
                        self._wake_r.recv(64)
                    except OSError:
                        pass
                    self._drain_new_conns()
                    continue

                conn: Connection = key.data
                if mask & selectors.EVENT_READ:
                    self._do_recv(conn)
                if mask & selectors.EVENT_WRITE:
                    self._do_send(conn)

            self._drain_new_conns()

    def _do_recv(self, conn: Connection):
        try:
            data = conn.sock.recv(_RECV_BUF_SIZE)
        except (ConnectionError, OSError) as e:
            if e.errno in (errno.EAGAIN, errno.EWOULDBLOCK):
                return
            self._disconnect(conn)
            return
        if not data:
            self._disconnect(conn)
            return
        conn._recv_buf.extend(data)
        self._parse_packets(conn)

    def _parse_packets(self, conn: Connection):
        buf = conn._recv_buf
        while len(buf) >= 8:
            length, pkt_type = struct.unpack_from(">II", buf, 0)
            if length > MAX_PACKET_SIZE:
                self._disconnect(conn)
                return
            total = 8 + length
            if len(buf) < total:
                break
            body = bytes(buf[8:total])
            if self.server.packet_logger is not None:
                self.server.packet_logger(
                    'client_to_server', conn, bytes(buf[:total]))
            del buf[:total]
            if self.server.on_packet:
                try:
                    self.server.on_packet(conn, pkt_type, body)
                except Exception as exc:
                    if self.server.on_packet_error is None:
                        raise
                    self.server.on_packet_error(conn, pkt_type, exc)

    def _do_send(self, conn: Connection):
        with conn._lock:
            if conn._closed:
                return
            data = bytes(conn._write_buf)
            if not data:
                try:
                    self._sel.modify(conn.sock, selectors.EVENT_READ, conn)
                except (KeyError, OSError, ValueError):
                    pass
                return
            del conn._write_buf[:]

        try:
            sent = conn.sock.send(data)
        except (ConnectionError, OSError) as e:
            if e.errno in (errno.EAGAIN, errno.EWOULDBLOCK):
                with conn._lock:
                    conn._write_buf[:0] = data
                return
            self._disconnect(conn)
            return
        if sent < len(data):
            with conn._lock:
                conn._write_buf[:0] = data[sent:]
        else:
            try:
                self._sel.modify(conn.sock, selectors.EVENT_READ, conn)
            except (KeyError, OSError, ValueError):
                pass

    def _disconnect(self, conn: Connection):
        with conn._lock:
            if conn._closed:
                return
            conn._close_locked()
        try:
            self._sel.unregister(conn.sock)
        except (KeyError, OSError, ValueError):
            pass
        self.server.connections.pop(conn.conn_id, None)
        if self.server.on_disconnect:
            self.server.on_disconnect(conn)


class NetServer:
    def __init__(self, host='0.0.0.0', port=5123, io_threads=2):
        self.host = host
        self.port = port
        self._next_id = 1
        self._next_id_lock = threading.Lock()
        self.connections: dict[int, Connection] = {}
        self._conn_lock = threading.Lock()
        self.on_connect = None
        self.on_disconnect = None
        self.on_packet = None
        self.on_packet_error = None
        self.packet_logger = None
        self._listen_sock = None
        self._accept_thread = None
        self._running = False
        self._io_threads: list[_IOThread] = []
        self._num_io_threads = io_threads
        self._round_robin = 0

    def start(self):
        self._listen_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._listen_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._listen_sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self._listen_sock.bind((self.host, self.port))
        self._listen_sock.listen(128)
        self._listen_sock.setblocking(False)
        self._running = True

        for i in range(self._num_io_threads):
            t = _IOThread(i, self)
            t.start()
            self._io_threads.append(t)

        self._accept_thread = threading.Thread(
            target=self._accept_loop, name="accept-thread", daemon=True)
        self._accept_thread.start()
        print(f"[NetServer] Listening on {self.host}:{self.port} ({self._num_io_threads} IO threads)")

    def stop(self):
        self._running = False
        for t in self._io_threads:
            t.stop()
        if self._listen_sock:
            try:
                self._listen_sock.close()
            except OSError:
                pass
        with self._conn_lock:
            for conn in list(self.connections.values()):
                conn.close()
            self.connections.clear()

    def _accept_loop(self):
        sel = selectors.EpollSelector()
        sel.register(self._listen_sock, selectors.EVENT_READ)
        while self._running:
            try:
                events = sel.select(timeout=0.5)
            except (OSError, ValueError):
                if not self._running:
                    break
                continue
            for key, mask in events:
                if not self._running:
                    break
                try:
                    client_sock, client_addr = self._listen_sock.accept()
                except OSError:
                    continue
                client_sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                client_sock.setblocking(False)
                try:
                    client_sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, _RECV_BUF_SIZE)
                    client_sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, _RECV_BUF_SIZE)
                except OSError:
                    pass

                with self._next_id_lock:
                    conn_id = self._next_id
                    self._next_id += 1
                addr = f"{client_addr[0]}:{client_addr[1]}"
                conn = Connection(client_sock, conn_id, addr,
                                  packet_logger=self.packet_logger)

                with self._conn_lock:
                    self.connections[conn_id] = conn

                if self.on_connect:
                    self.on_connect(conn)

                io = self._io_threads[self._round_robin % self._num_io_threads]
                self._round_robin += 1
                io.add_connection(conn)

        sel.close()

    def broadcast(self, pkt_type: int, body: bytes = b''):
        data = pack_header(pkt_type, len(body)) + body
        self.broadcast_raw(data)

    def broadcast_raw(self, data: bytes):
        with self._conn_lock:
            conns = [c for c in self.connections.values() if c.registered]
        for conn in conns:
            conn.queue_send(data)

    def send_to_team(self, team_id: int, pkt_type: int, body: bytes = b''):
        data = pack_header(pkt_type, len(body)) + body
        self.send_raw_to_team(team_id, data)

    def send_raw_to_team(self, team_id: int, data: bytes):
        with self._conn_lock:
            conns = [c for c in self.connections.values()
                     if c.registered and c.team_id == team_id]
        for conn in conns:
            conn.queue_send(data)
