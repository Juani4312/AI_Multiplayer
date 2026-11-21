// cathode-dump: volcados de solo lectura de archivos de CATHODE (Alien: Isolation) con CathodeLib.
// Nunca escribe sobre los archivos del juego: solo los lee y escribe texto en la salida indicada.
//
//   cathode-dump commands <COMMANDS.PAK> [salida.tsv]   árbol de instancias con el pid del mod (pins.c)
//   cathode-dump bml <archivo.BML> [salida.xml]         BML (árboles de comportamiento, configs) a XML
//   cathode-dump pak2 <UI.PAK|ANIMATION.PAK> <carpeta>  extrae un PAK2 (UI .GFX, animaciones)
//   cathode-dump chars <CHARACTERACCESSORYSETS.BIN> [salida.tsv]  personajes del nivel: id de 8 bytes como lo
//                                                       muestra el mod (personaje del RPC 0x26), género, contextura...
//
// Los nombres salen de CathodeLib (tabla de nombres de OpenCAGE): sirven para orientarse, no son
// evidencia de offsets. El pid replica path_id/pid_of de mod/src/pins.c (hallazgo 15).
using System.Text;
using System.Xml;
using CATHODE;
using CATHODE.Scripting;
using CATHODE.Scripting.Internal;

static class Program
{
    static int Main(string[] args)
    {
        if (args.Length < 2) return Usage();
        try
        {
            switch (args[0])
            {
                case "commands": return DumpCommands(args[1], args.Length > 2 ? args[2] : null);
                case "bml": return DumpBml(args[1], args.Length > 2 ? args[2] : null);
                case "pak2": return args.Length > 2 ? DumpPak2(args[1], args[2]) : Usage();
                case "chars": return DumpChars(args[1], args.Length > 2 ? args[2] : null);
                default: return Usage();
            }
        }
        catch (Exception e)
        {
            Console.Error.WriteLine("error: " + e);
            return 1;
        }
    }

    static int Usage()
    {
        Console.Error.WriteLine("uso: cathode-dump commands <COMMANDS.PAK> [salida.tsv]\n" +
                                "     cathode-dump bml <archivo.BML> [salida.xml]\n" +
                                "     cathode-dump pak2 <archivo.PAK> <carpeta>");
        return 2;
    }

    static TextWriter Open(string path) =>
        path == null ? Console.Out : new StreamWriter(path, false, new UTF8Encoding(false));

    // --- pid: igual que mod/src/pins.c ---
    static uint Hash2(uint a, uint b)
    {
        uint h = unchecked(a * 0x9E3779B1u ^ (b + 0x7F4A7C15u + (a << 6) + (a >> 2)));
        h ^= h >> 15;
        h = unchecked(h * 0x85EBCA6Bu);
        h ^= h >> 13;
        return h;
    }

    // anc[0] = raíz; el hash va de la entidad hacia la raíz (pid_of en pins.c).
    static uint PidOf(uint eid, List<uint> anc)
    {
        uint x = Hash2(0x811C9DC5u, eid);
        for (int j = anc.Count; j-- > 0;) x = Hash2(x, anc[j]);
        return x;
    }

    static string TypeName(Commands cmd, ShortGuid fn)
    {
        Composite c = cmd.GetComposite(fn);
        if (c != null) return "composite:" + c.name;
        if (Enum.IsDefined(typeof(FunctionType), fn.ToUInt32())) return ((FunctionType)fn.ToUInt32()).ToString();
        string s = ShortGuidUtils.FindString(fn);
        return string.IsNullOrEmpty(s) ? "?" : s;
    }

