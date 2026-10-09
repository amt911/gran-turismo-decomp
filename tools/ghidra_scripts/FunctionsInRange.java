// List the functions whose entry lies in [start, end) (ELF vaddrs, base 0): entry, size, callees.
// Usage: -postScript FunctionsInRange.java 0x330000 0x334000
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

public class FunctionsInRange extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        Address start = toAddr(Long.decode(args[0]));
        Address end = toAddr(Long.decode(args[1]));
        for (Function f : currentProgram.getFunctionManager().getFunctions(start, true)) {
            if (f.getEntryPoint().compareTo(end) >= 0) break;
            println("FUNC " + f.getEntryPoint() + " size=" + f.getBody().getNumAddresses()
                    + " callees=" + f.getCalledFunctions(monitor).size()
                    + " callers=" + f.getCallingFunctions(monitor).size());
        }
    }
}
