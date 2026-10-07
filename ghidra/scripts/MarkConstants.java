// Marks the constants Borland Pascal stores in code segments as data.
//
// Literal strings (and set constants) live in the code segment just before
// the procedure that uses them, and their address is passed as CS:offset:
//     MOV DI,offset / PUSH CS / PUSH DI       or      PUSH CS / PUSH offset
// Ghidra's auto-analysis sometimes disassembles these bytes as code, which
// produces garbage instructions and bogus functions. This script finds every
// CS-relative constant reference, turns valid Pascal strings into string data,
// removes functions that started inside them, and re-disassembles the code
// that follows. Unrecognised constants are labelled and reported.
//
// Usage (headless): -postScript MarkConstants.java [report.txt]
//@category Reunion

import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.data.*;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.*;

import java.io.*;
import java.util.*;

public class MarkConstants extends GhidraScript {

    record Ref(Address from, Address target) {}

    @Override
    public void run() throws Exception {
        Listing listing = currentProgram.getListing();
        Memory mem = currentProgram.getMemory();

        // 1. Collect CS-relative constant references.
        Map<Address, List<Address>> targets = new TreeMap<>();
        for (Instruction ins : listing.getInstructions(true)) {
            if (!isPushReg(ins, "CS"))
                continue;
            Instruction next = ins.getNext();
            if (next == null || !next.getMnemonicString().equals("PUSH"))
                continue;
            Integer off = null;
            Object[] ops = next.getOpObjects(0);
            if (ops.length == 1 && ops[0] instanceof Scalar s)
                off = (int) s.getUnsignedValue();
            else if (ops.length == 1 && ops[0] instanceof Register r)
                off = immediateLoadedInto(ins, r);
            if (off == null)
                continue;
            int seg = ((SegmentedAddress) ins.getAddress()).getSegment();
            Address t = toAddr(String.format("%04x:%04x", seg, off));
            targets.computeIfAbsent(t, k -> new ArrayList<>()).add(ins.getAddress());
        }

        // 2. Classify and mark.
        int strings = 0, other = 0, removedFuncs = 0;
        List<String> report = new ArrayList<>();
        Set<Address> resume = new TreeSet<>();
        for (Map.Entry<Address, List<Address>> e : targets.entrySet()) {
            Address t = e.getKey();
            int len = mem.getByte(t) & 0xff;
            Address end = t.add(len);
            if (isPrintable(mem, t.add(1), len)) {
                removedFuncs += removeFunctionsIn(t, end);
                clearListing(t, end);
                createData(t, PascalString255DataType.dataType);
                resume.add(end.add(1));
                strings++;
            }
            else if (listing.getInstructionAt(t) != null) {
                // A code address (e.g. a return address pushed by hand).
                report.add(String.format("%s code address, used at %s", t, e.getValue()));
            }
            else {
                if (getSymbolAt(t) == null || getSymbolAt(t).getSource() == SourceType.DEFAULT)
                    createLabel(t, String.format("const_%s", t.toString().replace(':', '_')), true);
                report.add(String.format("%s non-string constant, len byte %02x, used at %s", t, len, e.getValue()));
                other++;
            }
        }

        // 3. Code right after a string is the start of a procedure body or
        //    another constant; disassemble it if nothing claimed it.
        int redisassembled = 0;
        for (Address a : resume) {
            if (listing.getCodeUnitAt(a) instanceof Instruction || listing.getDataAt(a) != null
                    && listing.getDataAt(a).isDefined())
                continue;
            if (targets.containsKey(a))
                continue;
            if (new DisassembleCommand(a, null, true).applyTo(currentProgram, monitor)) {
                redisassembled++;
                if (getFunctionContaining(a) == null && looksLikePrologue(mem, a))
                    new CreateFunctionCmd(a).applyTo(currentProgram, monitor);
            }
        }

        String summary = String.format("MarkConstants: %d CS constants, %d strings, %d other, "
            + "%d bogus functions removed, %d code runs re-disassembled",
            targets.size(), strings, other, removedFuncs, redisassembled);
        println(summary);
        String[] args = getScriptArgs();
        if (args.length > 0) {
            try (PrintWriter out = new PrintWriter(new FileWriter(args[0]))) {
                out.println(summary);
                report.forEach(out::println);
            }
        }
    }

    static boolean isPushReg(Instruction ins, String reg) {
        if (!ins.getMnemonicString().equals("PUSH"))
            return false;
        Object[] ops = ins.getOpObjects(0);
        return ops.length == 1 && ops[0] instanceof Register r && r.getName().equals(reg);
    }

    // Looks back a couple of instructions for "MOV reg, imm16".
    static Integer immediateLoadedInto(Instruction pushCs, Register reg) {
        Instruction p = pushCs.getPrevious();
        for (int i = 0; i < 3 && p != null; i++, p = p.getPrevious()) {
            if (!p.getMnemonicString().equals("MOV"))
                continue;
            Object[] dst = p.getOpObjects(0);
            Object[] src = p.getOpObjects(1);
            if (dst.length == 1 && dst[0] instanceof Register r && r.equals(reg)) {
                if (src.length == 1 && src[0] instanceof Scalar s)
                    return (int) s.getUnsignedValue();
                return null;
            }
        }
        return null;
    }

    static boolean isPrintable(Memory mem, Address a, int len) throws Exception {
        for (int i = 0; i < len; i++) {
            int b = mem.getByte(a.add(i)) & 0xff;
            boolean ok = (b >= 0x20 && b < 0x7f) || b >= 0x80 || b == '\r' || b == '\n' || b == '\t';
            if (!ok)
                return false;
        }
        return true;
    }

    static boolean looksLikePrologue(Memory mem, Address a) throws Exception {
        int b0 = mem.getByte(a) & 0xff;
        int b1 = mem.getByte(a.add(1)) & 0xff;
        int b2 = mem.getByte(a.add(2)) & 0xff;
        return b0 == 0xc8 || (b0 == 0x55 && b1 == 0x89 && b2 == 0xe5);
    }

    int removeFunctionsIn(Address start, Address end) {
        int n = 0;
        FunctionManager fm = currentProgram.getFunctionManager();
        List<Function> overlapping = new ArrayList<>();
        fm.getFunctionsOverlapping(new AddressSet(start, end)).forEachRemaining(overlapping::add);
        for (Function f : overlapping) {
            Address entry = f.getEntryPoint();
            if (entry.compareTo(start) >= 0 && entry.compareTo(end) <= 0) {
                fm.removeFunction(entry);
                n++;
            }
        }
        return n;
    }
}
