#!/usr/bin/env python3
import fcntl, os, pty, select, struct, tempfile, termios, time

def read_until(fd,data,needle,timeout=4.0):
    end=time.time()+timeout
    while time.time()<end and needle not in data:
        r,_,_=select.select([fd],[],[],0.15)
        if r:
            try:data.extend(os.read(fd,65536))
            except OSError:break
    if needle not in data:
        raise SystemExit(f"missing {needle!r}\n{bytes(data[-4000:])!r}")

def mouse(fd,button,x,y):
    os.write(fd,f"\x1b[<{button};{x};{y}M".encode())

binary=os.path.abspath('./wb')
with tempfile.TemporaryDirectory(prefix='wb-sticky-row-') as home:
    os.makedirs(os.path.join(home,'.config','workbench'),exist_ok=True)
    cfg=os.path.join(home,'.config','workbench','config')
    with open(cfg,'w') as f:
        f.write('language=en\nconfirm_normal=0\nconfirm_sensitive=0\nconfirm_privileged=1\nlinux_profile=generic\n')
    mountinfo=os.path.join(home,'mountinfo')
    with open(mountinfo,'w') as f:f.write('24 1 0:1 / / rw,relatime - overlay overlay rw\n')
    pid,fd=pty.fork()
    if pid==0:
        env=os.environ.copy();env.update(HOME=home,LANG='C',LC_ALL='C',WB_MOUNTINFO=mountinfo)
        os.execve(binary,[binary],env)
    fcntl.ioctl(fd,termios.TIOCSWINSZ,struct.pack('HHHH',32,110,0,0));data=bytearray()
    try:
        read_until(fd,data,b'Command Sets')
        os.write(fd,b's');read_until(fd,data,b'Sticky reserved row')
        # Sticky row is setting index 9 -> logical/physical row 14 while disabled.
        data.clear();mouse(fd,0,10,14);read_until(fd,data,b'Sticky reserved row');read_until(fd,data,b'Enabled')
        if b'\x1b[2J\x1b[2;1H' not in data:
            raise SystemExit('sticky row did not move redraw origin to row 2')
        with open(cfg) as f:txt=f.read()
        if 'sticky_reserved_row=1\n' not in txt:raise SystemExit('sticky setting not persisted')
        # Back is logical row 15, now physical row 16; normalized mouse coordinates must still hit it.
        data.clear();mouse(fd,0,10,16);read_until(fd,data,b'Desktop')
        # Files first desktop row is logical 6, physical 7 with sticky enabled.
        data.clear();mouse(fd,0,10,7);read_until(fd,data,b'Locations')
        os.write(fd,b'q');read_until(fd,data,b'Desktop');os.write(fd,b'q')
        end=time.time()+3
        while time.time()<end:
            got,st=os.waitpid(pid,os.WNOHANG)
            if got==pid:
                if not os.WIFEXITED(st) or os.WEXITSTATUS(st)!=0:raise SystemExit(f'abnormal {st}')
                break
            time.sleep(.05)
        else:
            os.kill(pid,15);os.waitpid(pid,0);raise SystemExit('sticky row flow did not exit')
    finally:
        try:os.close(fd)
        except OSError:pass
print('STICKY RESERVED ROW PASS')
