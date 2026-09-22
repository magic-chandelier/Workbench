#!/usr/bin/env python3
import fcntl, os, pty, select, struct, tempfile, termios, time

def read_until(fd,data,needle,timeout=5):
    end=time.time()+timeout
    while time.time()<end and needle not in data:
        r,_,_=select.select([fd],[],[],.15)
        if r:
            try:data.extend(os.read(fd,65536))
            except OSError:break
    if needle not in data:raise SystemExit(f'missing {needle!r}\n{bytes(data[-4000:])!r}')
def mouse(fd,x,y,button=0):os.write(fd,f'\x1b[<{button};{x};{y}M'.encode())
with tempfile.TemporaryDirectory(prefix='wb-term-mouse-') as home:
    os.makedirs(os.path.join(home,'.config','workbench'),exist_ok=True)
    with open(os.path.join(home,'.config','workbench','config'),'w') as f:f.write('language=en\nlinux_profile=generic\nvirtual_keyboard=1\nshortcut_hints=1\nterminal_log_lines=1000\nterminal_load_lines=50\nterminal_clear_on_open=0\n')
    pid,fd=pty.fork()
    if pid==0:
        env=os.environ.copy();env.update(HOME=home,LANG='C',LC_ALL='C',SHELL='/bin/sh');os.execve(os.path.abspath('./wb'),[os.path.abspath('./wb')],env)
    fcntl.ioctl(fd,termios.TIOCSWINSZ,struct.pack('HHHH',32,112,0,0));data=bytearray()
    try:
        read_until(fd,data,b'Terminal')
        # Desktop row 7 is Terminal.
        mouse(fd,10,7);data.clear();read_until(fd,data,b'Workbench Terminal')
        # Back is the first stable toolbar button on row 3.
        mouse(fd,3,3);data.clear();read_until(fd,data,b'Desktop')
        # Exit row is 10: Files, Terminal, Commands, Settings, Exit.
        mouse(fd,10,10)
        end=time.time()+4
        while time.time()<end:
            got,st=os.waitpid(pid,os.WNOHANG)
            if got==pid:
                if not os.WIFEXITED(st) or os.WEXITSTATUS(st)!=0:raise SystemExit(f'abnormal {st}')
                break
            time.sleep(.05)
        else:raise SystemExit('mouse terminal flow did not exit')
    finally:
        try:os.close(fd)
        except OSError:pass
print('TERMINAL MOUSE PASS')
