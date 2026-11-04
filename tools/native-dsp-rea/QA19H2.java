import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import java.util.*;
public class QA19H2 extends GhidraScript {
    DecompInterface di;
    long[] cands={0x8042a26L,0x8042a60L,0x8042a9aL,0x8042ad4L,0x8042b0eL,0x8042b8cL,0x8042bc6L,0x8042c00L,0x8042c3aL,0x8042c74L,0x8042caeL,0x8042cf6L,0x8042d30L,0x8042d6aL,0x8042da4L,0x8042ddeL,0x8042e18L,0x8042eacL};
    Address a(long v){ return toAddr(v & 0xfffffffeL); }
    public void run() throws Exception {
        di=new DecompInterface(); di.openProgram(currentProgram);
        for(long c: cands){
            Address x=a(c);
            Function f=getFunctionContaining(x);
            if(f==null){ new CreateFunctionCmd(x).applyTo(currentProgram, monitor); f=getFunctionAt(x); }
            println("\n##### 0x"+Long.toHexString(c)+" -> "+(f==null?"NOFUNC":f.getEntryPoint()+" "+f.getName()));
            if(f==null) continue;
            DecompileResults rr=di.decompileFunction(f,180,monitor);
            if(rr!=null && rr.getDecompiledFunction()!=null) println("PSEUDO:\n"+rr.getDecompiledFunction().getC());
        }
        println("=== FIN QA19H2 ===");
    }
}
