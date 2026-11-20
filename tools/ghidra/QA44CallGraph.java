import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import java.util.*;

public class QA44CallGraph extends GhidraScript {
    void show(long v) throws Exception {
        Address a=toAddr(v);
        Function f=getFunctionAt(a);
        println("=== 0x"+Long.toHexString(v)+" "+(f==null?"NO_FUNCTION":f.getName())+" ===");
        if(f==null) return;
        println("callers:");
        for(Reference r: currentProgram.getReferenceManager().getReferencesTo(f.getEntryPoint())) {
            Function cf=getFunctionContaining(r.getFromAddress());
            println("  "+r.getFromAddress()+" "+(cf==null?"?":cf.getName())+" "+r.getReferenceType());
        }
        println("callees:");
        for(Function c: f.getCalledFunctions(monitor)) println("  "+c.getEntryPoint()+" "+c.getName());
    }
    public void run() throws Exception {
        long[] ts={0x080319e0L,0x0803198aL,0x080339f0L,0x08039250L,0x08039a5cL,
            0x0803d1e8L,0x0803d978L,0x08035b40L,0x08035ad4L,0x0802b0e0L};
        for(long t:ts) show(t);
        println("=== BUFFER XREFS ===");
        for(long x: new long[]{0x30000000L,0x30000c00L,0x24005a38L,0x24005dc4L,0x24005d4cL}) {
            println("addr 0x"+Long.toHexString(x));
            for(Reference r: currentProgram.getReferenceManager().getReferencesTo(toAddr(x))) {
                Function cf=getFunctionContaining(r.getFromAddress());
                println("  "+r.getFromAddress()+" "+(cf==null?"?":cf.getName())+" "+r.getReferenceType());
            }
        }
    }
}
