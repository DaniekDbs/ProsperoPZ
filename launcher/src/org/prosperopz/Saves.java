package org.prosperopz;

import java.io.IOException;
import java.io.OutputStream;
import java.nio.file.*;
import java.nio.file.attribute.BasicFileAttributes;
import java.text.SimpleDateFormat;
import java.util.*;
import java.util.zip.*;

/** Worlds under Zomboid/Saves/MODE/NAME. A save is addressed as "MODE/NAME".
 * Deleting moves the folder into ROOT/backups (instant and recoverable);
 * backups are ordinary ZIP files a PC can open. */
final class Saves {
    static final int MAX=128;
    private Saves() {}

    static final class Save { String key="", mode="", name="", modified=""; }

    static List<Save> list(Path zomboid)throws IOException {
        List<Save> saves=new ArrayList<Save>();
        Path root=zomboid.resolve("Saves");
        if(!Files.isDirectory(root))return saves;
        List<Path> modes=new ArrayList<Path>();
        try(DirectoryStream<Path> entries=Files.newDirectoryStream(root)) {
            for(Path p:entries)if(Files.isDirectory(p,LinkOption.NOFOLLOW_LINKS))modes.add(p);
        }
        Collections.sort(modes);
        SimpleDateFormat format=new SimpleDateFormat("yyyy-MM-dd HH:mm",Locale.ROOT);
        for(Path mode:modes) {
            List<Path> worlds=new ArrayList<Path>();
            try(DirectoryStream<Path> entries=Files.newDirectoryStream(mode)) {
                for(Path p:entries)if(Files.isDirectory(p,LinkOption.NOFOLLOW_LINKS))worlds.add(p);
            }
            Collections.sort(worlds);
            for(Path world:worlds) {
                Save s=new Save();
                s.mode=mode.getFileName().toString(); s.name=world.getFileName().toString();
                s.key=s.mode+"/"+s.name;
                s.modified=format.format(new Date(Files.getLastModifiedTime(world).toMillis()));
                saves.add(s);
                if(saves.size()>=MAX)return saves;
            }
        }
        return saves;
    }

    static Path resolve(Path zomboid,String key)throws IOException {
        String[] parts=key.split("/");
        if(parts.length!=2||parts[0].isEmpty()||parts[1].isEmpty()||key.contains(".."))throw new IOException("Invalid save name");
        Path save=FilesUtil.safe(zomboid.resolve("Saves"),key);
        if(!Files.isDirectory(save))throw new IOException("Save not found: "+key);
        return save;
    }

    static String stamp() { return new SimpleDateFormat("yyyyMMdd-HHmmss",Locale.ROOT).format(new Date()); }

    static String safeName(String s) {
        String n=s.replaceAll("[^A-Za-z0-9_.+-]","_");
        return n.length()>48?n.substring(0,48):n;
    }

    /** Zip one world into ROOT/backups/PROFILE/. */
    static Path backup(Main app,String profile,Path zomboid,String key)throws IOException {
        final Path save=resolve(zomboid,key);
        Path folder=app.root.resolve("backups").resolve(Main.id(profile));
        Files.createDirectories(folder);
        String[] parts=key.split("/");
        Path out=folder.resolve(safeName(parts[0])+"-"+safeName(parts[1])+"-"+stamp()+".zip");
        Path partial=out.resolveSibling(out.getFileName()+".part");
        final String prefix=parts[0]+"/"+parts[1]+"/";
        final long[] done={0};
        app.progress("working","Backing up "+parts[1],0,0);
        try(OutputStream file=Files.newOutputStream(partial);final ZipOutputStream zip=new ZipOutputStream(file)) {
            // Region files are already compact; light compression keeps PS5 backups quick.
            zip.setLevel(1);
            Files.walkFileTree(save,new SimpleFileVisitor<Path>() {
                public FileVisitResult visitFile(Path p,BasicFileAttributes a)throws IOException {
                    if(a.isSymbolicLink())return FileVisitResult.CONTINUE;
                    zip.putNextEntry(new ZipEntry(prefix+save.relativize(p).toString().replace('\\','/')));
                    Files.copy(p,zip); zip.closeEntry();
                    done[0]++;
                    return FileVisitResult.CONTINUE;
                }
            });
        } catch(IOException e) { Files.deleteIfExists(partial); throw e; }
        FilesUtil.atomic(partial,out);
        return out;
    }

    /** Move out of Saves; the world can be copied back from backups/ at any time. */
    static Path remove(Main app,String profile,Path zomboid,String key)throws IOException {
        Path save=resolve(zomboid,key);
        Path folder=app.root.resolve("backups").resolve(Main.id(profile));
        Files.createDirectories(folder);
        String[] parts=key.split("/");
        Path target=folder.resolve("deleted-"+safeName(parts[0])+"-"+safeName(parts[1])+"-"+stamp());
        Files.move(save,target);
        return target;
    }

    static final List<String> MARKERS=Arrays.asList("map_ver.bin","map_t.bin","players.db","map_meta.bin");

    /** Restore a world from a .zip or folder in imports/saves. */
    static String importSave(Main app,Path zomboid,Path source)throws IOException {
        Path stage=FilesUtil.safe(app.root.resolve("cache"),"save-import-"+System.nanoTime());
        Files.createDirectories(stage);
        try {
            Path tree=source;
            if(Files.isRegularFile(source)) {
                if(!source.getFileName().toString().toLowerCase(Locale.ROOT).endsWith(".zip"))throw new IOException("Saves must be a folder or a .zip");
                FilesUtil.unzip(source,stage,Collections.<String>emptySet());
                tree=stage;
            }
            final Path[] world={null};
            Files.walkFileTree(tree,EnumSet.noneOf(FileVisitOption.class),6,new SimpleFileVisitor<Path>() {
                public FileVisitResult visitFile(Path p,BasicFileAttributes a) {
                    if(MARKERS.contains(p.getFileName().toString())) { world[0]=p.getParent(); return FileVisitResult.TERMINATE; }
                    return FileVisitResult.CONTINUE;
                }
            });
            if(world[0]==null)throw new IOException("No Project Zomboid world found (map_ver.bin / map_t.bin)");
            Path w=world[0];
            String name=w.equals(stage)?source.getFileName().toString().replaceAll("(?i)\\.zip$",""):w.getFileName().toString();
            String mode="Sandbox";
            Path parent=w.getParent();
            if(parent!=null&&!w.equals(tree)&&parent.getFileName()!=null) {
                String p=parent.getFileName().toString();
                if(Arrays.asList("Survival","Sandbox","Apocalypse","Builder","Multiplayer","Challenge","Survivor","Kentucky","Stories").contains(p))mode=p;
            }
            name=safeName(name);
            Path target=FilesUtil.safe(zomboid.resolve("Saves/"+mode),name);
            String unique=name; int n=2;
            while(Files.exists(target)) { unique=name+"-"+(n++); target=FilesUtil.safe(zomboid.resolve("Saves/"+mode),unique); }
            Files.createDirectories(target.getParent());
            FilesUtil.copyTree(w,target);
            return mode+"/"+unique;
        } finally { FilesUtil.deleteTree(stage); }
    }
}
