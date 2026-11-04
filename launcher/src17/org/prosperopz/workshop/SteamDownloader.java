package org.prosperopz.workshop;

import in.dragonbra.javasteam.enums.EDepotFileFlag;
import in.dragonbra.javasteam.enums.EResult;
import in.dragonbra.javasteam.protobufs.steamclient.SteammessagesPublishedfileSteamclient.CPublishedFile_GetDetails_Request;
import in.dragonbra.javasteam.protobufs.steamclient.SteammessagesPublishedfileSteamclient.PublishedFileDetails;
import in.dragonbra.javasteam.rpc.service.PublishedFile;
import in.dragonbra.javasteam.steam.cdn.Client;
import in.dragonbra.javasteam.steam.cdn.Server;
import in.dragonbra.javasteam.steam.handlers.steamapps.SteamApps;
import in.dragonbra.javasteam.steam.handlers.steamapps.callback.DepotKeyCallback;
import in.dragonbra.javasteam.steam.handlers.steamcontent.SteamContent;
import in.dragonbra.javasteam.steam.handlers.steamunifiedmessages.SteamUnifiedMessages;
import in.dragonbra.javasteam.steam.handlers.steamuser.SteamUser;
import in.dragonbra.javasteam.steam.handlers.steamuser.callback.LoggedOnCallback;
import in.dragonbra.javasteam.steam.steamclient.SteamClient;
import in.dragonbra.javasteam.steam.steamclient.callbackmgr.CallbackManager;
import in.dragonbra.javasteam.steam.steamclient.callbacks.ConnectedCallback;
import in.dragonbra.javasteam.steam.steamclient.callbacks.DisconnectedCallback;
import in.dragonbra.javasteam.types.ChunkData;
import in.dragonbra.javasteam.types.DepotManifest;
import in.dragonbra.javasteam.types.FileData;
import kotlinx.coroutines.GlobalScope;
import kotlinx.coroutines.future.FutureKt;

import java.io.IOException;
import java.io.InputStream;
import java.io.RandomAccessFile;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.*;
import java.util.concurrent.*;

/** Downloads Steam Workshop items for Project Zomboid with an anonymous Steam
 * session, the way dedicated servers fetch their mods: published file details
 * give the item's content manifest, which is read from Steam's CDN with the
 * workshop depot's key. Runs on Java 17+ (JavaSteam). */
public final class SteamDownloader implements AutoCloseable {
    public static final int APP_ID=108600;
    private static final long TIMEOUT_S=60;

    /** Progress and log lines; implemented by the caller. */
    public interface Listener {
        void status(String line);
        default void progress(long done,long total) {}
    }

    /** One downloaded item: its details and the folder holding its files. */
    public static final class Result {
        public final long id; public final String title; public final long updated; public final Path folder;
        public final List<Long> children;
        Result(long id,String title,long updated,Path folder,List<Long> children) {
            this.id=id;this.title=title;this.updated=updated;this.folder=folder;this.children=children;
        }
    }

    private final SteamClient client=new SteamClient();
    private final CallbackManager callbacks=new CallbackManager(client);
    private final Listener listener;
    private volatile boolean running=true;
    private Thread pump;
    private byte[] depotKey;
    private int depotId=APP_ID;
    private List<Server> servers;
    /* One CDN client for the whole run: closing one shuts down an HTTP pool
     * the others share. */
    private Client cdn;

    public SteamDownloader(Listener listener) { this.listener=listener; }

    /** Connect and log on anonymously. */
    public void connect()throws Exception {
        CompletableFuture<Void> loggedOn=new CompletableFuture<>();
        SteamUser user=client.getHandler(SteamUser.class);
        callbacks.subscribe(ConnectedCallback.class,cb -> user.logOnAnonymous());
        callbacks.subscribe(LoggedOnCallback.class,cb -> {
            if(cb.getResult()==EResult.OK)loggedOn.complete(null);
            else loggedOn.completeExceptionally(new IOException("Steam refused the anonymous log on: "+cb.getResult()));
        });
        callbacks.subscribe(DisconnectedCallback.class,cb -> {
            if(!loggedOn.isDone())loggedOn.completeExceptionally(new IOException("Disconnected from Steam"));
        });
        pump=new Thread(() -> { while(running)callbacks.runWaitCallbacks(500L); },"steam-callbacks");
        pump.setDaemon(true);
        pump.start();
        listener.status("Connecting to Steam");
        client.connect();
        loggedOn.get(TIMEOUT_S,TimeUnit.SECONDS);
        listener.status("Logged on anonymously");
    }

