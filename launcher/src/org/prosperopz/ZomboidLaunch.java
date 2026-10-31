package org.prosperopz;

import java.io.*;
import java.lang.reflect.InvocationTargetException;
import java.nio.file.*;
import java.util.*;

/** Turns a profile into a prospero-win launch profile and an in-JVM plan.
 *
 * The game's own ProjectZomboid64.json supplies the classpath, main class and
 * JVM options. Options that do not work through Wine on PS5 are replaced:
 * ZGC needs multi-mapped memory Wine does not provide, compressed pointers
 * need a low heap address the PS5 cannot reserve, and Steam is unavailable. */
final class ZomboidLaunch {
    static final String CLIENT_MAIN="zombie.gameStates.MainScreenState";
    static final String SERVER_MAIN="zombie.network.GameServer";
    static final List<String> RESOLUTIONS=Arrays.asList("1280x720","1920x1080","2560x1440","3840x2160");

    private final Main app;
    ZomboidLaunch(Main app) { this.app=app; }

    /** JVM options from the game descriptor that are replaced or unusable on PS5. */
    static boolean replaced(String arg) {
        String a=arg.trim();
        String[] prefixes={"-Xmx","-Xms","-Xmn","-Xlog","-Dzomboid.steam=","-Djava.library.path=","-javaagent","-agentlib","-agentpath",
            "-XX:+UseZGC","-XX:-UseZGC","-XX:+ZGenerational","-XX:-ZGenerational","-XX:Z","-XX:+UseG1GC","-XX:G1","-XX:+UseShenandoahGC",
            "-XX:Shenandoah","-XX:+UseConcMarkSweepGC","-XX:+UseParallelGC","-XX:+UseSerialGC","-XX:+UseLargePages",
            "-XX:+AlwaysPreTouch","-XX:+UseCompressedOops","-XX:-UseCompressedOops","-XX:+UseCompressedClassPointers",
            "-XX:-UseCompressedClassPointers","-XX:ReservedCodeCacheSize","-XX:MaxRAMPercentage","-XX:InitialRAMPercentage",
            "-XX:MinRAMPercentage","-XX:MaxRAM","-XX:+UnlockExperimentalVMOptions","-XX:SoftMaxHeapSize","-XX:ConcGCThreads",
            "-XX:ParallelGCThreads","-XX:MetaspaceSize","-cp","-classpath","--class-path"};
        for(String p:prefixes)if(a.startsWith(p))return true;
        return a.isEmpty();
    }

    /** Native library folders, relative to the game folder ("win64/;." by default). */
    static List<Path> libraryPath(Games.Install game) {
        String spec="win64/;.";
        for(String a:game.vmArgs)if(a.startsWith("-Djava.library.path="))spec=a.substring(20);
        List<Path> out=new ArrayList<Path>();
        for(String part:spec.split("[;:]")) {
            String p=part.trim().replace('\\','/');
            while(p.endsWith("/")&&p.length()>1)p=p.substring(0,p.length()-1);
            if(p.isEmpty()||p.contains(".."))continue;
            Path resolved=p.equals(".")?game.dir:game.dir.resolve(p).normalize();
            if(resolved.startsWith(game.dir)&&!out.contains(resolved))out.add(resolved);
        }
        if(!out.contains(game.dir))out.add(game.dir);
        return out;
    }

    static int gcThreads() {
        // The PS5 exposes 13 usable CPU threads to a title. Leave room for the
        // render, audio and Wine threads so collections do not cause stutter.
        int cpus=Runtime.getRuntime().availableProcessors();
        return Math.max(2,Math.min(6,cpus/2));
    }

