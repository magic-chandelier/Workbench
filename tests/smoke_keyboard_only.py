#!/usr/bin/env python3
import fcntl, os, pty, select, struct, tempfile, termios, time

def read_until(fd,data,needle,timeout=5.0):
    end=time.time()+timeout
    while time.time()<end and needle not in data:
        r,_,_=select.select([fd],[],[],0.15)
        if r:
            try:data.extend(os.read(fd,65536))
            except OSError:break
    if needle not in data:raise SystemExit(f"missing {needle!r}\n{bytes(data[-4500:])!r}")

binary=os.path.abspath('./wb')
with tempfile.TemporaryDirectory(prefix='wb-keyboard-only-') as home:
    os.makedirs(os.path.join(home,'.config','workbench'),exist_ok=True)
    with open(os.path.join(home,'.config','workbench','config'),'w') as f:f.write('language=en\nconfirm_normal=0\nconfirm_sensitive=0\nconfirm_privileged=1\nlinux_profile=generic\n')
    mountinfo=os.path.join(home,'mountinfo');open(mountinfo,'w').write('24 1 0:1 / / rw,relatime - overlay overlay rw\n')
    pid,fd=pty.fork()
    if pid==0:
        env=os.environ.copy();env.update(HOME=home,LANG='C',LC_ALL='C',WB_MOUNTINFO=mountinfo);os.execve(binary,[binary],env)
    fcntl.ioctl(fd,termios.TIOCSWINSZ,struct.pack('HHHH',34,112,0,0));data=bytearray()
    try:
        read_until(fd,data,b'Command Sets')
        # Settings is fully keyboard-accessible.
        os.write(fd,b's');read_until(fd,data,b'Linux version');os.write(fd,b'q');read_until(fd,data,b'Desktop')
        # Command Sets -> Linux Commands -> System -> search -> properties -> back.
        os.write(fd,b'jj\r');read_until(fd,data,b'Linux Commands');os.write(fd,b'\r');read_until(fd,data,b'Only categories');os.write(fd,b'\r');read_until(fd,data,b'[Search]')
        data.clear();os.write(fd,b'/');read_until(fd,data,b'Enter keywords');os.write(fd,b'date\n');read_until(fd,data,b'Search: date')
        os.write(fd,b'p');read_until(fd,data,b'Properties');os.write(fd,b'q');read_until(fd,data,b'Search: date');os.write(fd,b'q');read_until(fd,data,b'Only categories');os.write(fd,b'q');read_until(fd,data,b'Command Sets');os.write(fd,b'q');read_until(fd,data,b'Desktop')
        # Desktop selection remains Command Sets row; move to Files, enter Locations/Home.
        os.write(fd,b'kk\r');read_until(fd,data,b'Locations');os.write(fd,b'\r');read_until(fd,data,b'Current path')
        # Create, chmod, rename and delete with keyboard only.
        data.clear();os.write(fd,b'n');read_until(fd,data,b'Enter name:');os.write(fd,b'a\n');read_until(fd,data,b'Current path');read_until(fd,data,b'a')
        target=os.path.join(home,'a')
        if not os.path.isfile(target):raise SystemExit('keyboard create failed')
        os.write(fd,b'jx');read_until(fd,data,b'octal digits');data.clear();os.write(fd,b'600\n');read_until(fd,data,b'0600')
        if os.stat(target).st_mode & 0o7777 != 0o600:raise SystemExit('keyboard chmod failed')
        data.clear();os.write(fd,b'r');read_until(fd,data,b'Enter new name:');data.clear();os.write(fd,b'b\n');read_until(fd,data,b' b ');renamed=os.path.join(home,'b')
        if not os.path.isfile(renamed):raise SystemExit('keyboard rename failed')
        data.clear();os.write(fd,b'jd');read_until(fd,data,b'Confirm action');data.clear();os.write(fd,b'y\n');read_until(fd,data,b'Items:')
        if os.path.exists(renamed):raise SystemExit('keyboard delete failed')
        os.write(fd,b'q');read_until(fd,data,b'Desktop');os.write(fd,b'q')
        end=time.time()+3;status=None
        while time.time()<end:
            got,st=os.waitpid(pid,os.WNOHANG)
            if got==pid:status=st;break
            time.sleep(.05)
        if status is None:os.kill(pid,15);os.waitpid(pid,0);raise SystemExit('keyboard-only flow did not exit')
        if not os.WIFEXITED(status) or os.WEXITSTATUS(status)!=0:raise SystemExit(f'abnormal {status}')
    finally:
        try:os.close(fd)
        except OSError:pass
print('KEYBOARD ONLY PASS')