    /** Published file details for one item (includes required items as children). */
    public PublishedFileDetails details(long id)throws Exception {
        PublishedFile service=client.getHandler(SteamUnifiedMessages.class).createService(PublishedFile.class);
        CPublishedFile_GetDetails_Request request=CPublishedFile_GetDetails_Request.newBuilder()
            .setAppid(APP_ID).setIncludechildren(true).addPublishedfileids(id).build();
        var response=service.getDetails(request).toFuture().get(TIMEOUT_S,TimeUnit.SECONDS);
        if(response.getResult()!=EResult.OK)throw new IOException("Steam could not read item "+id+": "+response.getResult());
        var list=response.getBody().getPublishedfiledetailsList();
        if(list.isEmpty())throw new IOException("Workshop item "+id+" was not found");
        PublishedFileDetails d=list.get(0);
        if(d.getResult()!=1)throw new IOException("Workshop item "+id+" is unavailable (result "+d.getResult()+")");
        if(d.getConsumerAppid()!=0&&d.getConsumerAppid()!=APP_ID&&d.getCreatorAppid()!=APP_ID)
            throw new IOException("Workshop item "+id+" is not a Project Zomboid item");
        return d;
    }

    /** Download one item's files into target (created). */
    public Result download(long id,Path target)throws Exception {
        PublishedFileDetails d=details(id);
        String title=d.getTitle().isEmpty()?Long.toString(id):d.getTitle();
        List<Long> children=new ArrayList<>();
        for(var child:d.getChildrenList())children.add(child.getPublishedfileid());
        Files.createDirectories(target);
        if(!d.getFileUrl().isEmpty()) {
            listener.status("Downloading "+title);
            String name=d.getFilename().isEmpty()?id+".zip":Path.of(d.getFilename().replace('\\','/')).getFileName().toString();
            web(d.getFileUrl(),safe(target,name));
        } else if(d.getHcontentFile()!=0) {
            depot(title,d.getHcontentFile(),target);
        } else if(d.getFileType()==2) {
            // A collection has no files of its own; its items are the children.
        } else throw new IOException(title+" has no downloadable content");
        return new Result(id,title,d.getTimeUpdated(),target,children);
    }

    private void prepareDepot()throws Exception {
        if(depotKey!=null)return;
        SteamApps apps=client.getHandler(SteamApps.class);
        DepotKeyCallback key=apps.getDepotDecryptionKey(depotId,APP_ID).toFuture().get(TIMEOUT_S,TimeUnit.SECONDS);
        if(key.getResult()!=EResult.OK)
            throw new IOException("Steam did not give the Workshop key to an anonymous session ("+key.getResult()+")");
        depotKey=key.getDepotKey();
        SteamContent content=client.getHandler(SteamContent.class);
        List<Server> all=FutureKt.asCompletableFuture(content.getServersForSteamPipe(null,null,GlobalScope.INSTANCE))
            .get(TIMEOUT_S,TimeUnit.SECONDS);
        servers=new ArrayList<>();
        for(Server s:all) {
            boolean eligible=s.getAllowedAppIds().length==0;
            for(int a:s.getAllowedAppIds())eligible|=a==APP_ID;
            if(eligible&&("SteamCache".equals(s.getType())||"CDN".equals(s.getType())))servers.add(s);
        }
        servers.sort(Comparator.comparingDouble(Server::getWeightedLoad));
        if(servers.isEmpty())throw new IOException("Steam returned no content servers");
        listener.status("Using "+servers.size()+" Steam content servers");
    }