    /** JVM arguments, in order: PS5 runtime, the game's own, then the classpath. */
    List<String> jvmArguments(Map<String,String> profile,Games.Install game,int memoryMb,Path zomboid,boolean server)throws IOException {
        List<String> jvm=new ArrayList<String>();
        Ps5Dns.launchArguments(app.root,jvm,game.javaMajor);
        int initial=Math.min(memoryMb,server?1024:2048);
        jvm.add("-Xms"+initial+"m");
        jvm.add("-Xmx"+memoryMb+"m");
        jvm.add("-XX:-UseCompressedOops");
        jvm.add("-XX:-UseCompressedClassPointers");
        // Parallel GC: contiguous generations, fewer native commits through Wine
        // than G1/ZGC, and short pauses for a heap of this size.
        jvm.add("-XX:+UseParallelGC");
        jvm.add("-XX:ParallelGCThreads="+gcThreads());
        jvm.add("-XX:MetaspaceSize=192M");
        StartupDiagnostics.codeCacheArguments(game.javaMajor,jvm);
        if(game.javaMajor>=17)jvm.add("-Xlog:gc=info,safepoint=info:stdout:uptime");
        jvm.add("-Dzomboid.steam=0");
        if(server)jvm.add("-Djava.awt.headless=true");
        StringBuilder library=new StringBuilder();
        for(Path p:libraryPath(game)) { if(library.length()>0)library.append(';'); library.append(Main.windows(p)); }
        jvm.add("-Djava.library.path="+library);
        for(String a:game.vmArgs) {
            if(replaced(a))continue;
            if(server&&a.startsWith("-Djava.awt.headless"))continue;
            if(!a.matches("[\\x20-\\x7e]{1,512}")||a.contains("\""))throw new IOException("Unsupported JVM option in "+Games.DESCRIPTOR);
            jvm.add(a);
        }
        return jvm;
    }

    String classpath(Games.Install game,Path launcher)throws IOException {
        StringBuilder cp=new StringBuilder();
        for(String entry:game.classpath) {
            Path p=Games.classpathPath(game.dir,entry);
            if(cp.length()>0)cp.append(';');
            cp.append(Main.windows(p));
        }
        cp.append(';').append(Main.windows(launcher));
        return cp.toString();
    }

    /** Game arguments. The Zomboid folder lives with the profile, not in the prefix. */
    static List<String> gameArguments(Map<String,String> profile,Path zomboid,boolean server,boolean debug) {
        List<String> args=new ArrayList<String>();
        args.add("-nosteam");
        args.add("-cachedir="+Main.windows(zomboid));
        if(debug)args.add("-debug");
        if(server) {
            args.add("-servername"); args.add(profile.getOrDefault("server","servertest"));
            String password=profile.getOrDefault("adminPassword","");
            if(!password.isEmpty()) { args.add("-adminpassword"); args.add(password); }
        }
        return args;
    }

    /** Window size and fullscreen live in the game's own options.ini. */
    static void displayOptions(Path zomboid,String resolution)throws IOException {
        String[] wh=resolution.split("x");
        Map<String,String> options=new LinkedHashMap<String,String>();
        options.put("width",wh[0]);
        options.put("height",wh[1]);
        options.put("fullScreen","true");
        ZomboidText.merge(zomboid.resolve("options.ini"),options,false);
    }

    static String inputSection(String controllerMode,boolean server) {
        // Project Zomboid supports XInput controllers natively; that is the default.
        if(server||controllerMode.equals("native"))return "mode=xinput\nmouse=none\n";
        // Keyboard and mouse emulation using the game's default bindings.
        return "mode=keyboard\nmouse=right_stick\n"+
            "cross=e\ncircle=space\nsquare=r\ntriangle=m\n"+
            "l1=wheel_up\nr1=wheel_down\nl2=mouse_right\nr2=mouse_left\n"+
            "l3=shift\nr3=c\n"+
            "up=w\ndown=s\nleft=a\nright=d\n"+
            "options=escape\ncreate=tab\ntouchpad=q\n";
    }

