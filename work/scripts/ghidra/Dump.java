// Volcado genérico para investigar (solo lectura; correr con -noanalysis -readOnly).
// Uso: -postScript Dump.java <salida.txt> <maxLineas> <cmd> [<cmd> ...]
// Comandos:
//   fn:0xADDR        decompila la función que contiene ADDR + callers y callees
//   hdr:0xADDR       solo encabezado: nombre, tamaño, callers y callees (sin código)
//   xref:0xADDR      referencias a ADDR con la función contenedora
//   vt:0xADDR:N      N punteros desde ADDR (vtable) con el nombre de cada destino
//   str:texto        strings definidas que contienen "texto" + xrefs (máx. 40)
//   sym:0xADDR       símbolos y dato en ADDR
//@category AI_Multiplayer
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.StringDataInstance;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.util.DefinedStringIterator;
import java.io.FileWriter;
import java.io.PrintWriter;
import java.util.Iterator;
import java.util.Set;
import java.util.TreeSet;

public class Dump extends GhidraScript {
  private PrintWriter out;
  private DecompInterface decomp;
  private int maxLines;

  @Override
  public void run() throws Exception {
    String[] a = getScriptArgs();
    out = new PrintWriter(new FileWriter(a[0]));
    maxLines = Integer.parseInt(a[1]);
    decomp = new DecompInterface();
    decomp.openProgram(currentProgram);
    try {
      for (int i = 2; i < a.length; i++) {
        monitor.checkCancelled();
        String c = a[i];
        out.println("######## " + c);
        try {
          if (c.startsWith("fn:")) fn(addr(c.substring(3)), true);
          else if (c.startsWith("hdr:")) fn(addr(c.substring(4)), false);
          else if (c.startsWith("xref:")) xref(addr(c.substring(5)));
          else if (c.startsWith("xrefw:")) {
            int n = 0;
            for (Reference r : getReferencesTo(addr(c.substring(6)))) {
              if (!r.getReferenceType().isWrite()) continue;
              if (++n > 60) break;
              out.printf("  %s %s en %s%n", r.getReferenceType(), r.getFromAddress(), fname(r.getFromAddress()));
            }
            if (n == 0) out.println("  sin escrituras");
          }
          else if (c.startsWith("vt:")) {
            String[] p = c.substring(3).split(":");
            vt(addr(p[0]), Integer.parseInt(p[1]));
          } else if (c.startsWith("str:")) str(c.substring(4));
          else if (c.startsWith("calls:")) calls(addr(c.substring(6)));
          else if (c.startsWith("asm:")) {
            String[] p = c.substring(4).split(":");
            asm(addr(p[0]), Integer.parseInt(p[1]));
          } else if (c.startsWith("rtti:")) rtti(c.substring(5));
          else if (c.startsWith("fieldw:")) {
            // fieldw:GLOBAL:OFF -> MOV reg,[GLOBAL] seguido (≤8 instr.) de MOV [reg+OFF],x
            String[] p = c.substring(7).split(":");
            Address g = addr(p[0]);
            long off = Long.decode(p[1]);
            int n = 0;
            for (Reference r : getReferencesTo(g)) {
              ghidra.program.model.listing.Instruction ins = getInstructionAt(r.getFromAddress());
              if (ins == null || !ins.getMnemonicString().equals("MOV") || ins.getNumOperands() < 2) continue;
              Object[] o0 = ins.getOpObjects(0);
              if (o0.length != 1 || !(o0[0] instanceof ghidra.program.model.lang.Register)) continue;
              String reg = o0[0].toString();
              ghidra.program.model.listing.Instruction x = ins.getNext();
              for (int j = 0; j < 8 && x != null; j++, x = x.getNext()) {
                String s = x.toString();
                String pat = "[" + reg + " + 0x" + Long.toHexString(off) + "]";
                if (off == 0) pat = "[" + reg + "]";
                if (s.startsWith("MOV ") && s.contains(pat) && s.indexOf(pat) < s.indexOf(',')) {
                  if (++n > 40) break;
                  out.printf("  %s %-40s en %s%n", x.getAddress(), s, fname(x.getAddress()));
                }
                if (x.getMnemonicString().startsWith("CALL") || x.getMnemonicString().startsWith("RET")) break;
              }
            }
            if (n == 0) out.println("  sin escrituras encontradas");
          } else if (c.startsWith("post:")) {
            // post:FUNC:N -> N instrucciones posteriores a cada CALL a FUNC
            String[] p = c.substring(5).split(":");
            int nn = Integer.parseInt(p[1]), k = 0;
            for (Reference r : getReferencesTo(addr(p[0]))) {
              if (!r.getReferenceType().isCall()) continue;
              if (++k > 200) break;
              out.println("  -- " + r.getFromAddress() + " en " + fname(r.getFromAddress()));
              ghidra.program.model.listing.Instruction x = getInstructionAt(r.getFromAddress());
              for (int j = 0; j < nn && x != null; j++) {
                x = x.getNext();
                if (x != null) out.printf("     %s  %s%n", x.getAddress(), x.toString());
              }
            }
          } else if (c.startsWith("both:")) {
            // both:A:B -> funciones que llaman (directo) a A y a B
            String[] p = c.substring(5).split(":");
            Set<Address> ca = new TreeSet<>(), cb = new TreeSet<>();
            for (Reference r : getReferencesTo(addr(p[0]))) {
              Function f = getFunctionContaining(r.getFromAddress());
              if (f != null) ca.add(f.getEntryPoint());
            }
            for (Reference r : getReferencesTo(addr(p[1]))) {
              Function f = getFunctionContaining(r.getFromAddress());
              if (f != null) cb.add(f.getEntryPoint());
            }
            ca.retainAll(cb);
            for (Address fa : ca) out.println("  " + fname(fa));
            if (ca.isEmpty()) out.println("  ninguna");
          } else if (c.startsWith("pairs:")) {
            // pairs:FUNC -> pares (string empujada con PUSH imm, siguiente MOV [ESP+x],imm) dentro de la función
            Function f = getFunctionContaining(addr(c.substring(6)));
            String last = null;
            int n = 0;
            for (ghidra.program.model.listing.Instruction x : currentProgram.getListing().getInstructions(f.getBody(), true)) {
              String s = x.toString();
              if (s.startsWith("PUSH 0x")) {
                String sv = stringAt(Long.decode(s.substring(5)));
                if (sv != null) last = sv;
              } else if (last != null && s.matches("MOV dword ptr \\[ESP \\+ 0x[0-9a-f]+\\],0x[0-9a-f]+")) {
                String imm = s.substring(s.lastIndexOf(",") + 1);
                out.printf("  %s %s %s%n", x.getAddress(), imm, last);
                last = null;
                n++;
              }
            }
            out.println("  total=" + n);
          } else if (c.startsWith("seq:")) {
            // seq:REGEX1|REGEX2|N|START|END  (separador '|') -> A seguida de B en <= N instrucciones
            String[] p = c.substring(4).split("\\|");
            java.util.regex.Pattern pa = java.util.regex.Pattern.compile(p[0]);
            java.util.regex.Pattern pb = java.util.regex.Pattern.compile(p[1]);
            int win = Integer.parseInt(p[2]), n = 0;
            for (ghidra.program.model.listing.Instruction x : currentProgram.getListing().getInstructions(
                new ghidra.program.model.address.AddressSet(addr(p[3]), addr(p[4])), true)) {
              if (!pa.matcher(x.toString()).find()) continue;
              ghidra.program.model.listing.Instruction y = x.getNext();
              for (int j = 0; j < win && y != null; j++, y = y.getNext()) {
                if (pb.matcher(y.toString()).find()) {
                  if (++n <= 80) out.printf("  %s %-34s | %s %-34s en %s%n", x.getAddress(), x, y.getAddress(), y, fname(x.getAddress()));
                  break;
                }
              }
            }
            out.println("  total=" + n);
          } else if (c.startsWith("insn:")) {
            // insn:REGEX:START:END -> instrucciones cuyo texto matchea (Java regex), máx. 80
            int last = c.lastIndexOf(':'), prev = c.lastIndexOf(':', last - 1);
            java.util.regex.Pattern pt = java.util.regex.Pattern.compile(c.substring(5, prev));
            Address s0 = addr(c.substring(prev + 1, last)), s1 = addr(c.substring(last + 1));
            int n = 0;
            for (ghidra.program.model.listing.Instruction x : currentProgram.getListing().getInstructions(
                new ghidra.program.model.address.AddressSet(s0, s1), true)) {
              if (!pt.matcher(x.toString()).find()) continue;
              if (++n > 80) {
                out.println("  ... (más)");
                break;
              }
              out.printf("  %s %-44s en %s%n", x.getAddress(), x.toString(), fname(x.getAddress()));
            }
            if (n == 0) out.println("  sin coincidencias");
          } else if (c.startsWith("funcs:")) {
            String[] p = c.substring(6).split(":");
            for (Function f : currentProgram.getFunctionManager().getFunctions(
                new ghidra.program.model.address.AddressSet(addr(p[0]), addr(p[1])), true)) {
              out.printf("  %s %-28s size=%d refs=%d%n", f.getEntryPoint(), f.getName(), f.getBody().getNumAddresses(),
                  getReferencesTo(f.getEntryPoint()).length);
            }
          } else if (c.startsWith("pre:")) {
            String[] p = c.substring(4).split(":");
            pre(addr(p[0]), Integer.parseInt(p[1]));
          }
          else if (c.startsWith("sym:")) sym(addr(c.substring(4)));
          else out.println("comando desconocido");
        } catch (Exception e) {
          out.println("ERROR: " + e);
        }
        out.println();
      }
    } finally {
      decomp.dispose();
      out.close();
    }
  }

