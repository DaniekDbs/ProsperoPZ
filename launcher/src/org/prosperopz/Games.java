package org.prosperopz;

import java.io.IOException;
import java.nio.file.*;
import java.util.*;

/** Project Zomboid installations copied from a PC (Steam or GOG, Windows build).
 *
 * Installed games live in ROOT/games/ID. Each one keeps the files exactly as
 * the PC had them: ProjectZomboid64.json, media/, java/ or projectzomboid.jar,
 * the native DLLs and the bundled jre64. Nothing here downloads the game;
 * Project Zomboid must be owned and copied from the owner's own computer. */
final class Games {
    static final String METADATA=".prosperopz-game.properties";
    static final String DESCRIPTOR="ProjectZomboid64.json";

    /** Everything needed to plan a launch from one installation. */
    static final class Install {
        String id="", name="", detail="", version="";
        Path dir;
        int build, javaMajor=17;
        boolean valid, bundledJava;
        String mainClass="zombie/gameStates/MainScreenState";
        List<String> classpath=new ArrayList<String>();
        List<String> vmArgs=new ArrayList<String>();
        Path javaHome;
    }

    private final Main app;
    Games(Main app) { this.app=app; }

    Path home()throws IOException {
        Path games=app.root.resolve("games");
        Files.createDirectories(games);
        return games;
    }

    Path dir(String id)throws IOException { return FilesUtil.safe(home(),Main.id(id)); }

    /** Installed games, sorted by id. Invalid folders are listed with a reason. */
    List<Install> installed()throws IOException {
        List<Install> list=new ArrayList<Install>();
        Path games=home();
        try(DirectoryStream<Path> entries=Files.newDirectoryStream(games)) {
            for(Path entry:entries) {
                if(!Files.isDirectory(entry,LinkOption.NOFOLLOW_LINKS))continue;
                String name=entry.getFileName().toString();
                if(!name.matches("[a-zA-Z0-9][a-zA-Z0-9_.+-]{0,63}"))continue;
                list.add(inspect(name,entry));
            }
        }
        Collections.sort(list,new Comparator<Install>() {
            public int compare(Install a,Install b) { return a.id.compareTo(b.id); }
        });
        return list;
    }

    Install require(String id)throws IOException {
        Path d=dir(id);
        if(!Files.isDirectory(d))throw new IOException("Game installation not found: "+id+". Import Project Zomboid first");
        Install install=inspect(id,d);
        if(!install.valid)throw new IOException("Game installation "+id+" is incomplete: "+install.detail);
        return install;
    }

    /** Read a game folder without changing it. */
    Install inspect(String id,Path d) {
        Install g=new Install();
        g.id=id; g.dir=d.toAbsolutePath().normalize(); g.name="Project Zomboid";
        try {
            Map<String,String> meta=FilesUtil.properties(d.resolve(METADATA));
            Path descriptor=d.resolve(DESCRIPTOR);
            if(!Files.isRegularFile(descriptor))throw new IOException(DESCRIPTOR+" missing; copy the whole Windows game folder");
            Map<String,Object> json=descriptor(descriptor);
            String main=Json.text(json,"mainClass");
            if(!main.isEmpty())g.mainClass=main;
            if(!g.mainClass.matches("[a-zA-Z_$][a-zA-Z0-9_$/.]*"))throw new IOException("Invalid main class in "+DESCRIPTOR);
            if(json.containsKey("classpath"))for(Object entry:Json.arr(json.get("classpath")))g.classpath.add(Json.str(entry));
            if(json.containsKey("vmArgs"))for(Object entry:Json.arr(json.get("vmArgs")))g.vmArgs.add(Json.str(entry));
            if(g.classpath.isEmpty())throw new IOException(DESCRIPTOR+" has no classpath");
            boolean jar=Files.isRegularFile(d.resolve("projectzomboid.jar")), code=false;
            for(String entry:g.classpath) {
                Path resolved=classpathPath(d,entry); // validates the entry stays inside the game folder
                String e=entry.replace('\\','/').toLowerCase(Locale.ROOT);
                if(e.endsWith("projectzomboid.jar"))jar=true;
                if(e.endsWith(".jar")&&Files.isRegularFile(resolved))code=true;
            }
            // Build 41 ships classes in the folder (zombie/ or java/zombie); Build 42 adds projectzomboid.jar.
            if(jar||Files.isDirectory(d.resolve("zombie"))||Files.isDirectory(d.resolve("java/zombie")))code=true;
            if(!code)throw new IOException("Game code not found (no zombie classes, projectzomboid.jar or classpath jars); copy the whole Windows game folder");
            if(!Files.isDirectory(d.resolve("media")))throw new IOException("media folder missing");
            g.build=jar?42:41;
            g.version=meta.getOrDefault("version","");
            if(meta.containsKey("build"))try { g.build=Integer.parseInt(meta.get("build")); }
                catch(NumberFormatException ignored) { /* Keep detected build. */ }
            if(!g.version.matches("[0-9A-Za-z .+_-]{0,32}"))g.version="";
            g.name="Project Zomboid "+(g.version.isEmpty()?"Build "+g.build:g.version);
            String label=meta.getOrDefault("name","");
            if(label.matches("[\\x20-\\x7e]{1,64}"))g.name=label;
            // The bundled runtime is what the game was built and tested with.
            Path jre=d.resolve("jre64");
            if(Files.isDirectory(jre)) {
                Map<String,String> release=FilesUtil.properties(jre.resolve("release"));
                String version=JavaRuntime.unquote(release.getOrDefault("JAVA_VERSION",""));
                java.util.regex.Matcher m=java.util.regex.Pattern.compile("^(?:1\\.)?([0-9]+).*").matcher(version);
                if(m.matches())g.javaMajor=Integer.parseInt(m.group(1));
                JavaRuntime.Info info=app.runtimes.inspect(g.javaMajor,jre);
                if(info.valid) { g.bundledJava=true; g.javaHome=jre; }
            }
            if(!hasNatives(d))throw new IOException("Windows native libraries missing (lwjgl/fmod DLLs); copy the Windows build");
            g.valid=true;
            g.detail=g.bundledJava?"Ready. Uses the game's bundled Java "+g.javaMajor
                :"Ready. Java "+g.javaMajor+" is downloaded on first launch (bundled jre64 missing or invalid)";
        } catch(Exception e) {
            g.valid=false;
            g.detail=e.getMessage()==null?e.getClass().getSimpleName():e.getMessage();
        }
        return g;
    }

