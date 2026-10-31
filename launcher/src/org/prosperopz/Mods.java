package org.prosperopz;

import java.io.IOException;
import java.nio.file.*;
import java.nio.file.attribute.BasicFileAttributes;
import java.util.*;

/** Project Zomboid mods for one profile's Zomboid folder.
 *
 * Steam Workshop is unavailable with -nosteam, so mods are folders under
 * Zomboid/mods (copied from a PC's workshop/content/108600 folder or from a
 * mod's own download). Activation uses the game's own mods/default.txt, and
 * dedicated servers receive the same list in Server/NAME.ini. */
final class Mods {
    static final int MAX=128;

    static final class Mod {
        String id="", name="", folder="", poster="", description="";
        Path infoDir;
        List<String> requires=new ArrayList<String>();
        List<String> maps=new ArrayList<String>();
        boolean enabled;
    }

    private Mods() {}

    /** B41: mods/X/mod.info. B42: mods/X/42[.N]/mod.info (+ common/). */
    static List<Mod> scan(Path zomboid,int build)throws IOException {
        List<Mod> mods=new ArrayList<Mod>();
        Set<String> seen=new HashSet<String>();
        List<Path> roots=new ArrayList<Path>();
        roots.add(zomboid.resolve("mods"));
        Path workshop=zomboid.resolve("Workshop");
        if(Files.isDirectory(workshop))try(DirectoryStream<Path> items=Files.newDirectoryStream(workshop)) {
            for(Path item:items)if(Files.isDirectory(item.resolve("Contents/mods")))roots.add(item.resolve("Contents/mods"));
        }
        for(Path root:roots) {
            if(!Files.isDirectory(root))continue;
            List<Path> folders=new ArrayList<Path>();
            try(DirectoryStream<Path> entries=Files.newDirectoryStream(root)) {
                for(Path p:entries)if(Files.isDirectory(p,LinkOption.NOFOLLOW_LINKS))folders.add(p);
            }
            Collections.sort(folders);
            for(Path folder:folders) {
                Path info=modInfo(folder,build);
                if(info==null)continue;
                Mod m=read(info);
                m.folder=folder.getFileName().toString();
                if(m.id.isEmpty())m.id=m.folder;
                if(m.name.isEmpty())m.name=m.id;
                if(!seen.add(m.id))continue;
                mods.add(m);
                if(mods.size()>=MAX)return mods;
            }
        }
        Set<String> active=new HashSet<String>(enabled(zomboid));
        for(Mod m:mods)m.enabled=active.contains(m.id);
        return mods;
    }

    /** Prefer the newest versioned folder that fits the game build. */
    static Path modInfo(Path folder,int build)throws IOException {
        Path best=null; String bestName="";
        try(DirectoryStream<Path> entries=Files.newDirectoryStream(folder)) {
            for(Path p:entries) {
                String n=p.getFileName().toString();
                if(!n.matches("4[0-9](\\.[0-9]+)*")||!Files.isRegularFile(p.resolve("mod.info")))continue;
                int major=Integer.parseInt(n.substring(0,2));
                if(major>build&&build>0)continue;
                if(best==null||compareVersions(n,bestName)>0) { best=p; bestName=n; }
            }
        }
        if(best!=null)return best.resolve("mod.info");
        Path flat=folder.resolve("mod.info");
        return Files.isRegularFile(flat)?flat:null;
    }

    static int compareVersions(String a,String b) {
        String[] x=a.split("\\."),y=b.split("\\.");
        for(int i=0;i<Math.max(x.length,y.length);i++) {
            int p=i<x.length?Integer.parseInt(x[i]):0,q=i<y.length?Integer.parseInt(y[i]):0;
            if(p!=q)return Integer.compare(p,q);
        }
        return 0;
    }

    static Mod read(Path info)throws IOException {
        Map<String,String> kv=ZomboidText.keyValues(info);
        Mod m=new Mod();
        m.infoDir=info.getParent();
        m.id=cleanId(kv.getOrDefault("id",""));
        m.name=kv.getOrDefault("name","").replaceAll("[\\x00-\\x1f]","");
        m.description=kv.getOrDefault("description","").replaceAll("<[^>]*>","").replaceAll("[\\x00-\\x1f]"," ");
        m.poster=kv.getOrDefault("poster","");
        for(Map.Entry<String,String> e:kv.entrySet())
            if(e.getKey().equals("require")||e.getKey().startsWith("require#"))
                for(String r:e.getValue().split(","))if(!cleanId(r).isEmpty())m.requires.add(cleanId(r));
        for(Path base:new Path[]{m.infoDir,m.infoDir.getParent().resolve("common")}) {
            Path maps=base.resolve("media/maps");
            if(!Files.isDirectory(maps))continue;
            try(DirectoryStream<Path> entries=Files.newDirectoryStream(maps)) {
                for(Path p:entries)if(Files.isDirectory(p)&&!m.maps.contains(p.getFileName().toString()))m.maps.add(p.getFileName().toString());
            }
        }
        Collections.sort(m.maps);
        return m;
    }

