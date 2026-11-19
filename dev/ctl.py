#!/usr/bin/env python3
"""A client of the dev build's control socket (xenon2-sdl-dev --control PATH).

    dev/ctl.py PATH [-o PREFIX] COMMAND [COMMAND ...]

Each COMMAND is one protocol line, e.g. "step" "sync 0x2034 3" "peek16 0x41c" "ram" (the protocol is in dev/src/dev.c).  The reply line is printed;
the bytes behind `ram` go to PREFIX.ram, a picture to PREFIX.ppm (default: only their sizes are reported).
"""
import socket
import sys


def main():
    args = sys.argv[1:]
    prefix = None
    if len(args) >= 3 and args[1] == '-o':
        prefix = args[2]
        args[1:3] = []
    if len(args) < 2:
        raise SystemExit(__doc__)
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.connect(args[0])
    f = sock.makefile('rwb')
    for command in args[1:]:
        f.write(command.encode() + b'\n')
        f.flush()
        reply = f.readline().decode().rstrip('\n')
        print(reply)
        words = reply.split()
        data = None
        if words[:1] == ['ram']:
            data = f.read(int(words[1]))
            suffix = '.ram'
        elif words[:1] == ['frame'] and len(words) >= 4:
            w, h = int(words[2]), int(words[3])
            data = f.read(w * h * 3)
            suffix = '.ppm'
            if prefix:
                data = b'P6\n%d %d\n255\n' % (w, h) + data
        if data is not None:
            print(f'  {len(data)} bytes')
            if prefix:
                with open(prefix + suffix, 'wb') as out:
                    out.write(data)


main()
