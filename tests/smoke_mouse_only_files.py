#!/usr/bin/env python3
import fcntl, os, pty, select, struct, tempfile, termios, time

def read_until(fd,data,needle,timeout=5.0):
    end=time.time()+timeout
    while time.time()<end and needle not in data:
        r,_,_=select.select([fd],[],[],0.15)
        if r:
            try:data.extend(os.read(fd,65536))
            except OSError:break
    if needle not in data:
        raise SystemExit(f"missing {needle!r}\n{bytes(data[-4000:])!r}")

def mouse(fd,button,x,y):
    os.write(fd,f"\x1b[<{button};{x};{y}M".encode('ascii'))

def vk_char(fd,ch):
    rows=["1234567890-_=" ,"qwertyuiop[]\\","asdfghjkl;'\"","zxcvbnm,./:","~@+$%&!*?|<>"]
    for ridx,row in enumerate(rows):
        if ch in row:
            idx=row.index(ch);mouse(fd,0,3+4*idx,7+ridx);return
    raise ValueError(ch)

def vk_ok(fd):mouse(fd,0,3,13)

binary=os.path.abspath('./wb')
with tempfile.TemporaryDirectory(prefix='wb-mouse-files-') as home:
    os.makedirs(os.path.join(home,'.config','workbench'),exist_ok=True)
    with open(os.path.join(home,'.config','workbench','config'),'w') as f:
        f.write('language=en\nconfirm_normal=0\nconfirm_sensitive=0\nconfirm_privileged=1\nlinux_profile=generic\nvirtual_keyboard=1\nshortcut_hints=1\n')
    mountinfo=os.path.join(home,'mountinfo')
    with open(mountinfo,'w') as f:
        f.write('24 1 0:1 / / rw,relatime - overlay overlay rw\n')
    pid,fd=pty.fork()
    if pid==0:
        env=os.environ.copy();env.update(HOME=home,LANG='C',LC_ALL='C',WB_MOUNTINFO=mountinfo)
        os.execve(binary,[binary],env)
    fcntl.ioctl(fd,termios.TIOCSWINSZ,struct.pack('HHHH',34,112,0,0));data=bytearray()
    try:
        read_until(fd,data,b'Workbench Files')
        # Desktop -> Files, then Locations -> Home. No ordinary keyboard bytes are sent.
        mouse(fd,0,10,6);read_until(fd,data,b'Locations')
        mouse(fd,0,10,7);read_until(fd,data,b'Current path')
        read_until(fd,data,home.encode())
        # New file -> virtual keyboard "a" -> OK.
        data.clear();mouse(fd,0,5,5);read_until(fd,data,b'Enter name:')
        vk_char(fd,'a');vk_ok(fd);read_until(fd,data,b'Current path');read_until(fd,data,b'a')
        target=os.path.join(home,'a')
        if not os.path.isfile(target):raise SystemExit('mouse virtual keyboard did not create file')
        # Right click selected file row: parent is row 8, file a row 9. Change mode is context row 12.
        data.clear();mouse(fd,2,10,9);read_until(fd,data,b'File actions');mouse(fd,0,10,12);read_until(fd,data,b'octal digits')
        vk_char(fd,'6');vk_char(fd,'0');vk_char(fd,'0');vk_ok(fd);read_until(fd,data,b'Current path')
        if os.stat(target).st_mode & 0o7777 != 0o600:raise SystemExit('mouse chmod failed')
        # Rename a -> b through context menu row 10.
        data.clear();mouse(fd,2,10,9);read_until(fd,data,b'File actions');mouse(fd,0,10,10);read_until(fd,data,b'Enter new name:')
        vk_char(fd,'b');vk_ok(fd);read_until(fd,data,b'Current path');read_until(fd,data,b'b')
        renamed=os.path.join(home,'b')
        if not os.path.isfile(renamed) or os.path.exists(target):raise SystemExit('mouse rename failed')
        # Properties, then mouse Back.
        data.clear();mouse(fd,2,10,9);read_until(fd,data,b'File actions');mouse(fd,0,10,7);read_until(fd,data,b'Properties');read_until(fd,data,b'/b')
        mouse(fd,0,3,3);read_until(fd,data,b'Current path')
        # Delete via context row 11 and mouse confirmation button on row 5.
        data.clear();mouse(fd,2,10,9);read_until(fd,data,b'File actions');mouse(fd,0,10,11);read_until(fd,data,b'Confirm action');mouse(fd,0,4,5);read_until(fd,data,b'Current path')
        if os.path.exists(renamed):raise SystemExit('mouse delete confirmation failed')
        # Files -> desktop through visible button, then click Exit row.
        mouse(fd,0,45,5);read_until(fd,data,b'Desktop');mouse(fd,0,10,10)
        deadline=time.time()+3;status=None
        while time.time()<deadline:
            got,st=os.waitpid(pid,os.WNOHANG)
            if got==pid:status=st;break
            time.sleep(.05)
        if status is None:
            os.kill(pid,15);os.waitpid(pid,0);raise SystemExit('mouse-only Files flow did not exit')
        if not os.WIFEXITED(status) or os.WEXITSTATUS(status)!=0:raise SystemExit(f'abnormal status {status}')
    finally:
        try:os.close(fd)
        except OSError:pass
print('MOUSE ONLY FILES PASS')
