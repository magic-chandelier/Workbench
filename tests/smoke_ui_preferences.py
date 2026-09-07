#!/usr/bin/env python3
import fcntl, os, pty, select, struct, tempfile, termios, time


def read_until(fd, data, needle, timeout=4.0):
    end=time.time()+timeout
    while time.time()<end and needle not in data:
        r,_,_=select.select([fd],[],[],0.15)
        if r:
            try:data.extend(os.read(fd,65536))
            except OSError:break
    if needle not in data:
        raise SystemExit(f"missing {needle!r}\n{bytes(data[-3500:])!r}")


def mouse(fd,button,x,y):
    os.write(fd,f"\x1b[<{button};{x};{y}M".encode())

binary=os.path.abspath('./wb')
with tempfile.TemporaryDirectory(prefix='wb-ui-prefs-') as home:
    os.makedirs(os.path.join(home,'.config','workbench'),exist_ok=True)
    cfg=os.path.join(home,'.config','workbench','config')
    # Old config: new preferences must take safe defaults when absent.
    with open(cfg,'w') as f:
        f.write('language=en\nconfirm_normal=0\nconfirm_sensitive=0\nconfirm_privileged=1\nlinux_profile=generic\n')
    mountinfo=os.path.join(home,'mountinfo')
    with open(mountinfo,'w') as f:f.write('24 1 0:1 / / rw,relatime - overlay overlay rw\n')
    pid,fd=pty.fork()
    if pid==0:
        env=os.environ.copy();env.update(HOME=home,LANG='C',LC_ALL='C',WB_MOUNTINFO=mountinfo)
        os.execve(binary,[binary],env)
    fcntl.ioctl(fd,termios.TIOCSWINSZ,struct.pack('HHHH',34,112,0,0));data=bytearray()
    try:
        read_until(fd,data,b'Command Sets')
        os.write(fd,b's')
        read_until(fd,data,b'Virtual keyboard')
        read_until(fd,data,b'Shortcut hints')
        # Defaults: virtual keyboard off, shortcut hints on.
        settings_tail=bytes(data[data.rfind(b'Settings'):])
        if b'Virtual keyboard' not in settings_tail or b'Disabled' not in settings_tail:
            raise SystemExit('virtual keyboard default is not Disabled')
        if b'Shortcut hints' not in settings_tail or b'Enabled' not in settings_tail:
            raise SystemExit('shortcut hints default is not Enabled')
        # Mouse-toggle both rows: y=8 virtual keyboard, y=9 shortcut hints.
        data.clear();mouse(fd,0,10,8);read_until(fd,data,b'Virtual keyboard');read_until(fd,data,b'Enabled')
        data.clear();mouse(fd,0,10,9);read_until(fd,data,b'Shortcut hints');read_until(fd,data,b'Disabled')
        # Back row is y=14 after terminal settings are added.
        mouse(fd,0,10,14);read_until(fd,data,b'Desktop')
        with open(cfg) as f:txt=f.read()
        if 'virtual_keyboard=1\n' not in txt:raise SystemExit('virtual keyboard setting not persisted')
        if 'shortcut_hints=0\n' not in txt:raise SystemExit('shortcut hints setting not persisted')
        # Re-enable shortcut hints so Files must render the restored operation shortcuts.
        os.write(fd,b's');read_until(fd,data,b'Shortcut hints')
        mouse(fd,0,10,9);read_until(fd,data,b'Enabled')
        mouse(fd,0,10,14);read_until(fd,data,b'Desktop')
        # Files -> Locations -> Home; full file operation hints must be visible by default/enabled.
        os.write(fd,b'\r');read_until(fd,data,b'Locations');os.write(fd,b'\r');read_until(fd,data,b'Current path')
        read_until(fd,data,b'n New file')
        read_until(fd,data,b'c Copy')
        read_until(fd,data,b'd Delete')
        # Disable hints and verify the same Files page no longer prints the shortcut legend after re-entry.
        os.write(fd,b'q');read_until(fd,data,b'Desktop');os.write(fd,b's');read_until(fd,data,b'Shortcut hints')
        mouse(fd,0,10,9);read_until(fd,data,b'Disabled');mouse(fd,0,10,14);read_until(fd,data,b'Desktop')
        data.clear();os.write(fd,b'\r');read_until(fd,data,b'Locations');os.write(fd,b'\r');read_until(fd,data,b'Current path')
        tail=bytes(data[data.rfind(b'Current path'):])
        if b'n New file' in tail or b'c Copy' in tail or b'd Delete' in tail:
            raise SystemExit('Files shortcut legend remained visible after disabling hints')
        os.write(fd,b'q');read_until(fd,data,b'Desktop');os.write(fd,b'q')
        end=time.time()+3
        while time.time()<end:
            got,st=os.waitpid(pid,os.WNOHANG)
            if got==pid:
                if not os.WIFEXITED(st) or os.WEXITSTATUS(st)!=0:raise SystemExit(f'abnormal {st}')
                break
            time.sleep(.05)
        else:
            os.kill(pid,15);os.waitpid(pid,0);raise SystemExit('preferences flow did not exit')
    finally:
        try:os.close(fd)
        except OSError:pass
print('UI PREFERENCES PASS')
