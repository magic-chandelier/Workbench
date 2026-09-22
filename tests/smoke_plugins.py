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

def plugin(home,pid='demo'):
    root=os.path.join(home,'.local','share','workbench','plugins',pid);os.makedirs(os.path.join(root,'bin'),exist_ok=True);os.makedirs(os.path.join(root,'command-sets'),exist_ok=True)
    with open(os.path.join(root,'plugin.wbp'),'w') as f:f.write(f'''WORKBENCH_PLUGIN=1
format=1
id={pid}
version=1.0.0
api_min=1
api_max=1
name_zh=演示插件
name_en=Demo Plugin
description_zh=插件测试
description_en=Plugin test
entry=bin/run
command_sets=command-sets
''')
    run=os.path.join(root,'bin','run')
    with open(run,'w') as f:f.write('#!/bin/sh\nprintf app-ok > "$HOME/plugin-app-marker"\n')
    os.chmod(run,0o700)
    with open(os.path.join(root,'command-sets','tools.wbc'),'w') as f:f.write('''WORKBENCH_COMMAND_SET=1
id=tools
name_zh=插件工具
name_en=Plugin Tools
desc_zh=插件只读
desc_en=Plugin read only
[action]
id=marker
title_zh=插件标记
title_en=Plugin Marker
desc_zh=x
desc_en=x
keywords=marker
command=printf command-ok > "$HOME/plugin-command-marker"
category=0
risk=0
''')
    return root

with tempfile.TemporaryDirectory(prefix='wb-plugin-smoke-') as home:
    os.makedirs(os.path.join(home,'.config','workbench'),exist_ok=True)
    with open(os.path.join(home,'.config','workbench','config'),'w') as f:f.write('language=en\nlinux_profile=generic\n')
    proot=plugin(home)
    pid,fd=pty.fork()
    if pid==0:
        e=os.environ.copy();e.update(HOME=home,LANG='C',LC_ALL='C',SHELL='/bin/sh');os.execve(os.path.abspath('./wb'),[os.path.abspath('./wb')],e)
    fcntl.ioctl(fd,termios.TIOCSWINSZ,struct.pack('HHHH',38,118,0,0));data=bytearray()
    try:
        read_until(fd,data,b'Applications')
        os.write(fd,b'jj\r');data.clear();read_until(fd,data,b'Demo Plugin')
        os.write(fd,b'\r');data.clear();read_until(fd,data,b'Demo Plugin')
        marker=os.path.join(home,'plugin-app-marker');
        if not os.path.exists(marker):raise SystemExit('plugin application did not execute')
        if b'Plugin finished (exit code 0)' in data:raise SystemExit('successful plugin exit should return directly')
        os.write(fd,b'q');data.clear();read_until(fd,data,b'Command Sets')
        # Dynamic Applications adds one row; Command Sets is now the fourth desktop item.
        os.write(fd,b'j\r');data.clear();read_until(fd,data,b'Plugin Tools');read_until(fd,data,b'[Plugin]');read_until(fd,data,b'[Read-only]')
        os.write(fd,b'j\r');data.clear();read_until(fd,data,b'Plugin Marker');read_until(fd,data,b'Plugin command sets are read-only')
        os.write(fd,b'qq');data.clear();read_until(fd,data,b'Desktop')
        # Settings shortcut, Plugins is immediately before Back.
        os.write(fd,b's');data.clear();read_until(fd,data,b'Settings');os.write(fd,b'j'*10+b'\r');data.clear();read_until(fd,data,b'Plugins');read_until(fd,data,b'Demo Plugin')
        os.write(fd,b'd');data.clear();read_until(fd,data,b'Uninstall');os.write(fd,b'y');data.clear();read_until(fd,data,b'Plugins')
        if os.path.exists(proot):raise SystemExit('plugin directory still exists after uninstall')
        os.write(fd,b'qq');data.clear();read_until(fd,data,b'Apps: 3')
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
print('PLUGINS TUI PASS')
