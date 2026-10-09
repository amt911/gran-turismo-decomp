// Print the function containing each given ELF vaddr (base 0), then its decompilation.
// Usage: -postScript Containing.java 0x24B638 [0x...]
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;

public class Containing extends GhidraScript {
    @Override
    public void run() throws Exception {
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        for (String arg : getScriptArgs()) {
            Function f = getFunctionContaining(toAddr(Long.decode(arg)));
            if (f == null) { println("CONTAINING none for " + arg); continue; }
            println("CONTAINING " + arg + " in " + f.getName() + "@" + f.getEntryPoint());
            DecompileResults r = decompiler.decompileFunction(f, 60, monitor);
            for (String line : r.getDecompiledFunction().getC().split("\n")) println("DECOMP| " + line);
        }
    }
}
