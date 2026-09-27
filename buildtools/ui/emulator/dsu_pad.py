#!/usr/bin/env python3
"""A GameCube controller for Dolphin, over its DSU (cemuhook) input protocol.

Dolphin's DSU client asks a UDP server which pads are connected and streams
the pad's state from it. Serving the pad from the test harness lets a test
press buttons without touching the machine's input devices, and lets it
plug the controller in only once Indigo has started: Swiss's startup stalls
in Dolphin when a controller is already connected (upstream's pad and
steering-wheel setup; a console is fine).

GCPAD_INI below maps this pad onto Dolphin's port 1.
"""

from __future__ import annotations

import socket
import struct
import threading
import time
import zlib

VERSION = 1001
PORT_INFO = 0x100001
PAD_DATA = 0x100002
DESCRIPTION = "indigo"

# Which DSU input each GameCube control reads (Dolphin names the DSU inputs
# after a DualShock 4's).
GCPAD_INI = f"""[GCPad1]
Device = DSUClient/0/{DESCRIPTION}
Buttons/A = Cross
Buttons/B = Circle
Buttons/X = Square
Buttons/Y = Triangle
Buttons/Z = R1
Buttons/Start = Options
Triggers/L = L2
Triggers/R = R2
Triggers/L-Analog = L2
Triggers/R-Analog = R2
D-Pad/Up = `Pad N`
D-Pad/Down = `Pad S`
D-Pad/Left = `Pad W`
D-Pad/Right = `Pad E`
Main Stick/Up = `Left Y+`
Main Stick/Down = `Left Y-`
Main Stick/Left = `Left X-`
Main Stick/Right = `Left X+`
C-Stick/Up = `Right Y+`
C-Stick/Down = `Right Y-`
C-Stick/Left = `Right X-`
C-Stick/Right = `Right X+`
"""

# Byte offset of each analog button in the pad-data message (after the
# 16-byte header and the 4-byte message type).
_ANALOG = {"LEFT": 44, "DOWN": 45, "RIGHT": 46, "UP": 47,
           "X": 48, "A": 49, "B": 50, "Y": 51, "Z": 52, "L": 55, "R": 54}
BUTTONS = frozenset(_ANALOG) | {"START"}


def _finish(message: bytearray, server_id: int) -> bytes:
    struct.pack_into("<4sHHII", message, 0, b"DSUS", VERSION, len(message) - 16, 0, server_id)
    struct.pack_into("<I", message, 8, zlib.crc32(message))
    return bytes(message)


def port_info(server_id: int, pad: int, connected: bool) -> bytes:
    message = bytearray(32)
    struct.pack_into("<IBBBB6sBB", message, 16, PORT_INFO, pad, 2 if connected else 0,
                     3 if connected else 0, 1 if connected else 0,
                     b"\0\0\0\0\0\1" if connected else bytes(6), 5 if connected else 0, 0)
    return _finish(message, server_id)


def pad_data(server_id: int, counter: int, held: frozenset[str]) -> bytes:
    message = bytearray(100)
    struct.pack_into("<IBBBB6sBBI", message, 16, PAD_DATA, 0, 2, 3, 1, b"\0\0\0\0\0\1", 5, 1, counter)
    message[36] = 0x08 if "START" in held else 0  # Options
    message[40:44] = bytes((128, 128, 128, 128))  # both sticks centred
    for button in held & _ANALOG.keys():
        message[_ANALOG[button]] = 255
    return _finish(message, server_id)


class Pad:
    """Serve one pad on 127.0.0.1:port until close(); unplugged until plug_in()."""

    def __init__(self, port: int = 26760) -> None:
        self._socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._socket.bind(("127.0.0.1", port))
        self.port = self._socket.getsockname()[1]
        self._socket.settimeout(0.005)
        self._id = int.from_bytes(b"indg", "little")
        self._lock = threading.Lock()
        self._held: frozenset[str] = frozenset()
        self._connected = False
        self._clients: set[tuple[str, int]] = set()
        self._counter = 0
        self._running = True
        self._thread = threading.Thread(target=self._serve, name="dsu-pad", daemon=True)
        self._thread.start()

    def plug_in(self) -> None:
        with self._lock:
            self._connected = True

    @property
    def streaming(self) -> bool:
        """True once Dolphin has asked for the plugged-in pad's data."""
        with self._lock:
            return bool(self._clients)

    def hold(self, *buttons: str) -> None:
        unknown = set(buttons) - BUTTONS
        if unknown:
            raise ValueError(f"not a GameCube button: {sorted(unknown)}")
        with self._lock:
            self._held = frozenset(buttons)

    def press(self, button: str, seconds: float = 0.1) -> None:
        self.hold(button)
        time.sleep(seconds)
        self.hold()

    def close(self) -> None:
        self._running = False
        self._thread.join(2)
        self._socket.close()

    def _serve(self) -> None:
        while self._running:
            try:
                data, client = self._socket.recvfrom(1024)
            except socket.timeout:
                data, client = b"", None
            except OSError:
                return
            with self._lock:
                connected, held, clients = self._connected, self._held, set(self._clients)
            if data[:4] == b"DSUC" and len(data) >= 20:
                kind = struct.unpack_from("<I", data, 16)[0]
                if kind == PORT_INFO:
                    self._socket.sendto(port_info(self._id, 0, connected), client)
                elif kind == PAD_DATA and connected:
                    with self._lock:
                        self._clients.add(client)
            for address in clients:
                self._counter += 1
                try:
                    self._socket.sendto(pad_data(self._id, self._counter, held), address)
                except OSError:
                    pass

