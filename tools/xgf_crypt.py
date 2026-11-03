#!/usr/bin/env python3
"""Decrypt/encrypt Xiegu .xgf images with AES-256-ECB (public G90Tools key).

The key is supplied explicitly; this tool does not embed or distribute keys.
Usage:
  xgf_crypt.py decrypt KEY HEX INPUT OUTPUT
  xgf_crypt.py encrypt KEY HEX INPUT OUTPUT
No padding is added/removed; input length must be a multiple of 16.
"""
import sys, subprocess, tempfile, os, hashlib

def run(kind, key, inp, outp):
    if len(key) != 64:
        raise SystemExit("key must be 64 hex chars (AES-256)")
    data = open(inp, "rb").read()
    if len(data) % 16:
        raise SystemExit("length not multiple of 16")
    cmd = ["openssl", "enc", "-aes-256-ecb", "-nopad",
           "-d" if kind == "decrypt" else "-e", "-K", key,
           "-in", inp, "-out", outp]
    subprocess.run(cmd, check=True)
    print(kind, inp, "->", outp, "sha256", hashlib.sha256(open(outp,"rb").read()).hexdigest())

if __name__ == "__main__":
    if len(sys.argv) != 6 or sys.argv[1] not in ("decrypt","encrypt"):
        raise SystemExit(__doc__)
    run(sys.argv[1], sys.argv[2], sys.argv[4], sys.argv[5])
