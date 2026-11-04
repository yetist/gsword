#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Run the HTTP install integration test against a temporary repository."""

import http.server
import os
import shutil
import socketserver
import subprocess
import sys
import tarfile
import tempfile
import threading


def build_repo(base):
    repo = os.path.join(base, "repo")
    mods = os.path.join(repo, "mods.d")
    data = os.path.join(repo, "modules", "texts", "rawtext", "tiny")
    os.makedirs(mods, exist_ok=True)
    os.makedirs(data, exist_ok=True)

    conf = (
        "[Tiny]\n"
        "Description=Tiny module\n"
        "DataPath=./modules/texts/rawtext/tiny/\n"
        "ModDrv=RawText\n"
        "Encoding=UTF-8\n"
        "Version=1.0\n"
        "InstallSize=4\n"
    )
    with open(os.path.join(mods, "tiny.conf"), "w", encoding="utf-8") as f:
        f.write(conf)

    with open(os.path.join(data, "ot"), "wb") as f:
        f.write(b"test")
    with open(os.path.join(data, "ot.vss"), "wb") as f:
        f.write(b"\x00\x00\x00\x00\x04\x00")

    with tarfile.open(os.path.join(repo, "mods.d.tar.gz"), "w:gz") as tar:
        tar.add(mods, arcname="mods.d")

    return repo


class Handler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, fmt, *args):
        pass


def main():
    if len(sys.argv) not in (2, 3):
        print("usage: run-http-install-test.py <sync-binary> [async-binary]",
              file=sys.stderr)
        return 2

    sync_binary = sys.argv[1]
    async_binary = sys.argv[2] if len(sys.argv) == 3 else None
    base = tempfile.mkdtemp(prefix="gsword-http-")
    try:
        repo = build_repo(base)
        private = os.path.join(base, "private")
        dest = os.path.join(base, "dest")
        os.makedirs(private)
        os.makedirs(dest)

        handler = lambda *args, **kwargs: Handler(*args, directory=repo, **kwargs)
        with socketserver.TCPServer(("127.0.0.1", 0), handler) as httpd:
            port = httpd.server_address[1]
            thread = threading.Thread(target=httpd.serve_forever, daemon=True)
            thread.start()
            try:
                result = subprocess.run(
                    [sync_binary, private, dest, f"127.0.0.1:{port}", "/"],
                    text=True,
                )
                if result.returncode != 0:
                    return result.returncode

                if async_binary is not None:
                    private2 = os.path.join(base, "private-async")
                    dest2 = os.path.join(base, "dest-async")
                    os.makedirs(private2)
                    os.makedirs(dest2)
                    result = subprocess.run(
                        [async_binary, private2, dest2,
                         f"127.0.0.1:{port}", "/"],
                        text=True,
                    )
                    if result.returncode != 0:
                        return result.returncode
            finally:
                httpd.shutdown()
                thread.join(timeout=5)
        return 0
    finally:
        shutil.rmtree(base, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
