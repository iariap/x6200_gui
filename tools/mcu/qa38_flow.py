#!/usr/bin/env python3
"""QA38 flow checks on X6200 BASE 1.0.8 (offline, no radio I/O)."""
import json, struct, sys
from pathlib import Path
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB

BASE=0x08020000
def main(path):
    d=Path(path).read_bytes(); md=Cs(CS_ARCH_ARM,CS_MODE_THUMB); out={}
    def ins_at(addr, n=12):
        return list(md.disasm(d[addr-BASE:addr-BASE+n],addr))[:6]
    # SPI3 I2SCFGR write: orr #1 then str [r2,#0x50]
    out['i2scfgr_site']=[{'addr':f'0x{x.address:08x}','asm':f'{x.mnemonic} {x.op_str}'} for x in ins_at(0x0802b1d0,16)]
    # DMA handles/requests in SPI3 init
    out['dma_request_sites']={}
    for addr in (0x08021b74,0x08021bbc):
        out['dma_request_sites'][f'0x{addr:08x}']=[f'{x.mnemonic} {x.op_str}' for x in ins_at(addr,12)]
    # SRAM buffers in 0x8035b40
    out['sram_buffer_sites']={}
    for addr in (0x08035b48,0x08035b56,0x08035bd4,0x08035bfe):
        out['sram_buffer_sites'][f'0x{addr:08x}']=[f'{x.mnemonic} {x.op_str}' for x in ins_at(addr,8)]
    out['state_struct']='0x24005a38 (from 0x08035b9a)'
    for s in ['CODEC buffer underrun','FPGA buffer underrun']:
        pos=d.find(s.encode()); out[s]={'va':f'0x{BASE+pos:08x}' if pos>=0 else None}
    out['notes']=['SPI3+0x50 is I2SCFGR in CMSIS H725; I2SMOD=bit0.',
                  '0x30000000/0x30000C00 are SRAM1 buffers, not FPGA memory.']
    return out
if __name__=='__main__':
    print(json.dumps(main(sys.argv[1]),indent=2))
