import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.util.*;

public class QA19Handlers extends GhidraScript {
    DecompInterface di;
    long[] strs = {0x08050dfbL,0x080505cbL,0x080508b4L,0x080508d0L,0x080508e8L,0x08050900L,
                   0x08050c60L,0x08050be4L,0x08050bfcL,0x08050c14L,0x08050c2cL,0x08050c44L,
                   0x08050d04L,0x08050c78L,0x08050c94L,0x08050cb0L,0x08050cccL,0x08050ce8L,
                   0x08050d98L,0x08050d20L,0x08050d38L,0x08050d50L,0x08050d68L,0x08050d80L,
                   0x08050e5cL,0x08050814L,0x08050e4cL};
    Set<Long> strset=new HashSet<>();
    Address a(long v){ return toAddr(v & 0xfffffffeL); }
    boolean isTerm(Instruction ins){
        if(ins==null) return true;
        String m=ins.getMnemonicString().toUpperCase();
        return m.startsWith("BX")||m.startsWith("POP")||m.startsWith("RET")||m.startsWith("LDR")&&ins.toString().contains("pc");
    }
    public void run() throws Exception {
        di=new DecompInterface(); di.openProgram(currentProgram);
        for(long s:strs) strset.add(s);
        Set<Long> refAddrs=new LinkedHashSet<>();
        for(Instruction ins: currentProgram.getListing().getInstructions(true)){
            boolean hit=false;
            for(int i=0;i<ins.getNumOperands()&&!hit;i++){
                for(Object o: ins.getOpObjects(i)){
                    if(o instanceof Address){
                        try{ long val=currentProgram.getMemory().getInt((Address)o)&0xffffffffL; if(strset.contains(val)) hit=true; }catch(Exception e){}
                    }
                }
            }
            for(Reference r: ins.getReferencesFrom()){
                Address to=r.getToAddress();
                if(to!=null && strset.contains(to.getOffset())) hit=true;
            }
            if(hit) refAddrs.add(ins.getAddress().getOffset()&0xffffffffL);
        }
        println("refs encontrados: "+refAddrs.size());
        Set<Long> starts=new LinkedHashSet<>();
        for(long r: refAddrs){
            println("ref insn=0x"+Long.toHexString(r));
            Address cur=toAddr(r); Instruction ins=getInstructionAt(cur);
            int back=0;
            while(cur!=null && back<0x120){
                Instruction put=getInstructionBefore(cur);
                if(put!=null && put.getMnemonicString().toUpperCase().startsWith("PUSH")){
                    // prologo candidato; verificar que no caiga dentro de otra funcion conocida
                    if(getFunctionContaining(put.getAddress())==null){ starts.add(put.getAddress().getOffset()&0xffffffffL); }
                    break;
                }
                if(put!=null) cur=put.getAddress(); else break;
                back+=2;
            }
        }
        println("starts candidatos: "+starts.size());
        for(long s: starts){
            Address x=toAddr(s);
            Function f=getFunctionContaining(x);
            if(f==null){ new CreateFunctionCmd(x).applyTo(currentProgram, monitor); f=getFunctionAt(x); }
            if(f==null){ println("NOFUNC 0x"+Long.toHexString(s)); continue; }
            println("\n##### 0x"+Long.toHexString(s)+" -> "+f.getEntryPoint()+" "+f.getName());
            DecompileResults rr=di.decompileFunction(f,220,monitor);
            if(rr!=null && rr.getDecompiledFunction()!=null) println("PSEUDO:\n"+rr.getDecompiledFunction().getC());
            else println("DECOMP FAILED");
        }
        println("=== FIN QA19Handlers ===");
    }
}
