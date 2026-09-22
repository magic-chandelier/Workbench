#!/usr/bin/env python3
import fcntl, os, pty, select, struct, tempfile, termios, time

def read_until(fd,data,needle,timeout=5.0):
    end=time.time()+timeout
    while time.time()<end and needle not in data:
        r,_,_=select.select([fd],[],[],0.15)
        if r:
            try:data.extend(os.read(fd,65536))
            except OSError:break
    if needle not in data:raise SystemExit(f'missing {needle!r}\n{bytes(data[-5000:])!r}')

def cfg(home,clear=0):
    os.makedirs(os.path.join(home,'.config','workbench'),exist_ok=True)
    with open(os.path.join(home,'.config','workbench','config'),'w') as f:
        f.write('language=en\nconfirm_normal=0\nconfirm_sensitive=0\nconfirm_privileged=1\nlinux_profile=generic\nvirtual_keyboard=0\nshortcut_hints=1\nterminal_log_lines=1000\nterminal_load_lines=50\nterminal_clear_on_open=%d\n'%clear)

def spawn(home):
    pid,fd=pty.fork()
    if pid==0:
        env=os.environ.copy();env.update(HOME=home,LANG='C',LC_ALL='C',SHELL='/bin/sh')
        os.execve(os.path.abspath('./wb'),[os.path.abspath('./wb')],env)
    fcntl.ioctl(fd,termios.TIOCSWINSZ,struct.pack('HHHH',32,112,0,0));return pid,fd

with tempfile.TemporaryDirectory(prefix='wb-terminal-') as home:
    cfg(home,0);pid,fd=spawn(home);data=bytearray()
    try:
        read_until(fd,data,b'Terminal')
        # Files is first, Terminal is second.
        os.write(fd,b'j\r');data.clear();read_until(fd,data,b'Workbench Terminal')
        os.write(fd,b"printf 'WB_KEEP_MARKER\\n'\n");read_until(fd,data,b'WB_KEEP_MARKER')
        # Ctrl+] opens Workbench terminal controls; b returns to desktop without ending shell.
        os.write(fd,b'\x1d');data.clear();read_until(fd,data,b'Terminal controls');os.write(fd,b'b');data.clear();read_until(fd,data,b'Desktop')
        # Selection remains Terminal; reopen and verify captured output is preserved.
        os.write(fd,b'\r');data.clear();read_until(fd,data,b'Workbench Terminal');read_until(fd,data,b'WB_KEEP_MARKER')
        os.write(fd,b'\x1d');read_until(fd,data,b'Terminal controls');os.write(fd,b'b');read_until(fd,data,b'Desktop');os.write(fd,b'q')
        end=time.time()+4
        while time.time()<end:
            got,st=os.waitpid(pid,os.WNOHANG)
            if got==pid:
                if not os.WIFEXITED(st) or os.WEXITSTATUS(st)!=0:raise SystemExit(f'abnormal {st}')
                break
            time.sleep(.05)
        else:raise SystemExit('terminal smoke did not exit')
    finally:
        try:os.close(fd)
        except OSError:pass
print('TERMINAL SMOKE PASS')
