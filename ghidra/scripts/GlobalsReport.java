// Lists every reference from code into the data segment as CSV:
//     offset,size,access,from_address,from_segment,label
// size is the operand size in bytes (0 if unknown), access is R, W or RW
// (or A for address-of). Used to work out which unit owns which globals.
//
// Usage (headless): -postScript GlobalsReport.java <out.csv> <data-seg>
//@category Reunion

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;

import java.io.*;

public class GlobalsReport extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        int ds = Integer.parseInt(args[1], 16);
        Listing listing = currentProgram.getListing();
        ReferenceManager rm = currentProgram.getReferenceManager();
        int n = 0;
        try (PrintWriter out = new PrintWriter(new FileWriter(args[0]))) {
            out.println("offset,size,access,from,from_seg,label");
            for (Instruction ins : listing.getInstructions(true)) {
                for (Reference r : ins.getReferencesFrom()) {
                    if (!(r.getToAddress() instanceof SegmentedAddress to) || to.getSegment() != ds)
                        continue;
                    RefType t = r.getReferenceType();
                    String access = t.isRead() && t.isWrite() ? "RW" : t.isWrite() ? "W"
                        : t.isRead() ? "R" : "A";
                    int size = 0;
                    int op = r.getOperandIndex();
                    if (op >= 0 && ins.getOperandType(op) != 0) {
                        // Operand size from the instruction: byte vs word access.
                        String rep = ins.getDefaultOperandRepresentation(op);
                        size = rep.startsWith("byte") ? 1 : rep.startsWith("word") ? 2
                            : rep.startsWith("dword") ? 4 : 0;
                        if (size == 0 && ins.getNumOperands() == 2) {
                            Object[] other = ins.getOpObjects(1 - op);
                            if (other.length == 1 && other[0] instanceof ghidra.program.model.lang.Register reg)
                                size = reg.getMinimumByteSize();
                        }
                    }
                    Symbol s = getSymbolAt(to);
                    out.printf("%04x,%d,%s,%s,%04x,%s%n", to.getSegmentOffset(), size, access,
                        ins.getAddress(), ((SegmentedAddress) ins.getAddress()).getSegment(),
                        s == null ? "" : s.getName());
                    n++;
                }
            }
        }
        println("GlobalsReport: " + n + " references");
    }
}
