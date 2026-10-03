// Applies TD32-derived symbols (TSV: va, kind, name, unit, size) before auto-analysis.
// @category OW
import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.app.util.NamespaceUtils;
import java.io.*;
import java.nio.file.*;

public class ApplyTD32 extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String tsv = getScriptArgs()[0];
        SymbolTable st = currentProgram.getSymbolTable();
        Listing listing = currentProgram.getListing();
        int nf = 0, nd = 0, bad = 0;
        for (String line : Files.readAllLines(Paths.get(tsv))) {
            String[] c = line.split("\t");
            if (c.length < 5) continue;
            long va = Long.parseLong(c[0], 16);
            if (va <= 0 || va > 0xffffffffL) { bad++; continue; }
            Address a = toAddr(va);
            String full = c[2];
            int idx = full.lastIndexOf("::");
            String ns = idx > 0 ? full.substring(0, idx) : null;
            String nm = idx > 0 ? full.substring(idx + 2) : full;
            try {
                Namespace n = currentProgram.getGlobalNamespace();
                if (ns != null) n = NamespaceUtils.createNamespaceHierarchy(ns, null, currentProgram, SourceType.IMPORTED);
                if (c[1].equals("P")) {
                    if (listing.getInstructionAt(a) == null) new DisassembleCommand(a, null, true).applyTo(currentProgram);
                    Function f = getFunctionAt(a);
                    if (f == null) f = createFunction(a, null);
                    if (f == null) { bad++; continue; }
                    f.setParentNamespace(n);
                    f.setName(nm, SourceType.IMPORTED);
                    try { f.setCallingConvention("__register"); } catch (Exception e) { }
                    f.setComment("unit: " + c[3]);
                    nf++;
                } else {
                    st.createLabel(a, nm, n, SourceType.IMPORTED);
                    nd++;
                }
            } catch (Exception e) {
                bad++;
            }
        }
        println("ApplyTD32: functions=" + nf + " data=" + nd + " failed=" + bad);
    }
}
