package org.prosperopz;

import org.prosperopz.workshop.SteamDownloader;

import java.nio.file.*;
import java.util.*;

/** Live check of the Workshop chain against real Steam (CI, not the PS5):
 * trending list, search, details, then anonymous downloads of small items
 * installed into a scratch Zomboid folder.
 * Usage: WorkshopProbe ROOT [item ids to download...] */
public final class WorkshopProbe {
    public static void main(String[] args) {
        Thread watchdog=new Thread(() -> {
            try { Thread.sleep(240_000); } catch(InterruptedException e) { return; }
            System.out.println("PROBE WATCHDOG: still running after 4 minutes; thread dump follows");
            for(var e:Thread.getAllStackTraces().entrySet()) {
                System.out.println("THREAD "+e.getKey().getName()+" "+e.getKey().getState());
                for(StackTraceElement f:e.getValue())System.out.println("    at "+f);
            }
            System.out.flush();
            Runtime.getRuntime().halt(3);
        },"probe-watchdog");
        watchdog.setDaemon(true);
        watchdog.start();
        int code;
        try { code=probe(args); }
        catch(Throwable t) { t.printStackTrace(System.out); code=2; }
        // JavaSteam's threads would keep the JVM alive after main returns.
        System.out.flush();
        System.exit(code);
    }

    static void step(String what) { System.out.println("PROBE ["+java.time.LocalTime.now()+"] "+what); }

    static int probe(String[] args)throws Exception {
        Path root=Paths.get(args[0]).toAbsolutePath();
        Files.createDirectories(root);
        Main app=new Main(root,Collections.<String,String>emptyMap());
        Workshop w=new Workshop(app);
        List<Long> trending=w.list("",42);
        System.out.println("PROBE trending ids="+trending.size()+" "+trending.subList(0,Math.min(8,trending.size())));
        List<Long> search=w.list("library",42);
        System.out.println("PROBE search(library) ids="+search.size());
        List<Workshop.Item> items=w.details(trending.size()>30?trending.subList(0,30):trending);
        for(Workshop.Item it:items.subList(0,Math.min(6,items.size())))
            System.out.println("PROBE item "+it.id+" \""+it.title+"\" "+it.size/1024+" KiB type="+it.type+" tags="+it.tags+" poster="+!w.poster(it).isEmpty());
        if(trending.isEmpty()) {
            String html=w.get(w.lastPage);
            System.out.println("PROBE page "+w.lastPage+" bytes="+html.length());
            java.util.regex.Matcher t=java.util.regex.Pattern.compile("<title>(.*?)</title>").matcher(html);
            if(t.find())System.out.println("PROBE page title: "+t.group(1));
            for(String marker:new String[]{"publishedfileid","filedetails","workshopItem","ugc","agecheck","g_rgWorkshop"}) {
                int at=html.indexOf(marker);
                System.out.println("PROBE marker "+marker+" at "+at+(at<0?"":": "+html.substring(Math.max(0,at-120),Math.min(html.length(),at+200)).replaceAll("\\s+"," ")));
            }
        }
        if(items.isEmpty()&&args.length<2)throw new IllegalStateException("No Workshop items listed");
        List<Long> picks=new ArrayList<>();
        for(int i=1;i<args.length;i++)picks.add(Long.parseLong(args[i]));
        if(picks.isEmpty()) {
            items.sort(Comparator.comparingLong(it -> it.size));
            for(Workshop.Item it:items)if(it.type==0&&it.size>0&&picks.size()<2)picks.add(it.id);
        }
        Path zomboid=root.resolve("Zomboid");
        int failures=0;
        try(SteamDownloader steam=new SteamDownloader(line -> System.out.println("PROBE steam: "+line))) {
            step("connecting");
            steam.connect();
            step("connected");
            for(long id:picks) {
                Path target=root.resolve("download/"+id);
                step("downloading "+id);
                try {
                    SteamDownloader.Result r=steam.download(id,target);
                    long files;
                    try(var walk=Files.walk(target)) { files=walk.filter(Files::isRegularFile).count(); }
                    List<String> names=WorkshopRun.hasMod(target)?Mods.importMods(app,zomboid,target):List.of();
                    System.out.println("PROBE downloaded "+id+" \""+r.title+"\" files="+files+" children="+r.children+" mods="+names);
                    if(names.isEmpty())failures++;
                } catch(Exception e) {
                    failures++;
                    System.out.println("PROBE FAILED "+id+": "+e);
                    e.printStackTrace(System.out);
                }
            }
        }
        System.out.println(failures==0?"PROBE_OK":"PROBE_FAILURES "+failures);
        return failures==0?0:1;
    }
}