    private void depot(String title,long manifestId,Path target)throws Exception {
        prepareDepot();
        SteamContent content=client.getHandler(SteamContent.class);
        long code=FutureKt.asCompletableFuture(content.getManifestRequestCode(depotId,APP_ID,manifestId,"public",null,GlobalScope.INSTANCE))
            .get(TIMEOUT_S,TimeUnit.SECONDS);
        DepotManifest manifest=null; Exception last=null;
        if(cdn==null)cdn=new Client(client);
        {
            for(int attempt=0;attempt<Math.min(4,servers.size())&&manifest==null;attempt++) {
                try { manifest=cdn.downloadManifestFuture(depotId,manifestId,code,servers.get(attempt),depotKey).get(TIMEOUT_S,TimeUnit.SECONDS); }
                catch(Exception e) { last=e; }
            }
            if(manifest==null)throw new IOException("Could not read the file list of "+title,last);
            if(manifest.getFilenamesEncrypted()&&!manifest.decryptFilenames(depotKey))throw new IOException("Could not decrypt the file names of "+title);
            long total=0;
            for(FileData f:manifest.getFiles())total+=f.getTotalSize();
            listener.status("Downloading "+title+" ("+String.format(Locale.ROOT,"%.1f",total/1048576.0)+" MB, "+manifest.getFiles().size()+" files)");
            // Many mods are hundreds of small files: download several files
            // at once (each file's parts in order), not one file at a time.
            for(FileData f:manifest.getFiles()) {
                Path out=safe(target,f.getFileName().replace('\\','/'));
                if(f.getFlags().contains(EDepotFileFlag.Directory))Files.createDirectories(out);
                else Files.createDirectories(out.getParent());
            }
            ExecutorService pool=Executors.newFixedThreadPool(8);
            java.util.concurrent.atomic.AtomicLong written=new java.util.concurrent.atomic.AtomicLong();
            final long size=total;
            try {
                List<Future<?>> files=new ArrayList<>();
                for(FileData f:manifest.getFiles()) {
                    if(f.getFlags().contains(EDepotFileFlag.Directory))continue;
                    Path out=safe(target,f.getFileName().replace('\\','/'));
                    files.add(pool.submit(() -> {
                        try(RandomAccessFile file=new RandomAccessFile(out.toFile(),"rw")) {
                            file.setLength(f.getTotalSize());
                            for(ChunkData chunk:f.getChunks()) {
                                byte[] data=chunk(cdn,chunk);
                                file.seek(chunk.getOffset());
                                file.write(data);
                            }
                        }
                        listener.progress(written.addAndGet(f.getTotalSize()),size);
                        return null;
                    }));
                }
                for(Future<?> file:files)file.get();
            } finally { pool.shutdownNow(); }
        }
    }

    private byte[] chunk(Client cdn,ChunkData chunk)throws Exception {
        // Tiny files can compress to more bytes than they hold.
        byte[] buffer=new byte[Math.max(chunk.getUncompressedLength(),chunk.getCompressedLength())];
        Exception last=null;
        for(int attempt=0;attempt<6;attempt++) {
            Server server=servers.get(attempt%servers.size());
            try {
                int written=cdn.downloadDepotChunkFuture(depotId,chunk,server,buffer,depotKey).get(TIMEOUT_S,TimeUnit.SECONDS);
                return Arrays.copyOf(buffer,Math.min(written,chunk.getUncompressedLength()));
            } catch(Exception e) { last=e; }
        }
        throw new IOException("A file part failed to download",last);
    }

    private static void web(String address,Path out)throws IOException {
        HttpURLConnection c=(HttpURLConnection)new URL(address).openConnection();
        c.setConnectTimeout(30000);c.setReadTimeout(60000);
        if(c.getResponseCode()!=200)throw new IOException("HTTP "+c.getResponseCode()+" for "+address);
        try(InputStream in=c.getInputStream()) { Files.copy(in,out); }
    }

    /** A path below root; manifest names never escape the item folder. */
    static Path safe(Path root,String relative)throws IOException {
        Path p=root.resolve(relative).normalize();
        if(!p.startsWith(root.normalize())||relative.isEmpty())throw new IOException("Unsafe file name in Workshop item: "+relative);
        return p;
    }

    @Override public void close() {
        running=false;
        if(cdn!=null)try { cdn.close(); } catch(RuntimeException ignored) {}
        try { client.disconnect(); } catch(RuntimeException ignored) {}
    }
}
