import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.util.task.ConsoleTaskMonitor;
import ghidra.program.model.symbol.Reference;

public class QA44Decomp extends GhidraScript {
    Address a(long v){ return toAddr(v & 0xfffffffeL); }
    Function mk(Address x){
        Function f=getFunctionAt(x);
        if(f==null){
            boolean ok=new CreateFunctionCmd(x).applyTo(currentProgram,monitor);
            f=getFunctionAt(x);
            println("CREATE "+x+" ok="+ok+" name="+(f==null?"null":f.getName()));
        }
        return f;
    }
    public void run() throws Exception {
        println("=== VECTOR TABLE ===");
        for(int i=0;i<256;i++){
            long v;
            try { v=getInt(toAddr(0x08020000L+i*4L)) & 0xffffffffL; } catch(Exception e){ break; }
            if((v & 0xff000000L)==0x08000000L){
                Address x=a(v); println("vec["+i+"] -> "+x); mk(x);
            }
        }
        long[] targets={0x080319e0L,0x0803198aL,0x080339f0L,0x08039250L,0x08039a5cL,
            0x0803d1e8L,0x0803d978L,0x08035b40L,0x08035ad4L,0x0802b0e0L,
            0x08021aacL,0x08028fe8L,0x080287dcL,0x08029614L,0x08023d04L,
            0x08023d18L,0x0803c540L,0x08039e88L,0x08039ea8L};
        for(long t:targets) mk(toAddr(t));
        println("=== STRING REFERENCES ===");
        long[] strs={0x0804fc99L,0x0804fcd5L,0x0804fd2dL,0x08067e2eL,0x08067e4aL};
        for(long s:strs){
            Address x=toAddr(s);
            println("string "+x+" refs:");
            for(Reference r: currentProgram.getReferenceManager().getReferencesTo(x))
                println("  "+r.getFromAddress()+" -> "+r.getReferenceType());
        }
        DecompInterface di=new DecompInterface();
        di.openProgram(currentProgram);
        println("=== DECOMPILATION ===");
        for(long t:targets){
            Function f=getFunctionAt(toAddr(t));
            if(f==null){ println("no function "+toAddr(t)); continue; }
            println("\n///// FUNCTION "+toAddr(t)+" "+f.getName()+" /////");
            DecompileResults rr=di.decompileFunction(f,120,monitor);
            if(rr!=null && rr.getDecompiledFunction()!=null)
                println(rr.getDecompiledFunction().getC());
            else
                println("DECOMP FAILED: "+(rr==null?"null":rr.getErrorMessage()));
        }
    }
}
