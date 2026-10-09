// Write every function Ghidra knows as "entry<TAB>size" (ELF vaddrs, base 0, hex) to the file named by
// the first script argument. Feeds tools/progress.py.
// Usage: -postScript ExportFunctions.java recomp/analysis/functions.tsv
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import java.io.PrintWriter;

public class ExportFunctions extends GhidraScript {
    @Override
    public void run() throws Exception {
        try (PrintWriter out = new PrintWriter(getScriptArgs()[0])) {
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                out.printf("%x\t%d%n", f.getEntryPoint().getOffset(), f.getBody().getNumAddresses());
            }
        }
    }
}