    static boolean hasNatives(Path d) {
        for(String folder:new String[]{"","win64/"})
            for(String name:new String[]{"lwjgl64.dll","lwjgl.dll","fmodintegration64.dll"})
                if(Files.isRegularFile(d.resolve(folder+name)))return true;
        return false;
    }

    /** Resolve a classpath entry to a path inside the game folder. "." (and
     * "java/.", "java/") mean a directory, with "." being the game root itself;
     * FilesUtil.safe refuses "..", absolute paths and symlinks. */
    static Path classpathPath(Path gameDir,String entry)throws IOException {
        String e=entry.replace('\\','/').trim();
        while(e.endsWith("/."))e=e.substring(0,e.length()-2);
        while(e.endsWith("/"))e=e.substring(0,e.length()-1);
        if(e.isEmpty()||e.equals("."))return gameDir.toAbsolutePath().normalize();
        return FilesUtil.safe(gameDir,e);
    }

    /** The descriptor is written by hand upstream; tolerate a BOM and trailing commas. */
    static Map<String,Object> descriptor(Path file)throws IOException {
        String text=FilesUtil.read(file).replaceAll(",(\\s*[\\]}])","$1");
        try { return Json.obj(Json.parseFabric(text)); }
        catch(RuntimeException e) { throw new IOException("Unreadable "+DESCRIPTOR+": "+e.getMessage()); }
    }

    /** Folders ready to import: ROOT/imports/** (FTP uploads) and USB drives. */
    List<Path> candidates() {
        List<Path> found=new ArrayList<Path>();
        List<Path> bases=new ArrayList<Path>();
        bases.add(app.root.resolve("imports"));
        bases.add(app.root.resolve("imports/game"));
        for(int i=0;i<8;i++) {
            Path usb=Paths.get(Main.ps5(app.root)?"Z:\\mnt\\usb"+i:"/mnt/usb"+i);
            bases.add(usb); bases.add(usb.resolve("ProjectZomboid"));
            bases.add(usb.resolve("SteamLibrary/steamapps/common"));
        }
        for(Path base:bases) {
            try {
                if(!Files.isDirectory(base))continue;
                if(Files.isRegularFile(base.resolve(DESCRIPTOR))) { add(found,base); continue; }
                try(DirectoryStream<Path> entries=Files.newDirectoryStream(base)) {
                    for(Path entry:entries)
                        if(Files.isDirectory(entry,LinkOption.NOFOLLOW_LINKS)&&Files.isRegularFile(entry.resolve(DESCRIPTOR)))add(found,entry);
                }
            } catch(Exception ignored) { /* Unmounted or unreadable drives are skipped. */ }
        }
        return found;
    }

    private static void add(List<Path> found,Path p) {
        Path n=p.toAbsolutePath().normalize();
        if(!found.contains(n))found.add(n);
    }

    /** Move an FTP upload into games/ (instant on the same drive), or copy from USB. */
    String importGame(Path source,String requested,String label)throws IOException {
        source=source.toAbsolutePath().normalize();
        if(!candidates().contains(source))throw new IOException("Choose a game folder from imports/ or a USB drive");
        Install probe=inspect("import",source);
        if(!probe.valid)throw new IOException("Not a complete Windows Project Zomboid folder: "+probe.detail);
        String id=requested.isEmpty()?"build"+probe.build:Main.id(requested);
        String base=id; int n=2;
        while(Files.exists(dir(id)))id=base+"-"+(n++);
        Path target=dir(id);
        app.progress("installing","Importing Project Zomboid "+probe.build+" as "+id,0,2);
        boolean moved=false;
        if(source.startsWith(app.root.resolve("imports")))try { Files.move(source,target); moved=true; }
            catch(IOException crossDevice) { /* Fall back to a verified copy. */ }
        if(!moved) {
            Path stage=home().resolve(".import-"+id+"-"+System.nanoTime());
            try {
                FilesUtil.copyTree(source,stage);
                Files.move(stage,target);
            } finally { FilesUtil.deleteTree(stage); }
        }
        Map<String,String> meta=FilesUtil.properties(target.resolve(METADATA));
        if(!label.isEmpty())meta.put("name",label);
        meta.put("importedFrom",moved?"imports":"copy");
        FilesUtil.properties(target.resolve(METADATA),meta);
        Install result=inspect(id,target);
        app.progress("installing","Project Zomboid "+result.build+" ready",2,2);
        if(!result.valid)throw new IOException("Imported game failed verification: "+result.detail);
        return id;
    }

    void delete(String id)throws IOException {
        Path d=dir(id);
        if(!Files.isDirectory(d))throw new IOException("Game installation not found");
        for(Map<String,String> profile:app.profiles())
            if(id.equals(profile.get("game")))throw new IOException("Profile "+profile.get("name")+" still uses this game; delete or move it first");
        FilesUtil.deleteTree(d);
    }
}
