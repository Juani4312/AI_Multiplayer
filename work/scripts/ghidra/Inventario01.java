// Hallazgo 01: inventario de uso de imports, carga dinámica de DLLs y RTTI.
// Solo lectura: correr con -process AI.exe -noanalysis -readOnly.
// Uso: analyzeHeadless <proj_dir> <proj_name> -process AI.exe -noanalysis -readOnly \
//        -scriptPath work/scripts/ghidra -postScript Inventario01.java <salida.txt>
//@category AI_Multiplayer
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.StringDataInstance;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.pcode.HighFunction;
import ghidra.program.model.pcode.PcodeOp;
import ghidra.program.model.pcode.PcodeOpAST;
import ghidra.program.model.pcode.Varnode;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceManager;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;
import ghidra.program.model.symbol.SymbolTable;
import ghidra.program.util.DefinedStringIterator;
import java.io.FileWriter;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.TreeSet;

public class Inventario01 extends GhidraScript {
  // Imports cuyos call sites se listan completos, con argumentos recuperados.
  private static final Set<String> DETAIL = Set.of(
      "LoadLibraryA", "LoadLibraryW", "GetProcAddress", "GetModuleHandleA",
      "GetModuleHandleW", "FreeLibrary", "IsDebuggerPresent", "GetThreadContext",
      "SetThreadContext", "SuspendThread", "VirtualProtect", "timeGetTime",
      "timeBeginPeriod", "timeEndPeriod", "SteamAPI_Init",
      "SteamAPI_RestartAppIfNecessary", "SteamAPI_RunCallbacks", "SteamAPI_Shutdown",
      "SetUnhandledExceptionFilter", "CoCreateInstance", "D3D11CreateDeviceAndSwapChain",
      "CreateDXGIFactory", "CreateDXGIFactory1", "PeekMessageA", "PeekMessageW",
      "GetProcessMemoryInfo", "TerminateProcess", "ExitProcess", "OutputDebugStringA",
      "OutputDebugStringW", "GetRawInputData", "RegisterRawInputDevices");

  private PrintWriter out;
  private DecompInterface decomp;
  private final Map<Address, HighFunction> highCache = new LinkedHashMap<>();

  @Override
  public void run() throws Exception {
    String[] args = getScriptArgs();
    if (args.length != 1) throw new IllegalArgumentException("falta la ruta de salida");
    out = new PrintWriter(new FileWriter(args[0]));
    try {
      decomp = new DecompInterface();
      decomp.openProgram(currentProgram);
      report();
    } finally {
      if (decomp != null) decomp.dispose();
      out.close();
    }
  }

  private void report() throws Exception {
    FunctionManager fm = currentProgram.getFunctionManager();
    ReferenceManager rm = currentProgram.getReferenceManager();
    SymbolTable st = currentProgram.getSymbolTable();
    Listing listing = currentProgram.getListing();

    out.printf("program=%s imagebase=%s sha256=%s language=%s compiler=%s%n",
        currentProgram.getName(), currentProgram.getImageBase(),
        currentProgram.getExecutableSHA256(), currentProgram.getLanguageID(),
        currentProgram.getCompilerSpec().getCompilerSpecID());
    int ext = 0, thunks = 0, total = 0;
    for (Function f : fm.getFunctions(true)) {
      total++;
      if (f.isThunk()) thunks++;
    }
    for (Iterator<Function> it = fm.getExternalFunctions(); it.hasNext(); it.next()) ext++;
    out.printf("functions_internal=%d thunks=%d external=%d%n", total, thunks, ext);
    int strings = 0;
    for (Iterator<Data> it = DefinedStringIterator.forProgram(currentProgram); it.hasNext(); it.next()) strings++;
    out.printf("defined_strings=%d%n", strings);

    // RTTI: descriptores de tipo y vftables creados por el analizador de RTTI.
    int typeDesc = 0, vftables = 0;
    Map<String, Integer> nsCount = new TreeMap<>();
    for (SymbolIterator it = st.getAllSymbols(true); it.hasNext();) {
      Symbol s = it.next();
      String n = s.getName();
      if (n.equals("RTTI_Type_Descriptor")) {
        typeDesc++;
        String parent = s.getParentNamespace().getName(true);
        String top = parent.contains("::") ? parent.substring(0, parent.indexOf("::")) : "(global)";
        nsCount.merge(top, 1, Integer::sum);
      } else if (n.equals("vftable")) {
        vftables++;
      }
    }
    out.printf("rtti_type_descriptors=%d vftable_labels=%d%n", typeDesc, vftables);
    nsCount.entrySet().stream().sorted((a, b) -> b.getValue() - a.getValue()).limit(12)
        .forEach(e -> out.printf("  rtti_ns %-24s %d%n", e.getKey(), e.getValue()));

    // Uso de cada import: slots de IAT y referencias desde código.
    out.println();
    out.println("== uso de imports (lib, nombre, slots IAT, sitios de código, funciones distintas) ==");
    List<Function> detailFns = new ArrayList<>();
    for (Iterator<Function> it = fm.getExternalFunctions(); it.hasNext();) {
      Function f = it.next();
      String lib = f.getExternalLocation().getLibraryName();
      TreeSet<Address> slots = new TreeSet<>();
      TreeSet<Address> sites = new TreeSet<>();
      for (Reference r : rm.getReferencesTo(f.getEntryPoint())) {
        Address from = r.getFromAddress();
        if (listing.getInstructionAt(from) != null) sites.add(from);
        else slots.add(from);
      }
      for (Address slot : slots) {
        for (Reference r : rm.getReferencesTo(slot)) {
          if (listing.getInstructionAt(r.getFromAddress()) != null) sites.add(r.getFromAddress());
        }
      }
      TreeSet<Address> owners = new TreeSet<>();
      for (Address a : sites) {
        Function o = fm.getFunctionContaining(a);
        if (o != null) owners.add(o.getEntryPoint());
      }
      out.printf("%-14s %-34s slots=%s sites=%d fns=%d%n", lib, f.getName(), slots, sites.size(), owners.size());
      if (DETAIL.contains(f.getName())) detailFns.add(f);
    }

    // Detalle con argumentos recuperados por el decompilador.
    for (Function f : detailFns) {
      out.println();
      out.printf("== %s::%s ==%n", f.getExternalLocation().getLibraryName(), f.getName());
      TreeSet<Address> slots = new TreeSet<>();
      TreeSet<Address> sites = new TreeSet<>();
      for (Reference r : rm.getReferencesTo(f.getEntryPoint())) {
        if (listing.getInstructionAt(r.getFromAddress()) != null) sites.add(r.getFromAddress());
        else slots.add(r.getFromAddress());
      }
      for (Address slot : slots)
        for (Reference r : rm.getReferencesTo(slot))
          if (listing.getInstructionAt(r.getFromAddress()) != null) sites.add(r.getFromAddress());
      TreeMap<Address, Function> owners = new TreeMap<>();
      for (Address a : sites) {
        Function o = fm.getFunctionContaining(a);
        if (o != null) owners.put(o.getEntryPoint(), o);
        else out.printf("  site %s (sin función contenedora): %s%n", a, listing.getInstructionAt(a));
      }
      for (Function owner : owners.values()) {
        monitor.checkCancelled();
        List<String> calls = callsTo(owner, f, slots);
        out.printf("  fn %s %s%n", owner.getEntryPoint(), owner.getName(true));
        if (calls.isEmpty()) {
          for (Address a : sites) {
            if (owner.getBody().contains(a)) {
              Instruction ins = listing.getInstructionAt(a);
              out.printf("    ref %s: %s (decompilador sin CALL resuelto)%n", a, ins);
            }
          }
        }
        for (String c : calls) out.println("    " + c);
      }
    }
  }

