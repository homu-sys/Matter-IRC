#!/usr/bin/env python3
"""Custom IRC backend — reads JSON commands from stdin, writes JSON events to stdout.

Protocol (C→S):
  WLC <token> <nick>
  JOIN #channel
  PART #channel
  MSG #channel :<text>
  HIST #channel <count>
  WHO #channel
  PONG

Protocol (S→C):
  OK <nick>
  ERR <code> :<msg>
  JOINED #channel <nick>
  PARTED #channel <nick>
  MSG #channel <nick> :<text>
  HISTBEGIN #channel
  MSG #channel <nick> :<text> <ts>   (inside HIST batch)
  HISTEND #channel
  PRESENCE #channel <nick1>,<nick2>,...
  PING
"""

import json
import socket
import ssl
import sys
import threading
import time


def send_event(fd, obj):
    fd.write(json.dumps(obj) + "\n")
    fd.flush()


class IRCBackend:
    def __init__(self):
        self.sock = None
        self.connected = False
        self.in_channel = False
        self.channel = ""
        self.nickname = ""
        self.password = ""
        self.lock = threading.Lock()
        self.reader_thread = None

    def connect(self, server, port, channel, nick, realname, username, password=""):
        if self.connected:
            return
        self.channel = channel
        self.nickname = nick
        self.password = password

        send_event(sys.stdout, {"event": "connecting", "server": server, "port": port})

        try:
            raw = socket.create_connection((server, int(port)), timeout=10)
            self.sock = raw
            self.connected = True
        except Exception as e:
            send_event(sys.stdout, {"event": "error", "text": f"Connection failed: {e}"})
            return

        if str(port) == "8443" or str(port) == "443":
            try:
                ctx = ssl.create_default_context()
                ctx.check_hostname = False
                ctx.verify_mode = ssl.CERT_NONE
                tls = ctx.wrap_socket(raw, server_hostname=server)
                self.sock = tls
                send_event(sys.stdout, {"event": "system", "text": "TLS connected"})
            except ssl.SSLError:
                send_event(sys.stdout, {"event": "system", "text": "Connecting without TLS"})
            except Exception:
                pass

        self.reader_thread = threading.Thread(target=self._read_loop, daemon=True)
        self.reader_thread.start()

        if password:
            self._send(f"REG {nick} {password}")
        else:
            self._send(f"LOG {nick} {password}")

    def disconnect(self, notify=True):
        if self.sock:
            try:
                self._send(f"PART {self.channel}")
            except Exception:
                pass
            try:
                self.sock.close()
            except Exception:
                pass
        self.sock = None
        was = self.connected
        self.connected = False
        self.in_channel = False
        if was and notify:
            send_event(sys.stdout, {"event": "disconnected"})

    def send_msg(self, target, text):
        self._send(f"MSG {target} :{text}")

    def join_channel(self, channel):
        self._send(f"JOIN {channel}")

    def part_channel(self, channel):
        self._send(f"PART {channel}")

    def send_raw(self, line):
        self._send(line)

    def _send(self, data):
        with self.lock:
            if self.sock:
                try:
                    self.sock.sendall((data + "\n").encode("utf-8", errors="replace"))
                except Exception:
                    pass

    def _read_loop(self):
        buf = b""
        try:
            while self.connected:
                try:
                    data = self.sock.recv(4096)
                except TimeoutError:
                    continue
                if not data:
                    break
                buf += data
                while b"\n" in buf:
                    line, buf = buf.split(b"\n", 1)
                    line = line.rstrip(b"\r").decode("utf-8", errors="replace")
                    if line:
                        self._handle_line(line)
        except Exception:
            pass
        finally:
            was_connected = self.connected
            self.connected = False
            self.in_channel = False
            if was_connected:
                send_event(sys.stdout, {"event": "disconnected"})

    def _handle_line(self, line):
        parts = line.split(None, 2)
        if not parts:
            return

        cmd = parts[0]

        if cmd == "PING":
            self._send("PONG")
            return

        if cmd == "OK":
            if len(parts) >= 2:
                self.nickname = parts[1]
            send_event(sys.stdout, {"event": "connected"})
            if self.channel:
                self._send(f"JOIN {self.channel}")
            return

        if cmd == "ERR":
            code = parts[1] if len(parts) >= 2 else "?"
            msg = parts[2].lstrip(":") if len(parts) >= 3 else "unknown error"

            if "already registered" in msg and self.password:
                self._send(f"LOG {self.nickname} {self.password}")
                return

            send_event(sys.stdout, {"event": "error", "text": f"{code}: {msg}"})
            return

        if cmd == "JOINED":
            if len(parts) >= 3:
                ch = parts[1]
                nick = parts[2]
                if nick == self.nickname:
                    self.in_channel = True
                    send_event(sys.stdout, {"event": "joined", "channel": ch})
                else:
                    send_event(sys.stdout, {"event": "join", "nick": nick, "channel": ch})
            return

        if cmd == "PARTED":
            if len(parts) >= 3:
                ch = parts[1]
                nick = parts[2]
                if nick == self.nickname:
                    self.in_channel = False
                send_event(sys.stdout, {
                    "event": "part",
                    "nick": nick,
                    "channel": ch,
                    "reason": "",
                })
            return

        if cmd == "MSG":
            if len(parts) >= 3:
                ch = parts[1]
                nick = parts[2]
                text = ""
                if len(parts) >= 4:
                    text = parts[3].lstrip(":")
                elif ":" in nick:
                    sp = parts[2].split(" :", 1)
                    nick = sp[0]
                    text = sp[1] if len(sp) > 1 else ""
                send_event(sys.stdout, {
                    "event": "message",
                    "sender": nick,
                    "text": text,
                    "channel": ch,
                })
            return

        if cmd == "HISTBEGIN":
            return

        if cmd == "HISTEND":
            return

        if cmd == "PRESENCE":
            return

        send_event(sys.stdout, {"event": "system", "text": line})


def main():
    backend = IRCBackend()

    for raw in sys.stdin:
        raw = raw.strip()
        if not raw:
            continue
        try:
            msg = json.loads(raw)
        except json.JSONDecodeError:
            continue

        cmd = msg.get("cmd", "")

        if cmd == "connect":
            backend.connect(
                msg.get("server", ""),
                msg.get("port", "8443"),
                msg.get("channel", ""),
                msg.get("nick", "user"),
                msg.get("realname", "User"),
                msg.get("username", "user"),
                msg.get("password", ""),
            )
        elif cmd == "disconnect":
            backend.disconnect(notify=False)
        elif cmd == "msg":
            backend.send_msg(msg.get("target", ""), msg.get("text", ""))
        elif cmd == "join":
            backend.join_channel(msg.get("channel", ""))
        elif cmd == "part":
            backend.part_channel(msg.get("channel", ""))
        elif cmd == "raw":
            backend.send_raw(msg.get("line", ""))
        elif cmd == "quit":
            backend.disconnect(notify=False)
            break

    if backend.connected:
        backend.disconnect(notify=False)


if __name__ == "__main__":
    main()
