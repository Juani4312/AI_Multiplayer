// Mapa vtable concreta -> clase CATHODE (hallazgo 15). Solo lectura.
// Para cada vftable RTTI CATHODE::X: las funciones que la escriben (constructor de la interfaz) y
// los callers directos de esas funciones (constructores de la clase concreta). En cada caller se
// juntan los MOV [reg+off], imm32 cuyo imm32 apunta a una tabla de punteros a funciones (vtable).
// Salida TSV: vtable(VA) clase origen(rtti|concreta) offset_en_objeto función
// Uso: -postScript MapVtables.java <salida.tsv>
//@category AI_Multiplayer
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;
import java.io.FileWriter;
import java.io.PrintWriter;
import java.util.HashSet;
import java.util.Set;

public class MapVtables extends GhidraScript {
  private Memory mem;
  private MemoryBlock rdata, text;

  private boolean isVtable(long va) {
    try {
      Address a = toAddr(va);
      if (!rdata.contains(a)) return false;
      for (int i = 0; i < 2; i++) {
        long p = mem.getInt(a.add(4L * i)) & 0xffffffffL;
        Address t = toAddr(p);
        if (!text.contains(t)) return false;
      }
      return true;
    } catch (Exception e) {
      return false;
    }
  }

  @Override
  public void run() throws Exception {
    String[] args = getScriptArgs();
    PrintWriter out = new PrintWriter(new FileWriter(args[0]));
    mem = currentProgram.getMemory();
    rdata = mem.getBlock(".rdata");
    text = mem.getBlock(".text");
    int nClasses = 0, nConcrete = 0;
    SymbolIterator it = currentProgram.getSymbolTable().getSymbols("vftable");
    while (it.hasNext()) {
      monitor.checkCancelled();
      Symbol s = it.next();
      String ns = s.getParentNamespace().getName(true);
      if (!ns.startsWith("CATHODE::")) continue;
      String cls = ns.substring(9);
      long vtA = s.getAddress().getOffset();
      out.printf("%08x\t%s\trtti\t-\t-%n", vtA, cls);
      nClasses++;
      Set<Function> ctors = new HashSet<>();
      for (Reference r : getReferencesTo(s.getAddress())) {
        Function f = getFunctionContaining(r.getFromAddress());
        if (f != null) ctors.add(f);
      }
      Set<Long> seen = new HashSet<>();
      for (Function f : ctors) {
        for (Reference r : getReferencesTo(f.getEntryPoint())) {
          if (!r.getReferenceType().isCall()) continue;
          Function c = getFunctionContaining(r.getFromAddress());
          if (c == null) continue;
          for (Instruction ins : currentProgram.getListing().getInstructions(c.getBody(), true)) {
            if (!ins.getMnemonicString().equals("MOV") || ins.getNumOperands() != 2) continue;
            Object[] o1 = ins.getOpObjects(1);
            if (o1.length != 1 || !(o1[0] instanceof Scalar)) continue;
            Object[] o0 = ins.getOpObjects(0);
            if (o0.length == 0 || o0[0] instanceof Register && o0.length == 1 && !ins.toString().contains("[")) continue;
            long imm = ((Scalar) o1[0]).getUnsignedValue();
            if (imm == vtA || seen.contains(imm) || !isVtable(imm)) continue;
            seen.add(imm);
            String off = "0";
            for (Object o : o0) if (o instanceof Scalar) off = "0x" + Long.toHexString(((Scalar) o).getUnsignedValue());
            out.printf("%08x\t%s\tconcreta\t%s\t%s%n", imm, cls, off, c.getEntryPoint());
            nConcrete++;
          }
        }
      }
    }
    out.close();
    println("MapVtables: clases " + nClasses + ", vtables concretas " + nConcrete);
  }
}
