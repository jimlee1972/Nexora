#!/usr/bin/env python3
"""Isolated native X server regression for queries after external destruction; no GPU required."""
import argparse
import os
from pathlib import Path
import select
import shutil
import subprocess

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--probe',required=True)
    parser.add_argument('--xvfb')
    parser.add_argument('--evidence-dir',type=Path,required=True)
    args=parser.parse_args()
    xvfb=args.xvfb or shutil.which('Xvfb')
    if not xvfb:
        print('SKIP: Xvfb is unavailable')
        return 77
    args.evidence_dir.mkdir(parents=True,exist_ok=True)
    read_fd,write_fd=os.pipe()
    server=None
    try:
        with (args.evidence_dir/'xvfb.log').open('wb') as log:
            server=subprocess.Popen([xvfb,'-screen','0','1024x768x24','-nolisten','tcp','-noreset',
                                     '-ac','-displayfd',str(write_fd)],pass_fds=(write_fd,),stdout=log,stderr=log)
            os.close(write_fd);write_fd=-1
            if not select.select([read_fd],[],[],10)[0]:
                raise RuntimeError('Xvfb failed to publish a display number')
            display=os.read(read_fd,64).decode().strip()
            if not display.isdigit():
                raise RuntimeError('invalid Xvfb display number')
            env=os.environ.copy();env['DISPLAY']=':'+display
            result=subprocess.run([args.probe],env=env,capture_output=True,text=True,timeout=15)
            (args.evidence_dir/'probe.log').write_text(result.stdout+result.stderr)
            print(result.stdout+result.stderr,end='')
            return result.returncode
    finally:
        if server is not None:
            server.terminate();server.wait(timeout=5)
        os.close(read_fd)
        if write_fd!=-1:os.close(write_fd)
if __name__=='__main__':
    raise SystemExit(main())
