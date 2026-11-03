package org.prosperopz;

import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.text.SimpleDateFormat;
import java.util.*;
import java.util.regex.*;

/** Steam Workshop browsing for a profile, without a Steam account.
 *
 * Lists come from the public Workshop pages (trending, or a text search),
 * filtered to the profile's game build; titles, sizes and posters from
 * Steam's public published-file API. A Steam Web API key (setting
 * steamApiKey) switches lists to the official query API. Items picked by the
 * player are queued in the profile's workshop.properties; the download itself
 * runs in a separate Java 17+ run (WorkshopRun) started like a game launch. */
final class Workshop {
    static final int APP_ID=108600;
    static final int MAX_RESULTS=48;
    static final String API="https://api.steampowered.com/";
    static final String WORKSHOP="https://steamcommunity.com/workshop/browse/";
    static final Pattern FILE_ID=Pattern.compile("data-publishedfileid=\"(\\d{1,20})\"");
    static final Pattern URL_ID=Pattern.compile("[?&]id=(\\d{1,20})");

    static final class Item {
        long id; String title=""; long size; long updated; long subscriptions; String preview=""; int type;
        boolean javaMod; List<String> tags=new ArrayList<String>();
    }

    private final Main app;
    Workshop(Main app) { this.app=app; }

    // ---------------------------------------------------------------- input

    /** A Workshop id from digits or a Steam link, or 0 for search text. */
    static long parseId(String text) {
        String t=text.trim();
        if(t.matches("\\d{4,20}"))return Long.parseLong(t);
        Matcher m=URL_ID.matcher(t);
        if(t.contains("steamcommunity.com")&&m.find())return Long.parseLong(m.group(1));
        return 0;
    }

    // ---------------------------------------------------------------- Steam web

    String get(String address)throws IOException {
        HttpURLConnection c=(HttpURLConnection)new URL(address).openConnection();
        return read(c);
    }
    String post(String address,String form)throws IOException {
        HttpURLConnection c=(HttpURLConnection)new URL(address).openConnection();
        c.setRequestMethod("POST");
        c.setDoOutput(true);
        c.setRequestProperty("Content-Type","application/x-www-form-urlencoded");
        try(OutputStream out=prepare(c).getOutputStream()) { out.write(form.getBytes(StandardCharsets.UTF_8)); }
        return read(c);
    }
    static HttpURLConnection prepare(HttpURLConnection c) {
        c.setConnectTimeout(20000);
        c.setReadTimeout(30000);
        c.setRequestProperty("User-Agent","ProsperoPZ/0.1 (Project Zomboid PS5 launcher)");
        return c;
    }
    static String read(HttpURLConnection c)throws IOException {
        prepare(c);
        int status=c.getResponseCode();
        if(status==429)throw new IOException("Steam is limiting requests; try again in a minute");
        if(status!=200)throw new IOException("Steam answered HTTP "+status);
        ByteArrayOutputStream out=new ByteArrayOutputStream();
        try(InputStream in=c.getInputStream()) {
            byte[] b=new byte[65536]; int n;
            while((n=in.read(b))>=0) { out.write(b,0,n); if(out.size()>8*1024*1024)throw new IOException("Steam response too large"); }
        }
        return new String(out.toByteArray(),StandardCharsets.UTF_8);
    }
    static String enc(String s) {
        try { return URLEncoder.encode(s,"UTF-8"); } catch(UnsupportedEncodingException e) { throw new AssertionError(e); }
    }

