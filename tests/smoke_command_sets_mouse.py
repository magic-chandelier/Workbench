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

def mouse(fd,x,y,button=0,release=False):os.write(fd,f"\x1b[<{button};{x};{y}{'m' if release else 'M'}".encode())

ROWS=[("1234567890-_=",7),("qwertyuiop[]\\",8),("asdfghjkl;'\"",9),("zxcvbnm,./:",10),("~@+$%&!*?|<>",11)]
def vk_char(fd,ch):
    for keys,y in ROWS:
        if ch in keys:
            i=keys.index(ch);mouse(fd,3+i*4,y);return
    raise SystemExit(f'unsupported vk char {ch!r}')
def vk_text(fd,data,text,clear=False):
    read_until(fd,data,b'Input:')
    if clear:mouse(fd,33,12)
    for ch in text:vk_char(fd,ch)
    mouse(fd,3,13);data.clear()

with tempfile.TemporaryDirectory(prefix='wb-command-mouse-') as home:
    os.makedirs(os.path.join(home,'.config','workbench'),exist_ok=True)
    with open(os.path.join(home,'.config','workbench','config'),'w') as f:f.write('language=en\nlinux_profile=generic\nvirtual_keyboard=1\nconfirm_normal=0\n')
    pid,fd=pty.fork()
    if pid==0:
        e=os.environ.copy();e.update(HOME=home,LANG='C',LC_ALL='C',SHELL='/bin/sh');os.execve(os.path.abspath('./wb'),[os.path.abspath('./wb')],e)
    fcntl.ioctl(fd,termios.TIOCSWINSZ,struct.pack('HHHH',38,118,0,0));data=bytearray()
    try:
        read_until(fd,data,b'Command Sets');mouse(fd,10,8);data.clear();read_until(fd,data,b'New Command Set')
        mouse(fd,5,4);data.clear();vk_text(fd,data,'mouse');vk_text(fd,data,'mouse')
        read_until(fd,data,b'mouse');mouse(fd,10,7);data.clear();read_until(fd,data,b'Add Command')
        mouse(fd,15,4);data.clear();vk_text(fd,data,'pwd');vk_text(fd,data,'pwd');vk_text(fd,data,'pwd');vk_text(fd,data,'pwd')
        read_until(fd,data,b'Risk');mouse(fd,5,5);data.clear();read_until(fd,data,b'pwd')
        mouse(fd,43,7);data.clear();read_until(fd,data,os.path.abspath('.').encode());read_until(fd,data,b'Return');mouse(fd,3,38);data.clear();read_until(fd,data,b'pwd')
        mouse(fd,75,7);data.clear();read_until(fd,data,b'Confirm');mouse(fd,5,5);data.clear();read_until(fd,data,b'No commands')
        mouse(fd,50,4);data.clear();read_until(fd,data,b'mouse')
        mouse(fd,23,4);data.clear();vk_text(fd,data,'new',clear=True);vk_text(fd,data,'new',clear=True);read_until(fd,data,b'new')
        mouse(fd,35,4);data.clear();read_until(fd,data,b'Confirm');mouse(fd,5,5);data.clear();read_until(fd,data,b'Linux Commands')
        mouse(fd,45,4);data.clear();read_until(fd,data,b'Desktop');mouse(fd,10,10)
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
print('COMMAND SETS MOUSE PASS')