    void prepare(String id)throws Exception {
        Files.deleteIfExists(app.root.resolve("profiles/launch.profile"));
        Files.deleteIfExists(app.root.resolve("profiles/launch-plan.json"));
        Map<String,String> profile=app.instance(id);
        Path d=app.dir(id), zomboid=d.resolve("Zomboid");
        Games.Install game=app.games.require(profile.getOrDefault("game",""));
        boolean server="server".equals(profile.get("mode"));
        Files.createDirectories(zomboid.resolve("mods"));
        Path launcher=app.root.resolve("launcher/prosperopz-launcher.jar");
        if(!Files.isRegularFile(launcher))throw new IOException("Launcher JAR missing from root/launcher; stage the app first");
        Path java;
        if(game.bundledJava)java=game.javaHome.resolve("bin/java.exe");
        else try { java=app.runtimes.ensure(game.javaMajor,false).executable; }
        catch(IOException e) {
            throw new IOException("The game's jre64 folder is missing or invalid, and Java "+game.javaMajor+
                " could not be downloaded ("+e.getMessage()+"). Copy jre64 from your PC or connect the PS5 to the internet",e);
        }
        int memory=Main.memory(app.settings.getOrDefault("memoryMb",Integer.toString(Main.DEFAULT_MEMORY)));
        String resolution=app.settings.getOrDefault("resolution","1920x1080");
        if(!RESOLUTIONS.contains(resolution))throw new IOException("Unsupported launch resolution");
        boolean debug="on".equals(app.settings.getOrDefault("debugMode","off"));
        if(Main.ps5(app.root))FileDiagnostics.probe(app.root);
        if(server)Mods.syncServer(zomboid,profile.getOrDefault("server","servertest"),game.build);
        else displayOptions(zomboid,resolution);

        List<String> jvm=jvmArguments(profile,game,memory,zomboid,server);
        String main=server?SERVER_MAIN:game.mainClass.replace('/','.');
        if(!main.matches("[a-zA-Z_$][a-zA-Z0-9_$.]*"))throw new IOException("Invalid game main class");
        List<String> args=gameArguments(profile,zomboid,server,debug);
        Path plan=app.root.resolve("profiles/launch-plan.json");
        FilesUtil.write(plan,Json.stringify(Main.map("instanceId",id,"mainClass",main,"arguments",args,"mode",server?"server":"client")));
        FilesUtil.restrict(plan);
        jvm.add("-cp"); jvm.add(classpath(game,launcher));
        jvm.add("org.prosperopz.Main");
        jvm.add("--root"); jvm.add(Main.windows(app.root));
        jvm.add("--execute-plan"); jvm.add(Main.windows(plan));
        StringBuilder line=new StringBuilder();
        for(String a:jvm) { if(line.length()>0)line.append(' '); line.append(FilesUtil.quote(a)); }
        if(line.length()>32768)throw new IOException("Project Zomboid command line exceeds the 32768-character Windows limit");

        String controller=app.settings.getOrDefault("controllerMode","native");
        if(!Arrays.asList("emulated","native").contains(controller))throw new IOException("Unsupported controller mode");
        String text="[application]\nid=zomboid\nname="+(server?"Project Zomboid Server":"Project Zomboid")+
            "\nexecutable="+Main.windows(java)+"\nworking_directory="+Main.windows(game.dir)+
            "\narguments="+line+"\nprefix=zomboid\nruntime=wine-wow64\narchitecture=pe64\ngraphics=zink\n"+
            "[runtime]\nthread_scheduling=true\nfast_clock=true\n"+
            "[display]\ndesktop="+resolution+"\nview=window\nscaling=fit\nrefresh=120\n"+
            "[input]\n"+inputSection(controller,server);
        FilesUtil.write(app.root.resolve("profiles/launch.profile"),text);
        FilesUtil.restrict(app.root.resolve("profiles/launch.profile"));
        profile.put("lastLaunch",Long.toString(System.currentTimeMillis()));
        FilesUtil.properties(d.resolve("instance.properties"),profile);
        String note=game.build>=42&&memory<4096?" (Build 42 runs best with 4096 MB or more)":"";
        app.message=(server?"Dedicated server "+profile.getOrDefault("server","")+" prepared":"Project Zomboid prepared")+
            " with Java "+game.javaMajor+", "+memory+" MB"+note;
    }

    /** Runs inside the game JVM, started by prospero-win from launch.profile. */
    void execute(Path requested)throws Exception {
        Path plan=app.root.resolve("profiles/launch-plan.json").toAbsolutePath().normalize();
        if(!requested.toAbsolutePath().normalize().equals(plan))throw new IOException("Only the internal launch plan can execute");
        Map<String,Object> data=Json.obj(Json.parse(FilesUtil.read(plan)));
        String id=Main.id(Json.text(data,"instanceId"));
        app.instance(id);
        String main=Json.text(data,"mainClass");
        if(!main.equals(CLIENT_MAIN)&&!main.equals(SERVER_MAIN)&&!main.startsWith("zombie."))throw new IOException("Unexpected main class");
        List<String> args=new ArrayList<String>();
        for(Object a:Json.arr(data.get("arguments")))args.add(Json.str(a));
        GameplayProfile profile=GameplayProfile.start(app.root,id);
        try {
            Class.forName(main).getMethod("main",String[].class).invoke(null,(Object)args.toArray(new String[0]));
        } catch(InvocationTargetException e) {
            StringWriter trace=new StringWriter();
            e.getCause().printStackTrace(new PrintWriter(trace));
            String diagnostic=trace.toString();
            for(int i=0;i+1<args.size();i++)if(args.get(i).equals("-adminpassword"))diagnostic=diagnostic.replace(args.get(i+1),"<redacted>");
            System.err.println(diagnostic);
            System.exit(2);
        } finally { if(profile!=null)profile.finish(); }
    }
}