    /** Item ids for a search (or trending when query is empty). */
    List<Long> list(String query,int build)throws IOException {
        String key=app.settings.getOrDefault("steamApiKey","").trim();
        if(!key.isEmpty())return query(key,query,build);
        List<Long> ids=scrape(query,build>=42?"Build 42":"Build 41");
        if(ids.isEmpty())ids=scrape(query,"");
        return ids;
    }
    List<Long> scrape(String query,String tag)throws IOException {
        StringBuilder u=new StringBuilder(WORKSHOP).append("?appid=").append(APP_ID)
            .append("&section=readytouseitems&numperpage=30");
        if(query.isEmpty())u.append("&browsesort=trend&actualsort=trend&days=7");
        else u.append("&browsesort=textsearch&actualsort=textsearch&searchtext=").append(enc(query));
        if(!tag.isEmpty())u.append("&requiredtags%5B%5D=").append(enc(tag));
        return ids(get(u.toString()));
    }
    static List<Long> ids(String html) {
        LinkedHashSet<Long> out=new LinkedHashSet<Long>();
        Matcher m=FILE_ID.matcher(html);
        while(m.find()&&out.size()<MAX_RESULTS)out.add(Long.parseLong(m.group(1)));
        return new ArrayList<Long>(out);
    }
    List<Long> query(String key,String query,int build)throws IOException {
        StringBuilder u=new StringBuilder(API).append("IPublishedFileService/QueryFiles/v1/?key=").append(enc(key))
            .append("&appid=").append(APP_ID).append("&numperpage=30&return_short_description=false")
            .append("&query_type=").append(query.isEmpty()?3:12).append("&days=7")
            .append("&requiredtags%5B0%5D=").append(enc(build>=42?"Build 42":"Build 41"));
        if(!query.isEmpty())u.append("&search_text=").append(enc(query));
        Map<String,Object> r=Json.obj(Json.obj(Json.parse(get(u.toString()))).get("response"));
        List<Long> ids=new ArrayList<Long>();
        if(r.containsKey("publishedfiledetails"))for(Object o:Json.arr(r.get("publishedfiledetails"))) {
            Object id=Json.obj(o).get("publishedfileid");
            if(id!=null)ids.add(Long.parseLong(Json.str(id)));
        }
        return ids;
    }

    /** Public details for up to 100 items, in the given order. */
    List<Item> details(List<Long> ids)throws IOException {
        List<Item> out=new ArrayList<Item>();
        if(ids.isEmpty())return out;
        StringBuilder form=new StringBuilder("itemcount=").append(ids.size());
        for(int i=0;i<ids.size();i++)form.append("&publishedfileids%5B").append(i).append("%5D=").append(ids.get(i));
        Map<String,Object> r=Json.obj(Json.obj(Json.parse(post(API+"ISteamRemoteStorage/GetPublishedFileDetails/v1/",form.toString()))).get("response"));
        if(!r.containsKey("publishedfiledetails"))return out;
        for(Object o:Json.arr(r.get("publishedfiledetails"))) {
            Map<String,Object> d=Json.obj(o);
            if(num(d,"result")!=1)continue;
            long app=num(d,"consumer_app_id");
            if(app!=0&&app!=APP_ID)continue;
            Item it=new Item();
            it.id=num(d,"publishedfileid");
            it.title=d.containsKey("title")?Json.str(d.get("title")).trim():"";
            it.size=num(d,"file_size");
            it.updated=num(d,"time_updated");
            it.subscriptions=num(d,"lifetime_subscriptions");
            it.type=(int)num(d,"file_type");
            it.preview=d.containsKey("preview_url")?Json.str(d.get("preview_url")):"";
            if(d.containsKey("tags"))for(Object t:Json.arr(d.get("tags"))) {
                Object tag=Json.obj(t).get("tag");
                if(tag!=null)it.tags.add(Json.str(tag));
            }
            String description=d.containsKey("description")?Json.str(d.get("description")).toLowerCase(Locale.ROOT):"";
            it.javaMod=description.contains("zombiebuddy")||description.contains("java mod");
            out.add(it);
        }
        return out;
    }
    static long num(Map<String,Object> m,String k) {
        Object v=m.get(k);
        if(v instanceof Number)return ((Number)v).longValue();
        try { return v==null?0:Long.parseLong(Json.str(v)); } catch(NumberFormatException e) { return 0; }
    }

