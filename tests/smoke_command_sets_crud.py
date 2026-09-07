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

def send(fd,s):os.write(fd,s if isinstance(s,bytes) else s.encode())

def input_text(fd,data,text):
    read_until(fd,data,b'Input:');send(fd,text);send(fd,b'\r');data.clear()

def replace_default(fd,data,text):
    read_until(fd,data,b'Default:');send(fd,b'\x15');send(fd,text);send(fd,b'\r');data.clear()

with tempfile.TemporaryDirectory(prefix='wb-command-crud-') as home:
    os.makedirs(os.path.join(home,'.config','workbench'),exist_ok=True)
    with open(os.path.join(home,'.config','workbench','config'),'w') as f:f.write('language=en\nlinux_profile=generic\nconfirm_normal=0\n')
    pid,fd=pty.fork()
    if pid==0:
        e=os.environ.copy();e.update(HOME=home,LANG='C',LC_ALL='C',SHELL='/bin/sh');os.execve(os.path.abspath('./wb'),[os.path.abspath('./wb')],e)
    fcntl.ioctl(fd,termios.TIOCSWINSZ,struct.pack('HHHH',38,118,0,0));data=bytearray()
    try:
        read_until(fd,data,b'Command Sets');send(fd,b'jj\r');data.clear();read_until(fd,data,b'New Command Set')
        send(fd,b'n');data.clear();input_text(fd,data,'My Set');input_text(fd,data,'Ops commands')
        read_until(fd,data,b'My Set');send(fd,b'\r');data.clear();read_until(fd,data,b'Add Command')
        send(fd,b'a');data.clear();input_text(fd,data,'Say OK');input_text(fd,data,'Print marker');input_text(fd,data,"printf 'CUSTOM_OK\\n'");input_text(fd,data,'custom ok')
        read_until(fd,data,b'Risk');send(fd,b'\r');data.clear();read_until(fd,data,b'Say OK')
        send(fd,b'\r');data.clear();read_until(fd,data,b'CUSTOM_OK');read_until(fd,data,b'Return');send(fd,b'\r');data.clear();read_until(fd,data,b'Say OK')
        send(fd,b'e');data.clear();replace_default(fd,data,'Say NEW');replace_default(fd,data,'Edited marker')
        read_until(fd,data,b'Default:');send(fd,b'\r');data.clear();read_until(fd,data,b'Default:');send(fd,b'\r');data.clear()
        read_until(fd,data,b'Risk');send(fd,b'\r');data.clear();read_until(fd,data,b'Say NEW')
        send(fd,b'd');data.clear();read_until(fd,data,b'Confirm');send(fd,b'y');data.clear();read_until(fd,data,b'Add Command')
        if b'Say NEW' in bytes(data[-1500:]):raise SystemExit('action still visible after delete')
        send(fd,b'q');data.clear();read_until(fd,data,b'My Set')
        send(fd,b'e');data.clear();replace_default(fd,data,'Renamed Set');replace_default(fd,data,'Renamed description');read_until(fd,data,b'Renamed Set')
        send(fd,b'd');data.clear();read_until(fd,data,b'Confirm');send(fd,b'y');data.clear();read_until(fd,data,b'Linux Commands')
        if b'Renamed Set' in bytes(data[-1500:]):raise SystemExit('set still visible after delete')
        send(fd,b'q');data.clear();read_until(fd,data,b'Desktop');send(fd,b'q')
        end=time.time()+4
        while time.time()<end:
            got,st=os.waitpid(pid,os.WNOHANG)
            if got==pid:
                if not os.WIFEXITED(st) or os.WEXITSTATUS(st)!=0:raise SystemExit(f'abnormal {st}')
                break
            time.sleep(.05)
        else:raise SystemExit('did not exit')
        root=os.path.join(home,'.local','share','workbench','command-sets')
        if not os.path.isdir(root):raise SystemExit('command-set directory missing')
        if [x for x in os.listdir(root) if x.endswith('.wbc')]:raise SystemExit('deleted set file remains')
    finally:
        try:os.close(fd)
        except OSError:pass
print('COMMAND SETS CRUD PASS')
