#!/usr/bin/env python3
# Copyright 2026 FCC Project at CERN. SPDX-License-Identifier: Apache-2.0
"""Interactive fccvis-shell on a pseudo-terminal: completion, history,
continuation lines, and asynchronous backend messages printed above the
prompt. Usage: test_interactive.py <fccvis-shell> <work-dir>"""
import os
import pty
import select
import shutil
import sys
import time

shell, work = sys.argv[1], sys.argv[2]
shutil.rmtree(work, ignore_errors=True)
os.makedirs(os.path.join(work, "out", "a.png"))  # a directory where an image should go
history = os.path.join(work, "history")

pid, fd = pty.fork()
if pid == 0:
    os.chdir(work)
    env = dict(os.environ, TERM="xterm", FCCVIS_HISTORY=history)
    os.execve(shell, [shell], env)

buffer = b""


def read_until(token, timeout=10.0):
    global buffer
    end = time.time() + timeout
    while token.encode() not in buffer:
        left = end - time.time()
        if left <= 0:
            raise SystemExit("timeout waiting for %r; got:\n%r" % (token, buffer[-2000:]))
        r, _, _ = select.select([fd], [], [], left)
        if r:
            try:
                data = os.read(fd, 4096)
            except OSError:
                break
            if not data:
                break
            buffer += data
    i = buffer.index(token.encode()) + len(token)
    seen, buffer = buffer[:i], buffer[i:]
    return seen.decode(errors="replace")


def send(text):
    os.write(fd, text.encode())


def fail(message):
    print("FAIL:", message)
    try:
        os.kill(pid, 9)
    except ProcessLookupError:  # the shell already exited
        pass
    sys.exit(1)


marks = [0]


PROMPT = "\x1b[?2004hfccvis> "  # a fresh prompt (linenoise enables bracketed paste when it starts editing)


def command(text):
    """Types a command, waits for the next fresh prompt; returns the output in between."""
    send(text + "\r")
    return read_until(PROMPT, timeout=20)


read_until(PROMPT)
# Tab completes the command name; typing on accepts the completion.
send("sce\t")
read_until("scene ")
out = command("new a b")
out = command("scene ls")
if "\na\r\nb\r\n" not in out:
    fail("scene ls output: %r" % out)
# Continuation line
send("scene \\\r")
read_until("     .. ")
command("new c")
out = command("scene ls")
if "\nc\r\n" not in out:
    fail("continuation line not joined: %r" % out)
# Asynchronous message from the backend's render thread while the prompt waits
command("backend init raster r output_dir=out")
send("open r a\r")
out = read_until("cannot write", timeout=20)
out += read_until("fccvis> ")  # the prompt is drawn again below the message
if "error: r: cannot write" not in out:
    fail("asynchronous message missing: %r" % out)
send("quit\r")
os.waitpid(pid, 0)
with open(history) as h:
    lines = h.read().splitlines()
if "scene ls" not in lines or "scene new c" not in lines:  # continuation joined with one blank
    fail("history file: %r" % lines)
print("ok")
