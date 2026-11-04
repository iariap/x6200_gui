// QA390: barrido de constantes de periféricos, callers y decompilación dirigida (MCU 1.0.8)
import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.*;

public class QA390Scan extends GhidraScript {
    DecompInterface di;
    long[] periph = {0x40007400L,0x40007404L,0x40007408L,0x4000740cL,0x40007410L,0x40007414L,
                     0x40007418L,0x4000741cL,0x40007420L,0x40007424L,0x40007428L,0x4000742cL,
                     0x40007430L,0x40007434L,0x30000c00L,0x30000000L,0x40003c00L,0x40003c20L,
                     0x40003c30L,0x40003c50L,0x40020010L,0x40020088L,0x40020810L,0x40020888L,
                     0x40023830L,0x40023840L,0x40023844L};
    long[] fns = {0x08021090L,0x0802110cL,0x08021138L,0x08031734L,0x08035cb0L,0x08035b40L,
                  0x080450c8L,0x08043840L,0x080439b4L,0x080432a8L,0x08044470L,0x0802a634L,
                  0x08045320L,0x0802b0e0L,0x08021aacL,0x08028fe8L,0x0803995cL,0x0803195cL,
                  0x08039250L,0x08039e88L,0x08035ad4L,0x0803c540L,0x0803d1e8L};
    long[] strs = {0x0804fc99L,0x0804fcd5L,0x0804fd2dL,0x080502e7L,0x080503b2L,0x08050304L,0x08050372L};
    Address a(long v){ return toAddr(v & 0xfffffffeL); }
    Function fn(Address x){
        Function f=getFunctionContaining(x);
        if(f==null){ new CreateFunctionCmd(x).applyTo(currentProgram, monitor); f=getFunctionAt(x); }
        return f;
    }
    String nf(Function f){ return f==null?"?":(f.getEntryPoint()+" "+f.getName()); }
    public void run() throws Exception {
        di=new DecompInterface(); di.openProgram(currentProgram);
        println("=== CONSTANTES DE PERIFERICOS EN INSTRUCCIONES ===");
        java.util.Set<Long> want=new java.util.HashSet<>(); for(long v:periph) want.add(v);
        for(Instruction ins: currentProgram.getListing().getInstructions(true)){
            for(int i=0;i<ins.getNumOperands();i++){
                for(Object o: ins.getOpObjects(i)){
                    if(o instanceof Scalar){
                        long v=((Scalar)o).getUnsignedValue();
                        if(want.contains(v)){
                            Function cf=getFunctionContaining(ins.getAddress());
                            println("CONST 0x"+Long.toHexString(v)+" at "+ins.getAddress()+" func="+nf(cf)+" instr="+ins.toString());
                        }
                    }
                }
            }
        }
        println("=== CALLERS (referencias desde instrucciones) ===");
        java.util.Set<Long> callees=new java.util.HashSet<>(); for(long v:fns) callees.add(v & 0xfffffffeL);
        for(Instruction ins: currentProgram.getListing().getInstructions(true)){
            for(Reference r: ins.getReferencesFrom()){
                Address to=r.getToAddress();
                if(to==null) continue;
                long v=to.getOffset() & 0xfffffffeL;
                if(callees.contains(v)){
                    Function cf=getFunctionContaining(ins.getAddress());
                    Function tf=getFunctionAt(to);
                    println("CALL to=0x"+Long.toHexString(v)+" from="+ins.getAddress()+" caller="+nf(cf)+" callee="+nf(tf)+" type="+r.getReferenceType());
                }
            }
        }
        println("=== REFS A STRINGS ===");
        for(long s: strs){
            println("str 0x"+Long.toHexString(s)+" refs:");
            for(Reference r: currentProgram.getReferenceManager().getReferencesTo(toAddr(s))){
                Function cf=getFunctionContaining(r.getFromAddress());
                println("  from="+r.getFromAddress()+" func="+nf(cf)+" type="+r.getReferenceType());
            }
        }
        println("=== DECOMPILACION DIRIGIDA ===");
        for(long t: fns){
            Address x=a(t);
            Function f=fn(x);
            println("\n##### 0x"+Long.toHexString(t)+" -> "+nf(f));
            if(f==null) continue;
            DecompileResults rr=di.decompileFunction(f,200,monitor);
            if(rr!=null && rr.getDecompiledFunction()!=null) println("PSEUDO:\n"+rr.getDecompiledFunction().getC());
            else println("DECOMP FAILED "+(rr==null?"null":rr.getErrorMessage()));
        }
        println("=== FIN QA390Scan ===");
    }
}