    /** The items of a collection (empty when id is not a collection). */
    List<Long> collection(long id)throws IOException {
        String form="collectioncount=1&publishedfileids%5B0%5D="+id;
        Map<String,Object> r=Json.obj(Json.obj(Json.parse(post(API+"ISteamRemoteStorage/GetCollectionDetails/v1/",form))).get("response"));
        List<Long> out=new ArrayList<Long>();
        if(!r.containsKey("collectiondetails"))return out;
        for(Object c:Json.arr(r.get("collectiondetails"))) {
            Map<String,Object> m=Json.obj(c);
            if(num(m,"result")!=1||!m.containsKey("children"))continue;
            for(Object child:Json.arr(m.get("children")))out.add(num(Json.obj(child),"publishedfileid"));
        }
        return out;
    }

    /** A small poster in the cache, or "" (Steam's image CDN resizes on request). */
    String poster(Item it) {
        if(it.preview.isEmpty()||!it.preview.startsWith("https://"))return "";
        try {
            Path file=FilesUtil.safe(app.root.resolve("cache/workshop/posters"),it.id+".img");
            if(!Icons.valid(file)) {
                String sized=it.preview+(it.preview.contains("?")?"&":"?")+"imw=160&imh=160&ima=fit&impolicy=Letterbox&letterbox=true";
                Files.createDirectories(file.getParent());
                Path part=file.resolveSibling(it.id+".part");
                HttpURLConnection c=prepare((HttpURLConnection)new URL(sized).openConnection());
                if(c.getResponseCode()!=200)return "";
                try(InputStream in=c.getInputStream()) {
                    Files.copy(in,part,StandardCopyOption.REPLACE_EXISTING);
                }
                if(!Icons.valid(part)) { Files.deleteIfExists(part); return ""; }
                Files.move(part,file,StandardCopyOption.REPLACE_EXISTING);
            }
            return Icons.nativePath(file);
        } catch(IOException e) { return ""; }
    }

    // ---------------------------------------------------------------- profile records

    static Path records(Path profileDir) { return profileDir.resolve("workshop.properties"); }

    static List<Long> queue(Map<String,String> r) {
        List<Long> out=new ArrayList<Long>();
        for(String s:r.getOrDefault("queue","").split(","))if(s.matches("\\d{1,20}"))out.add(Long.parseLong(s));
        return out;
    }
    static void setQueue(Map<String,String> r,List<Long> ids) {
        StringBuilder b=new StringBuilder();
        for(Long id:new LinkedHashSet<Long>(ids)) { if(b.length()>0)b.append(','); b.append(id); }
        if(b.length()==0)r.remove("queue"); else r.put("queue",b.toString());
    }

    /** Remember the last list so the screen can be redrawn without Steam. */
    void saveResults(Path profileDir,String query,List<Item> items)throws IOException {
        Map<String,String> m=new LinkedHashMap<String,String>();
        m.put("query",query);
        m.put("count",Integer.toString(items.size()));
        for(int i=0;i<items.size();i++) {
            Item it=items.get(i);
            m.put(i+".id",Long.toString(it.id));
            m.put(i+".title",it.title.replaceAll("[\\r\\n]+"," "));
            m.put(i+".size",Long.toString(it.size));
            m.put(i+".updated",Long.toString(it.updated));
            m.put(i+".subs",Long.toString(it.subscriptions));
            m.put(i+".type",Integer.toString(it.type));
            m.put(i+".java",Boolean.toString(it.javaMod));
            m.put(i+".poster",poster(it));
        }
        FilesUtil.properties(profileDir.resolve("workshop-results.properties"),m);
    }

    // ---------------------------------------------------------------- commands

    /** content-list for kind=workshop: source is search text, a link or an id. */
    String list(String profileId,String text,int build)throws IOException {
        Path dir=app.dir(profileId);
        Map<String,String> saved=FilesUtil.properties(dir.resolve("workshop-results.properties"));
        if(text==null) {
            if(saved.containsKey("count"))return "Workshop: Cross queues a mod, Square searches";
            text="";
        }
        long id=parseId(text);
        List<Long> ids;
        String label;
        if(id!=0) {
            List<Long> children=collection(id);
            ids=children.isEmpty()?Collections.singletonList(id):children;
            label=children.isEmpty()?"Workshop item "+id:"Collection "+id+": "+children.size()+" items";
        } else {
            ids=list(text.trim(),build);
            label=text.trim().isEmpty()?"Trending this week":"Results for \""+text.trim()+"\"";
        }
        List<Item> items=details(ids.size()>100?ids.subList(0,100):ids);
        saveResults(dir,text.trim(),items);
        if(items.isEmpty())return id!=0?"That Workshop item is not available for Project Zomboid":"No Workshop mods found";
        return label+" ("+items.size()+"). Cross queues a mod, Square searches";
    }

