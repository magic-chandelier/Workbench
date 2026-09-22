#!/usr/bin/env python3
import fcntl, os, pty, select, struct, tempfile, termios, time


def read_until(fd, data, needle, timeout=6):
    end = time.time() + timeout
    while time.time() < end and needle not in data:
        r, _, _ = select.select([fd], [], [], 0.12)
        if r:
            try:
                data.extend(os.read(fd, 65536))
            except OSError:
                break
    if needle not in data:
        raise SystemExit(f'missing {needle!r}\n{bytes(data[-7000:])!r}')


def make_plugin(home):
    root = os.path.join(home, '.local', 'share', 'workbench', 'plugins', 'tty-demo')
    os.makedirs(os.path.join(root, 'bin'), exist_ok=True)
    with open(os.path.join(root, 'plugin.wbp'), 'w') as f:
        f.write('''WORKBENCH_PLUGIN=1
format=1
id=tty-demo
version=1.0.0
api_min=1
api_max=1
name_zh=TTY 交互测试
name_en=TTY Interactive Test
description_zh=单键 raw TTY 回归
description_en=single-key raw TTY regression
entry=bin/run
''')
    run = os.path.join(root, 'bin', 'run')
    with open(run, 'w') as f:
        f.write(r'''#!/usr/bin/env python3
import os, sys, termios
fd=0
old=termios.tcgetattr(fd)
raw=termios.tcgetattr(fd)
raw[3] &= ~(termios.ICANON | termios.ECHO)
raw[6][termios.VMIN]=1
raw[6][termios.VTIME]=0
termios.tcsetattr(fd, termios.TCSAFLUSH, raw)
try:
    sys.stdout.write('PLUGIN-CBREAK-READY\n')
    sys.stdout.flush()
    c=os.read(fd,1)
finally:
    termios.tcsetattr(fd, termios.TCSAFLUSH, old)
if c != b'q':
    raise SystemExit(9)
''')
    os.chmod(run, 0o700)


with tempfile.TemporaryDirectory(prefix='wb-plugin-tty-') as home:
    os.makedirs(os.path.join(home, '.config', 'workbench'), exist_ok=True)
    with open(os.path.join(home, '.config', 'workbench', 'config'), 'w') as f:
        f.write('language=en\nlinux_profile=generic\n')
    make_plugin(home)
    pid, fd = pty.fork()
    if pid == 0:
        env = os.environ.copy()
        env.update(HOME=home, LANG='C', LC_ALL='C', SHELL='/bin/sh')
        os.execve(os.path.abspath('./wb'), [os.path.abspath('./wb')], env)
    fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack('HHHH', 38, 118, 0, 0))
    data = bytearray()
    try:
        read_until(fd, data, b'Applications')
        os.write(fd, b'jj\r')
        data.clear(); read_until(fd, data, b'TTY Interactive Test')
        os.write(fd, b'\r')
        data.clear(); read_until(fd, data, b'PLUGIN-CBREAK-READY')
        data.clear(); os.write(fd, b'q')  # deliberately no Enter
        read_until(fd, data, b'Applications', timeout=3)
        if b'Plugin finished (exit code 0)' in data:
            raise SystemExit('successful plugin exit still showed confirmation page')
        # Applications should be live again immediately.
        os.write(fd, b'q')
        data.clear(); read_until(fd, data, b'Desktop')
        os.write(fd, b'q')
        end=time.time()+4
        while time.time()<end:
            got, st = os.waitpid(pid, os.WNOHANG)
            if got == pid:
                if not os.WIFEXITED(st) or os.WEXITSTATUS(st) != 0:
                    raise SystemExit(f'abnormal Workbench exit: {st}')
                break
            time.sleep(0.05)
        else:
            raise SystemExit('Workbench did not exit after tty handoff regression')
    finally:
        try: os.close(fd)
        except OSError: pass
print('PLUGIN TTY HANDOFF PASS')
