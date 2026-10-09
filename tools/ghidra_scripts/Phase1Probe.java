// Phase 1 acceptance probe: language, image base, function count, VFPU decoding, a known string.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.mem.*;

public class Phase1Probe extends GhidraScript {
    @Override
    public void run() throws Exception {
        Program p = currentProgram;
        println("PROBE language=" + p.getLanguageID() + " compiler=" + p.getCompilerSpec().getCompilerSpecID());
        println("PROBE imageBase=" + p.getImageBase());
        println("PROBE functions=" + p.getFunctionManager().getFunctionCount());
        long vfpu = 0, total = 0;
        InstructionIterator it = p.getListing().getInstructions(true);
        while (it.hasNext()) {
            Instruction ins = it.next();
            total++;
            String m = ins.getMnemonicString().toLowerCase();
            if (m.startsWith("lv.") || m.startsWith("sv.") || m.startsWith("vmov") || m.startsWith("vadd")
                    || m.startsWith("vmul") || m.startsWith("vscl") || m.startsWith("mtv") || m.startsWith("mfv")) vfpu++;
        }
        println("PROBE instructions=" + total + " vfpu_like=" + vfpu);
        Memory mem = p.getMemory();
        byte[] needle = "adhoc_system.cpp".getBytes("ASCII");
        Address hit = mem.findBytes(mem.getMinAddress(), needle, null, true, monitor);
        println("PROBE adhoc_system.cpp at " + hit);
        for (MemoryBlock b : mem.getBlocks()) if (b.getName().equals(".text")) println("PROBE .text " + b.getStart() + " size=0x" + Long.toHexString(b.getSize()));
    }
}
