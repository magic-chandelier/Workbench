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
        tail = bytes(data[-4000:])
        raise SystemExit(f"TUI did not render expected text: {needle!r}\n{tail!r}")


binary = os.path.abspath("./wb")
with tempfile.TemporaryDirectory(prefix="wb-fm-home-") as home, tempfile.TemporaryDirectory(prefix="wb-fm-cwd-") as cwd:
    os.makedirs(os.path.join(home, ".config", "workbench"), exist_ok=True)
    with open(os.path.join(home, ".config", "workbench", "config"), "w", encoding="utf-8") as f:
        f.write("language=en\nconfirm_normal=0\nconfirm_sensitive=0\nconfirm_privileged=1\nlinux_profile=generic\n")
    with open(os.path.join(cwd, "visible.txt"), "w", encoding="utf-8") as f:
        f.write("visible\n")
    with open(os.path.join(cwd, ".hidden.txt"), "w", encoding="utf-8") as f:
        f.write("hidden\n")
    os.mkdir(os.path.join(cwd, "folder"))
    with open(os.path.join(cwd, "evil\x1b[2J.txt"), "w", encoding="utf-8") as f:
        f.write("safe display\n")
    system_tmp = os.path.join(home, "system-tmp")
    os.mkdir(system_tmp)
    mountinfo = os.path.join(home, "mountinfo")
    with open(mountinfo, "w", encoding="utf-8") as f:
        f.write("24 1 0:1 / / rw,relatime - overlay overlay rw\n")
        f.write(f"25 24 8:17 / {cwd} rw,relatime - xfs /dev/test-disk rw\n")
        f.write(f"26 24 0:43 / {system_tmp} rw,nosuid,nodev - tmpfs tmpfs-ui rw\n")

    pid, fd = pty.fork()
    if pid == 0:
        env = os.environ.copy()
        env["HOME"] = home
        env["LANG"] = "C"
        env["LC_ALL"] = "C"
        env["WB_MOUNTINFO"] = mountinfo
        os.chdir(cwd)
        os.execve(binary, [binary], env)

    fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", 34, 112, 0, 0))
    data = bytearray()
    try:
        read_until(fd, data, b"Workbench Files")
        read_until(fd, data, b"Command Sets")
        if data.find(b"Workbench Files") > data.find(b"Command Sets"):
            raise SystemExit("Workbench Files is not above Command Sets")

        os.write(fd, b"\r")
        read_until(fd, data, b"Locations")
        read_until(fd, data, b"Home")
        read_until(fd, data, b"System")
        read_until(fd, data, b"/dev/test-disk")
        if b"tmpfs-ui" in data[data.rfind(b"Locations"):]:
            raise SystemExit("system tmpfs mount visible before Locations toggle")
        data.clear()
        os.write(fd, b"h")
        read_until(fd, data, b"tmpfs-ui")
        data.clear()
        os.write(fd, b"h")
        read_until(fd, data, b"System mounts: hidden")
        # Home, System root, local test mount. Open the test mount.
        os.write(fd, b"jj\r")
        read_until(fd, data, b"Current path")
        read_until(fd, data, cwd.encode())
        read_until(fd, data, b"visible.txt")
        read_until(fd, data, b"folder")
        if b".hidden.txt" in data[data.rfind(b"Current path"):]:
            raise SystemExit("hidden file visible before toggle")
        read_until(fd, data, b"evil?[2J.txt")

        os.write(fd, b"h")
        read_until(fd, data, b".hidden.txt")

        # Parent, folder, hidden file, sanitized evil file, visible file.
        os.write(fd, b"jjjjp")
        read_until(fd, data, b"Properties")
        read_until(fd, data, b"visible.txt")
        os.write(fd, b"q")
        read_until(fd, data, b"Current path")

        data.clear()
        os.write(fd, b"v")
        read_until(fd, data, b"View file")
        read_until(fd, data, b"     1  visible")
        os.write(fd, b"q")
        read_until(fd, data, b"Current path")

        # Create a file through the TUI.
        data.clear()
        os.write(fd, b"n")
        read_until(fd, data, b"Enter name:")
        data.clear()
        os.write(fd, b"created-ui.txt\n")
        read_until(fd, data, b"Hidden files: shown")
        read_until(fd, data, b"created-ui.txt")
        if not os.path.isfile(os.path.join(cwd, "created-ui.txt")):
            raise SystemExit("new-file action did not create file")

        # Create a directory through the TUI.
        data.clear()
        os.write(fd, b"N")
        read_until(fd, data, b"Enter name:")
        data.clear()
        os.write(fd, b"created-dir\n")
        read_until(fd, data, b"Hidden files: shown")
        read_until(fd, data, b"created-dir")
        if not os.path.isdir(os.path.join(cwd, "created-dir")):
            raise SystemExit("new-directory action did not create directory")

        # Parent, created-dir, folder, hidden, created-ui. Change mode to 0600.
        data.clear()
        os.write(fd, b"jjjjx")
        read_until(fd, data, b"octal digits")
        data.clear()
        os.write(fd, b"600\n")
        read_until(fd, data, b"Current path")
        if (os.stat(os.path.join(cwd, "created-ui.txt")).st_mode & 0o7777) != 0o600:
            raise SystemExit("chmod action did not apply requested mode")

        # Delete the file and verify the explicit confirmation path.
        data.clear()
        os.write(fd, b"d")
        read_until(fd, data, b"Confirm action")
        data.clear()
        os.write(fd, b"y\n")
        read_until(fd, data, b"Current path")
        if os.path.exists(os.path.join(cwd, "created-ui.txt")):
            raise SystemExit("delete action did not remove file")

        # Delete the empty directory recursively (same confirmation path).
        data.clear()
        os.write(fd, b"jd")
        read_until(fd, data, b"Recursively delete directory")
        data.clear()
        os.write(fd, b"y\n")
        read_until(fd, data, b"Current path")
        if os.path.exists(os.path.join(cwd, "created-dir")):
            raise SystemExit("recursive delete action did not remove directory")

        # Parent, folder, hidden, evil, visible. Copy visible using the default destination.
        data.clear()
        os.write(fd, b"jjjjc")
        read_until(fd, data, b"Destination path")
        data.clear()
        os.write(fd, b"\n")
        read_until(fd, data, b"Current path")
        read_until(fd, data, b"visible.txt.copy")
        copied = os.path.join(cwd, "visible.txt.copy")
        if not os.path.isfile(copied):
            raise SystemExit("copy action did not create destination")
        with open(copied, "r", encoding="utf-8") as f:
            if f.read() != "visible\n":
                raise SystemExit("copy action changed file content")

        # The copy is the next row after visible; rename it.
        data.clear()
        os.write(fd, b"jr")
        read_until(fd, data, b"Enter new name:")
        data.clear()
        os.write(fd, b"renamed-copy.txt\n")
        read_until(fd, data, b"Current path")
        read_until(fd, data, b"renamed-copy.txt")
        renamed = os.path.join(cwd, "renamed-copy.txt")
        if not os.path.isfile(renamed) or os.path.exists(copied):
            raise SystemExit("rename action did not move copied file")

        # Parent, folder, hidden, evil, renamed-copy. Remove the renamed copy.
        data.clear()
        os.write(fd, b"jjjjd")
        read_until(fd, data, b"Confirm action")
        data.clear()
        os.write(fd, b"y\n")
        read_until(fd, data, b"Current path")
        if os.path.exists(renamed):
            raise SystemExit("delete action did not remove renamed copy")

        os.write(fd, b"q")
        read_until(fd, data, b"Desktop")
        os.write(fd, b"q")

        deadline = time.time() + 3
        status = None
        while time.time() < deadline:
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


