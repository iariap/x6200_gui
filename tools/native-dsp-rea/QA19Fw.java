import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.util.*;

public class QA19Fw extends GhidraScript {
    DecompInterface di;
    long[] tg = {0x080444d4L,0x080444b0L,0x08044470L};
    Address a(long v){ return toAddr(v & 0xfffffffeL); }
    String nf(Function f){ return f==null?"?":(f.getEntryPoint()+" "+f.getName()); }
    public void run() throws Exception {
        di=new DecompInterface(); di.openProgram(currentProgram);
        Set<Long> tset=new HashSet<>(); for(long t:tg) tset.add(t & 0xfffffffeL);
        Set<Function> fset=new LinkedHashSet<>();
        println("=== CALLERS DE WRAPPERS FPGA ===");
        for(Instruction ins: currentProgram.getListing().getInstructions(true)){
            for(Reference r: ins.getReferencesFrom()){
                Address to=r.getToAddress(); if(to==null) continue;
                long v=to.getOffset()&0xfffffffeL;
                if(tset.contains(v)){
                    Function cf=getFunctionContaining(ins.getAddress());
                    println("CALL reg="+Long.toHexString(v)+" from="+ins.getAddress()+" caller="+nf(cf));
                    if(cf!=null) fset.add(cf);
                }
            }
        }
        println("=== DECOMPILACION CALLERS ("+fset.size()+") ===");
        for(Function f: fset){
            println("\n##### "+f.getEntryPoint()+" "+f.getName());
            DecompileResults rr=di.decompileFunction(f,200,monitor);
            if(rr!=null && rr.getDecompiledFunction()!=null) println("PSEUDO:\n"+rr.getDecompiledFunction().getC());
        }
        println("=== FIN QA19Fw ===");
    }
}