    static int DumpCommands(string pak, string outPath)
    {
        // CommandsPAK resuelve los recursos de las entidades (modelos, colisiones, animaciones del
        // entorno) contra REDS.BIN, COLLISION.MAP y ENVIRONMENT_ANIMATION.DAT, que a su vez piden
        // modelos y materiales. No hacen falta para el árbol: se pasan vacíos (índice fuera de rango
        // -> null). Level/Global de CathodeLib arman rutas con '\\' y en mayúsculas: en Linux no sirven.
        string none = Path.Combine(Path.GetTempPath(), "cathode-dump-no-existe");
        var reds = new RenderableElements(none, null, null);
        var colMaps = new CollisionMaps(none, null, null);
        var envAnims = new EnvironmentAnimations(none, null);
        var cmd = new Commands(pak, envAnims, colMaps, reds);
        if (!cmd.Loaded) { Console.Error.WriteLine("no se pudo cargar " + pak); return 1; }

        using TextWriter w = Open(outPath);
        w.WriteLine("# cathode-dump commands " + Path.GetFullPath(pak));
        w.WriteLine("# composites: " + cmd.Entries.Count + "; raíces: " +
                    string.Join(", ", (cmd.EntryPoints ?? Array.Empty<Composite>()).Select(c => c?.name)));
        w.WriteLine("pid\tdepth\tid\ttype\tvariant\ttype_name\tname\tpath\tlinks_out\tanc_ids\tanc_types\tinst");

        long rows = 0;
        var anc = new List<uint>();
        var ancT = new List<uint>();
        var names = new List<string>();
        var stack = new HashSet<ShortGuid>();
        ShortGuid instTerm = ShortGuid.InstanceGuid;

        void Walk(Composite comp)
        {
            if (!stack.Add(comp.shortGUID)) return; // ciclo (no debería pasar)
            foreach (Entity e in comp.GetEntities())
            {
                uint eid = e.shortGUID.ToUInt32();
                uint type = 0;
                string tname = e.variant.ToString();
                Composite child = null;
                if (e is FunctionEntity f)
                {
                    type = f.function.ToUInt32();
                    tname = TypeName(cmd, f.function);
                    child = cmd.GetComposite(f.function);
                }
                else if (e is VariableEntity v)
                {
                    type = v.name.ToUInt32();
                    tname = "variable:" + ShortGuidUtils.FindString(v.name) + ":" + v.type;
                }
                string name = cmd.Utils.GetEntityName(comp, e) ?? "";
                string links = string.Join(",", e.childLinks.Select(l =>
                    ShortGuidUtils.FindString(l.thisParamID) + ">" + l.linkedEntityID.ToUInt32().ToString("x8") + "." +
                    ShortGuidUtils.FindString(l.linkedParamID)));
                string path = string.Join("/", names) + "/" + name;
                // inst: id de instancia del composite que contiene a la entidad (EntityHandle de CathodeLib:
                // ruta desde la raíz sin ella + la entidad). Con el id de la entidad forma el id de 8 bytes de los
                // objetos usables del RPC 0x26 (corrida 119: "eed179bb11699aac" = bytes de bb79d1ee y de inst).
                var hier = anc.Skip(1).Select(x => new ShortGuid(x)).ToList();
                hier.Add(e.shortGUID);
                uint inst = hier.GenerateCompositeInstanceID(true, instTerm).ToUInt32();
                w.WriteLine($"{PidOf(eid, anc):x8}\t{anc.Count + 1}\t{eid:x8}\t{type:x8}\t{e.variant}\t{tname}\t{name}\t{path}\t{links}\t{Hex(anc)}\t{Hex(ancT)}\t{inst:x8}");
                rows++;
                if (child != null)
                {
                    anc.Add(eid); ancT.Add(type); names.Add(name);
                    Walk(child);
                    anc.RemoveAt(anc.Count - 1); ancT.RemoveAt(ancT.Count - 1); names.RemoveAt(names.Count - 1);
                }
            }
            stack.Remove(comp.shortGUID);
        }

        // Último eslabón de la cadena del pid (verificado contra las trazas de HAB_ShoppingCentre,
        // 11 726 de 14 055 pids exactos; ver work/NOTES.md, "cathode-dump"):
        //  - raíz del nivel (EntryPoints[0]): ShortGuid.InstanceGuid (0x4AF05BFE), no el guid del composite;
        //  - Global y PauseMenu: el guid de su propio composite (en el juego, d1 con id == tipo).
        // Lo que falta son entidades creadas en runtime (arrojables, armas, NPCs que aparecen) y
        // composites compartidos que el juego instancia más arriba que en el árbol del archivo.
        var entry = cmd.EntryPoints ?? Array.Empty<Composite>();
        for (int ri = 0; ri < entry.Length; ri++)
        {
            Composite root = entry[ri];
            if (root == null) continue;
            uint rid = root.shortGUID.ToUInt32();
            uint term = ri == 0 ? ShortGuid.InstanceGuid.ToUInt32() : rid;
            instTerm = ri == 0 ? ShortGuid.InstanceGuid : ri == 1 ? ShortGuid.GlobalGuid : ShortGuid.PauseGuid;
            w.WriteLine($"{PidOf(term, anc):x8}\t1\t{term:x8}\t{rid:x8}\tROOT\tcomposite:{root.name}\t{root.name}\t{root.name}\t\t\t\t{instTerm.ToUInt32():x8}");
            rows++;
            anc.Add(term); ancT.Add(rid); names.Add(root.name);
            Walk(root);
            anc.Clear(); ancT.Clear(); names.Clear();
        }
        Console.Error.WriteLine($"{rows} filas");
        return 0;
    }

