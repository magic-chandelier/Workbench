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
def mouse(fd,x,y):os.write(fd,f'\x1b[<0;{x};{y}M'.encode())

def make_plugin(home,pid):
    root=os.path.join(home,'.local','share','workbench','plugins',pid);os.makedirs(root,exist_ok=True)
    with open(os.path.join(root,'plugin.wbp'),'w') as f:f.write(f'''WORKBENCH_PLUGIN=1
format=1
id={pid}
version=1
api_min=1
api_max=1
name_zh={pid}
name_en={pid}
description_zh=x
description_en=x
''')
    return root

with tempfile.TemporaryDirectory(prefix='wb-plugin-mouse-') as home:
    os.makedirs(os.path.join(home,'.config','workbench'),exist_ok=True)
    with open(os.path.join(home,'.config','workbench','config'),'w') as f:f.write('language=en\nlinux_profile=generic\n')
    p1=make_plugin(home,'one');p2=make_plugin(home,'two')
    pid,fd=pty.fork()
    if pid==0:
        e=os.environ.copy();e.update(HOME=home,LANG='C',LC_ALL='C');os.execve(os.path.abspath('./wb'),[os.path.abspath('./wb')],e)
    fcntl.ioctl(fd,termios.TIOCSWINSZ,struct.pack('HHHH',34,112,0,0));data=bytearray()
    try:
        read_until(fd,data,b'Command Sets')
        # No app entry, so Desktop remains the original five rows. Settings is y=9.
        mouse(fd,10,9);data.clear();read_until(fd,data,b'Plugins')
        # Plugins row is index 10 => y=15.
        mouse(fd,10,15);data.clear();read_until(fd,data,b'one');read_until(fd,data,b'two')
        # Click first plugin row y=6 opens properties; return via Back button y=12.
        mouse(fd,10,6);data.clear();read_until(fd,data,b'Plugin Properties');mouse(fd,4,12);data.clear();read_until(fd,data,b'Plugins')
        # Select second row via click => properties, return, then click row and use Uninstall button.
        mouse(fd,10,7);data.clear();read_until(fd,data,b'Plugin Properties');mouse(fd,4,12);data.clear();read_until(fd,data,b'Plugins')
        # Click second row once more, then click Uninstall toolbar x~25 y=4.
        mouse(fd,10,7);data.clear();read_until(fd,data,b'Plugin Properties');mouse(fd,4,12);data.clear();read_until(fd,data,b'Plugins')
        # Selection remains second; toolbar positions: Refresh x1, Properties, Uninstall around x24.
        mouse(fd,28,4);data.clear();read_until(fd,data,b'Uninstall plugin');
        # Confirm yes button begins x2 around y5.
        mouse(fd,4,5);data.clear();read_until(fd,data,b'Plugins')
        if os.path.exists(p2):raise SystemExit(f'mouse uninstall did not remove selected plugin; one={os.path.exists(p1)} two={os.path.exists(p2)}')
        if not os.path.exists(p1):raise SystemExit('mouse uninstall affected another plugin')
        # Back toolbar x~42 y4, then Settings Back row y16 while a Plugins row is present.
        mouse(fd,38,4);data.clear();read_until(fd,data,b'Settings');mouse(fd,10,16);data.clear();read_until(fd,data,b'Desktop');mouse(fd,10,10)
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
print('PLUGINS MOUSE PASS')
