// QA390: decompilacion selectiva + xrefs/callers de funciones del MCU 1.0.8 (#390)
import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.Reference;

public class QA390Dump extends GhidraScript {
    long[] targets = {
        0x08021090L,0x0802110cL,0x08021138L,           // DAC 0x40007400 refs
        0x08045320L,0x080450c8L,0x08043840L,0x080439b4L,0x080432a8L,0x08044470L, // voice_rec/switch/helpers/FPGA write
        0x08021aacL,0x0802b0e0L,0x08035b40L,           // I2S3/DMA init, I2SCFGR, buffers
        0x080319e0L,0x08039250L,0x08039e88L,           // RX consumer/copy known
        0x08035ad4L,0x0803d1e8L,0x0803d978L,0x0803c540L,
        0x08024881L,0x08024880L                        // reset handler
    };
    long[] addrs = {0x40007400L,0x40007408L,0x4000740cL,0x40007410L,0x30000c00L,0x30000000L,0x40003c00L,0x40003c20L,0x40003c30L,0x40003c50L,0x4000740cL};

    Address a(long v){ return toAddr(v & 0xfffffffeL); }
    Function fn(Address x){
        Function f=getFunctionContaining(x);
        if(f==null){ new CreateFunctionCmd(x).applyTo(currentProgram, monitor); f=getFunctionAt(x); }
        return f;
    }
    public void run() throws Exception {
        DecompInterface di=new DecompInterface();
        di.openProgram(currentProgram);
        println("=== XREFS A DIRECCIONES CLAVE ===");
        for(long v: addrs){
            println("--- addr 0x"+Long.toHexString(v));
            for(Reference r: currentProgram.getReferenceManager().getReferencesTo(toAddr(v))){
                Function cf=getFunctionContaining(r.getFromAddress());
                println("  from="+r.getFromAddress()+" func="+(cf==null?"?":cf.getEntryPoint()+" "+cf.getName())+" type="+r.getReferenceType());
            }
        }
        println("=== FUNCIONES OBJETIVO: callers / callees / pseudocodigo ===");
        for(long t: targets){
            Address x=a(t);
            Function f=getFunctionContaining(x);
            println("\n##### 0x"+Long.toHexString(t)+" containing="+(f==null?"NONE":f.getEntryPoint()+" "+f.getName()));
            if(f==null){ new CreateFunctionCmd(x).applyTo(currentProgram, monitor); f=getFunctionAt(x); println("  created="+(f==null?"fail":f.getName())); }
            if(f==null) continue;
            println("callers:");
            for(Reference r: currentProgram.getReferenceManager().getReferencesTo(f.getEntryPoint())){
                Function cf=getFunctionContaining(r.getFromAddress());
                println("  "+r.getFromAddress()+" "+(cf==null?"?":cf.getEntryPoint()+" "+cf.getName())+" "+r.getReferenceType());
            }
            println("callees:");
            for(Function c: f.getCalledFunctions(monitor)) println("  "+c.getEntryPoint()+" "+c.getName());
            DecompileResults rr=di.decompileFunction(f,180,monitor);
            if(rr!=null && rr.getDecompiledFunction()!=null) println("PSEUDO:\n"+rr.getDecompiledFunction().getC());
            else println("DECOMP FAILED "+(rr==null?"null":rr.getErrorMessage()));
        }
        println("=== FIN QA390Dump ===");
    }
}
