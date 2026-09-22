#!/usr/bin/env python3
import fcntl, os, pty, select, struct, tempfile, termios, time

def read_until(fd,data,needle,timeout=6):
    end=time.time()+timeout
    while time.time()<end and needle not in data:
        r,_,_=select.select([fd],[],[],0.15)
        if r:
            try:data.extend(os.read(fd,65536))
            except OSError:break
    if needle not in data: raise SystemExit(f'missing {needle!r}\n{bytes(data[-6000:])!r}')

def hx(s): return s.encode().hex()

binary=os.path.abspath('./wb')
with tempfile.TemporaryDirectory(prefix='wb-preview-ui-') as home:
    os.makedirs(os.path.join(home,'.config','workbench'),exist_ok=True)
    with open(os.path.join(home,'.config','workbench','config'),'w') as f:f.write('language=en\nlinux_profile=generic\n')
    root=os.path.join(home,'.local','share','workbench','plugins','plugin.preview')
    os.makedirs(os.path.join(root,'bin'),exist_ok=True);os.makedirs(os.path.join(root,'preview-providers'),exist_ok=True)
    with open(os.path.join(root,'plugin.wbp'),'w') as f:f.write('WORKBENCH_PLUGIN=1\nformat=1\nid=plugin.preview\nversion=3\napi_min=3\napi_max=3\nname_zh=预览\nname_en=Preview\ndescription_zh=x\ndescription_en=x\nentry=bin/run\npreview_providers=preview-providers\n')
    run=os.path.join(root,'bin','run')
    listing=f"E\\tD\\t0\\t{hx('folder')}\\nE\\tF\\t7\\t{hx('folder/inside.txt')}\\nE\\tF\\t5\\t{hx('root.txt')}\\n"
    with open(run,'w') as f:
        f.write('#!/bin/sh\ncase "$1" in\n')
        f.write(f" --workbench-preview-list) printf '{listing}' ;;\n")
        f.write(' --workbench-preview-materialize) printf "inside\\n" > "$6" ;;\n')
        f.write(' *) exit 2 ;;\nesac\n')
    os.chmod(run,0o700)
    with open(os.path.join(root,'preview-providers','archive.wbp'),'w') as f:f.write('WORKBENCH_FILE_PREVIEW=1\nformat=1\nid=archive\nname_zh=归档预览\nname_en=Archive Preview\nentry=bin/run\nextensions=7z\n')
    archive=os.path.join(home,'sample.7z');open(archive,'wb').write(b'x')
    mountinfo=os.path.join('/tmp',f'wb-preview-mountinfo-{os.getpid()}');open(mountinfo,'w').write('24 1 0:1 / / rw,relatime - overlay overlay rw\n')
    pid,fd=pty.fork()
    if pid==0:
        env=os.environ.copy();env.update(HOME=home,LANG='C',LC_ALL='C',WB_MOUNTINFO=mountinfo);os.execve(binary,[binary],env)
    fcntl.ioctl(fd,termios.TIOCSWINSZ,struct.pack('HHHH',36,118,0,0));data=bytearray()
    try:
        read_until(fd,data,b'Command Sets');os.write(fd,b'\r');read_until(fd,data,b'Locations');os.write(fd,b'\r');read_until(fd,data,b'sample.7z')
        data.clear();os.write(fd,b'j\r');read_until(fd,data,b'Archive Preview (read-only)');read_until(fd,data,b'folder');read_until(fd,data,b'root.txt')
        data.clear();os.write(fd,b'\r');read_until(fd,data,b'inside.txt')
        data.clear();os.write(fd,b'jp');read_until(fd,data,b'Archive item properties');read_until(fd,data,b'folder/inside.txt');os.write(fd,b'q');read_until(fd,data,b'Archive Preview (read-only)')
        data.clear();os.write(fd,b'v');read_until(fd,data,b'View file');read_until(fd,data,b'inside');os.write(fd,b'q');read_until(fd,data,b'Archive Preview (read-only)')
        os.write(fd,b'\x1b[D');time.sleep(.2);os.write(fd,b'q');read_until(fd,data,b'Current path');os.write(fd,b'q');read_until(fd,data,b'Desktop');os.write(fd,b'q')
        end=time.time()+3
        while time.time()<end:
            got,st=os.waitpid(pid,os.WNOHANG)
            if got==pid:
                if not os.WIFEXITED(st) or os.WEXITSTATUS(st)!=0:raise SystemExit(f'abnormal {st}')
                break
            time.sleep(.05)
        else: os.kill(pid,15);os.waitpid(pid,0);raise SystemExit('preview UI did not exit')
    finally:
        try:os.close(fd)
        except OSError:pass
        try:os.unlink(mountinfo)
        except OSError:pass
print('FILES PLUGIN PREVIEW UI PASS')