  private Address addr(String s) {
    return toAddr(Long.decode(s.trim()));
  }

  private String fname(Address a) {
    Function f = getFunctionContaining(a);
    return f == null ? "(sin función)" : f.getName(true) + "@" + f.getEntryPoint();
  }

  private void fn(Address a, boolean code) throws Exception {
    FunctionManager fm = currentProgram.getFunctionManager();
    Function f = fm.getFunctionContaining(a);
    if (f == null) {
      out.println("no hay función en " + a);
      return;
    }
    out.printf("%s @ %s size=%d thunk=%s cc=%s sig=%s%n", f.getName(true), f.getEntryPoint(),
        f.getBody().getNumAddresses(), f.isThunk(), f.getCallingConventionName(), f.getSignature());
    Set<String> callers = new TreeSet<>();
    int refs = 0;
    for (Reference r : getReferencesTo(f.getEntryPoint())) {
      refs++;
      Function c = fm.getFunctionContaining(r.getFromAddress());
      callers.add((c == null ? "(dato/sin función)" : c.getName(true) + "@" + c.getEntryPoint())
          + " [" + r.getReferenceType() + " desde " + r.getFromAddress() + "]");
      if (callers.size() > 40) break;
    }
    out.println("refs_a_entry=" + refs);
    for (String s : callers) out.println("  caller " + s);
    Set<String> callees = new TreeSet<>();
    for (Function c : f.getCalledFunctions(monitor)) callees.add(c.getName(true) + "@" + c.getEntryPoint());
    out.println("callees(" + callees.size() + "): " + String.join(", ", callees));
    if (!code) return;
    DecompileResults r = decomp.decompileFunction(f, 120, monitor);
    if (r == null || !r.decompileCompleted()) {
      out.println("decompilación fallida: " + (r == null ? "null" : r.getErrorMessage()));
      return;
    }
    String[] lines = r.getDecompiledFunction().getC().split("\n");
    int n = 0;
    for (String l : lines) {
      if (l.isBlank()) continue;
      if (++n > maxLines) {
        out.println("  ... (" + (lines.length - maxLines) + " líneas más)");
        break;
      }
      out.println(l);
    }
  }

