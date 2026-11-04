import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import java.util.*;

public class QA19Region extends GhidraScript {
    DecompInterface di;
    boolean term(Instruction ins){
        if(ins==null) return true;
        String m=ins.getMnemonicString().toUpperCase();
        return m.startsWith("BX")||m.startsWith("POP")||m.startsWith("RET");
    }
    public void run() throws Exception {
        di=new DecompInterface(); di.openProgram(currentProgram);
        Address start=toAddr(0x08042000L), end=toAddr(0x08043300L);
        DisassembleCommand dc=new DisassembleCommand(start, null, true); dc.applyTo(currentProgram, monitor);
        List<Address> starts=new ArrayList<>();
        for(Instruction ins: currentProgram.getListing().getInstructions(start,true)){
            if(ins.getAddress().compareTo(end)>0) break;
            String m=ins.getMnemonicString().toUpperCase();
            if(m.startsWith("PUSH")){
                Instruction prev=getInstructionBefore(ins.getAddress());
                if(term(prev)) starts.add(ins.getAddress());
            }
        }
        println("starts="+starts.size());
        for(Address s: starts){
            Function f=getFunctionContaining(s);
            if(f==null){ new CreateFunctionCmd(s).applyTo(currentProgram, monitor); f=getFunctionAt(s); }
            if(f==null){ println("NOFUNC "+s); continue; }
            println("\n##### "+s+" -> "+f.getName());
            DecompileResults rr=di.decompileFunction(f,220,monitor);
            if(rr!=null && rr.getDecompiledFunction()!=null) println("PSEUDO:\n"+rr.getDecompiledFunction().getC());
            else println("DECOMP FAILED");
        }
        println("=== FIN QA19Region ===");
    }
}