  private List<String> callsTo(Function owner, Function ext, Set<Address> slots) {
    List<String> res = new ArrayList<>();
    HighFunction hf = highCache.get(owner.getEntryPoint());
    if (hf == null) {
      DecompileResults dr = decomp.decompileFunction(owner, 60, monitor);
      if (dr == null || !dr.decompileCompleted()) {
        res.add("decompilación fallida: " + (dr == null ? "null" : dr.getErrorMessage()));
        return res;
      }
      hf = dr.getHighFunction();
      highCache.put(owner.getEntryPoint(), hf);
    }
    for (Iterator<PcodeOpAST> it = hf.getPcodeOps(); it.hasNext();) {
      PcodeOpAST op = it.next();
      int opc = op.getOpcode();
      if (opc != PcodeOp.CALL && opc != PcodeOp.CALLIND) continue;
      Varnode target = op.getInput(0);
      if (!targets(target, ext, slots)) continue;
      StringBuilder sb = new StringBuilder();
      sb.append("call @").append(op.getSeqnum().getTarget()).append(" args=[");
      for (int i = 1; i < op.getNumInputs(); i++) {
        if (i > 1) sb.append(", ");
        sb.append(describe(op.getInput(i), 0));
      }
      sb.append("]");
      res.add(sb.toString());
    }
    return res;
  }

  private boolean targets(Varnode v, Function ext, Set<Address> slots) {
    if (v == null) return false;
    if (v.isAddress()) {
      Address a = v.getAddress();
      if (a.equals(ext.getEntryPoint()) || slots.contains(a)) return true;
      Function f = currentProgram.getFunctionManager().getFunctionAt(a);
      if (f != null && f.isThunk() && ext.equals(f.getThunkedFunction(true))) return true;
    }
    PcodeOp def = v.getDef();
    if (def != null && def.getOpcode() == PcodeOp.LOAD) {
      Varnode ptr = def.getInput(1);
      if (ptr.isConstant()) {
        Address a = toAddr(ptr.getOffset());
        return slots.contains(a);
      }
    }
    if (def != null && def.getOpcode() == PcodeOp.COPY) return targets(def.getInput(0), ext, slots);
    return false;
  }

  private String describe(Varnode v, int depth) {
    if (v == null) return "?";
    if (v.isConstant()) {
      long off = v.getOffset();
      String s = stringAt(off);
      return s != null ? String.format("0x%x %s", off, s) : String.format("0x%x", off);
    }
    PcodeOp def = v.getDef();
    if (def != null && depth < 4) {
      int opc = def.getOpcode();
      if (opc == PcodeOp.COPY || opc == PcodeOp.CAST || opc == PcodeOp.INT_ZEXT) {
        return describe(def.getInput(0), depth + 1);
      }
      if (opc == PcodeOp.PTRSUB && def.getInput(0).isConstant() && def.getInput(1).isConstant()) {
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
      Address a = toAddr(off);
      Data d = getDataAt(a);
      if (d == null) d = getDataContaining(a);
      if (d == null) return null;
      StringDataInstance sdi = StringDataInstance.getStringDataInstance(d);
      if (sdi == StringDataInstance.NULL_INSTANCE) return null;
      String val = sdi.getStringValue();
      return val == null ? null : "\"" + val.replace("\n", "\\n") + "\"";
    } catch (Exception e) {
      return null;
    }
  }
}
