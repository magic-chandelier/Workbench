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
        raise SystemExit(f"missing {needle!r}\n{bytes(data[-3000:])!r}")

def mouse(fd,button,x,y,release=False):
    os.write(fd,f"\x1b[<{button};{x};{y}{'m' if release else 'M'}".encode())

binary=os.path.abspath('./wb')
with tempfile.TemporaryDirectory(prefix='wb-mouse-settings-') as home:
    os.makedirs(os.path.join(home,'.config','workbench'),exist_ok=True)
    cfg=os.path.join(home,'.config','workbench','config')
    with open(cfg,'w') as f:f.write('language=en\nconfirm_normal=0\nconfirm_sensitive=0\nconfirm_privileged=1\nlinux_profile=generic\n')
    pid,fd=pty.fork()
    if pid==0:
        env=os.environ.copy();env.update(HOME=home,LANG='C',LC_ALL='C')
        os.execve(binary,[binary],env)
    fcntl.ioctl(fd,termios.TIOCSWINSZ,struct.pack('HHHH',32,110,0,0));data=bytearray()
    try:
        read_until(fd,data,b'Command Sets')
        # Mouse-only: desktop Settings row.
        mouse(fd,0,10,9); read_until(fd,data,b'Linux version')
        # Open Linux version row, select CentOS, then Back.
        mouse(fd,0,10,6); read_until(fd,data,b'Detected')
        mouse(fd,0,10,6); read_until(fd,data,b'Active profile: CentOS')
        mouse(fd,0,10,7); read_until(fd,data,b'Settings')
        # Back to desktop by clicking Back row.
        mouse(fd,0,10,14); read_until(fd,data,b'Command Sets')
        # Exit by mouse only.
        mouse(fd,0,10,10)
        deadline=time.time()+3;status=None
        while time.time()<deadline:
            got,st=os.waitpid(pid,os.WNOHANG)
            if got==pid:status=st;break
            time.sleep(.05)
        if status is None:
            os.kill(pid,15);os.waitpid(pid,0);raise SystemExit('mouse-only settings flow did not exit')
        if not os.WIFEXITED(status) or os.WEXITSTATUS(status)!=0:raise SystemExit(f'abnormal {status}')
        with open(cfg) as f:cfg_text=f.read()
        if 'linux_profile=centos' not in cfg_text:raise SystemExit('mouse profile choice not persisted')
    finally:
        try:os.close(fd)
        except OSError:pass
print('MOUSE ONLY SETTINGS PASS')