def mouse(fd, button, x, y, release=False):
    suffix = "m" if release else "M"
    os.write(fd, f"\x1b[<{button};{x};{y}{suffix}".encode("ascii"))


# Dedicated SGR mouse path: Locations click, browser double-click, nav buttons, wheel and context menu.
with tempfile.TemporaryDirectory(prefix="wb-fm-mouse-home-") as home, tempfile.TemporaryDirectory(prefix="wb-fm-mouse-cwd-") as cwd:
    os.makedirs(os.path.join(home, ".config", "workbench"), exist_ok=True)
    with open(os.path.join(home, ".config", "workbench", "config"), "w", encoding="utf-8") as f:
        f.write("language=en\nconfirm_normal=0\nconfirm_sensitive=0\nconfirm_privileged=1\nlinux_profile=generic\n")
    os.mkdir(os.path.join(cwd, "folder"))
    with open(os.path.join(cwd, "click.txt"), "w", encoding="utf-8") as f:
        f.write("mouse-open\n")
    mountinfo = os.path.join(home, "mountinfo")
    with open(mountinfo, "w", encoding="utf-8") as f:
        f.write("24 1 0:1 / / rw,relatime - overlay overlay rw\n")
        f.write(f"25 24 8:17 / {cwd} rw,relatime - xfs /dev/mouse-disk rw\n")

    pid, fd = pty.fork()
    if pid == 0:
        env = os.environ.copy()
        env["HOME"] = home
        env["LANG"] = "C"
        env["LC_ALL"] = "C"
        env["WB_MOUNTINFO"] = mountinfo
        os.chdir(cwd)
        os.execve(binary, [binary], env)

    fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", 34, 112, 0, 0))
    data = bytearray()
    try:
        read_until(fd, data, b"Workbench Files")
        os.write(fd, b"\r")
        read_until(fd, data, b"Locations")
        read_until(fd, data, b"/dev/mouse-disk")

        # Locations rows start at terminal row 7: Home, root, local test mount.
        data.clear()
        mouse(fd, 0, 10, 9)
        read_until(fd, data, b"Current path")
        read_until(fd, data, cwd.encode())

        # Browser rows start at row 8: parent, folder, click.txt. First click selects; second opens.
        data.clear()
        mouse(fd, 0, 10, 10)
        time.sleep(0.08)
        mouse(fd, 0, 10, 10)
        read_until(fd, data, b"View file")
        read_until(fd, data, b"mouse-open")
        os.write(fd, b"q")
        read_until(fd, data, b"Current path")

        # Home and Locations are clickable navigation controls on browser row 4.
        data.clear()
        mouse(fd, 0, 15, 4)
        read_until(fd, data, home.encode())
        data.clear()
        mouse(fd, 0, 5, 4)
        read_until(fd, data, b"Locations")

        # Re-enter local location; wheel moves selection, right click opens context menu.
        data.clear()
        mouse(fd, 0, 10, 9)
        read_until(fd, data, cwd.encode())
        data.clear()
        mouse(fd, 65, 10, 8)  # wheel down maps to KEY_DOWN
        read_until(fd, data, b"Selected: 2/3")
        data.clear()
        mouse(fd, 2, 10, 10)
        read_until(fd, data, b"File actions")
        read_until(fd, data, b"Properties")
        # Context menu row 7 is Properties (row 6 is Open/View).
        data.clear()
        mouse(fd, 0, 10, 7)
        read_until(fd, data, b"Properties")
        read_until(fd, data, b"click.txt")
        os.write(fd, b"q")
        read_until(fd, data, b"Current path")

        os.write(fd, b"q")
        read_until(fd, data, b"Desktop")
        os.write(fd, b"q")
        deadline = time.time() + 3
        status = None
        while time.time() < deadline:
            got, st = os.waitpid(pid, os.WNOHANG)
            if got == pid:
                status = st
                break
            time.sleep(0.05)
        if status is None:
            os.kill(pid, 15)
            os.waitpid(pid, 0)
            raise SystemExit("mouse TUI did not exit after q")
        if not os.WIFEXITED(status) or os.WEXITSTATUS(status) != 0:
            raise SystemExit(f"mouse TUI exited abnormally: {status}")
    finally:
        try:
            os.close(fd)
        except OSError:
            pass

print("FILES TUI SMOKE PASS")
