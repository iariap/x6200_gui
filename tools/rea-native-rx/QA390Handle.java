import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.util.*;
public class QA390Handle extends GhidraScript {
    DecompInterface di;
    long[] lits = {0x08022820L,0x08023144L,0x08023b00L,0x08023c60L,0x0803eec4L,0x0803ef74L,0x080429c8L};
    Address a(long v){ return toAddr(v & 0xfffffffeL); }
    Function fn(Address x){
        Function f=getFunctionContaining(x);
        if(f==null){ new CreateFunctionCmd(x).applyTo(currentProgram, monitor); f=getFunctionAt(x); }
        return f;
    }
    String nf(Function f){ return f==null?"?":(f.getEntryPoint()+" "+f.getName()); }
    public void run() throws Exception {
        di=new DecompInterface(); di.openProgram(currentProgram);
        Set<Function> fs=new LinkedHashSet<>();
        for(long l: lits){
            println("=== literal 0x"+Long.toHexString(l)+" ===");
            for(Reference r: currentProgram.getReferenceManager().getReferencesTo(toAddr(l))){
                Function cf=getFunctionContaining(r.getFromAddress());
                println("  from="+r.getFromAddress()+" func="+nf(cf)+" type="+r.getReferenceType());
                if(cf!=null) fs.add(cf);
            }
        }
        for(Function f: fs){
            println("\n##### "+f.getEntryPoint()+" "+f.getName());
            DecompileResults rr=di.decompileFunction(f,180,monitor);
            if(rr!=null && rr.getDecompiledFunction()!=null) println("PSEUDO:\n"+rr.getDecompiledFunction().getC());
            else println("DECOMP FAILED");
        }
        println("=== FIN QA390Handle ===");
    }
}
