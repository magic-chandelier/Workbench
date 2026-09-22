#!/usr/bin/env python3
import fcntl, os, pty, select, struct, tempfile, termios, time

def read_until(fd,data,needle,timeout=6):
    end=time.time()+timeout
    while time.time()<end and needle not in data:
        r,_,_=select.select([fd],[],[],0.15)
        if r:
            try:data.extend(os.read(fd,65536))
            except OSError:break
    if needle not in data:raise SystemExit(f'missing {needle!r}\n{bytes(data[-7000:])!r}')

def send_keys(fd,keys,delay=.04):
    for b in keys:
        os.write(fd,bytes([b]));time.sleep(delay)

def drain(fd,data,seconds=.35):
    end=time.time()+seconds
    while time.time()<end:
        r,_,_=select.select([fd],[],[],0.05)
        if not r:continue
        try:data.extend(os.read(fd,65536))
        except OSError:break

def plugin(home):
    root=os.path.join(home,'.local','share','workbench','plugins','demo');os.makedirs(os.path.join(root,'bin'),exist_ok=True);os.makedirs(os.path.join(root,'command-sets'),exist_ok=True)
    with open(os.path.join(root,'plugin.wbp'),'w') as f:f.write('''WORKBENCH_PLUGIN=1
format=1
id=demo
version=1
api_min=1
api_max=1
name_zh=演示插件
name_en=Demo Plugin
description_zh=x
description_en=x
entry=bin/run
command_sets=command-sets
''')
    run=os.path.join(root,'bin','run')
    with open(run,'w') as f:f.write('#!/bin/sh\nexit 0\n')
    os.chmod(run,0o700)
    with open(os.path.join(root,'command-sets','tools.wbc'),'w') as f:f.write('''WORKBENCH_COMMAND_SET=1
id=tools
name_zh=插件工具
name_en=Plugin Tools
desc_zh=x
desc_en=x
[action]
id=x
title_zh=x
title_en=x
desc_zh=x
desc_en=x
keywords=x
command=true
category=0
risk=0
''')
    return root

with tempfile.TemporaryDirectory(prefix='wb-plugin-toggle-smoke-') as home:
    os.makedirs(os.path.join(home,'.config','workbench'),exist_ok=True)
    with open(os.path.join(home,'.config','workbench','config'),'w') as f:f.write('language=en\nlinux_profile=generic\n')
    root=plugin(home)
    pid,fd=pty.fork()
    if pid==0:
        e=os.environ.copy();e.update(HOME=home,LANG='C',LC_ALL='C',SHELL='/bin/sh');os.execve(os.path.abspath('./wb'),[os.path.abspath('./wb')],e)
    fcntl.ioctl(fd,termios.TIOCSWINSZ,struct.pack('HHHH',38,118,0,0));data=bytearray()
    try:
        read_until(fd,data,b'Applications')
        os.write(fd,b's');data.clear();read_until(fd,data,b'Settings');drain(fd,data,.2);send_keys(fd,b'j'*10+b'\r');data.clear();read_until(fd,data,b'Demo Plugin')
        os.write(fd,b'e');data.clear();read_until(fd,data,b'Disabled')
        marker=os.path.join(root,'.workbench-disabled')
        if not os.path.isfile(marker):raise SystemExit('disable marker missing')
        os.write(fd,b'qq');data.clear();read_until(fd,data,b'Desktop');drain(fd,data)
        if b'1 application plugins installed' in data:raise SystemExit('Applications row still visible after disable')
        os.write(fd,b's');data.clear();read_until(fd,data,b'Settings');drain(fd,data,.2);send_keys(fd,b'j'*10+b'\r');data.clear();read_until(fd,data,b'Disabled')
        os.write(fd,b'e');data.clear();read_until(fd,data,b'Active')
        if os.path.exists(marker):raise SystemExit('disable marker still exists after enable')
        os.write(fd,b'qq');data.clear();read_until(fd,data,b'Applications')
        os.write(fd,b'q')
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
print('PLUGIN TOGGLE TUI PASS')
