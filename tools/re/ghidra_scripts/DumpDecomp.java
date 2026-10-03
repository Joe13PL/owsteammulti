// Decompiles all functions and writes one .c file per original Delphi unit.
// Args: <outDir> [unitRegex]
// @category OW
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import java.io.*;
import java.util.*;
import java.util.regex.*;

public class DumpDecomp extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        File out = new File(args[0]);
        out.mkdirs();
        Pattern unitRe = Pattern.compile(args.length > 1 ? args[1].replace(",", "|") : ".*");
        DecompInterface di = new DecompInterface();
        DecompileOptions opts = new DecompileOptions();
        di.setOptions(opts);
        di.toggleCCode(true);
        di.openProgram(currentProgram);
        Map<String, List<Function>> byUnit = new TreeMap<>();
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            String cm = f.getComment();
            String unit = (cm != null && cm.startsWith("unit: ")) ? cm.substring(6).trim() : "_unknown";
            if (!unitRe.matcher(unit).matches()) continue;
            byUnit.computeIfAbsent(unit, k -> new ArrayList<>()).add(f);
        }
        int done = 0;
        for (Map.Entry<String, List<Function>> e : byUnit.entrySet()) {
            try (PrintWriter pw = new PrintWriter(new FileWriter(new File(out, e.getKey().replaceAll("[^A-Za-z0-9_.-]", "_") + ".c")))) {
                pw.println("// Original War - decompiled unit " + e.getKey() + " (from OwarOGL_SGUI_DEBUG.exe, TD32 symbols)");
                for (Function f : e.getValue()) {
                    if (monitor.isCancelled()) return;
                    pw.println();
                    pw.println("// " + f.getEntryPoint() + "  " + f.getName(true));
                    DecompileResults r = di.decompileFunction(f, 60, monitor);
                    if (r != null && r.decompileCompleted()) pw.print(r.getDecompiledFunction().getC());
                    else pw.println("/* decompilation failed: " + (r == null ? "null" : r.getErrorMessage()) + " */");
                    done++;
                }
            }
        }
        println("DumpDecomp: " + done + " functions in " + byUnit.size() + " units");
    }
}
