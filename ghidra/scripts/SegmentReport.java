// Prints one Markdown table row per code segment: size, function counts, how
// often it is called from other segments, and hardware/DOS hints, as a basis
// for docs/segments.md.
//
// Usage (headless): -postScript SegmentReport.java <outfile>
//@category Reunion

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;

import java.io.*;
import java.util.*;

public class SegmentReport extends GhidraScript {
    static class Seg {
        int seg, size, funcs, named, far, near, extCalls, outCalls, ports, ints, vga;
        Set<String> intNums = new TreeSet<>();
        Set<Integer> callees = new TreeSet<>();
    }

    @Override
    public void run() throws Exception {
        Map<Integer, Seg> segs = new TreeMap<>();
        Listing L = currentProgram.getListing();
        ReferenceManager rm = currentProgram.getReferenceManager();

        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            int s = ((SegmentedAddress) f.getEntryPoint()).getSegment();
            Seg g = segs.computeIfAbsent(s, k -> { Seg n = new Seg(); n.seg = k; return n; });
            g.funcs++;
            if (f.getSymbol().getSource() == SourceType.USER_DEFINED)
                g.named++;
            if ("__pascal16near".equals(f.getCallingConventionName()))
                g.near++;
            else
                g.far++;
            for (Reference r : rm.getReferencesTo(f.getEntryPoint()))
                if (r.getReferenceType().isCall()
                        && ((SegmentedAddress) r.getFromAddress()).getSegment() != s)
                    g.extCalls++;
            for (Instruction ins : L.getInstructions(f.getBody(), true)) {
                String m = ins.getMnemonicString();
                if (m.equals("IN") || m.equals("OUT")) {
                    g.ports++;
                    String t = ins.toString();
                    if (t.contains("0x3c") || t.contains("0x3d") || t.contains("0x3da"))
                        g.vga++;
                }
                else if (m.equals("INT")) {
                    g.ints++;
                    g.intNums.add(ins.getDefaultOperandRepresentation(0).replace("0x", ""));
                }
                else if (m.equals("CALLF") && ins.getFlows().length > 0) {
                    int t = ((SegmentedAddress) ins.getFlows()[0]).getSegment();
                    if (t != s) {
                        g.outCalls++;
                        g.callees.add(t);
                    }
                }
            }
        }

        // A segment runs to the next one; the last runs to the end of its block.
        Integer prev = null;
        for (int s : segs.keySet()) {
            if (prev != null)
                segs.get(prev).size = (s - prev) * 16;
            prev = s;
        }
        if (prev != null) {
            Address a = toAddr(String.format("%04x:0000", prev));
            long end = currentProgram.getMemory().getBlock(a).getEnd().getOffset();
            segs.get(prev).size = (int) (end + 1 - a.getOffset());
        }

        try (PrintWriter out = new PrintWriter(new FileWriter(getScriptArgs()[0]))) {
            out.println("| Seg | Size | Funcs (far/near) | Named | Called from outside | Calls out | Port I/O | INTs | Calls segments |");
            out.println("|---|---|---|---|---|---|---|---|---|");
            for (Seg g : segs.values()) {
                StringBuilder cs = new StringBuilder();
                for (int c : g.callees)
                    cs.append(String.format("%04x ", c));
                out.printf("| `%04x` | %d | %d (%d/%d) | %d | %d | %d | %d | %s | %s |%n",
                    g.seg, g.size, g.funcs, g.far, g.near, g.named, g.extCalls, g.outCalls,
                    g.ports, String.join(" ", g.intNums), cs.toString().strip());
            }
        }
    }
}