    /** B42 writes "\ModId" or "WorkshopId\ModId"; compare on the bare mod id. */
    static String cleanId(String raw) {
        String s=raw.trim();
        int slash=s.lastIndexOf('\\');
        if(slash>=0)s=s.substring(slash+1);
        return s.replaceAll("[\\x00-\\x1f,;]","").trim();
    }

    static List<String> enabled(Path zomboid)throws IOException {
        List<String> ids=new ArrayList<String>();
        Path file=zomboid.resolve("mods/default.txt");
        if(!Files.isRegularFile(file))return ids;
        String section="";
        for(String line:ZomboidText.read(file).split("\\r?\\n")) {
            String t=line.trim();
            if(t.equals("mods")||t.equals("maps"))section=t;
            else if(t.equals("}"))section="";
            else if(section.equals("mods")&&t.startsWith("mod")&&t.contains("=")) {
                String id=cleanId(t.substring(t.indexOf('=')+1));
                if(!id.isEmpty()&&!ids.contains(id))ids.add(id);
            }
        }
        return ids;
    }

    /** Rewrite the mods block; the maps block and anything unknown is preserved. */
    static void writeEnabled(Path zomboid,List<String> ids,int build)throws IOException {
        Path file=zomboid.resolve("mods/default.txt");
        List<String> mapsBlock=new ArrayList<String>();
        if(Files.isRegularFile(file)) {
            boolean inMaps=false;
            for(String line:ZomboidText.read(file).split("\\r?\\n")) {
                String t=line.trim();
                if(t.equals("maps")) { inMaps=true; continue; }
                if(inMaps&&t.equals("{"))continue;
                if(inMaps&&t.equals("}")) { inMaps=false; continue; }
                if(inMaps&&!t.isEmpty())mapsBlock.add(t);
            }
        }
        StringBuilder b=new StringBuilder("VERSION = 1,\n\nmods\n{\n");
        for(String id:ids)b.append("\tmod = ").append(build>=42?"\\":"").append(id).append(",\n");
        b.append("}\n\nmaps\n{\n");
        for(String map:mapsBlock)b.append('\t').append(map).append('\n');
        b.append("}\n\n");
        FilesUtil.write(file,b.toString());
    }

    /** Enabling also enables installed dependencies; disabling never cascades. */
    static String toggle(Path zomboid,int build,String id,boolean enable)throws IOException {
        List<Mod> mods=scan(zomboid,build);
        Map<String,Mod> byId=new LinkedHashMap<String,Mod>();
        for(Mod m:mods)byId.put(m.id,m);
        if(!byId.containsKey(id))throw new IOException("Mod not found: "+id);
        List<String> active=enabled(zomboid);
        List<String> missing=new ArrayList<String>();
        if(enable) {
            // Dependencies load first, in the order mod.info names them.
            List<String> order=new ArrayList<String>();
            addWithRequirements(id,byId,order,missing,new HashSet<String>(),0);
            for(String need:order)if(!active.contains(need))active.add(need);
        } else active.remove(id);
        writeEnabled(zomboid,active,build);
        String name=byId.get(id).name;
        if(!enable)return "Mod disabled: "+name;
        if(!missing.isEmpty())return "Mod enabled: "+name+". Missing required mods: "+join(missing,", ");
        return "Mod enabled: "+name;
    }

    private static void addWithRequirements(String id,Map<String,Mod> byId,List<String> order,List<String> missing,Set<String> visiting,int depth) {
        if(order.contains(id)||depth>32||!visiting.add(id))return;
        Mod m=byId.get(id);
        if(m==null) { if(!missing.contains(id))missing.add(id); return; }
        for(String need:m.requires)addWithRequirements(need,byId,order,missing,visiting,depth+1);
        order.add(id);
    }

    static String join(List<String> values,String separator) {
        StringBuilder b=new StringBuilder();
        for(String v:values) { if(b.length()>0)b.append(separator); b.append(v); }
        return b.toString();
    }

