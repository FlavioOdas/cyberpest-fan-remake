// Ghidra script: teach the decompiler that MSC's 16-bit stack probe (__chkstk, 1030:067c,
// called as `mov ax, <locals>; call far __chkstk`) allocates the local frame: it lowers SP by AX.
// Without this every local variable is mis-addressed and comparisons decompile as garbage.
// @category CyberPest
import ghidra.app.cmd.function.CallDepthChangeInfo;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.scalar.Scalar;

public class FixChkstk extends GhidraScript {
    @Override
    public void run() throws Exception {
        Address chkstk = toAddr("1030:067c");
        int fixed = 0;
        InstructionIterator it = currentProgram.getListing().getInstructions(true);
        while (it.hasNext()) {
            Instruction insn = it.next();
            if (!insn.getMnemonicString().startsWith("CALL")) {
                continue;
            }
            Address[] flows = insn.getFlows();
            if (flows.length != 1 || !flows[0].equals(chkstk)) {
                continue;
            }
            Instruction prev = insn.getPrevious();
            if (prev == null || !prev.getMnemonicString().equals("MOV")) {
                continue;
            }
            Object[] ops = prev.getOpObjects(1);
            if (ops.length != 1 || !(ops[0] instanceof Scalar)) {
                continue;
            }
            int size = (int) ((Scalar) ops[0]).getUnsignedValue();
            // A far call/retf pair is stack-neutral; __chkstk additionally does `sub sp, ax`.
            CallDepthChangeInfo.setStackDepthChange(currentProgram, insn.getAddress(), -size);
            fixed++;
        }
        println("fixed " + fixed + " __chkstk call sites");
    }
}
