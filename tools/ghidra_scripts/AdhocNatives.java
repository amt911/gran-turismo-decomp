// List the native methods GT registers for Adhoc scripts: every call to a registrar function with a
// constant method-name string and a constant function pointer, plus the module/class name strings
// the same initializer passes to other calls. Output lines (tab-separated):
//   NATIVE <registrar> <initializer entry> <module names, comma-separated> <method> <function>
// Usage: -postScript AdhocNatives.java <out.tsv> <registrar vaddr> [<registrar vaddr>...]
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.MemoryAccessException;
import ghidra.program.model.pcode.HighFunction;
import ghidra.program.model.pcode.PcodeOp;
import ghidra.program.model.pcode.PcodeOpAST;
import ghidra.program.model.pcode.Varnode;
import ghidra.program.model.symbol.Reference;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

public class AdhocNatives extends GhidraScript {
    private String cString(long offset) {
        try {
            Address a = toAddr(offset);
            if (currentProgram.getMemory().getBlock(a) == null) return null;
            StringBuilder s = new StringBuilder();
            for (int i = 0; i < 128; i++) {
                byte b = getByte(a.add(i));
                if (b == 0) break;
                if (b < 0x20 || b > 0x7e) return null;
                s.append((char) b);
            }
            return s.length() >= 2 ? s.toString() : null;
        } catch (MemoryAccessException | RuntimeException e) {
            return null;
        }
    }

    private Long constant(Varnode v) {
        if (v == null) return null;
        if (v.isConstant()) return v.getOffset();
        if (v.isAddress()) return v.getOffset();
        PcodeOp def = v.getDef();
        if (def != null && (def.getOpcode() == PcodeOp.COPY || def.getOpcode() == PcodeOp.CAST)) return constant(def.getInput(0));
        if (def != null && def.getOpcode() == PcodeOp.PTRSUB && def.getInput(1).isConstant()) {
            Long base = constant(def.getInput(0));
            return base == null ? null : base + def.getInput(1).getOffset();
        }
        return null;
    }

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        try (PrintWriter out = new PrintWriter(args[0])) {
            for (int r = 1; r < args.length; r++) {
                Address registrar = toAddr(Long.decode(args[r]));
                Set<Function> callers = new LinkedHashSet<>();
                for (Reference ref : getReferencesTo(registrar)) {
                    Function f = getFunctionContaining(ref.getFromAddress());
                    if (f != null) callers.add(f);
                }
                for (Function f : callers) {
                    DecompileResults res = decompiler.decompileFunction(f, 120, monitor);
                    HighFunction high = res.getHighFunction();
                    if (high == null) continue;
                    List<String> modules = new ArrayList<>();
                    List<String[]> methods = new ArrayList<>();
                    Iterator<PcodeOpAST> ops = high.getPcodeOps();
                    while (ops.hasNext()) {
                        PcodeOpAST op = ops.next();
                        if (op.getOpcode() != PcodeOp.CALL) continue;
                        Address target = op.getInput(0).getAddress();
                        if (target.equals(registrar) && op.getNumInputs() >= 4) {
                            Long name = constant(op.getInput(2));
                            Long fn = constant(op.getInput(3));
                            String method = name == null ? null : cString(name);
                            if (method != null && fn != null) methods.add(new String[] {method, String.format("%x", fn)});
                        } else {
                            for (int i = 1; i < op.getNumInputs(); i++) {
                                Long c = constant(op.getInput(i));
                                String s = c == null ? null : cString(c);
                                if (s != null && s.matches("[A-Za-z_][A-Za-z0-9_:]*") && !s.equals("HModule") && !modules.contains(s)) modules.add(s);
                            }
                        }
                    }
                    for (String[] m : methods)
                        out.printf("NATIVE\t%s\t%s\t%s\t%s\t%s%n", registrar, f.getEntryPoint(), String.join(",", modules), m[0], m[1]);
                }
            }
        }
    }
}
