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
with tempfile.TemporaryDirectory(prefix='wb-term-cmd-') as home:
    os.makedirs(os.path.join(home,'.config','workbench'),exist_ok=True)
    with open(os.path.join(home,'.config','workbench','config'),'w') as f:f.write('language=en\nlinux_profile=generic\nterminal_log_lines=1000\nterminal_load_lines=50\nterminal_clear_on_open=0\n')
    pid,fd=pty.fork()
    if pid==0:
        e=os.environ.copy();e.update(HOME=home,LANG='C',LC_ALL='C',SHELL='/bin/sh');os.execve(os.path.abspath('./wb'),[os.path.abspath('./wb')],e)
    fcntl.ioctl(fd,termios.TIOCSWINSZ,struct.pack('HHHH',34,112,0,0));data=bytearray()
    try:
        read_until(fd,data,b'Terminal');os.write(fd,b'j\r');data.clear();read_until(fd,data,b'Workbench Terminal')
        os.write(fd,b'\x1d');data.clear();read_until(fd,data,b'Terminal controls')
        # Command Sets is second row. Use the shared browser: Linux -> System -> Kernel version.
        os.write(fd,b'j\r');data.clear();read_until(fd,data,b'Command Sets');read_until(fd,data,b'Linux Commands')
        os.write(fd,b'\r');data.clear();read_until(fd,data,b'Only categories')
        os.write(fd,b'\r');data.clear();read_until(fd,data,b'[Insert]')
        os.write(fd,b'j\r');data.clear();read_until(fd,data,b'Workbench Terminal');read_until(fd,data,b'uname -r')
        # Insertion must not auto-run: there should still be no finalized command-output line caused by Enter.
        tail=bytes(data[data.rfind(b'Workbench Terminal'):])
        if b'Kernel version' in tail:raise SystemExit('task picker UI leaked instead of returning terminal')
        # Cancel the inserted shell line, then return to desktop.
        os.write(fd,b'\x03');time.sleep(.15);os.write(fd,b'\x1d');read_until(fd,data,b'Terminal controls');os.write(fd,b'b');read_until(fd,data,b'Desktop');os.write(fd,b'q')
        end=time.time()+4
        while time.time()<end:
            got,st=os.waitpid(pid,os.WNOHANG)
            if got==pid:
                if not os.WIFEXITED(st) or os.WEXITSTATUS(st)!=0:raise SystemExit(f'abnormal {st}')
                break
            time.sleep(.05)
        else:raise SystemExit('terminal command flow did not exit')
    finally:
        try:os.close(fd)
        except OSError:pass
print('TERMINAL COMMAND INSERT PASS')
