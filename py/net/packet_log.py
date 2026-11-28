"""Asynchronous JSONL capture of logical Rusted Warfare packets."""

from __future__ import annotations

import base64
from datetime import datetime, timezone
import json
import logging
from logging.handlers import RotatingFileHandler
from pathlib import Path
import queue
import struct
import threading
import time

from resource_paths import project_path
from .packet import PacketType


class PacketCapture:
    def __init__(self, path: str | Path, *, max_bytes: int = 64 * 1024 * 1024,
                 backup_count: int = 3):
        self.path = project_path(path)
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self._queue = queue.SimpleQueue()
        self._handler = RotatingFileHandler(
            self.path, maxBytes=max_bytes, backupCount=backup_count,
            encoding='utf-8',
        )
        self._handler.setFormatter(logging.Formatter('%(message)s'))
        self._thread = threading.Thread(
            target=self._write_loop, name='packet-log-writer', daemon=True)
        self._thread.start()

    def packet(self, direction, conn, wire_bytes):
        self._queue.put((
            'packet', time.time_ns(), str(direction), conn.conn_id,
            conn.addr, bytes(wire_bytes),
        ))

    def event(self, event_name, **fields):
        self._queue.put(('event', time.time_ns(), str(event_name), fields))

    def close(self):
        self._queue.put(None)
        self._thread.join()
        self._handler.close()

    def _write_loop(self):
        while True:
            entry = self._queue.get()
            if entry is None:
                return
            if entry[0] == 'event':
                _, timestamp_ns, name, fields = entry
                self._write({
                    'time': _utc_time(timestamp_ns),
                    'event': name,
                    **fields,
                })
                continue

            _, timestamp_ns, direction, conn_id, peer, data = entry
            self._write_packets(timestamp_ns, direction, conn_id, peer, data)

    def _write_packets(self, timestamp_ns, direction, conn_id, peer, data):
        offset = 0
        while offset + 8 <= len(data):
            body_length, packet_type = struct.unpack_from('>II', data, offset)
            packet_length = 8 + body_length
            if packet_length > len(data) - offset:
                break
            wire = data[offset:offset + packet_length]
            try:
                packet_name = PacketType(packet_type).name
            except ValueError:
                packet_name = 'UNKNOWN'
            self._write({
                'time': _utc_time(timestamp_ns),
                'event': 'packet',
                'direction': direction,
                'connection_id': conn_id,
                'peer': peer,
                'packet_type': packet_type,
                'packet_name': packet_name,
                'packet_length': packet_length,
                'wire_base64': base64.b64encode(wire).decode('ascii'),
            })
            offset += packet_length

        if offset < len(data):
            self._write({
                'time': _utc_time(timestamp_ns),
                'event': 'packet_data_unparsed',
                'direction': direction,
                'connection_id': conn_id,
                'peer': peer,
                'data_length': len(data) - offset,
                'data_base64': base64.b64encode(data[offset:]).decode('ascii'),
            })

    def _write(self, row):
        record = logging.LogRecord(
            'packet_capture', logging.INFO, '', 0,
            json.dumps(row, separators=(',', ':'), ensure_ascii=False), (), None,
        )
        self._handler.emit(record)
        self._handler.flush()


def _utc_time(timestamp_ns):
    value = datetime.fromtimestamp(timestamp_ns / 1_000_000_000, timezone.utc)
    return value.isoformat(timespec='milliseconds')
