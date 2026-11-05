// Crea funciones en destinos de vtables/tablas de punteros que el auto-análisis no alcanzó.
// Modifica el proyecto de análisis (correr SIN -readOnly). No toca el binario.
// Uso: -process AI.exe -noanalysis -scriptPath work/scripts/ghidra -postScript SeedVtables.java <salida.txt> [minRun]
//@category AI_Multiplayer
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import java.io.FileWriter;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;

public class SeedVtables extends GhidraScript {
  @Override
  public void run() throws Exception {
    String[] a = getScriptArgs();
    PrintWriter out = new PrintWriter(new FileWriter(a[0]));
    int minRun = a.length > 1 ? Integer.parseInt(a[1]) : 3;
    Memory mem = currentProgram.getMemory();
    MemoryBlock text = mem.getBlock(".text"), rdata = mem.getBlock(".rdata");
    long t0 = text.getStart().getOffset(), t1 = text.getEnd().getOffset();
    List<Address> targets = new ArrayList<>();
    Address p = rdata.getStart();
    Address end = rdata.getEnd().subtract(4);
    int runs = 0;
    while (p.compareTo(end) < 0) {
      monitor.checkCancelled();
      List<Address> run = new ArrayList<>();
      Address q = p;
      while (q.compareTo(end) < 0) {
        long v = mem.getInt(q) & 0xffffffffL;
        if (v < t0 || v > t1) break;
        run.add(toAddr(v));
        q = q.add(4);
      }
      if (run.size() >= minRun) {
        runs++;
        targets.addAll(run);
        p = q;
      } else {
        p = p.add(4);
      }
    }
    int created = 0, disasm = 0, skipped = 0;
    for (Address t : targets) {
      monitor.checkCancelled();
      if (getFunctionAt(t) != null) continue;
      if (getInstructionAt(t) == null) {
        if (getDataAt(t) != null && getDataAt(t).isDefined()) { skipped++; continue; }
        DisassembleCommand cmd = new DisassembleCommand(t, null, true);
        if (cmd.applyTo(currentProgram, monitor)) disasm++;
      }
      if (getInstructionAt(t) == null || getFunctionContaining(t) != null && !getFunctionContaining(t).getEntryPoint().equals(t)) {
        // dentro de otra función: no crear (puede ser un thunk interno o dato mal tipado)
        if (getInstructionAt(t) == null) skipped++;
        continue;
      }
      Function f = createFunction(t, null);
      if (f != null) {
        created++;
        if (created <= 200) out.println("creada " + t);
      }
    }
    analyzeChanges(currentProgram);
    out.printf("corridas=%d destinos=%d desensamblados=%d funciones_creadas=%d salteados=%d%n",
        runs, targets.size(), disasm, created, skipped);
    out.close();
  }
}