  private void xref(Address a) {
    int n = 0;
    for (Reference r : getReferencesTo(a)) {
      if (++n > 60) {
        out.println("  ... (más)");
        break;
      }
      out.printf("  %s %s en %s%n", r.getReferenceType(), r.getFromAddress(), fname(r.getFromAddress()));
    }
    if (n == 0) out.println("  sin referencias");
  }

  private void vt(Address a, int count) throws Exception {
    Memory m = currentProgram.getMemory();
    Symbol s = getSymbolAt(a);
    out.println("símbolo: " + (s == null ? "-" : s.getName(true)));
    for (int i = 0; i < count; i++) {
      Address slot = a.add(4L * i);
      long v = m.getInt(slot) & 0xffffffffL;
      Address t = toAddr(v);
      Function f = getFunctionAt(t);
      Symbol ts = getSymbolAt(t);
      Symbol ss = i > 0 ? getSymbolAt(slot) : null;
      if (ss != null) out.println("  == " + ss.getName(true) + " @ " + slot);
      out.printf("  [%2d] +0x%02x %s -> 0x%08x %s%n", i, 4 * i, slot, v,
          f != null ? f.getName(true) : (ts != null ? "(" + ts.getName(true) + ")" : "?"));
    }
  }

  private void str(String needle) {
    int n = 0;
    for (Iterator<Data> it = DefinedStringIterator.forProgram(currentProgram); it.hasNext();) {
      Data d = it.next();
      StringDataInstance sdi = StringDataInstance.getStringDataInstance(d);
      String v = sdi.getStringValue();
      if (v == null || !v.contains(needle)) continue;
      if (++n > 40) {
        out.println("  ... (más)");
        break;
      }
      out.printf("  %s \"%s\"%n", d.getAddress(), v.length() > 140 ? v.substring(0, 140) + "…" : v);
      int k = 0;
      for (Reference r : getReferencesTo(d.getAddress())) {
        if (++k > 8) {
          out.println("      ...");
          break;
        }
        out.printf("      ref %s en %s%n", r.getFromAddress(), fname(r.getFromAddress()));
      }
    }
    if (n == 0) out.println("  sin coincidencias");
  }

