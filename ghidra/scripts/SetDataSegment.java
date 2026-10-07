// Tells Ghidra that DS holds the program's data segment throughout the code,
// as it does in Borland Pascal code (asm routines that load another DS are
// handled by normal data flow). With DS known, near pointers resolve to the
// data segment: globals get labels and cross-references, and the decompiler
// shows them by name instead of as raw offsets.
//
// Usage (headless, before analysis): -preScript SetDataSegment.java <seg>
//@category Reunion

import ghidra.app.script.GhidraScript;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.ProgramContext;
import ghidra.program.model.mem.MemoryBlock;

import java.math.BigInteger;

public class SetDataSegment extends GhidraScript {
    @Override
    public void run() throws Exception {
        BigInteger ds = new BigInteger(getScriptArgs()[0], 16);
        Register reg = currentProgram.getRegister("DS");

        // The MZ loader marks the initialised part of the data segment as code;
        // fix that so analysis doesn't look for functions in it.
        MemoryBlock data = currentProgram.getMemory().getBlock(toAddr(ds.toString(16) + ":0000"));
        if (data != null && data.isExecute()) {
            data.setExecute(false);
            data.setName("DATA_INIT");
        }
        ProgramContext ctx = currentProgram.getProgramContext();
        int n = 0;
        for (MemoryBlock b : currentProgram.getMemory().getBlocks()) {
            if (!b.isExecute())
                continue;
            ctx.setValue(reg, b.getStart(), b.getEnd(), ds);
            n++;
        }
        println("SetDataSegment: DS = " + ds.toString(16) + " in " + n + " code blocks");
    }
}
