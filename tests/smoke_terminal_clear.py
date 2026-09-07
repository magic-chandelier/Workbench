#!/usr/bin/env python3
import fcntl, os, pty, select, struct, tempfile, termios, time

def read_until(fd,data,needle,timeout=5):
    end=time.time()+timeout
    while time.time()<end and needle not in data:
        r,_,_=select.select([fd],[],[],0.15)
        if r:
            try:data.extend(os.read(fd,65536))
            except OSError:break
    if needle not in data:raise SystemExit(f'missing {needle!r}\n{bytes(data[-5000:])!r}')
def drain(fd,data,seconds=.5):
    end=time.time()+seconds
    while time.time()<end:
        r,_,_=select.select([fd],[],[],0.08)
        if r:
            try:data.extend(os.read(fd,65536))
            except OSError:break
with tempfile.TemporaryDirectory(prefix='wb-term-clear-') as home:
    os.makedirs(os.path.join(home,'.config','workbench'),exist_ok=True)
    with open(os.path.join(home,'.config','workbench','config'),'w') as f:f.write('language=en\nlinux_profile=generic\nterminal_log_lines=1000\nterminal_load_lines=50\nterminal_clear_on_open=1\n')
    pid,fd=pty.fork()
    if pid==0:
        e=os.environ.copy();e.update(HOME=home,LANG='C',LC_ALL='C',SHELL='/bin/sh');os.execve(os.path.abspath('./wb'),[os.path.abspath('./wb')],e)
    fcntl.ioctl(fd,termios.TIOCSWINSZ,struct.pack('HHHH',32,112,0,0));data=bytearray()
    try:
        read_until(fd,data,b'Terminal');os.write(fd,b'j\r');data.clear();read_until(fd,data,b'Workbench Terminal')
        os.write(fd,b"echo WB_CLEAR_MARKER\n");read_until(fd,data,b'WB_CLEAR_MARKER')
        os.write(fd,b'\x1d');read_until(fd,data,b'Terminal controls');os.write(fd,b'b');read_until(fd,data,b'Desktop')
        # Reopen: clear-on-open hides old screen but does not destroy history.
        data.clear();os.write(fd,b'\r');read_until(fd,data,b'Workbench Terminal');drain(fd,data,.4)
        live=bytes(data[data.rfind(b'Workbench Terminal'):])
        if b'WB_CLEAR_MARKER' in live:raise SystemExit('clear-on-open did not hide previous screen')
        # Ctrl+] -> Older history (index 4): jjjj Enter. Marker must become visible again.
        os.write(fd,b'\x1d');read_until(fd,data,b'Terminal controls');os.write(fd,b'jjjj\r');data.clear();read_until(fd,data,b'WB_CLEAR_MARKER')
        os.write(fd,b'\x1d');read_until(fd,data,b'Terminal controls');os.write(fd,b'b');read_until(fd,data,b'Desktop');os.write(fd,b'q')
        end=time.time()+4
        while time.time()<end:
            got,st=os.waitpid(pid,os.WNOHANG)
            if got==pid:
                if not os.WIFEXITED(st) or os.WEXITSTATUS(st)!=0:raise SystemExit(f'abnormal {st}')
                break
            time.sleep(.05)
        else:raise SystemExit('clear-on-open flow did not exit')
    finally:
        try:os.close(fd)
        except OSError:pass
print('TERMINAL CLEAR-ON-OPEN PASS')
