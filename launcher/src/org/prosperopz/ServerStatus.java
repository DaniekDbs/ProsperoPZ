package org.prosperopz;

import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;

/** Status and control for a dedicated server running on the PS5.
 *
 * The server has no window, so the title draws a status screen from
 * root/server-status.properties, written here every two seconds: starting or
 * running, the address and port friends join, the admin password and the end
 * of server-console.txt. Pressing Options writes server-command.properties;
 * the command is typed into the server's console as "quit", which saves the
 * world and stops it (the title then returns to the launcher). */
final class ServerStatus implements Runnable {
    static final int LOG_LINES=10;

    private final Path root, zomboid;
    private final String name, password;
    private final PipedOutputStream console;
    private boolean started, stopping;

    private ServerStatus(Path root,Path zomboid,String name,String password,PipedOutputStream console) {
        this.root=root; this.zomboid=zomboid; this.name=name; this.password=password; this.console=console;
    }

    /** Replace System.in with a console this class can type into, then start. */
    static void start(Path root,Path zomboid,String name,String password)throws IOException {
        Files.deleteIfExists(root.resolve("server-command.properties"));
        PipedOutputStream out=new PipedOutputStream();
        PipedInputStream in=new PipedInputStream(out,4096);
        System.setIn(in);
        Thread t=new Thread(new ServerStatus(root,zomboid,name,password,out),"ProsperoPZ-server-status");
        t.setDaemon(true);
        t.start();
    }

    public void run() {
        for(;;) {
            try {
                command();
                write();
                Thread.sleep(2000);
            } catch(InterruptedException e) { return; }
            catch(Throwable t) {
                System.out.println("PS5 server status: "+t);
                try { Thread.sleep(5000); } catch(InterruptedException e) { return; }
            }
        }
    }

    void command()throws IOException {
        Path file=root.resolve("server-command.properties");
        if(!Files.isRegularFile(file))return;
        String command=FilesUtil.properties(file).getOrDefault("command","");
        Files.deleteIfExists(file);
        if(command.equals("stop")&&!stopping) {
            stopping=true;
            System.out.println("PS5 server status: stop requested, sending quit");
            console.write("quit\n".getBytes(StandardCharsets.UTF_8));
            console.flush();
        }
    }

    void write()throws IOException {
        List<String> log=tail(zomboid.resolve("server-console.txt"),LOG_LINES);
        for(String line:log) {
            String l=line.toLowerCase(Locale.ROOT);
            if(l.contains("server started")||l.contains("listening on port"))started=true;
        }
        Map<String,String> m=new LinkedHashMap<String,String>();
        m.put("state",stopping?"stopping":started?"running":"starting");
        m.put("name",name);
        m.put("address",address());
        m.put("port",Integer.toString(port()));
        m.put("password",password);
        m.put("players",Integer.toString(players()));
        m.put("detail",started?"Server profile: "+name+". Worlds are saved every 10 minutes and when stopped with Options.":
            "The first start builds the world and can take a few minutes.");
        for(int i=0;i<log.size();i++)m.put("log."+i,log.get(i));
        Path file=root.resolve("server-status.properties");
        FilesUtil.properties(file,m);
        FilesUtil.restrict(file);
    }

    /** The last lines of the server log, without the logger's prefixes. */
    static List<String> tail(Path file,int lines)throws IOException {
        List<String> out=new ArrayList<String>();
        if(!Files.isRegularFile(file))return out;
        byte[] data;
        try(RandomAccessFile f=new RandomAccessFile(file.toFile(),"r")) {
            long size=f.length(), from=Math.max(0,size-16384);
            data=new byte[(int)(size-from)];
            f.seek(from);
            f.readFully(data);
        }
        String[] all=new String(data,StandardCharsets.UTF_8).split("\r?\n");
        for(int i=Math.max(0,all.length-lines*3);i<all.length;i++) {
            String l=all[i].replaceAll("^(LOG|WARN|ERROR|DEBUG)\\s*:\\s*\\S+\\s+f:\\d+(,\\s*t:\\d+)?(,\\s*st:[\\d,]+)?>\\s*","$1: ")
                .replaceAll("[\\x00-\\x1f]"," ").trim();
            if(l.isEmpty()||l.startsWith("at "))continue;
            out.add(l.length()>120?l.substring(0,117)+"...":l);
        }
        return out.size()>lines?new ArrayList<String>(out.subList(out.size()-lines,out.size())):out;
    }

    int port() {
        try {
            for(String line:Files.readAllLines(zomboid.resolve("Server/"+name+".ini"),StandardCharsets.UTF_8))
                if(line.startsWith("DefaultPort="))return Integer.parseInt(line.substring(12).trim());
        } catch(IOException|NumberFormatException ignored) {}
        return 16261;
    }

    /** The console's local IPv4 address, when Wine reports one. */
    static String address() {
        try {
            for(NetworkInterface n:Collections.list(NetworkInterface.getNetworkInterfaces())) {
                if(!n.isUp()||n.isLoopback())continue;
                for(InetAddress a:Collections.list(n.getInetAddresses()))
                    if(a instanceof Inet4Address&&a.isSiteLocalAddress())return a.getHostAddress();
            }
        } catch(Throwable ignored) {}
        return "";
    }

    /** Connected players, when the game exposes a count (else -1). */
    static int players() {
        try {
            Class<?> server=Class.forName("zombie.network.GameServer",false,ClassLoader.getSystemClassLoader());
            for(String method:new String[]{"getPlayerCount","getPlayersCount"}) {
                try {
                    Object n=server.getMethod(method).invoke(null);
                    if(n instanceof Number)return ((Number)n).intValue();
                } catch(NoSuchMethodException ignored) {}
            }
        } catch(Throwable ignored) {}
        return -1;
    }
}
