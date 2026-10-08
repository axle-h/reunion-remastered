// Applies ghidra/symbols.txt to the current program: names, namespaces,
// Pascal-style prototypes (with exact stack/register storage), data types and
// plate comments. Safe to run repeatedly.
//
// Usage (headless): -postScript ApplySymbols.java <path/to/symbols.txt>
//
// See ghidra/README.md for the file format.
//@category Reunion

import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.util.NamespaceUtils;
import ghidra.program.model.address.*;
import ghidra.program.model.data.*;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;

import java.nio.file.*;
import java.util.*;

public class ApplySymbols extends GhidraScript {

    // Defined by ghidra/lang/x86-16-pascal.cspec. Arguments are pushed left to
    // right and popped by the callee, so storage is assigned explicitly by
    // this script rather than by the prototype model.
    static final String CC_FAR = "__pascal16far";
    static final String CC_NEAR = "__pascal16near";

    static final java.util.regex.Pattern QUALIFIED_NAME =
        java.util.regex.Pattern.compile("[A-Za-z_]\\w*(::[A-Za-z_]\\w*)*");

    DataTypeManager dtm;
    CategoryPath cat = new CategoryPath("/Pascal");
    Map<Integer, String> unitBySeg = new HashMap<>();
    int applied = 0, errors = 0;

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) {
            printerr("usage: ApplySymbols.java <symbols.txt>");
            return;
        }
        dtm = currentProgram.getDataTypeManager();
        if (currentProgram.getCompilerSpec().getCallingConvention(CC_FAR) == null) {
            printerr("Program wasn't imported with the Borland Pascal language; run ghidra/rebuild.sh");
            return;
        }
        definePascalTypes();
        Path typesFile = Path.of(args[0]).resolveSibling("types.pas");
        if (Files.exists(typesFile))
            defineRecords(Files.readString(typesFile));
        fixEntryStack();

        List<String> lines = Files.readAllLines(Path.of(args[0]));
        for (int i = 0; i < lines.size(); i++) {
            String line = lines.get(i).strip();
            if (line.isEmpty() || line.startsWith("#"))
                continue;
            try {
                applyLine(line);
            }
            catch (Exception e) {
                errors++;
                printerr("symbols.txt:" + (i + 1) + ": " + e.getMessage());
            }
        }
        int conv = applyDefaultConvention();
        println("ApplySymbols: " + applied + " applied, " + errors + " errors, "
            + conv + " functions switched to Pascal convention");
    }

    // The MZ loader records the header's initial SS:SP as fixed register
    // values at the entry point. The decompiler then treats every push in the
    // main program as a write to a fixed address, so drop those values.
    void fixEntryStack() throws Exception {
        for (Address entry : currentProgram.getSymbolTable().getExternalEntryPointIterator()) {
            Function f = getFunctionAt(entry);
            if (f == null)
                continue;
            ProgramContext ctx = currentProgram.getProgramContext();
            for (String r : new String[] { "SP", "SS" }) {
                Register reg = currentProgram.getRegister(r);
                for (AddressRange range : f.getBody())
                    ctx.remove(range.getMinAddress(), range.getMaxAddress(), reg);
            }
        }
    }

    // Everything was compiled by Borland Pascal, so any function without an
    // explicit prototype gets the Pascal convention (callee pops, only
    // BP/SP/SS/DS preserved). Storage stays dynamic for these.
    int applyDefaultConvention() throws Exception {
        int n = 0;
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            if (f.hasCustomVariableStorage())
                continue;
            String cc = isFar(f) ? CC_FAR : CC_NEAR;
            if (!cc.equals(f.getCallingConventionName())) {
                f.setCallingConvention(cc);
                n++;
            }
        }
        return n;
    }

    void definePascalTypes() {
        add(new TypedefDataType(cat, "byte", UnsignedCharDataType.dataType, dtm));
        add(new TypedefDataType(cat, "shortint", SignedCharDataType.dataType, dtm));
        add(new TypedefDataType(cat, "word", UnsignedShortDataType.dataType, dtm));
        add(new TypedefDataType(cat, "integer", ShortDataType.dataType, dtm));
        add(new TypedefDataType(cat, "longint", LongDataType.dataType, dtm));

        StructureDataType str = new StructureDataType(cat, "PString", 0, dtm);
        str.add(UnsignedCharDataType.dataType, "len", "length byte");
        str.add(new ArrayDataType(CharDataType.dataType, 255, 1, dtm), "s", null);
        add(str);

        StructureDataType real = new StructureDataType(cat, "Real", 0, dtm);
        real.add(UnsignedCharDataType.dataType, "exp", "biased exponent, 0 = zero");
        real.add(new ArrayDataType(UnsignedCharDataType.dataType, 5, 1, dtm), "mant",
            "mantissa, sign in top bit of last byte");
        add(real);

        DataType w = pascal("word");
        DataType p = farPtr(VoidDataType.dataType);
        StructureDataType file = new StructureDataType(cat, "FileRec", 0, dtm);
        file.add(w, "Handle", null);
        file.add(w, "Mode", "D7B0h closed, D7B1h input, D7B2h output, D7B3h in/out");
        file.add(w, "RecSize", null);
        file.add(new ArrayDataType(UnsignedCharDataType.dataType, 26, 1, dtm), "Private", null);
        file.add(new ArrayDataType(UnsignedCharDataType.dataType, 16, 1, dtm), "UserData", null);
        file.add(new ArrayDataType(CharDataType.dataType, 80, 1, dtm), "Name", null);
        add(file);

        StructureDataType text = new StructureDataType(cat, "TextRec", 0, dtm);
        text.add(w, "Handle", null);
        text.add(w, "Mode", "D7B0h closed, D7B1h input, D7B2h output");
        text.add(w, "BufSize", null);
        text.add(w, "Private", null);
        text.add(w, "BufPos", null);
        text.add(w, "BufEnd", null);
        text.add(farPtr(CharDataType.dataType), "BufPtr", null);
        text.add(p, "OpenFunc", null);
        text.add(p, "InOutFunc", null);
        text.add(p, "FlushFunc", null);
        text.add(p, "CloseFunc", null);
        text.add(new ArrayDataType(UnsignedCharDataType.dataType, 16, 1, dtm), "UserData", null);
        text.add(new ArrayDataType(CharDataType.dataType, 80, 1, dtm), "Name", null);
        text.add(new ArrayDataType(CharDataType.dataType, 128, 1, dtm), "Buffer", null);
        add(text);
    }

    // Parses the record declarations in types.pas (see that file for syntax).
    void defineRecords(String src) throws Exception {
        src = src.replaceAll("(?s)\\{.*?\\}", "");
        StructureDataType rec = null;
        for (String raw : src.split("\n")) {
            String line = raw.strip();
            if (line.isEmpty() || line.equals("type"))
                continue;
            java.util.regex.Matcher m = java.util.regex.Pattern
                .compile("(\\w+)\\s*=\\s*record").matcher(line);
            if (m.matches()) {
                rec = new StructureDataType(cat, m.group(1), 0, dtm);
                continue;
            }
            if (line.startsWith("end")) {
                if (rec == null)
                    throw new Exception("types.pas: 'end' without record");
                DataType dt = add(rec);
                println("types.pas: " + dt.getName() + " = " + dt.getLength() + " bytes");
                rec = null;
                continue;
            }
            if (rec == null)
                throw new Exception("types.pas: unexpected line: " + line);
            int colon = line.indexOf(':');
            DataType t = parseType(line.substring(colon + 1).replaceAll(";\\s*$", "").strip());
            for (String n : line.substring(0, colon).split(","))
                rec.add(t, n.strip(), null);
        }
    }

    DataType add(DataType dt) {
        return dtm.addDataType(dt, DataTypeConflictHandler.REPLACE_HANDLER);
    }

    // ---- line handling ---------------------------------------------------

    void applyLine(String line) throws Exception {
        String comment = null;
        int c = line.indexOf("//");
        if (c >= 0) {
            comment = line.substring(c + 2).strip();
            line = line.substring(0, c).strip();
        }

        if (line.startsWith("@unit")) {
            String[] t = line.split("\\s+");
            unitBySeg.put(Integer.parseInt(t[1], 16), t[2]);
            return;
        }

        int sp = line.indexOf(' ');
        String addrText = sp < 0 ? line : line.substring(0, sp);
        String decl = sp < 0 ? "" : line.substring(sp + 1).strip();
        Address addr = toAddr(addrText);
        if (addr == null)
            throw new Exception("bad address " + addrText);

        int seg = ((SegmentedAddress) addr).getSegment();
        java.util.regex.Matcher m = QUALIFIED_NAME.matcher(decl);
        String nameEnd = m.lookingAt() ? m.group() : "";
        String rest = decl.substring(nameEnd.length()).strip();
        Namespace ns = namespaceFor(nameEnd, seg);
        String name = nameEnd.contains("::") ? nameEnd.substring(nameEnd.lastIndexOf("::") + 2) : nameEnd;

        boolean isCode = currentProgram.getMemory().getBlock(addr).isExecute()
            && !rest.startsWith(":");
        if (isCode)
            applyFunction(addr, ns, name, rest, comment);
        else
            applyData(addr, ns, name, rest, comment);
        applied++;
    }

    Namespace namespaceFor(String qualified, int seg) throws Exception {
        String path = null;
        int i = qualified.lastIndexOf("::");
        if (i >= 0)
            path = qualified.substring(0, i);
        else if (unitBySeg.containsKey(seg))
            path = unitBySeg.get(seg);
        if (path == null)
            return currentProgram.getGlobalNamespace();
        return NamespaceUtils.createNamespaceHierarchy(path, null, currentProgram,
            SourceType.USER_DEFINED);
    }

    void applyFunction(Address addr, Namespace ns, String name, String sig, String comment)
            throws Exception {
        Function f = getFunctionAt(addr);
        if (f == null) {
            disassemble(addr);
            CreateFunctionCmd cmd = new CreateFunctionCmd(addr);
            if (!cmd.applyTo(currentProgram, monitor))
                throw new Exception("cannot create function at " + addr + ": " + cmd.getStatusMsg());
            f = getFunctionAt(addr);
        }
        if (!name.isEmpty())
            f.getSymbol().setNameAndNamespace(name, ns, SourceType.USER_DEFINED);
        if (comment != null)
            f.setComment(comment);
        if (sig.startsWith("("))
            applySignature(f, sig);
    }

    void applyData(Address addr, Namespace ns, String name, String rest, String comment)
            throws Exception {
        Symbol s = createLabel(addr, name, ns, true, SourceType.USER_DEFINED);
        if (rest.startsWith(":")) {
            DataType dt = parseType(rest.substring(1).strip());
            clearListing(addr, addr.add(dt.getLength() - 1));
            createData(addr, dt);
        }
        if (comment != null)
            setPlateComment(addr, comment);
    }

    // ---- prototypes ------------------------------------------------------

    record Param(String name, DataType type, List<Register> regs, boolean keep) {}

    void applySignature(Function f, String sig) throws Exception {
        int close = matchingParen(sig, 0);
        String inner = sig.substring(1, close).strip();
        String after = sig.substring(close + 1).strip();

        List<Param> params = new ArrayList<>();
        if (!inner.isEmpty())
            for (String group : inner.split(";"))
                params.addAll(parseParamGroup(group.strip()));

        DataType retType = VoidDataType.dataType;
        List<Register> retRegs = null;
        if (after.startsWith(":") && !after.substring(1).isBlank()) {
            String[] tr = splitRegs(after.substring(1).strip());
            retType = parseType(tr[0]);
            retRegs = parseRegs(tr[1]);
        }
        // Functions returning a string get a pointer to the caller's result
        // buffer, pushed before all other arguments.
        if (isPString(retType)) {
            params.add(0, new Param("result", farPtr(retType), null, false));
            retType = VoidDataType.dataType;
        }

        boolean far = isFar(f);
        int offset = far ? 4 : 2;
        Variable[] vars = new Variable[params.size()];
        for (int i = params.size() - 1; i >= 0; i--) {
            Param p = params.get(i);
            VariableStorage st;
            if (p.regs != null)
                st = new VariableStorage(currentProgram, p.regs.toArray(new Register[0]));
            else {
                st = new VariableStorage(currentProgram, offset, p.type.getLength());
                offset += (p.type.getLength() + 1) & ~1;
            }
            vars[i] = new ParameterImpl(p.name, p.type, st, currentProgram);
        }

        VariableStorage retSt;
        if (retType instanceof VoidDataType)
            retSt = VariableStorage.VOID_STORAGE;
        else if (retRegs != null)
            retSt = new VariableStorage(currentProgram, retRegs.toArray(new Register[0]));
        else
            retSt = new VariableStorage(currentProgram, defaultReturnRegs(retType.getLength()));
        Variable ret = new ReturnParameterImpl(retType, retSt, currentProgram);

        // Cross-check with the callee's own RET n. A string result pointer
        // stays on the stack for the caller, so it isn't popped.
        int popped = offset - (far ? 4 : 2);
        for (Param p : params)
            if (p.keep() || isPString(derefIfResult(p)))
                popped -= (p.type().getLength() + 1) & ~1;
        int purge = f.getStackPurgeSize();
        if (purge != Function.UNKNOWN_STACK_DEPTH_CHANGE && purge != Function.INVALID_STACK_DEPTH_CHANGE
                && purge != popped)
            throw new Exception(f.getName() + ": prototype has " + popped
                + " bytes of stack arguments but the routine pops " + purge);

        f.updateFunction(far ? CC_FAR : CC_NEAR, ret, Arrays.asList(vars),
            Function.FunctionUpdateType.CUSTOM_STORAGE, true, SourceType.USER_DEFINED);
    }

    List<Param> parseParamGroup(String g) throws Exception {
        boolean byRef = false;
        if (g.startsWith("var ") || g.startsWith("const ")) {
            byRef = true;
            g = g.substring(g.indexOf(' ') + 1).strip();
        }
        int colon = g.indexOf(':');
        if (colon < 0 && byRef) {
            // Untyped var/const parameter (like Move's): a far pointer to anything.
            List<Param> out = new ArrayList<>();
            for (String n : g.split(","))
                out.add(new Param(n.strip(), farPtr(VoidDataType.dataType), null, false));
            return out;
        }
        if (colon < 0)
            throw new Exception("parameter without type: " + g);
        String[] tr = splitRegs(g.substring(colon + 1).strip());
        DataType t = parseType(tr[0]);
        // "@keep": the routine leaves this argument on the stack for the caller.
        boolean keep = "keep".equals(tr[1]);
        List<Register> regs = keep ? null : parseRegs(tr[1]);
        // Strings and anything bigger than 4 bytes (except Real) are passed by
        // reference even when declared by value; the callee makes its copy.
        if (byRef || isPString(t) || (t.getLength() > 4 && !t.getName().equals("Real")))
            t = farPtr(t);
        List<Param> out = new ArrayList<>();
        for (String n : g.substring(0, colon).split(","))
            out.add(new Param(n.strip(), t, regs, keep));
        return out;
    }

    // "longint @DX:AX" -> {"longint", "DX:AX"}
    static String[] splitRegs(String s) {
        int at = s.indexOf('@');
        if (at < 0)
            return new String[] { s.strip(), null };
        return new String[] { s.substring(0, at).strip(), s.substring(at + 1).strip() };
    }

    List<Register> parseRegs(String spec) throws Exception {
        if (spec == null)
            return null;
        List<Register> regs = new ArrayList<>();
        for (String r : spec.split(":")) {
            Register reg = currentProgram.getRegister(r.strip());
            if (reg == null)
                throw new Exception("unknown register " + r);
            regs.add(reg);
        }
        return regs;
    }

    Register[] defaultReturnRegs(int size) throws Exception {
        String spec = switch (size) {
            case 1 -> "AL";
            case 2 -> "AX";
            case 4 -> "DX:AX";
            case 6 -> "DX:BX:AX";
            default -> throw new Exception("no default return register for size " + size);
        };
        return parseRegs(spec).toArray(new Register[0]);
    }

    boolean isFar(Function f) {
        for (Instruction ins : currentProgram.getListing().getInstructions(f.getBody(), true)) {
            String m = ins.getMnemonicString();
            if (m.equals("RETF"))
                return true;
            if (m.equals("RET"))
                return false;
        }
        return true;
    }

    static int matchingParen(String s, int open) throws Exception {
        int depth = 0;
        for (int i = open; i < s.length(); i++) {
            if (s.charAt(i) == '(')
                depth++;
            else if (s.charAt(i) == ')' && --depth == 0)
                return i;
        }
        throw new Exception("unbalanced parentheses: " + s);
    }

    // ---- types -----------------------------------------------------------

    DataType farPtr(DataType t) {
        return new PointerDataType(t, 4, dtm);
    }

    boolean isPString(DataType t) {
        return t != null && t.getName().equals("PString");
    }

    // The hidden result parameter of a string function: "result: ^string".
    static DataType derefIfResult(Param p) {
        return p.name().equals("result") && p.type() instanceof Pointer ptr ? ptr.getDataType() : null;
    }

    DataType parseType(String t) throws Exception {
        t = t.strip();
        if (t.startsWith("^"))
            return farPtr(parseType(t.substring(1)));
        if (t.startsWith("array")) {
            // array[lo..hi] of T, or array[a..b, c..d] of T (= array of arrays)
            int lb = t.indexOf('['), rb = t.indexOf(']');
            String[] dims = t.substring(lb + 1, rb).split(",");
            DataType dt = parseType(t.substring(t.indexOf(" of ", rb) + 4));
            for (int i = dims.length - 1; i >= 0; i--) {
                String[] range = dims[i].split("\\.\\.");
                int n = Integer.decode(range[1].strip()) - Integer.decode(range[0].strip()) + 1;
                dt = new ArrayDataType(dt, n, dt.getLength(), dtm);
            }
            return dt;
        }
        if (t.startsWith("string[")) {
            // string[N]: length byte + N characters
            int n = Integer.parseInt(t.substring(7, t.indexOf(']')).strip());
            StructureDataType s = new StructureDataType(cat, "String" + n, 0, dtm);
            s.add(UnsignedCharDataType.dataType, "len", "length byte");
            s.add(new ArrayDataType(CharDataType.dataType, n, 1, dtm), "s", null);
            return add(s);
        }
        DataType own = dtm.getDataType(cat, t);
        if (own != null)
            return own;
        switch (t) {
            case "pointer": return farPtr(VoidDataType.dataType);
            case "char": return CharDataType.dataType;
            case "boolean": return BooleanDataType.dataType;
            case "string": return pascal("PString");
            case "byte": case "shortint": case "word": case "integer": case "longint":
            case "Real":
                return pascal(t);
            case "real": return pascal("Real");
            case "Text": return pascal("TextRec");
            case "File": return pascal("FileRec");
        }
        List<DataType> found = new ArrayList<>();
        dtm.findDataTypes(t, found);
        if (found.isEmpty())
            throw new Exception("unknown type " + t);
        return found.get(0);
    }

    DataType pascal(String name) {
        return dtm.getDataType(cat, name);
    }
}
