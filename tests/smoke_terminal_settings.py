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
with tempfile.TemporaryDirectory(prefix='wb-term-settings-') as home:
    os.makedirs(os.path.join(home,'.config','workbench'),exist_ok=True);cfg=os.path.join(home,'.config','workbench','config')
    with open(cfg,'w') as f:f.write('language=en\nlinux_profile=generic\n')
    pid,fd=pty.fork()
    if pid==0:
        e=os.environ.copy();e.update(HOME=home,LANG='C',LC_ALL='C');os.execve(os.path.abspath('./wb'),[os.path.abspath('./wb')],e)
    fcntl.ioctl(fd,termios.TIOCSWINSZ,struct.pack('HHHH',36,112,0,0));data=bytearray()
    try:
        read_until(fd,data,b'Terminal');os.write(fd,b's');data.clear();read_until(fd,data,b'Terminal log lines');read_until(fd,data,b'1000')
        # row 5: terminal log lines
        os.write(fd,b'jjjjj\r');data.clear();read_until(fd,data,b'Current value: 1000');os.write(fd,b'l');read_until(fd,data,b'Current value: 1100');os.write(fd,b'q')
        # row 6: history load
        os.write(fd,b'j\r');data.clear();read_until(fd,data,b'Current value: 50');os.write(fd,b'l');read_until(fd,data,b'Current value: 60');os.write(fd,b'q')
        # row 7: clear on reopen toggle
        os.write(fd,b'j\r');data.clear();read_until(fd,data,b'Clear terminal on reopen');read_until(fd,data,b'Enabled');os.write(fd,b'q');read_until(fd,data,b'Desktop');os.write(fd,b'q')
        end=time.time()+4
        while time.time()<end:
            got,st=os.waitpid(pid,os.WNOHANG)
            if got==pid:break
            time.sleep(.05)
        with open(cfg) as f:txt=f.read()
        for line in ('terminal_log_lines=1100\n','terminal_load_lines=60\n','terminal_clear_on_open=1\n'):
            if line not in txt:raise SystemExit(f'missing persisted {line!r}: {txt!r}')
    finally:
        try:os.close(fd)
        except OSError:pass
print('TERMINAL SETTINGS PASS')
