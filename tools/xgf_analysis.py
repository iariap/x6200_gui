#!/usr/bin/env python3
"""Offline structural analysis of Xiegu .xgf images. No decryption, no radio I/O.

Usage: xgf_analysis.py FILE [FILE...]
Outputs JSON to stdout with hashes, entropy, block-repeat and differential data.
"""
import sys, json, hashlib, math, collections, zlib, gzip, bz2, lzma

def entropy(data):
    if not data: return 0.0
    c = collections.Counter(data)
    n = len(data)
    return -sum(v/n * math.log2(v/n) for v in c.values())

def block_stats(data, size):
    blocks = [data[i:i+size] for i in range(0, len(data), size)]
    ents = [entropy(b) for b in blocks]
    return {"size": size, "count": len(blocks), "min": round(min(ents),3),
            "avg": round(sum(ents)/len(ents),3), "max": round(max(ents),3)}

def compress_tests(data):
    out = {}
    for name, fn in (("zlib", zlib.decompress), ("gzip", gzip.decompress),
                     ("bz2", bz2.decompress), ("lzma", lzma.decompress)):
        try:
            d = fn(data); out[name] = {"ok": True, "len": len(d)}
        except Exception as e:
            out[name] = {"ok": False, "error": type(e).__name__}
    return out

def analyze(path):
    data = open(path, "rb").read()
    n = len(data)
    blocks16 = [data[i:i+16] for i in range(0, n, 16)]
    c16 = collections.Counter(blocks16)
    low512 = []
    for off in range(0, n, 512):
        e = entropy(data[off:off+512])
        if e < 5.0:
            low512.append({"off": off, "entropy": round(e,3),
                           "unique16": len(set(data[off+i:off+i+16] for i in range(0,512,16)))})
    return {
        "path": path, "size": n, "sha256": hashlib.sha256(data).hexdigest(),
        "entropy": round(entropy(data),4),
        "zeros": data.count(0), "ff": data.count(255),
        "ascii_pct": round(sum(32 <= x < 127 for x in data)/n,4),
        "blocks": [block_stats(data, s) for s in (128,256,512,1024,4096)],
        "top16": [{"hex": b.hex(), "count": c} for b,c in c16.most_common(8)],
        "low512": low512[:40],
        "low512_count": len(low512),
        "compression": compress_tests(data),
    }

if __name__ == "__main__":
    print(json.dumps([analyze(p) for p in sys.argv[1:]], indent=2))
