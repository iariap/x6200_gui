// QA390: funciones DAC (MSP init/deinit) y wrappers FPGA + callers
import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;

public class QA390Dac extends GhidraScript {
    DecompInterface di;
    long[] starts = {0x08021020L,0x08021098L,0x08021118L,0x08021144L,0x080444b0L,0x080444d4L,0x08028fe8L,0x08035b40L};
    long[] callTargets = {0x08021020L,0x08021098L,0x08021118L,0x08021144L,0x080444b0L,0x080444d4L,0x08044470L,0x08031734L,0x0802a634L};
    Address a(long v){ return toAddr(v & 0xfffffffeL); }
    Function fn(Address x){
        Function f=getFunctionContaining(x);
        if(f==null){ new CreateFunctionCmd(x).applyTo(currentProgram, monitor); f=getFunctionAt(x); }
        return f;
    }
    String nf(Function f){ return f==null?"?":(f.getEntryPoint()+" "+f.getName()); }
    public void run() throws Exception {
        di=new DecompInterface(); di.openProgram(currentProgram);
        println("=== CALLERS DE FUNCIONES CLAVE ===");
        java.util.Set<Long> tg=new java.util.HashSet<>(); for(long v:callTargets) tg.add(v & 0xfffffffeL);
        for(Instruction ins: currentProgram.getListing().getInstructions(true)){
            for(Reference r: ins.getReferencesFrom()){
                Address to=r.getToAddress();
                if(to==null) continue;
                long v=to.getOffset() & 0xfffffffeL;
                if(tg.contains(v)){
                    Function cf=getFunctionContaining(ins.getAddress());
                    println("CALL to=0x"+Long.toHexString(v)+" from="+ins.getAddress()+" caller="+nf(cf));
                }
            }
        }
        println("=== DECOMPILACION ===");
        for(long t: starts){
            Function f=fn(a(t));
            println("\n##### 0x"+Long.toHexString(t)+" -> "+nf(f));
            if(f==null) continue;
            DecompileResults rr=di.decompileFunction(f,200,monitor);
            if(rr!=null && rr.getDecompiledFunction()!=null) println("PSEUDO:\n"+rr.getDecompiledFunction().getC());
            else println("DECOMP FAILED");
        }
        println("=== FIN QA390Dac ===");
    }
}