  // Para cada llamada directa a la función en ADDR: sitio, función contenedora y argumentos
  // (constantes, strings apuntadas o nombre de variable), recuperados del decompilador.
  private void calls(Address target) throws Exception {
    FunctionManager fm = currentProgram.getFunctionManager();
    Function tf = fm.getFunctionAt(target);
    Set<Address> entries = new TreeSet<>();
    entries.add(target);
    // incluir thunks que saltan a la función
    for (Reference r : getReferencesTo(target)) {
      Function c = fm.getFunctionContaining(r.getFromAddress());
      if (c != null && c.isThunk()) entries.add(c.getEntryPoint());
    }
    TreeSet<Address> owners = new TreeSet<>();
    for (Address e : entries)
      for (Reference r : getReferencesTo(e)) {
        Function c = fm.getFunctionContaining(r.getFromAddress());
        if (c != null && !c.isThunk()) owners.add(c.getEntryPoint());
      }
    out.println("destino " + (tf == null ? target : tf.getName(true)) + " entradas=" + entries + " funciones_llamadoras=" + owners.size());
    int shown = 0;
    for (Address oa : owners) {
      if (++shown > 80) {
        out.println("  ... (más)");
        break;
      }
      Function o = fm.getFunctionAt(oa);
      DecompileResults dr = decomp.decompileFunction(o, 60, monitor);
      if (dr == null || !dr.decompileCompleted()) {
        out.println("  " + o.getName(true) + ": decompilación fallida");
        continue;
      }
      ghidra.program.model.pcode.HighFunction hf = dr.getHighFunction();
      for (Iterator<ghidra.program.model.pcode.PcodeOpAST> it = hf.getPcodeOps(); it.hasNext();) {
        ghidra.program.model.pcode.PcodeOpAST op = it.next();
        if (op.getOpcode() != ghidra.program.model.pcode.PcodeOp.CALL) continue;
        ghidra.program.model.pcode.Varnode t = op.getInput(0);
        if (t == null || !t.isAddress() || !entries.contains(t.getAddress())) continue;
        StringBuilder sb = new StringBuilder();
        for (int i = 1; i < op.getNumInputs(); i++) {
          if (i > 1) sb.append(", ");
          sb.append(describe(op.getInput(i), 0));
        }
        out.printf("  %s en %s@%s args=[%s]%n", op.getSeqnum().getTarget(), o.getName(true), oa, sb);
      }
    }
  }

  private String describe(ghidra.program.model.pcode.Varnode v, int depth) {
    if (v == null) return "?";
    if (v.isConstant()) {
      String s = stringAt(v.getOffset());
      return s != null ? String.format("0x%x %s", v.getOffset(), s) : String.format("0x%x", v.getOffset());
    }
    ghidra.program.model.pcode.PcodeOp def = v.getDef();
    if (def != null && depth < 4) {
      int opc = def.getOpcode();
      if (opc == ghidra.program.model.pcode.PcodeOp.COPY || opc == ghidra.program.model.pcode.PcodeOp.CAST)
        return describe(def.getInput(0), depth + 1);
      if (opc == ghidra.program.model.pcode.PcodeOp.PTRSUB && def.getInput(0).isConstant() && def.getInput(1).isConstant()) {
        long off = def.getInput(0).getOffset() + def.getInput(1).getOffset();
        String s = stringAt(off);
        return s != null ? String.format("0x%x %s", off, s) : String.format("&0x%x", off);
      }
    }
    if (v.getHigh() != null && v.getHigh().getName() != null) return "var:" + v.getHigh().getName();
    return "expr";
  }

