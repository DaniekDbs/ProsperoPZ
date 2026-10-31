package org.prosperopz;

import java.io.*;
import java.nio.file.*;
import java.util.*;

/** Optional bounded PNG/JPEG/WebP artwork (profile icons, mod posters). */
final class Icons {
    static final int MAX_BYTES=2*1024*1024;
    static boolean valid(Path file)throws IOException {
        if(!Files.isRegularFile(file,LinkOption.NOFOLLOW_LINKS)||Files.size(file)<24||Files.size(file)>MAX_BYTES)return false;
        byte[] b=new byte[24];
        try(DataInputStream in=new DataInputStream(Files.newInputStream(file))) { in.readFully(b); }
        if(b[0]=='R'&&b[1]=='I'&&b[2]=='F'&&b[3]=='F'&&b[8]=='W'&&b[9]=='E'&&b[10]=='B'&&b[11]=='P'&&
           b[12]=='V'&&b[13]=='P'&&b[14]=='8'&&(b[15]==' '||b[15]=='L'||b[15]=='X')) {
            long length=0;for(int i=7;i>=4;i--)length=(length<<8)|(b[i]&255);
            return length+8==Files.size(file); // Native decoder validates dimensions/payload.
        }
        if((b[0]&255)==255&&(b[1]&255)==216&&(b[2]&255)==255)return true;
        byte[] signature={(byte)137,80,78,71,13,10,26,10};
        for(int i=0;i<8;i++)if(b[i]!=signature[i])return false;
        if(b[12]!=73||b[13]!=72||b[14]!=68||b[15]!=82)return false;
        int width=0,height=0;
        for(int i=16;i<20;i++)width=(width<<8)|(b[i]&255);
        for(int i=20;i<24;i++)height=(height<<8)|(b[i]&255);
        return width>0&&height>0&&width<=2048&&height<=2048;
    }
    static String nativePath(Path file) {
        String value=file.toAbsolutePath().normalize().toString().replace('\\','/');
        return value.regionMatches(true,0,"Z:/data/ProsperoPZ/",0,18)?value.substring(2):value;
    }
    static String instance(Main app,Path directory,Map<String,String> metadata)throws IOException {
        String relative=metadata.getOrDefault("icon","");
        if(!relative.isEmpty())try {Path icon=FilesUtil.safe(directory,relative);if(valid(icon))return nativePath(icon);}
            catch(IOException ignored) { /* Imported metadata cannot choose an external file. */ }
        Path own=directory.resolve("icon.png");
        if(valid(own))return nativePath(own);
        return "";
    }
}
