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

with tempfile.TemporaryDirectory(prefix='wb-term-plugin-cs-') as home:
    os.makedirs(os.path.join(home,'.config','workbench'),exist_ok=True)
    with open(os.path.join(home,'.config','workbench','config'),'w') as f:f.write('language=en\nlinux_profile=generic\nterminal_log_lines=1000\nterminal_load_lines=50\n')
    root=os.path.join(home,'.local','share','workbench','plugins','demo');cs=os.path.join(root,'command-sets');os.makedirs(cs,exist_ok=True)
    with open(os.path.join(root,'plugin.wbp'),'w') as f:f.write('''WORKBENCH_PLUGIN=1
format=1
id=demo
version=1
api_min=1
api_max=1
name_zh=Demo
name_en=Demo
description_zh=x
description_en=x
''')
    marker=os.path.join(home,'marker')
    with open(os.path.join(cs,'tools.wbc'),'w') as f:f.write(f'''WORKBENCH_COMMAND_SET=1
id=tools
name_zh=Plugin Tools
name_en=Plugin Tools
desc_zh=x
desc_en=x
[action]
id=mark
title_zh=Mark
title_en=Mark
desc_zh=x
desc_en=x
keywords=mark
command=printf plugin-insert > {marker}
category=0
risk=0
''')
    pid,fd=pty.fork()
    if pid==0:
        e=os.environ.copy();e.update(HOME=home,LANG='C',LC_ALL='C',SHELL='/bin/sh');os.execve(os.path.abspath('./wb'),[os.path.abspath('./wb')],e)
    fcntl.ioctl(fd,termios.TIOCSWINSZ,struct.pack('HHHH',36,118,0,0));data=bytearray()
    try:
        read_until(fd,data,b'Terminal');os.write(fd,b'j\r');data.clear();read_until(fd,data,b'Workbench Terminal')
        os.write(fd,b'\x1d');data.clear();read_until(fd,data,b'Terminal controls');os.write(fd,b'j\r');data.clear();read_until(fd,data,b'Plugin Tools')
        os.write(fd,b'j\r');data.clear();read_until(fd,data,b'Mark');os.write(fd,b'\r');data.clear();read_until(fd,data,b'Workbench Terminal')
        time.sleep(.25)
        if os.path.exists(marker):raise SystemExit('plugin command auto-ran instead of insert-only')
        os.write(fd,b'\r')
        end=time.time()+3
        while time.time()<end and not os.path.exists(marker):time.sleep(.05)
        if not os.path.exists(marker):raise SystemExit('plugin inserted command did not execute after explicit Enter')
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
print('TERMINAL PLUGIN COMMAND SETS PASS')
