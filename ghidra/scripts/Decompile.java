// Writes decompiled C for the given functions or whole segments to a file.
//
// Usage (headless): -postScript Decompile.java <outfile> <target>...
//   target = an address inside a function (e.g. 1b8a:006e)
//          | a segment (e.g. 48c7) for every function in it
//          | all
//@category Reunion

import ghidra.app.decompiler.*;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;

import java.io.*;
import java.util.*;

public class Decompile extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        List<Function> funcs = new ArrayList<>();
        FunctionManager fm = currentProgram.getFunctionManager();
        for (int i = 1; i < args.length; i++) {
            if (args[i].equals("all")) {
                fm.getFunctions(true).forEach(funcs::add);
            }
            else if (args[i].contains(":")) {
                Function f = fm.getFunctionContaining(toAddr(args[i]));
                if (f == null)
                    printerr("no function at " + args[i]);
                else
                    funcs.add(f);
            }
            else {
                int seg = Integer.parseInt(args[i], 16);
                for (Function f : fm.getFunctions(true))
                    if (((SegmentedAddress) f.getEntryPoint()).getSegment() == seg)
                        funcs.add(f);
            }
        }

        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        try (PrintWriter out = new PrintWriter(new FileWriter(args[0]))) {
            for (Function f : funcs) {
                DecompileResults r = di.decompileFunction(f, 60, monitor);
                out.println("// ===== " + f.getName(true) + " @ " + f.getEntryPoint());
                out.println(r.decompileCompleted() ? r.getDecompiledFunction().getC()
                        : "// FAILED: " + r.getErrorMessage());
            }
        }
        di.dispose();
    }
}
