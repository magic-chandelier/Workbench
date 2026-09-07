#!/usr/bin/env python3
import fcntl
import os
import pty
import select
import struct
import tempfile
import termios
import time


def read_until(fd, data, needle, timeout=4.0):
    deadline = time.time() + timeout
    while time.time() < deadline and needle not in data:
        ready, _, _ = select.select([fd], [], [], 0.2)
        if ready:
            try:
                data.extend(os.read(fd, 65536))
            except OSError:
                break
    if needle not in data:
        raise SystemExit(f"TUI did not render expected text: {needle!r}")


with tempfile.TemporaryDirectory(prefix="wb-home-") as home:
    os.makedirs(os.path.join(home, ".config", "workbench"), exist_ok=True)
    with open(os.path.join(home, ".config", "workbench", "config"), "w", encoding="utf-8") as f:
        f.write("language=en\nconfirm_normal=0\nconfirm_sensitive=0\nconfirm_privileged=1\nlinux_profile=generic\n")

    pid, fd = pty.fork()
    if pid == 0:
        env = os.environ.copy()
        env["HOME"] = home
        env["LANG"] = "C"
        env["LC_ALL"] = "C"
        os.execve("./wb", ["./wb"], env)

    fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", 32, 110, 0, 0))
    data = bytearray()
    try:
        read_until(fd, data, b"Command Sets")
        os.write(fd, b"s")
        read_until(fd, data, b"Linux version")

        # Move to Linux version, open selector, choose CentOS.
        os.write(fd, b"j\r")
        read_until(fd, data, b"Linux Generic")
        os.write(fd, b"j\r")
        read_until(fd, data, b"Detected")
        os.write(fd, b"q")
        read_until(fd, data, b"Settings")
        os.write(fd, b"q")
        read_until(fd, data, b"Command Sets")
        # Desktop -> Command Sets -> Linux Commands, verify active CentOS profile title.
        data.clear()
        os.write(fd, b"jj\r")
        read_until(fd, data, b"Linux Commands")
        os.write(fd, b"\r")
        read_until(fd, data, b"CentOS Linux Command Set")
        os.write(fd, b"q")
        read_until(fd, data, b"Command Sets")
        os.write(fd, b"q")
        read_until(fd, data, b"Desktop")
        os.write(fd, b"q")
        end = time.time() + 3
        status = None
        while time.time() < end:
            got, st = os.waitpid(pid, os.WNOHANG)
            if got == pid:
                status = st
                break
            time.sleep(0.05)
        if status is None:
            os.kill(pid, 15)
            os.waitpid(pid, 0)
            raise SystemExit("TUI did not exit after q")
        if not os.WIFEXITED(status) or os.WEXITSTATUS(status) != 0:
            raise SystemExit(f"TUI exited abnormally: {status}")
    finally:
        try:
            os.close(fd)
        except OSError:
            pass

print("TUI SMOKE PASS")
