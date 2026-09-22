#!/usr/bin/env python3
import fcntl, os, pty, select, struct, tempfile, termios, time

def read_until(fd,data,needle,timeout=4.0):
    end=time.time()+timeout
    while time.time()<end and needle not in data:
        r,_,_=select.select([fd],[],[],0.15)
        if r:
            try:data.extend(os.read(fd,65536))
            except OSError:break
    if needle not in data:raise SystemExit(f"missing {needle!r}\n{bytes(data[-2500:])!r}")
def mouse(fd,b,x,y):os.write(fd,f"\x1b[<{b};{x};{y}M".encode())
def vk_char(fd,ch):
    rows=["1234567890-_=" ,"qwertyuiop[]\\","asdfghjkl;'\"","zxcvbnm,./:","~@+$%&!*?|<>"]
    for ri,row in enumerate(rows):
        if ch in row:mouse(fd,0,3+4*row.index(ch),7+ri);return
    raise ValueError(ch)

binary=os.path.abspath('./wb')
with tempfile.TemporaryDirectory(prefix='wb-vk-') as home:
    os.makedirs(os.path.join(home,'.config','workbench'),exist_ok=True)
    with open(os.path.join(home,'.config','workbench','config'),'w') as f:f.write('language=en\nconfirm_normal=0\nconfirm_sensitive=0\nconfirm_privileged=1\nlinux_profile=generic\nvirtual_keyboard=1\nshortcut_hints=1\n')
    mountinfo=os.path.join(home,'mountinfo');open(mountinfo,'w').write('24 1 0:1 / / rw,relatime - overlay overlay rw\n')
    pid,fd=pty.fork()
    if pid==0:
        env=os.environ.copy();env.update(HOME=home,LANG='C',LC_ALL='C',WB_MOUNTINFO=mountinfo);os.execve(binary,[binary],env)
    fcntl.ioctl(fd,termios.TIOCSWINSZ,struct.pack('HHHH',34,112,0,0));data=bytearray()
    try:
        read_until(fd,data,b'Workbench Files');mouse(fd,0,10,6);read_until(fd,data,b'Locations');mouse(fd,0,10,7);read_until(fd,data,b'Current path')
        # Backspace + Shift + Space: type a, erase it, Shift, type A, space, B, accept.
        mouse(fd,0,5,5);read_until(fd,data,b'Enter name:');vk_char(fd,'a');mouse(fd,0,20,12);mouse(fd,0,4,12);vk_char(fd,'a');mouse(fd,0,12,12);vk_char(fd,'b');data.clear();mouse(fd,0,3,13);read_until(fd,data,b'Current path');read_until(fd,data,b'A B')
        wanted=os.path.join(home,'A B')
        if not os.path.isfile(wanted):raise SystemExit('Shift/Space/Backspace virtual keyboard path failed')
        # Clear + Cancel: type c, clear, type d, then cancel. No new file may be created.
        data.clear();mouse(fd,0,5,5);read_until(fd,data,b'Enter name:');vk_char(fd,'c');mouse(fd,0,32,12);vk_char(fd,'d');data.clear();mouse(fd,0,9,13);read_until(fd,data,b'Current path')
        if os.path.exists(os.path.join(home,'c')) or os.path.exists(os.path.join(home,'d')):raise SystemExit('Clear/Cancel virtual keyboard path failed')
        mouse(fd,0,45,5);read_until(fd,data,b'Desktop');mouse(fd,0,10,10)
        end=time.time()+3
        while time.time()<end:
            got,st=os.waitpid(pid,os.WNOHANG)
            if got==pid:
                if not os.WIFEXITED(st) or os.WEXITSTATUS(st)!=0:raise SystemExit(f'abnormal {st}')
                break
            time.sleep(.05)
        else:os.kill(pid,15);os.waitpid(pid,0);raise SystemExit('vk flow did not exit')
    finally:
        try:os.close(fd)
        except OSError:pass
print('VIRTUAL KEYBOARD PASS')
