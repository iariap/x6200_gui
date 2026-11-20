// QA390: quien referencia los literales de interes (DAC 0x40007400 y buffers) + decompilacion
import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;

public class QA390Lit extends GhidraScript {
    DecompInterface di;
    long[] lits = {0x08021090L,0x0802110cL,0x08021138L,   // pools con 0x40007400
                   0x08031954L,0x080321f8L,0x08032518L,0x080354b8L,0x08035c88L,0x08035d10L, // pools con 0x30000C00
                   0x08035d14L,0x08023f2dL,0x08031c38L,0x08033d08L}; // pools con 0x30000000
    Address a(long v){ return toAddr(v & 0xfffffffeL); }
    Function fn(Address x){
        Function f=getFunctionContaining(x);
        if(f==null){ new CreateFunctionCmd(x).applyTo(currentProgram, monitor); f=getFunctionAt(x); }
        return f;
    }
    String nf(Function f){ return f==null?"?":(f.getEntryPoint()+" "+f.getName()); }
    public void run() throws Exception {
        di=new DecompInterface(); di.openProgram(currentProgram);
        java.util.Set<Function> fset=new java.util.LinkedHashSet<>();
        for(long l: lits){
            println("=== literal 0x"+Long.toHexString(l)+" ===");
            for(Reference r: currentProgram.getReferenceManager().getReferencesTo(toAddr(l))){
                Function cf=getFunctionContaining(r.getFromAddress());
                println("  from="+r.getFromAddress()+" func="+nf(cf)+" type="+r.getReferenceType());
                if(cf!=null) fset.add(cf);
            }
        }
        println("=== DECOMPILACION REFERENTES ===");
        for(Function f: fset){
            println("\n##### "+f.getEntryPoint()+" "+f.getName());
            DecompileResults rr=di.decompileFunction(f,200,monitor);
            if(rr!=null && rr.getDecompiledFunction()!=null) println("PSEUDO:\n"+rr.getDecompiledFunction().getC());
            else println("DECOMP FAILED");
        }
        println("=== FIN QA390Lit ===");
    }
}
