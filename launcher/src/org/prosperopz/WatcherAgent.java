package org.prosperopz;

import java.io.*;
import java.lang.instrument.ClassFileTransformer;
import java.lang.instrument.Instrumentation;
import java.security.ProtectionDomain;

/** Java agent loaded into the game JVM with -javaagent.
 *
 * Build 42 always starts zombie.DebugFileWatcher, which registers a watch on
 * every folder under media/. Through Wine each watch keeps a directory handle
 * open, and on PS5 the process runs out of handles after a few hundred: the
 * registration fails, the watcher is set to null and the next register call
 * throws a NullPointerException that ends the game thread (black screen, then
 * back to the launcher). The watcher only hot-reloads edited media files, so
 * its registerDir methods are replaced with empty bodies as the class loads. */
public final class WatcherAgent implements ClassFileTransformer {
    static final String TARGET="zombie/DebugFileWatcher";
    static final String PREFIX="registerDir";

    public static void premain(String args,Instrumentation inst) {
        inst.addTransformer(new WatcherAgent());
    }

    public static void agentmain(String args,Instrumentation inst) { premain(args,inst); }

    @Override
    public byte[] transform(ClassLoader loader,String name,Class<?> redefined,ProtectionDomain domain,byte[] bytes) {
        if("zombie/input/JoypadManager".equals(name))ControllerActivator.joypadManagerLoaded=true;
        if(!TARGET.equals(name))return null;
        try {
            Result r=stub(bytes,PREFIX);
            System.out.println("PS5 file watcher: "+(r.patched>0?"disabled "+r.patched+" folder registration method(s)":"no registerDir method found"));
            return r.patched>0?r.bytes:null;
        } catch(Throwable t) {
            System.out.println("PS5 file watcher: patch failed ("+t+")");
            return null;
        }
    }

    static final class Result {
        final byte[] bytes; final int patched;
        Result(byte[] bytes,int patched) { this.bytes=bytes; this.patched=patched; }
    }

    /** Rewrites the Code attribute of every method whose name starts with
     * prefix so it returns immediately (void, 0/false, or null). */
    static Result stub(byte[] in,String prefix)throws IOException {
        DataInputStream d=new DataInputStream(new ByteArrayInputStream(in));
        ByteArrayOutputStream buf=new ByteArrayOutputStream(in.length);
        DataOutputStream o=new DataOutputStream(buf);
        if(d.readInt()!=0xCAFEBABE)throw new IOException("not a class file");
        o.writeInt(0xCAFEBABE);
        o.writeShort(d.readUnsignedShort());
        o.writeShort(d.readUnsignedShort());
        int count=d.readUnsignedShort();
        o.writeShort(count);
        String[] utf=new String[count];
        for(int i=1;i<count;i++) {
            int tag=d.readUnsignedByte();
            o.writeByte(tag);
            switch(tag) {
                case 1: { String s=d.readUTF(); utf[i]=s; o.writeUTF(s); break; }
                case 3: case 4: copy(d,o,4); break;
                case 5: case 6: copy(d,o,8); i++; break;
                case 7: case 8: case 16: case 19: case 20: copy(d,o,2); break;
                case 9: case 10: case 11: case 12: case 17: case 18: copy(d,o,4); break;
                case 15: copy(d,o,3); break;
                default: throw new IOException("unknown constant tag "+tag);
            }
        }
        copy(d,o,6);
        int interfaces=d.readUnsignedShort();
        o.writeShort(interfaces);
        copy(d,o,2*interfaces);
        int fields=d.readUnsignedShort();
        o.writeShort(fields);
        for(int i=0;i<fields;i++) { copy(d,o,6); attributes(d,o); }
        int methods=d.readUnsignedShort();
        o.writeShort(methods);
        int patched=0;
        for(int i=0;i<methods;i++) {
            int access=d.readUnsignedShort(), nameIndex=d.readUnsignedShort(), descIndex=d.readUnsignedShort();
            o.writeShort(access); o.writeShort(nameIndex); o.writeShort(descIndex);
            String name=utf[nameIndex], desc=utf[descIndex];
            byte[] body=name!=null&&desc!=null&&name.startsWith(prefix)?returnCode(desc):null;
            int attrs=d.readUnsignedShort();
            o.writeShort(attrs);
            for(int a=0;a<attrs;a++) {
                int attrName=d.readUnsignedShort();
                int length=d.readInt();
                if(body!=null&&"Code".equals(utf[attrName])) {
                    int maxStack=d.readUnsignedShort(), maxLocals=d.readUnsignedShort();
                    skip(d,length-4);
                    o.writeShort(attrName);
                    o.writeInt(2+2+4+body.length+2+2);
                    o.writeShort(Math.max(maxStack,1));
                    o.writeShort(maxLocals);
                    o.writeInt(body.length);
                    o.write(body);
                    o.writeShort(0); // exception table
                    o.writeShort(0); // attributes (no stack map needed for straight-line code)
                    patched++;
                } else {
                    o.writeShort(attrName); o.writeInt(length); copy(d,o,length);
                }
            }
        }
        attributes(d,o);
        if(d.read()!=-1)throw new IOException("trailing bytes in class file");
        o.flush();
        return new Result(buf.toByteArray(),patched);
    }

    static byte[] returnCode(String desc) {
        char r=desc.charAt(desc.indexOf(')')+1);
        switch(r) {
            case 'V': return new byte[]{(byte)0xB1};                       // return
            case 'Z': case 'B': case 'C': case 'S': case 'I':
                return new byte[]{0x03,(byte)0xAC};                        // iconst_0; ireturn
            case 'L': case '[': return new byte[]{0x01,(byte)0xB0};        // aconst_null; areturn
            default: return null;                                          // leave long/float/double alone
        }
    }

    static void attributes(DataInputStream d,DataOutputStream o)throws IOException {
        int n=d.readUnsignedShort();
        o.writeShort(n);
        for(int i=0;i<n;i++) { o.writeShort(d.readUnsignedShort()); int len=d.readInt(); o.writeInt(len); copy(d,o,len); }
    }

    static void copy(DataInputStream d,DataOutputStream o,int n)throws IOException {
        byte[] b=new byte[n]; d.readFully(b); o.write(b);
    }

    static void skip(DataInputStream d,int n)throws IOException { d.readFully(new byte[n]); }
}
