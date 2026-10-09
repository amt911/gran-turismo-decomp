// Print Ghidra's pseudo-C for the functions at the given ELF vaddrs (base 0) — a local reading aid
// for the decomp workflow; never paste its output into src/ (docs/agents/decomp-workflow.md).
// Usage: -postScript Decompile.java 0x2d50ac [0x...]
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;

public class Decompile extends GhidraScript {
    @Override
    public void run() throws Exception {
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        for (String arg : getScriptArgs()) {
            Function f = getFunctionAt(toAddr(Long.decode(arg)));
            if (f == null) { println("DECOMP none at " + arg); continue; }
            DecompileResults r = decompiler.decompileFunction(f, 60, monitor);
            println("DECOMP-BEGIN " + f.getName());
            for (String line : r.getDecompiledFunction().getC().split("\n")) println("DECOMP| " + line);
            println("DECOMP-END");
        }
    }
}
