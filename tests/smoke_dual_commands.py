#!/usr/bin/env python3
import fcntl, os, pty, select, struct, tempfile, termios, time

def read_until(fd,data,needle,timeout=5.0):
    end=time.time()+timeout
    while time.time()<end and needle not in data:
        r,_,_=select.select([fd],[],[],0.15)
        if r:
            try:data.extend(os.read(fd,65536))
            except OSError:break
    if needle not in data:raise SystemExit(f"missing {needle!r}\n{bytes(data[-5000:])!r}")
def mouse(fd,b,x,y):os.write(fd,f"\x1b[<{b};{x};{y}M".encode())
def vk(fd,ch):
    rows=["1234567890-_=" ,"qwertyuiop[]\\","asdfghjkl;'\"","zxcvbnm,./:","~@+$%&!*?|<>"]
    for ri,row in enumerate(rows):
        if ch in row:mouse(fd,0,3+4*row.index(ch),7+ri);return
    raise ValueError(ch)
def vk_text(fd,text):
    for ch in text:vk(fd,ch)
def vk_ok(fd):mouse(fd,0,3,13)

binary=os.path.abspath('./wb')
with tempfile.TemporaryDirectory(prefix='wb-mouse-cmd-') as home:
    os.makedirs(os.path.join(home,'.config','workbench'),exist_ok=True)
    with open(os.path.join(home,'.config','workbench','config'),'w') as f:
        # Force confirmation even for normal actions so the command-confirmation mouse path is exercised.
        f.write('language=en\nconfirm_normal=1\nconfirm_sensitive=0\nconfirm_privileged=1\nlinux_profile=generic\nvirtual_keyboard=1\nshortcut_hints=1\n')
    pid,fd=pty.fork()
    if pid==0:
        env=os.environ.copy();env.update(HOME=home,LANG='C',LC_ALL='C');os.execve(binary,[binary],env)
    fcntl.ioctl(fd,termios.TIOCSWINSZ,struct.pack('HHHH',34,112,0,0));data=bytearray()
    try:
        read_until(fd,data,b'Command Sets')
        # Desktop -> Command Sets -> Linux Commands -> System, mouse only.
        mouse(fd,0,10,8);read_until(fd,data,b'Linux Commands')
        mouse(fd,0,10,6);read_until(fd,data,b'Only categories')
        mouse(fd,0,10,7);read_until(fd,data,b'[Search]')
        # Click Search and enter 'date' via virtual keyboard.
        data.clear();mouse(fd,0,4,4);read_until(fd,data,b'Enter keywords')
        vk_text(fd,'date');data.clear();vk_ok(fd);read_until(fd,data,b'Search: date')
        # Run first result by its [Run] cell. Confirm with mouse, then return from command output by mouse.
        data.clear();mouse(fd,0,44,8);read_until(fd,data,b'Run confirmation');mouse(fd,0,4,5);read_until(fd,data,b'[Return]')
        mouse(fd,0,3,34);read_until(fd,data,b'Search: date')
        # Open properties by mouse and return through visible Back button.
        data.clear();mouse(fd,0,53,8);read_until(fd,data,b'Properties');mouse(fd,0,20,3);read_until(fd,data,b'Search: date')
        # Task list Back -> category Back -> Command Sets -> desktop -> Exit, all mouse.
        mouse(fd,0,34,4);read_until(fd,data,b'Only categories')
        mouse(fd,0,25,4);read_until(fd,data,b'Command Sets')
        mouse(fd,0,43,4);read_until(fd,data,b'Desktop')
        mouse(fd,0,10,10)
        end=time.time()+3;status=None
        while time.time()<end:
            got,st=os.waitpid(pid,os.WNOHANG)
            if got==pid:status=st;break
            time.sleep(.05)
        if status is None:os.kill(pid,15);os.waitpid(pid,0);raise SystemExit('mouse command flow did not exit')
        if not os.WIFEXITED(status) or os.WEXITSTATUS(status)!=0:raise SystemExit(f'abnormal {status}')
    finally:
        try:os.close(fd)
        except OSError:pass
print('DUAL COMMANDS MOUSE FLOW PASS')