    /** content-toggle for kind=workshop: queue or unqueue one item. */
    String toggle(String profileId,String item,boolean queued)throws IOException {
        if(!item.matches("\\d{1,20}"))throw new IOException("Invalid Workshop item");
        Path file=records(app.dir(profileId));
        Map<String,String> r=FilesUtil.properties(file);
        List<Long> q=queue(r);
        long id=Long.parseLong(item);
        if(queued) { if(!q.contains(id))q.add(id); } else q.remove(id);
        if(q.size()>64)throw new IOException("The download queue holds up to 64 items");
        setQueue(r,q);
        FilesUtil.properties(file,r);
        return q.isEmpty()?"Queue is empty":q.size()+" queued. Choose DOWNLOAD QUEUED to install them";
    }

    /** Rows for the view: the last list, marking queued and installed items. */
    int view(String profileId,Map<String,String> out)throws IOException {
        Path dir=app.dir(profileId);
        Map<String,String> saved=FilesUtil.properties(dir.resolve("workshop-results.properties"));
        Map<String,String> r=FilesUtil.properties(records(dir));
        List<Long> q=queue(r);
        int n=0, count=0;
        try { count=Integer.parseInt(saved.getOrDefault("count","0")); } catch(NumberFormatException ignored) {}
        SimpleDateFormat day=new SimpleDateFormat("yyyy-MM-dd",Locale.ROOT);
        for(int i=0;i<count&&n<Mods.MAX;i++) {
            String id=saved.getOrDefault(i+".id","");
            if(!id.matches("\\d{1,20}"))continue;
            long size=parse(saved.get(i+".size")), updated=parse(saved.get(i+".updated")), subs=parse(saved.get(i+".subs"));
            StringBuilder d=new StringBuilder();
            if("2".equals(saved.get(i+".type")))d.append("Collection  /  ");
            else d.append(String.format(Locale.ROOT,"%.1f MB  /  ",size/1048576.0));
            if(updated>0)d.append("updated ").append(day.format(new Date(updated*1000))).append("  /  ");
            d.append(String.format(Locale.ROOT,"%,d subscribers",subs));
            String installed=r.get("item."+id+".updated");
            if(installed!=null)d.append(parse(installed)<updated?"  /  UPDATE AVAILABLE":"  /  installed");
            if("true".equals(saved.get(i+".java")))d.append("  /  Java mod: needs ZombieBuddy");
            out.put("mod."+n+".name",id);
            out.put("mod."+n+".title",saved.getOrDefault(i+".title",id));
            out.put("mod."+n+".detail",d.toString());
            out.put("mod."+n+".enabled",Boolean.toString(q.contains(Long.parseLong(id))));
            String poster=saved.getOrDefault(i+".poster","");
            if(!poster.isEmpty())out.put("mod."+n+".icon",poster);
            n++;
        }
        out.put("workshop.queued",Integer.toString(q.size()));
        return n;
    }
    static long parse(String s) { try { return s==null?0:Long.parseLong(s); } catch(NumberFormatException e) { return 0; } }

    /** Ids of installed items that have a newer version in the last list. */
    static List<Long> outdated(Path profileDir)throws IOException {
        Map<String,String> saved=FilesUtil.properties(profileDir.resolve("workshop-results.properties"));
        Map<String,String> r=FilesUtil.properties(records(profileDir));
        List<Long> out=new ArrayList<Long>();
        int count=(int)parse(saved.get("count"));
        for(int i=0;i<count;i++) {
            String id=saved.getOrDefault(i+".id","");
            String installed=r.get("item."+id+".updated");
            if(installed!=null&&parse(installed)<parse(saved.get(i+".updated")))out.add(Long.parseLong(id));
        }
        return out;
    }
}