    /** Copy one or more mod folders from a .zip or a folder in imports/mods. */
    static List<String> importMods(Main app,Path zomboid,Path source)throws IOException {
        Path stage=FilesUtil.safe(app.root.resolve("cache"),"mod-import-"+System.nanoTime());
        Files.createDirectories(stage);
        try {
            Path tree=source;
            if(Files.isRegularFile(source)) {
                if(!source.getFileName().toString().toLowerCase(Locale.ROOT).endsWith(".zip"))throw new IOException("Mods must be a folder or a .zip");
                FilesUtil.unzip(source,stage,Collections.<String>emptySet());
                tree=stage;
            } else if(!Files.isDirectory(source))throw new IOException("Mod source not found");
            final List<Path> infos=new ArrayList<Path>();
            final Path walked=tree;
            Files.walkFileTree(tree,EnumSet.noneOf(FileVisitOption.class),8,new SimpleFileVisitor<Path>() {
                public FileVisitResult visitFile(Path p,BasicFileAttributes a) {
                    if(p.getFileName().toString().equalsIgnoreCase("mod.info"))infos.add(p);
                    return infos.size()>256?FileVisitResult.TERMINATE:FileVisitResult.CONTINUE;
                }
            });
            if(infos.isEmpty())throw new IOException("No mod.info found. Copy the mod's folder (Steam: workshop/content/108600/ID/mods/NAME)");
            Set<Path> folders=new LinkedHashSet<Path>();
            for(Path info:infos)folders.add(modFolder(walked,info.getParent()));
            Path mods=zomboid.resolve("mods");
            Files.createDirectories(mods);
            List<String> names=new ArrayList<String>();
            for(Path folder:folders) {
                String name=folder.getFileName()==null?"":folder.getFileName().toString();
                if(folder.equals(walked))name=source.getFileName().toString().replaceAll("(?i)\\.zip$","");
                name=name.replaceAll("[^A-Za-z0-9_.+-]","_").replaceAll("^[^A-Za-z0-9]+","");
                name=Main.id(name.length()>64?name.substring(0,64):name);
                Path target=FilesUtil.safe(mods,name);
                Path fresh=FilesUtil.safe(mods,"."+name+".importing");
                FilesUtil.deleteTree(fresh);
                FilesUtil.copyTree(folder,fresh);
                FilesUtil.deleteTree(target);
                Files.move(fresh,target);
                names.add(name);
            }
            return names;
        } finally { FilesUtil.deleteTree(stage); }
    }

    /** The folder that belongs in Zomboid/mods for a given mod.info directory. */
    static Path modFolder(Path tree,Path infoDir) {
        Path p=infoDir;
        String n=p.getFileName()==null?"":p.getFileName().toString();
        if((n.matches("4[0-9](\\.[0-9]+)*")||n.equals("common"))&&p.getParent()!=null&&p.getParent().startsWith(tree))p=p.getParent();
        // Steam layout: ID/mods/NAME/mod.info
        for(Path q=p;q!=null&&q.startsWith(tree);q=q.getParent())
            if(q.getParent()!=null&&q.getParent().getFileName()!=null&&q.getParent().getFileName().toString().equalsIgnoreCase("mods"))return q;
        return p;
    }

    static void delete(Path zomboid,int build,String id)throws IOException {
        for(Mod m:scan(zomboid,build)) {
            if(!m.id.equals(id))continue;
            Path folder=FilesUtil.safe(zomboid.resolve("mods"),m.folder);
            if(!Files.isDirectory(folder))throw new IOException("Only mods in Zomboid/mods can be deleted here");
            List<String> active=enabled(zomboid);
            if(active.remove(id))writeEnabled(zomboid,active,build);
            FilesUtil.deleteTree(folder);
            return;
        }
        throw new IOException("Mod not found: "+id);
    }

    /** Dedicated servers read mods and maps from their own ini, not default.txt. */
    static void syncServer(Path zomboid,String server,int build)throws IOException {
        List<Mod> mods=scan(zomboid,build);
        List<String> active=enabled(zomboid);
        List<String> ids=new ArrayList<String>(), maps=new ArrayList<String>();
        for(String id:active)for(Mod m:mods)if(m.id.equals(id)) {
            ids.add((build>=42?"\\":"")+id);
            for(String map:m.maps)if(!maps.contains(map))maps.add(map);
        }
        maps.add("Muldraugh, KY");
        Path ini=zomboid.resolve("Server/"+server+".ini");
        Map<String,String> defaults=new LinkedHashMap<String,String>();
        // First-run defaults that suit a console host. Owners can edit the ini later.
        defaults.put("PauseEmpty","true");
        defaults.put("SaveWorldEveryMinutes","10");
        defaults.put("MaxPlayers","8");
        defaults.put("Public","false");
        defaults.put("Open","true");
        defaults.put("ServerWelcomeMessage","Hosted on PS5 with ProsperoPZ.");
        ZomboidText.merge(ini,defaults,true);
        Map<String,String> sync=new LinkedHashMap<String,String>();
        sync.put("Mods",join(ids,";"));
        sync.put("Map",join(maps,";"));
        sync.put("WorkshopItems","");
        ZomboidText.merge(ini,sync,false);
    }
}
