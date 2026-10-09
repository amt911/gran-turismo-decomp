// Print who references the given ELF vaddrs (base 0) and whether Ghidra sees a function there.
// Usage: -postScript RefsTo.java 0x332150 [0x...]
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;

public class RefsTo extends GhidraScript {
    @Override
    public void run() throws Exception {
        for (String arg : getScriptArgs()) {
            Address target = toAddr(Long.decode(arg));
            Function f = getFunctionAt(target);
            println("REFS target=" + target + " function_here=" + (f != null ? f.getName() : "none"));
            for (Reference r : getReferencesTo(target)) {
                Function from = getFunctionContaining(r.getFromAddress());
                println("REFS   from=" + r.getFromAddress() + " type=" + r.getReferenceType()
                        + " in=" + (from != null ? from.getName() + "@" + from.getEntryPoint() : "?")
                        + " insn=" + getInstructionAt(r.getFromAddress()));
            }
        }
    }
}
