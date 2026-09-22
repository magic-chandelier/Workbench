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
        raise SystemExit(f"missing {needle!r}\n{bytes(data[-5000:])!r}")

def mouse(fd,button,x,y):
    os.write(fd,f"\x1b[<{button};{x};{y}M".encode())

binary=os.path.abspath('./wb')
with tempfile.TemporaryDirectory(prefix='wb-file-action-ui-') as home:
    os.makedirs(os.path.join(home,'.config','workbench'),exist_ok=True)
    with open(os.path.join(home,'.config','workbench','config'),'w') as f:
        f.write('language=en\nconfirm_normal=0\nconfirm_sensitive=0\nconfirm_privileged=1\nlinux_profile=generic\n')
    root=os.path.join(home,'.local','share','workbench','plugins','plugin.archive')
    os.makedirs(os.path.join(root,'bin'),exist_ok=True);os.makedirs(os.path.join(root,'file-actions'),exist_ok=True)
    with open(os.path.join(root,'plugin.wbp'),'w') as f:
        f.write('WORKBENCH_PLUGIN=1\nformat=1\nid=plugin.archive\nversion=2\napi_min=2\napi_max=2\nname_zh=测试归档\nname_en=Test Archive\ndescription_zh=x\ndescription_en=x\nentry=bin/run\nfile_actions=file-actions\n')
    run=os.path.join(root,'bin','run')
    with open(run,'w') as f:f.write('#!/bin/sh\nprintf "%s\\n" "$@" > "$HOME/file-action.log"\n')
    os.chmod(run,0o700)
    with open(os.path.join(root,'file-actions','test.wba'),'w') as f:
        f.write('WORKBENCH_FILE_ACTION=1\nformat=1\nid=test-action\nname_zh=测试插件操作\nname_en=Test plugin action\nentry=bin/run\ntarget=file\nextensions=7z\n')
    selected=os.path.join(home,'sample with spaces.7z')
    with open(selected,'w') as f:f.write('not an archive')
    mountinfo=os.path.join('/tmp',f'wb-mountinfo-{os.getpid()}')
    with open(mountinfo,'w') as f:f.write('24 1 0:1 / / rw,relatime - overlay overlay rw\n')
    pid,fd=pty.fork()
    if pid==0:
        env=os.environ.copy();env.update(HOME=home,LANG='C',LC_ALL='C',WB_MOUNTINFO=mountinfo)
        os.execve(binary,[binary],env)
    fcntl.ioctl(fd,termios.TIOCSWINSZ,struct.pack('HHHH',36,118,0,0));data=bytearray()
    try:
        read_until(fd,data,b'Command Sets')
        os.write(fd,b'\r');read_until(fd,data,b'Locations')
        os.write(fd,b'\r');read_until(fd,data,b'Current path')
        read_until(fd,data,b'sample with spaces.7z')
        # Parent is row 8, sample file row 9. Right click must open the generic context menu.
        data.clear();mouse(fd,2,12,9);read_until(fd,data,b'File actions');read_until(fd,data,b'Test plugin action')
        # Seven built-ins precede the plugin contribution.
        os.write(fd,b'\x1b[B'*7+b'\r');read_until(fd,data,b'Plugin action finished (exit code 0).')
        os.write(fd,b'\r');read_until(fd,data,b'Current path')
        log=os.path.join(home,'file-action.log')
        end=time.time()+2
        while time.time()<end and not os.path.exists(log):time.sleep(.05)
        if not os.path.exists(log):raise SystemExit('plugin action did not create argv log')
        with open(log) as f:lines=f.read().splitlines()
        expected=['--workbench-file-action','test-action','--',selected]
        if lines!=expected:raise SystemExit(f'wrong plugin argv: {lines!r} != {expected!r}')
        os.write(fd,b'q');read_until(fd,data,b'Desktop');os.write(fd,b'q')
        end=time.time()+3
        while time.time()<end:
            got,st=os.waitpid(pid,os.WNOHANG)
            if got==pid:
                if not os.WIFEXITED(st) or os.WEXITSTATUS(st)!=0:raise SystemExit(f'abnormal {st}')
                break
            time.sleep(.05)
        else:
            os.kill(pid,15);os.waitpid(pid,0);raise SystemExit('file action UI flow did not exit')
    finally:
        try:os.close(fd)
        except OSError:pass
        try:os.unlink(mountinfo)
        except OSError:pass
print('FILES PLUGIN ACTION UI PASS')
