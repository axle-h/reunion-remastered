// Dumps every function in the given segments: entry, size, caller count, disassembly.
// Args: outfile seg [seg...]   (segments in Ghidra numbering, e.g. 48c7)
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import java.io.*;
public class DumpSeg extends GhidraScript {
  public void run() throws Exception {
    String[] a=getScriptArgs();
    PrintWriter out=new PrintWriter(new FileWriter(a[0]));
    Listing L=currentProgram.getListing();
    ReferenceManager rm=currentProgram.getReferenceManager();
    for(int i=1;i<a.length;i++){
      int seg=Integer.parseInt(a[i],16);
      for(Function f: currentProgram.getFunctionManager().getFunctions(true)){
        SegmentedAddress e=(SegmentedAddress)f.getEntryPoint();
        if(e.getSegment()!=seg) continue;
        int calls=0;
        for(Reference r: rm.getReferencesTo(e)) if(r.getReferenceType().isCall()) calls++;
        out.printf("===== %s %s size=%d callers=%d%n", f.getName(), e, f.getBody().getNumAddresses(), calls);
        for(Instruction ins: L.getInstructions(f.getBody(),true))
          out.printf("  %s  %s%n", ins.getAddress(), ins);
      }
    }
    out.close();
  }
}
