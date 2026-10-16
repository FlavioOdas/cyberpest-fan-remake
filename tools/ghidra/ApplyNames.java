// Ghidra headless script: apply function names/comments from docs/re/names.tsv.
// TSV columns: <selector:offset> <name> [comment]. Lines starting with # are ignored.
// Args: <names.tsv>
// @category CyberPest
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;

import java.nio.file.Files;
import java.nio.file.Path;

public class ApplyNames extends GhidraScript {
    @Override
    public void run() throws Exception {
        int applied = 0;
        for (String line : Files.readAllLines(Path.of(getScriptArgs()[0]))) {
            if (line.isBlank() || line.startsWith("#")) {
                continue;
            }
            String[] f = line.split("\t");
            Address addr = toAddr(f[0].trim());
            Function fn = getFunctionAt(addr);
            if (fn == null) {
                fn = createFunction(addr, null);
            }
            if (fn == null) {
                printerr("no function at " + f[0]);
                continue;
            }
            fn.setName(f[1].trim(), SourceType.USER_DEFINED);
            if (f.length > 2 && !f[2].isBlank()) {
                fn.setComment(f[2].trim());
            }
            applied++;
        }
        println("applied " + applied + " names");
    }
}
