// Ghidra headless post-script: decompile every function to one C file + a function index.
// Args: <out.c> <index.tsv>
// @category CyberPest
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;

import java.io.FileWriter;
import java.io.PrintWriter;

public class DecompileAll extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        DecompInterface decomp = new DecompInterface();
        decomp.setOptions(new DecompileOptions());
        decomp.openProgram(currentProgram);
        int ok = 0, failed = 0;
        try (PrintWriter c = new PrintWriter(new FileWriter(args[0]));
             PrintWriter idx = new PrintWriter(new FileWriter(args[1]))) {
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                if (monitor.isCancelled()) {
                    break;
                }
                DecompileResults r = decomp.decompileFunction(f, 120, monitor);
                c.println("// ==== " + f.getName() + " @ " + f.getEntryPoint() + (f.isThunk() ? " (thunk)" : ""));
                if (r.decompileCompleted()) {
                    c.println(r.getDecompiledFunction().getC());
                    ok++;
                } else {
                    c.println("// decompile failed: " + r.getErrorMessage());
                    failed++;
                }
                idx.println(f.getEntryPoint() + "\t" + f.getName() + "\t" + f.getBody().getNumAddresses()
                        + "\t" + f.getCallingConventionName());
            }
        }
        decomp.dispose();
        println("decompiled " + ok + " functions, " + failed + " failed");
    }
}