    static string Hex(List<uint> l) => string.Join(",", l.Select(v => v.ToString("x8")));

    static int DumpBml(string path, string outPath)
    {
        var bml = new BML(path);
        if (!bml.Loaded || bml.Content == null) { Console.Error.WriteLine("no se pudo cargar " + path); return 1; }
        var settings = new XmlWriterSettings { Indent = true, Encoding = new UTF8Encoding(false) };
        if (outPath == null)
        {
            using var xw = XmlWriter.Create(Console.Out, settings);
            bml.Content.Save(xw);
        }
        else
        {
            using var xw = XmlWriter.Create(outPath, settings);
            bml.Content.Save(xw);
        }
        return 0;
    }

    // Bytes de un ShortGuid en orden de memoria, como los imprime el mod ("objetivo"/"personaje" de 8 bytes).
    static string MemHex(ShortGuid g) => string.Concat(BitConverter.GetBytes(g.ToUInt32()).Select(b => b.ToString("x2")));

    static int DumpChars(string bin, string outPath)
    {
        var cas = new CharacterAccessorySets(bin);
        if (!cas.Loaded) { Console.Error.WriteLine("no se pudo cargar " + bin); return 1; }
        using TextWriter w = Open(outPath);
        w.WriteLine("handle\tid\tinst\tgender\tethnicity\tbuild\tface_skeleton\tgender_skeleton\tasset_type\tvoice\ttorso\tlegs\thead");
        string Comp(CharacterAccessorySets.CharacterAttributes.Components.Component c) =>
            c == null || c.Composite.IsInvalid ? "" : (ShortGuidUtils.FindString(c.Composite) ?? c.Composite.ToUInt32().ToString("x8"));
        foreach (var e in cas.Entries)
        {
            var h = e.character;
            w.WriteLine($"{MemHex(h.entity_id)}{MemHex(h.composite_instance_id)}\t{h.entity_id.ToUInt32():x8}\t{h.composite_instance_id.ToUInt32():x8}\t" +
                        $"{e.gender}\t{e.ethnicity}\t{e.build}\t{e.face_skeleton}\t{e.gender_skeleton}\t{e.asset_type}\t{e.voice_actor}\t" +
                        $"{Comp(e.components.Torso)}\t{Comp(e.components.Legs)}\t{Comp(e.components.Head)}");
        }
        Console.Error.WriteLine($"{cas.Entries.Count} personajes");
        return 0;
    }

    static int DumpPak2(string pak, string outDir)
    {
        var p = new PAK2(pak);
        if (!p.Loaded) { Console.Error.WriteLine("no se pudo cargar " + pak); return 1; }
        string root = Path.GetFullPath(outDir);
        foreach (var f in p.Entries)
        {
            string dest = Path.GetFullPath(Path.Combine(root, f.Filename.Replace('\\', '/')));
            if (!dest.StartsWith(root + Path.DirectorySeparatorChar)) { Console.Error.WriteLine("ruta rara, salteada: " + f.Filename); continue; }
            Directory.CreateDirectory(Path.GetDirectoryName(dest));
            File.WriteAllBytes(dest, f.Content);
        }
        Console.Error.WriteLine($"{p.Entries.Count} archivos en {root}");
        return 0;
    }
}
