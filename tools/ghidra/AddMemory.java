import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
public class AddMemory extends GhidraScript {
    void add(String n,long s,long z) throws Exception {
        Address a=toAddr(s);
        if(currentProgram.getMemory().getBlock(a)==null){
            currentProgram.getMemory().createUninitializedBlock(n,a,z,false);
            println("added "+n+" "+a+" size 0x"+Long.toHexString(z));
        } else println("exists "+n+" "+a);
    }
    public void run() throws Exception { add("AXI_SRAM",0x24000000L,0x80000L); add("SRAM1",0x30000000L,0x20000L); add("SRAM2",0x30020000L,0x20000L); add("PERIPH",0x40000000L,0x20000000L); }
}
