package org.prosperopz;
import java.io.*;
import java.nio.channels.*;
import java.nio.file.*;
import java.security.*;
import java.util.*;
/** Headless Project Zomboid launcher backend used by the PS5 frontend and host CLI.
 *
 * The native UI writes request.properties; this JVM answers with
 * response.properties, progress.properties and launcher-view.properties.
 * Profiles ("instances") pair a game installation with their own Zomboid
 * folder: saves, mods, options and server configuration stay separate. */
public final class Main {
    static final int DEFAULT_MEMORY=4096;
    static final List<String> THEMES=Arrays.asList("knox","blood","rosewood","louisville","muldraugh","westpoint");
    final Path root;
    final Map<String, String> settings;
    final Map<String, String> request;
    final Map<String, String> extraView=new LinkedHashMap<String, String>();
    String message="Ready", status="ready";
    final JavaRuntime runtimes;
    final Games games;
    Main(Path root, Map<String, String> req)throws IOException {
        this.root=root.toAbsolutePath().normalize();
        Files.createDirectories(this.root);
        settings=FilesUtil.properties(this.root.resolve("settings.properties"));
        request=req;
        runtimes=new JavaRuntime(this);
        games=new Games(this);
    }
    static boolean ps5(Path root) {
        return root.toString().replace('\\','/').equalsIgnoreCase("Z:/data/ProsperoPZ");
    }
    public static void main(String[] args)throws Exception {
        Path root=null, requestPath=null, plan=null;
        boolean serve=false;
        for(int i=0; i<args.length; i++) {
            String a=args[i];
            if(a.equals("--root")&&i+1<args.length)root=Paths.get(args[++i]);
            else if(a.equals("--request")&&i+1<args.length)requestPath=Paths.get(args[++i]);
            else if(a.equals("--execute-plan")&&i+1<args.length)plan=Paths.get(args[++i]);
            else if(a.equals("--serve"))serve=true;
            else throw new IllegalArgumentException("Usage: --root PATH --request PATH | --serve | --execute-plan PATH");
        }
        if(root==null)throw new IllegalArgumentException("--root is required");
        if(ps5(root)) {
            if(plan==null)Ps5Compat.resourceArchives();
            System.setProperty("prosperopz.ps5Dns","true");
            System.setProperty("java.net.preferIPv4Stack","true");
            try {
                if(System.getProperty("java.specification.version").equals("1.8"))Ps5Compat.localAddress();
                else Ps5Dns.initializeModern();
                Ps5DirectoryDns.install();
            } catch(Throwable unavailable) {
                // A trimmed game runtime may omit java.naming; networking by IP still works.
                System.out.println("PS5 DNS compatibility unavailable: "+unavailable.getClass().getSimpleName());
            }
        }
        if(serve) {
            if(plan!=null||requestPath!=null)throw new IllegalArgumentException("--serve cannot be combined with a request or plan");
            serve(root);
            return;
        }
        if(plan!=null) {
            Main app=new Main(root, Collections.<String, String>emptyMap());
            if(ps5(root)) {
                FileDiagnostics.probe(root);
                HeapDiagnostics.probe();
                StartupDiagnostics.start(Thread.currentThread());
            }
            new ZomboidLaunch(app).execute(plan);
            return;
        }
        if(requestPath==null)throw new IllegalArgumentException("--request is required");
        if(!perform(root,requestPath))System.exit(1);
    }
    /** One JVM serves launcher work while the native controller UI remains visible. */
    static void serve(Path root)throws Exception {
        Files.createDirectories(root);
        Path queued=root.resolve("request.properties"), active=root.resolve("request.inflight.properties");
        perform(root,stringMap("command","view"),true);
        FilesUtil.properties(root.resolve("backend-ready.properties"),stringMap("ready","true"));
        for(;;) {
            if(!Files.isRegularFile(queued)) { Thread.sleep(50); continue; }
            FilesUtil.atomic(queued,active);
            String command=FilesUtil.properties(active).getOrDefault("command","");
            boolean ok=perform(root,active);
            Files.deleteIfExists(active);
            // The game uses its own JVM in a fresh title process.
            if(ok&&command.equals("prepare-launch"))return;
        }
    }
    static boolean perform(Path root,Path requestPath)throws Exception {
        return perform(root,FilesUtil.properties(requestPath));
    }
    static boolean perform(Path root,Map<String,String> request)throws Exception {
        return perform(root,request,false);
    }
    static boolean perform(Path root,Map<String,String> request,boolean background)throws Exception {
        Main app;
        try { app=new Main(root,request); }
        catch(IOException e) {
            String failure="Invalid launcher request/settings: "+e.getMessage();
            FilesUtil.properties(root.resolve("response.properties"),stringMap("ok","false","command","","message",failure));
            FilesUtil.properties(root.resolve("launcher-view.properties"),stringMap("status","error","detail",failure,"instance.count","0","game.count","0","mod.count","0"));
            System.err.println("ERROR: "+failure);
            return false;
        }
        String command=app.request.getOrDefault("command", "");
        boolean ok=false;
        try(FileChannel channel=FileChannel.open(app.root.resolve("launcher.lock"), StandardOpenOption.CREATE, StandardOpenOption.WRITE); FileLock lock=channel.tryLock()) {
            if(lock==null)throw new IOException("Another launcher operation is running");
            final Main current=app;
            FilesUtil.downloadProgress.set(new FilesUtil.DownloadProgress() {
                public void update(Path file,long done,long total)throws IOException {
                    String mb=String.format(Locale.ROOT,"%.1f",done/(1024.0*1024.0));
                    String size=total>0?" / "+String.format(Locale.ROOT,"%.1f",total/(1024.0*1024.0))+" MB":" MB";
                    current.progress("downloading","Downloading "+file.getFileName()+": "+mb+size,
                        done/1024,total>0?total/1024:0);
                }
            });
            app.command(command);
            ok=true;
        }
        catch(Exception e) {
            app.status="error";
            app.message=e.getMessage()==null?e.getClass().getSimpleName():e.getMessage();
            for(Throwable t=e;t!=null;t=t.getCause()) {
                System.err.println("Launcher cause: "+t.getClass().getName());
                for(StackTraceElement frame:t.getStackTrace())System.err.println("  at "+frame);
            }
        }
        finally { FilesUtil.downloadProgress.remove(); }
        try {
            app.view();
            app.progress(app.status, app.message, ok?1:0, 1);
        }
        catch(Exception e) {
            if(ok)app.message="Failed to write launcher state: "+e.getClass().getSimpleName();
            ok=false;
            app.status="error";
            FilesUtil.properties(app.root.resolve("launcher-view.properties"),stringMap("status","error","detail",app.message,"instance.count","0","game.count","0","mod.count","0"));
        }
        if(!background)FilesUtil.properties(app.root.resolve("response.properties"), stringMap("ok", Boolean.toString(ok), "command", command, "message", app.message));
        System.out.println((ok?"OK: ":"ERROR: ")+app.message);
        return ok;
    }
    static Map<String, String> stringMap(String... kv) {
        Map<String, String> m=new LinkedHashMap<String, String>();
        for(int i=0; i<kv.length; i+=2)m.put(kv[i], kv[i+1]);
        return m;
    }
    static Map<String, Object> map(Object... kv) {
        Map<String, Object> m=new LinkedHashMap<String, Object>();
        for(int i=0; i<kv.length; i+=2)m.put((String)kv[i], kv[i+1]);
        return m;
    }
    static String id(String s)throws IOException {
        if(s==null||!s.matches("[a-zA-Z0-9][a-zA-Z0-9_.+-]{0,63}")||s.equals(".")||s.equals(".."))throw new IOException("Invalid identifier");
        return s;
    }
    static long size(Map<String,Object> m) {
        Object v=m.get("size");
        return v instanceof Number?((Number)v).longValue():-1;
    }
    String required(String key)throws IOException {
        String s=request.getOrDefault(key, "");
        if(s.isEmpty())throw new IOException("Missing request property: "+key);
        return s;
    }
    Path dir(String id)throws IOException {
        return FilesUtil.safe(root.resolve("instances"), id(id));
    }
    Map<String, String> instance(String id)throws IOException {
        Path p=dir(id).resolve("instance.properties");
        if(!Files.isRegularFile(p))throw new IOException("Profile does not exist: "+id);
        return FilesUtil.properties(p);
    }
    Path zomboid(String id)throws IOException { return dir(id).resolve("Zomboid"); }
    int build(Map<String,String> profile) {
        try { return games.inspect(profile.get("game"),games.dir(profile.getOrDefault("game",""))).build; }
        catch(IOException e) { return 41; }
    }
    /** Every profile's properties, sorted by id. */
    List<Map<String,String>> profiles()throws IOException {
        List<Map<String,String>> list=new ArrayList<Map<String,String>>();
        Path inst=root.resolve("instances");
        if(!Files.isDirectory(inst))return list;
        List<Path> paths=new ArrayList<Path>();
        try(DirectoryStream<Path> ds=Files.newDirectoryStream(inst)) {
            for(Path p:ds)if(Files.isRegularFile(p.resolve("instance.properties")))paths.add(p);
        }
        Collections.sort(paths);
        for(Path p:paths)list.add(FilesUtil.properties(p.resolve("instance.properties")));
        return list;
    }
    void progress(String status, String detail, long done, long total)throws IOException {
        FilesUtil.properties(root.resolve("progress.properties"), stringMap("status", status, "detail", detail, "done", Long.toString(done), "total", Long.toString(total)));
    }
    void command(String cmd)throws Exception {
        if(cmd.equals("catalog")||cmd.equals("scan-games")) {
            List<Games.Install> list=games.installed();
            int ready=0; for(Games.Install g:list)if(g.valid)ready++;
            int found=games.candidates().size();
            message=ready+" game installation"+(ready==1?"":"s")+" ready"+(found>0?", "+found+" ready to import":"");
            if(list.isEmpty()&&found==0)message="No game found. Copy your Windows Project Zomboid folder to /data/ProsperoPZ/imports";
        }
        else if(cmd.equals("import-game")) {
            String id=games.importGame(Paths.get(required("source")),request.getOrDefault("game",""),request.getOrDefault("name",""));
            Games.Install g=games.require(id);
            message=g.name+" imported";
            if(profiles().isEmpty()) {
                createProfile("survivor",g.name,id,"client");
                message+=" and a first profile was created. Press Cross to play";
            }
        }
        else if(cmd.equals("delete-game")) {
            games.delete(required("game"));
            message="Game installation deleted";
        }
        else if(cmd.equals("create")||cmd.equals("create-install")) {
            String mode=request.getOrDefault("mode","client");
            String game=request.getOrDefault("game","");
            if(game.isEmpty())for(Games.Install g:games.installed())if(g.valid)game=g.id;
            String requested=request.getOrDefault("id","");
            String id=requested.isEmpty()?freeId(mode.equals("server")?"server":"profile"):id(requested);
            createProfile(id,request.getOrDefault("name",""),game,mode);
            message=(mode.equals("server")?"Dedicated server profile":"Profile")+" created";
        }
        else if(cmd.equals("install")) {
            // Kept for older frontends: a profile is ready once its game verifies.
            games.require(instance(required("id")).getOrDefault("game",""));
            message="Game files verified";
        }
        else if(cmd.equals("prepare-launch")) new ZomboidLaunch(this).prepare(required("id"));
        else if(cmd.startsWith("content-")) content(cmd);
        else if(cmd.equals("settings")) updateSettings();
        else if(cmd.equals("install-java")) {
            int major;
            try { major=Integer.parseInt(required("major")); }
            catch(NumberFormatException e) { throw new IOException("Invalid Java major version"); }
            JavaRuntime.Info info=runtimes.ensure(major,Boolean.parseBoolean(request.getOrDefault("repair","false")));
            message="Windows x64 Java "+info.version+" ready";
        }
        else if(cmd.equals("detect-java")) message="Detected "+runtimes.inventory().size()+" Windows Java runtimes";
        else if(cmd.equals("clone")) cloneProfile();
        else if(cmd.equals("rename-instance")) {
            Map<String,String> m=instance(required("id"));
            String name=required("name"); validateName(name);
            m.put("name",name);
            FilesUtil.properties(dir(required("id")).resolve("instance.properties"),m);
            message="Profile renamed";
        }
        else if(cmd.equals("server-password")) {
            String i=required("id");
            Map<String,String> m=instance(i);
            if(!"server".equals(m.get("mode")))throw new IOException("Only server profiles have an admin password");
            m.put("adminPassword",password());
            FilesUtil.properties(dir(i).resolve("instance.properties"),m);
            FilesUtil.restrict(dir(i).resolve("instance.properties"));
            message="New admin password set. It applies to a new server world; existing worlds keep theirs (use /setaccesslevel or the admin's own password)";
        }
        else if(cmd.equals("delete")||cmd.equals("delete-instance")) {
            String i=required("id");
            instance(i);
            // Worlds are kept: the profile folder moves to backups, it is not erased.
            Path backups=root.resolve("backups");
            Files.createDirectories(backups);
            Files.move(dir(i),backups.resolve("profile-"+i+"-"+Saves.stamp()));
            Files.deleteIfExists(root.resolve("profiles/launch.profile"));
            Files.deleteIfExists(root.resolve("profiles/launch-plan.json"));
            message="Profile removed. Its worlds were moved to /data/ProsperoPZ/backups";
        }
        else if(cmd.equals("view")) message="Launcher ready";
        else throw new IOException("Unknown command: "+cmd);
    }
    String freeId(String base)throws IOException {
        for(int n=1;n<10000;n++) {
            String candidate=base+"-"+String.format(Locale.ROOT,"%04d",n);
            if(!Files.exists(dir(candidate)))return candidate;
        }
        throw new IOException("Too many profiles");
    }
    static String password() {
        String alphabet="abcdefghjkmnpqrstuvwxyz23456789";
        SecureRandom random=new SecureRandom();
        StringBuilder b=new StringBuilder();
        for(int i=0;i<10;i++)b.append(alphabet.charAt(random.nextInt(alphabet.length())));
        return b.toString();
    }
    void createProfile(String id,String name,String game,String mode)throws IOException {
        id(id);
        if(!Arrays.asList("client","server").contains(mode))throw new IOException("mode must be client or server");
        Games.Install g=games.require(game);
        Path d=dir(id);
        if(Files.exists(d))throw new IOException("Profile ID already exists");
        if(name.isEmpty())name=mode.equals("server")?"Dedicated Server":"Survivor";
        validateName(name);
        Files.createDirectories(d.resolve("Zomboid/mods"));
        Map<String,String> m=stringMap("id",id,"name",name,"game",g.id,"mode",mode);
        if(mode.equals("server")) {
            m.put("server",id.replaceAll("[^A-Za-z0-9_-]","_"));
            m.put("adminPassword",password());
        }
        FilesUtil.properties(d.resolve("instance.properties"),m);
        if(mode.equals("server"))FilesUtil.restrict(d.resolve("instance.properties"));
    }
    void cloneProfile()throws IOException {
        String from=required("id");
        Map<String,String> m=instance(from);
        String base=from.length()>52?from.substring(0,52):from;
        String i=base+"-copy"; int n=2;
        while(Files.exists(dir(i)))i=base+"-copy-"+(n++);
        FilesUtil.copyTree(dir(from),dir(i));
        m.put("id",i);
        String name=m.getOrDefault("name",from)+" copy";
        m.put("name",name.length()>96?name.substring(0,96):name);
        if("server".equals(m.get("mode")))m.put("server",i.replaceAll("[^A-Za-z0-9_-]","_"));
        FilesUtil.properties(dir(i).resolve("instance.properties"),m);
        message="Profile duplicated with its worlds and mods";
    }
    static void validateName(String name)throws IOException {
        if(name.isEmpty()||name.length()>96||name.matches("(?s).*[\\r\\n\\x00-\\x1f].*"))throw new IOException("Name must be 1-96 printable characters");
    }
    /** Mods and saves for one profile. "mod" carries the mod id or the MODE/NAME save key. */
    void content(String cmd)throws Exception {
        String i=required("id");
        Map<String,String> profile=instance(i);
        String kind=request.getOrDefault("content_kind","mods");
        if(!kind.equals("mods")&&!kind.equals("saves"))throw new IOException("Content must be mods or saves");
        Path zomboid=zomboid(i);
        int build=build(profile);
        String item=request.getOrDefault("mod","");
        if(cmd.equals("content-toggle")) {
            if(kind.equals("mods"))message=Mods.toggle(zomboid,build,required("mod"),Boolean.parseBoolean(request.getOrDefault("enabled","true")));
            else message="Backup saved: "+Saves.backup(this,i,zomboid,required("mod")).getFileName();
        }
        else if(cmd.equals("content-backup")) message="Backup saved: "+Saves.backup(this,i,zomboid,required("mod")).getFileName();
        else if(cmd.equals("content-delete")) {
            if(kind.equals("mods")) { Mods.delete(zomboid,build,required("mod")); message="Mod deleted"; }
            else message="World moved to backups/"+i+"/"+Saves.remove(this,i,zomboid,required("mod")).getFileName();
        }
        else if(cmd.equals("content-import")) {
            Path source=Paths.get(required("source")).toAbsolutePath().normalize();
            if(!source.startsWith(root.resolve("imports")))throw new IOException("Import from /data/ProsperoPZ/imports/"+kind);
            if(kind.equals("mods")) {
                List<String> names=Mods.importMods(this,zomboid,source);
                message="Imported "+names.size()+" mod folder"+(names.size()==1?"":"s")+": "+Mods.join(names,", ")+". Select a mod to enable it";
            } else message="World restored as "+Saves.importSave(this,zomboid,source);
        }
        else if(!cmd.equals("content-list")) throw new IOException("Unknown content command: "+cmd);
        else message=kind.equals("mods")?"Mods loaded":"Worlds loaded";
        int n=0;
        if(kind.equals("mods"))for(Mods.Mod m:Mods.scan(zomboid,build)) {
            extraView.put("mod."+n+".name",m.id);
            extraView.put("mod."+n+".title",m.name);
            extraView.put("mod."+n+".enabled",Boolean.toString(m.enabled));
            String detail=m.description.length()>180?m.description.substring(0,177)+"...":m.description;
            if(!m.maps.isEmpty())detail="Map: "+Mods.join(m.maps,", ")+(detail.isEmpty()?"":"  /  "+detail);
            if(!m.requires.isEmpty())detail="Requires "+Mods.join(m.requires,", ")+(detail.isEmpty()?"":"  /  "+detail);
            extraView.put("mod."+n+".detail",detail.trim());
            if(!m.poster.isEmpty())try {
                Path poster=FilesUtil.safe(m.infoDir,m.poster);
                if(Icons.valid(poster))extraView.put("mod."+n+".icon",Icons.nativePath(poster));
            } catch(IOException ignored) { /* Posters are optional. */ }
            n++;
        }
        else for(Saves.Save s:Saves.list(zomboid)) {
            extraView.put("mod."+n+".name",s.key);
            extraView.put("mod."+n+".title",s.name);
            extraView.put("mod."+n+".enabled","true");
            extraView.put("mod."+n+".detail",s.mode+"  /  last played "+s.modified);
            n++;
        }
        extraView.put("mod.count",Integer.toString(n));
        extraView.put("content.kind",kind);
        extraView.put("content.instance",i);
    }
    /** Native settings can change while optional backend work is in flight. */
    void persistSettings(String... keys)throws IOException {
        Map<String,String> merged=FilesUtil.properties(root.resolve("settings.properties"));
        for(String key:keys)if(settings.containsKey(key))merged.put(key,settings.get(key));
        FilesUtil.properties(root.resolve("settings.properties"),merged);
        settings.clear();settings.putAll(merged);
    }
    void updateSettings()throws IOException {
        for(Map.Entry<String, String> e:request.entrySet()) {
            String k=e.getKey(), v=e.getValue();
            if(k.equals("command"))continue;
            if(!(k.matches("java[0-9]+")||Arrays.asList("resolution","memoryMb","graphics","controllerMode","themeColor","vrrMode","debugMode","javaApi","javaGithubApi").contains(k)))
                throw new IOException("Unknown setting: "+k);
            if(k.equals("resolution")&&!ZomboidLaunch.RESOLUTIONS.contains(v))throw new IOException("Choose 720p, 1080p, 1440p or 2160p");
            if(k.equals("memoryMb"))memory(v);
            if(k.equals("themeColor")&&!THEMES.contains(v))throw new IOException("Unknown theme");
            if(k.equals("vrrMode")&&!Arrays.asList("auto","stable").contains(v))throw new IOException("Choose Auto or Stable refresh");
            if(k.equals("graphics")&&!v.equals("zink"))throw new IOException("Project Zomboid uses OpenGL through Zink");
            if(k.equals("controllerMode")&&!Arrays.asList("emulated","native").contains(v))throw new IOException("controllerMode must be emulated or native");
            if(k.equals("debugMode")&&!Arrays.asList("on","off").contains(v))throw new IOException("debugMode must be on or off");
            settings.put(k,v);
        }
        List<String> changed=new ArrayList<String>();
        for(String key:request.keySet())if(settings.containsKey(key))changed.add(key);
        persistSettings(changed.toArray(new String[0]));
        message="Launcher settings saved";
    }
    static int memory(String s)throws IOException {
        try {
            int n=Integer.parseInt(s);
            if(n<1024||n>12288)throw new IOException("Memory must be 1024-12288 MiB");
            return n;
        }
        catch(NumberFormatException e) {
            throw new IOException("Invalid memory size");
        }
    }
    static String digest(Path p, String algorithm)throws Exception {
        MessageDigest d=MessageDigest.getInstance(algorithm);
        try(InputStream in=Files.newInputStream(p)) {
            byte[] b=new byte[65536];
            int n;
            while((n=in.read(b))>=0)d.update(b, 0, n);
        }
        StringBuilder out=new StringBuilder();
        for(byte v:d.digest())out.append(String.format("%02x", v&255));
        return out.toString();
    }
    void view()throws IOException {
        Map<String, String> v=stringMap("status", status, "detail", message);
        Map<String,Games.Install> byId=new HashMap<String,Games.Install>();
        int n=0;
        for(Games.Install g:games.installed()) {
            byId.put(g.id,g);
            if(n>=32)continue;
            v.put("game."+n+".id",g.id);
            v.put("game."+n+".name",g.name);
            v.put("game."+n+".build",Integer.toString(g.build));
            v.put("game."+n+".java",Integer.toString(g.javaMajor));
            v.put("game."+n+".valid",Boolean.toString(g.valid));
            v.put("game."+n+".detail",g.detail);
            n++;
        }
        v.put("game.count",Integer.toString(n));
        n=0;
        for(Path source:games.candidates()) {
            if(n>=16)break;
            v.put("source."+n+".name",source.getFileName()==null?source.toString():source.getFileName().toString());
            v.put("source."+n+".path",source.toString());
            n++;
        }
        v.put("source.count",Integer.toString(n));
        n=0;
        for(Map<String,String> m:profiles()) {
            if(n>=64)break;
            String id=m.getOrDefault("id","");
            Games.Install g=byId.get(m.getOrDefault("game",""));
            v.put("instance."+n+".id",id);
            v.put("instance."+n+".name",m.getOrDefault("name",id));
            v.put("instance."+n+".game",m.getOrDefault("game",""));
            v.put("instance."+n+".version",g==null?"Missing game":g.name);
            v.put("instance."+n+".loader",m.getOrDefault("mode","client"));
            v.put("instance."+n+".installed",Boolean.toString(g!=null&&g.valid));
            v.put("instance."+n+".server",m.getOrDefault("server",""));
            v.put("instance."+n+".password",m.getOrDefault("adminPassword",""));
            try { v.put("instance."+n+".icon",Icons.instance(this,dir(id),m)); }
            catch(IOException ignored) { v.put("instance."+n+".icon",""); }
            n++;
        }
        v.put("instance.count", Integer.toString(n));
        v.put("settings.memoryMb", settings.getOrDefault("memoryMb", Integer.toString(DEFAULT_MEMORY)));
        v.put("settings.themeColor",settings.getOrDefault("themeColor","knox"));
        v.put("settings.vrrMode",settings.getOrDefault("vrrMode","auto"));
        v.put("settings.resolution",settings.getOrDefault("resolution","1920x1080"));
        v.put("settings.graphics","zink");
        v.put("settings.controllerMode",settings.getOrDefault("controllerMode","native"));
        v.put("settings.debugMode",settings.getOrDefault("debugMode","off"));
        for(Map.Entry<String, String> e:settings.entrySet())if(e.getKey().matches("java[0-9]+"))v.put("settings."+e.getKey(), e.getValue());
        n=0;
        for(JavaRuntime.Info info:runtimes.inventory()) {
            if(n>=16)break;
            v.put("java."+n+".major",Integer.toString(info.major));
            v.put("java."+n+".version",info.version);
            v.put("java."+n+".path",info.executable.toString());
            v.put("java."+n+".valid",Boolean.toString(info.valid));
            v.put("java."+n+".detail",info.detail);
            n++;
        }
        v.put("java.count",Integer.toString(n));
        v.putAll(extraView);
        FilesUtil.properties(root.resolve("launcher-view.properties"), v);
    }
    static String windows(Path p) {
        String s=p.toAbsolutePath().normalize().toString();
        return s.matches("^[a-zA-Z]:.*")?s.replace('/', '\\'):"Z:"+s.replace('/', '\\');
    }
}
