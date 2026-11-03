package org.prosperopz;

import org.prosperopz.workshop.SteamDownloader;

import java.io.IOException;
import java.nio.file.*;
import java.util.*;

/** The download run: started like a game launch on a Java 17+ runtime, it
 * downloads the profile's queued Workshop items (and the items they require),
 * installs their mod folders into the profile and records what was installed.
 * The result is shown by the launcher when it starts again. */
final class WorkshopRun {
    private WorkshopRun() {}

    static void log(String line) { System.out.println("PS5 workshop: "+line); }

    /** Called reflectively by ZomboidLaunch.execute for a plan with task=workshop. */
    static void run(Main app,Map<String,Object> plan)throws Exception {
        String id=Main.id(Json.text(plan,"instanceId"));
        Map<String,String> profile=app.instance(id);
        Path dir=app.dir(id), zomboid=app.zomboid(id), file=Workshop.records(dir);
        Map<String,String> records=FilesUtil.properties(file);
        Deque<Long> todo=new ArrayDeque<>(Workshop.queue(records));
        Set<Long> seen=new HashSet<>(todo);
        List<String> installed=new ArrayList<>(), failed=new ArrayList<>(), modNames=new ArrayList<>();
        Path stage=app.root.resolve("cache/workshop-download");
        long started=System.nanoTime();
        try(SteamDownloader steam=new SteamDownloader(new SteamDownloader.Listener() {
            public void status(String line) { log(line); }
            public void progress(long done,long total) {
                try { app.progress("downloading","Workshop "+done/1048576+" / "+total/1048576+" MB",done/1024,total/1024); }
                catch(IOException ignored) {}
            }
        })) {
            steam.connect();
            while(!todo.isEmpty()) {
                long item=todo.poll();
                Path target=stage.resolve(Long.toString(item));
                FilesUtil.deleteTree(target);
                boolean ok=false;
                try {
                    SteamDownloader.Result r=steam.download(item,target);
                    // Required items (and a collection's items) come along.
                    for(long child:r.children) {
                        boolean current=records.containsKey("item."+child+".updated");
                        if(seen.size()<128&&!current&&seen.add(child))todo.add(child);
                    }
                    List<String> names=hasMod(target)?Mods.importMods(app,zomboid,target):Collections.emptyList();
                    if(names.isEmpty()&&!r.children.isEmpty()) { log(r.title+": collection, "+r.children.size()+" items"); }
                    else if(names.isEmpty())throw new IOException("no mod.info inside");
                    records.put("item."+item+".updated",Long.toString(r.updated));
                    records.put("item."+item+".title",r.title.replaceAll("[\\r\\n]+"," "));
                    if(!names.isEmpty())records.put("item."+item+".mods",String.join(",",names));
                    modNames.addAll(names);
                    installed.add(r.title);
                    log("installed "+r.title+" -> "+names);
                    ok=true;
                } catch(Exception e) {
                    String reason=e.getMessage()==null?e.getClass().getSimpleName():e.getMessage();
                    failed.add(item+" ("+reason+")");
                    log("failed "+item+": "+reason);
                    e.printStackTrace(System.out);
                } finally {
                    FilesUtil.deleteTree(target);
                }
                if(ok) {
                    // Failed items stay queued for another try.
                    List<Long> queue=Workshop.queue(records);
                    queue.remove(item);
                    Workshop.setQueue(records,queue);
                }
                FilesUtil.properties(file,records);
            }
        } catch(Exception e) {
            String reason=e.getMessage()==null?e.getClass().getSimpleName():e.getMessage();
            failed.add("Steam: "+reason);
            log("stopped: "+reason);
            e.printStackTrace(System.out);
        }
        StringBuilder m=new StringBuilder();
        if(!installed.isEmpty())m.append("Downloaded ").append(installed.size()).append(" Workshop item").append(installed.size()==1?"":"s")
            .append(": ").append(Mods.join(modNames.isEmpty()?installed:modNames,", ")).append(". Enable them in Mods.");
        if(!failed.isEmpty())m.append(m.length()>0?" ":"").append("Failed: ").append(Mods.join(failed,"; ")).append(".");
        if(m.length()==0)m.append("Nothing was queued for download.");
        String text=m.length()>480?m.substring(0,477)+"...":m.toString();
        FilesUtil.properties(app.root.resolve("workshop-result.properties"),
            Main.stringMap("status",failed.isEmpty()?"ready":"error","message",text,"instance",id));
        log("done in "+(System.nanoTime()-started)/1000000000L+" s: "+text);
    }

    static boolean hasMod(Path tree)throws IOException {
        if(!Files.isDirectory(tree))return false;
        try(var walk=Files.walk(tree,8)) { return walk.anyMatch(p -> p.getFileName()!=null&&p.getFileName().toString().equalsIgnoreCase("mod.info")); }
    }
}
