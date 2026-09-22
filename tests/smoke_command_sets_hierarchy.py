#!/usr/bin/env python3
import fcntl, os, pty, select, struct, tempfile, termios, time

def read_until(fd,data,needle,timeout=5):
    end=time.time()+timeout
    while time.time()<end and needle not in data:
        r,_,_=select.select([fd],[],[],0.15)
        if r:
            try:data.extend(os.read(fd,65536))
            except OSError:break
    if needle not in data:raise SystemExit(f'missing {needle!r}\n{bytes(data[-4000:])!r}')

with tempfile.TemporaryDirectory(prefix='wb-command-hierarchy-') as home:
    os.makedirs(os.path.join(home,'.config','workbench'),exist_ok=True)
    with open(os.path.join(home,'.config','workbench','config'),'w') as f:f.write('language=en\nlinux_profile=generic\n')
    pid,fd=pty.fork()
    if pid==0:
        e=os.environ.copy();e.update(HOME=home,LANG='C',LC_ALL='C',SHELL='/bin/sh');os.execve(os.path.abspath('./wb'),[os.path.abspath('./wb')],e)
    fcntl.ioctl(fd,termios.TIOCSWINSZ,struct.pack('HHHH',34,112,0,0));data=bytearray()
    try:
        read_until(fd,data,b'Command Sets')
        os.write(fd,b'jj\r');data.clear();read_until(fd,data,b'Linux Commands')
        read_until(fd,data,b'[System]');read_until(fd,data,b'[Read-only]');read_until(fd,data,b'New Command Set')
        os.write(fd,b'\r');data.clear();read_until(fd,data,b'System')
        read_until(fd,data,b'Only categories with actions')
        os.write(fd,b'qqq')
        end=time.time()+3
        while time.time()<end:
            got,st=os.waitpid(pid,os.WNOHANG)
            if got==pid:
                if not os.WIFEXITED(st) or os.WEXITSTATUS(st)!=0:raise SystemExit(f'abnormal {st}')
                break
            time.sleep(.05)
        else:raise SystemExit('did not exit')
    finally:
        try:os.close(fd)
        except OSError:pass
print('COMMAND SETS HIERARCHY PASS')