  private String stringAt(long off) {
    try {
      Data d = getDataAt(toAddr(off));
      if (d == null) return null;
      StringDataInstance sdi = StringDataInstance.getStringDataInstance(d);
      if (sdi == StringDataInstance.NULL_INSTANCE) return null;
      String val = sdi.getStringValue();
      if (val == null) return null;
      val = val.replace("\n", "\\n");
      return "\"" + (val.length() > 100 ? val.substring(0, 100) + "…" : val) + "\"";
    } catch (Exception e) {
      return null;
    }
  }

  // N instrucciones desde ADDR, con referencias a datos/funciones resueltas.
  private void asm(Address a, int n) {
    ghidra.program.model.listing.Instruction ins = getInstructionAt(a);
    if (ins == null) ins = getInstructionAfter(a);
    for (int i = 0; i < n && ins != null; i++) {
      StringBuilder refs = new StringBuilder();
      for (Reference r : ins.getReferencesFrom()) {
        Address t = r.getToAddress();
        Symbol s = getSymbolAt(t);
        String sv = stringAt(t.getOffset());
        refs.append(" ->").append(t).append(s != null ? "(" + s.getName(true) + ")" : "").append(sv != null ? sv : "");
      }
      out.printf("  %s  %-40s%s%n", ins.getAddress(), ins.toString(), refs);
      ins = ins.getNext();
    }
  }

  // Para cada llamada a ADDR: las N instrucciones anteriores al CALL (para ver de dónde sale ECX/args).
  private void pre(Address target, int n) {
    int k = 0;
    for (Reference r : getReferencesTo(target)) {
      if (!r.getReferenceType().isCall()) continue;
      if (++k > 60) {
        out.println("  ... (más)");
        break;
      }
      Address site = r.getFromAddress();
      out.println("  -- " + site + " en " + fname(site));
      java.util.ArrayDeque<ghidra.program.model.listing.Instruction> q = new java.util.ArrayDeque<>();
      ghidra.program.model.listing.Instruction ins = getInstructionAt(site);
      for (int i = 0; i < n && ins != null; i++) {
        ins = ins.getPrevious();
        if (ins != null) q.addFirst(ins);
      }
      for (ghidra.program.model.listing.Instruction x : q) {
        StringBuilder refs = new StringBuilder();
        for (Reference rr : x.getReferencesFrom()) {
          Symbol s = getSymbolAt(rr.getToAddress());
          if (s != null && !rr.getToAddress().isStackAddress()) refs.append(" ->").append(s.getName(true));
        }
        out.printf("     %s  %-36s%s%n", x.getAddress(), x.toString(), refs);
      }
    }
  }

  // Clases RTTI cuyo nombre (namespace incluido) contiene el texto; muestra la(s) vftable(s).
  private void rtti(String needle) {
    int n = 0;
    for (ghidra.program.model.symbol.SymbolIterator it = currentProgram.getSymbolTable().getAllSymbols(true); it.hasNext();) {
      Symbol s = it.next();
      if (!s.getName().equals("vftable")) continue;
      String cls = s.getParentNamespace().getName(true);
      if (!cls.toLowerCase().contains(needle.toLowerCase())) continue;
      if (++n > 60) {
        out.println("  ... (más)");
        break;
      }
      int refs = getReferencesTo(s.getAddress()).length;
      out.printf("  %s vftable@%s refs=%d%n", cls, s.getAddress(), refs);
    }
    if (n == 0) out.println("  sin coincidencias");
  }

  private void sym(Address a) {
    for (Symbol s : currentProgram.getSymbolTable().getSymbols(a)) out.println("  símbolo " + s.getName(true) + " (" + s.getSymbolType() + ")");
    Data d = getDataAt(a);
    out.println("  dato: " + (d == null ? "-" : d.getDataType().getName() + " = " + d.getDefaultValueRepresentation()));
    out.println("  en función: " + fname(a));
  }
}
