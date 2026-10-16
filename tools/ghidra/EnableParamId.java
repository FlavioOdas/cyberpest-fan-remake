// Ghidra headless pre-script: enable decompiler-driven parameter recovery (off by default),
// which 16-bit pascal far calls need for readable call arguments.
// @category CyberPest
import ghidra.app.script.GhidraScript;

public class EnableParamId extends GhidraScript {
    @Override
    public void run() throws Exception {
        setAnalysisOption(currentProgram, "Decompiler Parameter ID", "true");
    }
}
