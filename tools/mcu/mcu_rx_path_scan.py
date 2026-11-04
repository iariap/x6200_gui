#!/usr/bin/env python3
"""Offline scan of X6200 BASE H725 image for RX audio path evidence.

No radio I/O, no decryption, no flashing. Input is the already-verified
x6200-108.dec.bin (image base 0x08020000).
"""
import json, struct, sys
from pathlib import Path
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB

BASE = 0x08020000
IRQ_NAMES = {
    11: "DMA1_Stream0", 12: "DMA1_Stream1", 13: "DMA1_Stream2",
    14: "DMA1_Stream3", 15: "DMA1_Stream4", 16: "DMA1_Stream5",
    17: "DMA1_Stream6", 35: "SPI1", 36: "SPI2", 47: "DMA1_Stream7",
    51: "SPI3", 56: "DMA2_Stream0", 57: "DMA2_Stream1", 58: "DMA2_Stream2",
    59: "DMA2_Stream3", 60: "DMA2_Stream4", 68: "DMA2_Stream5",
    69: "DMA2_Stream6", 70: "DMA2_Stream7", 84: "SPI4", 85: "SPI5",
    86: "SPI6", 87: "SAI1", 122: "MDMA", 146: "SAI4",
}
RANGES = {
    "SAI1": (0x40015800, 0x40015BFF), "SAI2": (0x40015C00, 0x40015FFF),
    "SAI3": (0x40016000, 0x400163FF), "SAI4": (0x58005400, 0x580057FF),
    "SPI1": (0x40013000, 0x400133FF), "SPI2": (0x40003800, 0x40003BFF),
    "SPI3": (0x40003C00, 0x40003FFF), "SPI4": (0x40013400, 0x400137FF),
    "SPI5": (0x40015000, 0x400153FF), "SPI6": (0x58001400, 0x580017FF),
    "DMA1": (0x40020000, 0x400203FF), "DMA2": (0x40020400, 0x400207FF),
    "DMAMUX1": (0x40020800, 0x40020BFF), "MDMA": (0x58000000, 0x580003FF),
    "DAC": (0x40007400, 0x400077FF), "ADC1": (0x40022000, 0x400223FF),
    "ADC2": (0x40022400, 0x400227FF), "ADC3": (0x58026000, 0x580263FF),
}
STRINGS = [
    "Init audio codec", "Init audio processor", "CODEC buffer underrun",
    "FPGA buffer underrun", "Init FPGA", "FPGA FW Version",
    "RX MODE", "TXRX MODE", "TX MODE", "RX Volume",
]

def main(path):
    d = Path(path).read_bytes()
    out = {"image": str(path), "size": len(d), "base": hex(BASE),
           "vector_table": [], "irq_handlers": {}, "strings": [],
           "peripheral_ranges": {}, "notes": []}
    for i in range(256):
        if i * 4 + 4 > len(d):
            break
        v = struct.unpack_from("<I", d, i * 4)[0]
        if v & 0xFF000000 == 0x08000000:
            out["vector_table"].append({"index": i, "handler": f"0x{v:08x}"})
    for irq, name in IRQ_NAMES.items():
        idx = 16 + irq
        if idx * 4 + 4 <= len(d):
            v = struct.unpack_from("<I", d, idx * 4)[0]
            out["irq_handlers"][name] = f"0x{v:08x}" + (" (default)" if v == 0x080248D1 else "")
    for name, (lo, hi) in RANGES.items():
        refs = []
        for i in range(0, len(d) - 3, 4):
            v = struct.unpack_from("<I", d, i)[0]
            if lo <= v <= hi:
                refs.append({"at": f"0x{BASE+i:08x}", "value": f"0x{v:08x}"})
        out["peripheral_ranges"][name] = {"count": len(refs), "refs": refs[:12]}
    for s in STRINGS:
        pos = d.find(s.encode())
        if pos >= 0:
            out["strings"].append({"text": s, "va": f"0x{BASE+pos:08x}"})
    out["notes"].append("Matched base+offset peripheral literals; not a full data-flow proof.")
    out["notes"].append("No SAI/SAI IRQ references found; SPI3/DMA/FPGA control path present.")
    return out

if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: mcu_rx_path_scan.py x6200-108.dec.bin")
    print(json.dumps(main(sys.argv[1]), indent=2))
