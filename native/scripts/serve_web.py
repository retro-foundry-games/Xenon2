#!/usr/bin/env python3
"""Serve the browser build: python3 serve_web.py [DIR [PORT]]   (default: the folder this script is in, 8080)

`nix build .#web` puts this script next to index.html, xenon2.js and xenon2.wasm in `result`."""
import functools
import http.server
import pathlib
import sys

folder = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else pathlib.Path(__file__).resolve().parent)
port = int(sys.argv[2]) if len(sys.argv) > 2 else 8080
if not (folder / "index.html").exists():
    sys.exit(f"{folder}/index.html is not there: serve the folder `nix build .#web` makes (`result`)")
http.server.SimpleHTTPRequestHandler.extensions_map[".wasm"] = "application/wasm"
print(f"http://localhost:{port}/  ({folder})")
http.server.ThreadingHTTPServer(("", port), functools.partial(http.server.SimpleHTTPRequestHandler, directory=str(folder))).serve_forever()
