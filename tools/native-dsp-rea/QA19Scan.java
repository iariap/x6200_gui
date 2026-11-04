// QA19: handlers de filtros/DSP en MCU 1.0.8 (strings + mascaras SDK) y decompilacion
import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.*;
import java.util.*;

public class QA19Scan extends GhidraScript {
    DecompInterface di;
    long[] strs = {0x08050dfbL,0x080505cbL,0x080508b4L,0x080508d0L,0x080508e8L,0x08050900L,
                   0x08050c60L,0x08050be4L,0x08050bfcL,0x08050c14L,0x08050c2cL,0x08050c44L,
                   0x08050d04L,0x08050c78L,0x08050c94L,0x08050cb0L,0x08050cccL,0x08050ce8L,
                   0x08050d98L,0x08050d20L,0x08050d38L,0x08050d50L,0x08050d68L,0x08050d80L,
                   0x08050e5cL,0x08050814L,0x08050e4cL};
    long[] masks = {0xFF0000L,0x1000000L,0x2000000L,0xFFF000L,0x3F000000L,0xFFF00000L,
                    0xF80000L,0x1F00000L,0x3E0L,0xF800L,0xF00L,0x1000L,0x2000L,0x3E0000L,
                    0xFFFF0000L,0x1F000000L,0x7C00000L};
    Address a(long v){ return toAddr(v & 0xfffffffeL); }
    Address straddr(long v){ return toAddr(v); }
    Function fn(Address x){
        Function f=getFunctionContaining(x);
        if(f==null){ new CreateFunctionCmd(x).applyTo(currentProgram, monitor); f=getFunctionAt(x); }
        return f;
    }
    String nf(Function f){ return f==null?"?":(f.getEntryPoint()+" "+f.getName()); }
    public void run() throws Exception {
        di=new DecompInterface(); di.openProgram(currentProgram);
        Set<Function> fset=new LinkedHashSet<>();
        Set<Long> strset=new HashSet<>(); for(long s:strs) strset.add(s);
        println("=== STRING DATA REFS (LDR literal -> string) ===");
        for(Instruction ins: currentProgram.getListing().getInstructions(true)){
            for(int i=0;i<ins.getNumOperands();i++){
                for(Object o: ins.getOpObjects(i)){
                    if(o instanceof Address){
                        Address ad=(Address)o;
                        try {
                            long val=currentProgram.getMemory().getInt(ad)&0xffffffffL;
                            if(strset.contains(val)){
                                Function cf=getFunctionContaining(ins.getAddress());
                                println("STR 0x"+Long.toHexString(val)+" insn="+ins.getAddress()+" func="+nf(cf)+" via literal 0x"+ad);
                                if(cf!=null) fset.add(cf);
                            }
                        } catch(Exception e){}
                    }
                }
            }
            for(Reference r: ins.getReferencesFrom()){
                Address to=r.getToAddress();
                if(to!=null && strset.contains(to.getOffset())){ Function cf=getFunctionContaining(ins.getAddress()); println("STRREF 0x"+Long.toHexString(to.getOffset())+" insn="+ins.getAddress()+" func="+nf(cf)+" type="+r.getReferenceType()); if(cf!=null) fset.add(cf); }
            }
        }
        println("=== MASCARAS SDK EN INSTRUCCIONES ===");
        Set<Long> mset=new HashSet<>(); for(long m:masks) mset.add(m);
        for(Instruction ins: currentProgram.getListing().getInstructions(true)){
            for(int i=0;i<ins.getNumOperands();i++){
                for(Object o: ins.getOpObjects(i)){
                    if(o instanceof Scalar){
                        long v=((Scalar)o).getUnsignedValue();
                        if(mset.contains(v)){
                            Function cf=getFunctionContaining(ins.getAddress());
                            println("MASK 0x"+Long.toHexString(v)+" insn="+ins.getAddress()+" func="+nf(cf)+" | "+ins);
                            if(cf!=null) fset.add(cf);
                        }
                    }
                }
            }
        }
        println("=== DECOMPILACION ("+fset.size()+" funciones) ===");
        for(Function f: fset){
            println("\n##### "+f.getEntryPoint()+" "+f.getName());
            DecompileResults rr=di.decompileFunction(f,220,monitor);
            if(rr!=null && rr.getDecompiledFunction()!=null) println("PSEUDO:\n"+rr.getDecompiledFunction().getC());
            else println("DECOMP FAILED");
        }
        println("=== FIN QA19Scan ===");
    }
}
