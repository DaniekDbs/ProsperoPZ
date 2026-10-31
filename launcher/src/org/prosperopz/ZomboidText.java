package org.prosperopz;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;

/** Lenient readers/writers for Project Zomboid's own text formats.
 * mod.info, options.ini and server .ini files allow comments, blank lines,
 * repeated keys and stray lines; launcher properties files do not. */
final class ZomboidText {
    private ZomboidText() {}

    static String read(Path file)throws IOException {
        if(Files.size(file)>4L*1024*1024)throw new IOException("Text file too large: "+file.getFileName());
        String text=new String(Files.readAllBytes(file),StandardCharsets.UTF_8);
        return text.startsWith("﻿")?text.substring(1):text;
    }

    /** First value wins; repeated keys are collected under key#N. */
    static Map<String,String> keyValues(Path file)throws IOException {
        Map<String,String> values=new LinkedHashMap<String,String>();
        if(!Files.isRegularFile(file))return values;
        for(String line:read(file).split("\\r?\\n")) {
            String trimmed=line.trim();
            if(trimmed.isEmpty()||trimmed.startsWith("#")||trimmed.startsWith(";"))continue;
            int equal=trimmed.indexOf('=');
            if(equal<=0)continue;
            String key=trimmed.substring(0,equal).trim(),value=trimmed.substring(equal+1).trim();
            if(!values.containsKey(key)) { values.put(key,value); continue; }
            int n=1; while(values.containsKey(key+"#"+n))n++;
            values.put(key+"#"+n,value);
        }
        return values;
    }

    /** Replace or append keys in a key=value file and keep every other line. */
    static void merge(Path file,Map<String,String> changes,boolean onlyIfMissing)throws IOException {
        List<String> lines=new ArrayList<String>();
        if(Files.isRegularFile(file))lines.addAll(Arrays.asList(read(file).split("\\r?\\n",-1)));
        while(!lines.isEmpty()&&lines.get(lines.size()-1).isEmpty())lines.remove(lines.size()-1);
        Set<String> done=new HashSet<String>();
        for(int i=0;i<lines.size();i++) {
            String line=lines.get(i);
            int equal=line.indexOf('=');
            if(equal<=0||line.trim().startsWith("#"))continue;
            String key=line.substring(0,equal).trim();
            if(!changes.containsKey(key)||done.contains(key))continue;
            done.add(key);
            if(!onlyIfMissing)lines.set(i,key+"="+changes.get(key));
        }
        for(Map.Entry<String,String> change:changes.entrySet())
            if(!done.contains(change.getKey()))lines.add(change.getKey()+"="+change.getValue());
        StringBuilder out=new StringBuilder();
        for(String line:lines)out.append(line).append("\n");
        FilesUtil.write(file,out.toString());
    }
}
