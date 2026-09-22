#!/usr/bin/env python3
import fcntl, os, pty, select, struct, tempfile, termios, time

def read_until(fd,data,needle,timeout=6):
    end=time.time()+timeout
    while time.time()<end and needle not in data:
        r,_,_=select.select([fd],[],[],0.15)
        if r:
            try:data.extend(os.read(fd,65536))
            except OSError:break
    if needle not in data:raise SystemExit(f'missing {needle!r}\n{bytes(data[-6000:])!r}')

with tempfile.TemporaryDirectory(prefix='wb-term-command-sets-') as home:
    os.makedirs(os.path.join(home,'.config','workbench'),exist_ok=True)
    with open(os.path.join(home,'.config','workbench','config'),'w') as f:f.write('language=en\nlinux_profile=generic\nterminal_log_lines=1000\nterminal_load_lines=50\n')
    root=os.path.join(home,'.local','share','workbench','command-sets');os.makedirs(root,mode=0o700,exist_ok=True)
    marker=os.path.join(home,'terminal-marker')
    cmd=f"printf custom > {marker}"
    with open(os.path.join(root,'mine.wbc'),'w') as f:
        f.write('WORKBENCH_COMMAND_SET=1\nid=mine\nname_zh=Mine\nname_en=Mine\ndesc_zh=Custom\ndesc_en=Custom\n\n[action]\nid=marker\ntitle_zh=Marker\ntitle_en=Marker\ndesc_zh=Marker\ndesc_en=Marker\nkeywords=marker\ncategory=0\nrisk=0\ncommand='+cmd+'\n')
    os.chmod(os.path.join(root,'mine.wbc'),0o600)
    pid,fd=pty.fork()
    if pid==0:
        e=os.environ.copy();e.update(HOME=home,LANG='C',LC_ALL='C',SHELL='/bin/sh');os.execve(os.path.abspath('./wb'),[os.path.abspath('./wb')],e)
    fcntl.ioctl(fd,termios.TIOCSWINSZ,struct.pack('HHHH',36,118,0,0));data=bytearray()
    try:
        read_until(fd,data,b'Terminal');os.write(fd,b'j\r');data.clear();read_until(fd,data,b'Workbench Terminal')
        os.write(fd,b'\x1d');data.clear();read_until(fd,data,b'Terminal controls');os.write(fd,b'j\r');data.clear();read_until(fd,data,b'Command Sets')
        read_until(fd,data,b'Linux Commands');read_until(fd,data,b'Mine')
        os.write(fd,b'j\r');data.clear();read_until(fd,data,b'Marker');os.write(fd,b'\r');data.clear();read_until(fd,data,b'Workbench Terminal')
        time.sleep(.25)
        if os.path.exists(marker):raise SystemExit('custom command auto-ran instead of insert-only')
        os.write(fd,b'\r')
        end=time.time()+3
        while time.time()<end and not os.path.exists(marker):time.sleep(.05)
        if not os.path.exists(marker):raise SystemExit('inserted command did not run after explicit Enter')
        os.write(fd,b'\x1d');read_until(fd,data,b'Terminal controls');os.write(fd,b'b');read_until(fd,data,b'Desktop');os.write(fd,b'q')
        end=time.time()+4
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
print('TERMINAL COMMAND SETS PASS')
