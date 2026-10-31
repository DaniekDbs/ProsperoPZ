package org.prosperopz;

import java.lang.reflect.*;
import java.util.*;

/** Turns on the PS5 controller in Project Zomboid.
 *
 * The game only uses a controller after its "Enable" box is ticked in
 * Options > Controller, which needs a pointer the native gamepad mode does
 * not have. Inside the game JVM this thread waits for the game's
 * JoypadManager, finds the connected controllers' GUIDs (the DualSense is an
 * XInput controller to the game) and calls setControllerActive(guid, true),
 * on the game's main thread when it has a queue for that. Everything is
 * looked up by reflection and logged, as the game's code is not shipped. */
final class ControllerActivator implements Runnable {
    /** GLFW's GUID for an XInput gamepad (subtype 1). */
    static final String XINPUT_GUID="78696e70757401000000000000000000";
    static volatile boolean joypadManagerLoaded;

    static void start() {
        Thread t=new Thread(new ControllerActivator(),"ProsperoPZ-controller");
        t.setDaemon(true);
        t.start();
    }

    static void log(String s) { System.out.println("PS5 controller: "+s); }

    public void run() {
        try { loop(); } catch(Throwable t) { log("stopped ("+t+")"); }
    }

    void loop()throws Exception {
        ClassLoader loader=ClassLoader.getSystemClassLoader();
        long start=System.currentTimeMillis(), loadedAt=0;
        Set<String> done=new HashSet<String>();
        boolean described=false, reportedNone=false;
        while(System.currentTimeMillis()-start<15*60*1000L) {
            Thread.sleep(1000);
            // Let the game load and initialize JoypadManager itself; the agent
            // reports the load. Without the agent, wait for the window instead.
            if(joypadManagerLoaded&&loadedAt==0)loadedAt=System.currentTimeMillis();
            boolean ready=loadedAt!=0?System.currentTimeMillis()-loadedAt>3000:System.currentTimeMillis()-start>30000;
            if(!ready)continue;
            Class<?> jm;
            try { jm=Class.forName("zombie.input.JoypadManager",true,loader); }
            catch(ClassNotFoundException e) { log("JoypadManager not found"); return; }
            Object manager=staticValue(jm,"instance");
            if(manager==null)continue;
            if(!described) { described=true; describe(jm); }
            List<String> guids=connected(loader);
            if(guids.isEmpty()) {
                if(!reportedNone) { reportedNone=true; log("no controller reported by the game yet"); }
                // The DualSense is always XInput controller 0; enable its GUID
                // so the game uses it as soon as it does see it.
                if(System.currentTimeMillis()-start>60000&&done.add(XINPUT_GUID))activate(loader,jm,manager,XINPUT_GUID);
                continue;
            }
            for(String guid:guids)if(done.add(guid))activate(loader,jm,manager,guid);
        }
    }

    static void describe(Class<?> jm) {
        StringBuilder b=new StringBuilder();
        for(Method m:jm.getDeclaredMethods()) {
            String n=m.getName().toLowerCase(Locale.ROOT);
            if(n.contains("active")||n.contains("controller")||n.contains("guid"))b.append(' ').append(m.getName());
        }
        log("JoypadManager methods:"+b);
    }

    /** GUIDs of the controllers the game sees: through its input object
     * (GameWindow's static field with getControllerCount/getController),
     * then through GLFW. */
    static List<String> connected(ClassLoader loader) {
        List<String> out=new ArrayList<String>();
        try {
            Class<?> gw=Class.forName("zombie.GameWindow",false,loader);
            for(Field f:gw.getDeclaredFields()) {
                if(!Modifier.isStatic(f.getModifiers()))continue;
                Method count=method(f.getType(),"getControllerCount"), get=method(f.getType(),"getController",int.class);
                if(count==null||get==null)continue;
                f.setAccessible(true);
                Object input=f.get(null);
                if(input==null)continue;
                int n=((Number)count.invoke(input)).intValue();
                for(int i=0;i<n;i++) {
                    Object c=get.invoke(input,i);
                    if(c==null)continue;
                    String guid=text(c,"getGUID","getGuid","getJoystickGUID");
                    if(guid!=null&&!guid.isEmpty()&&!out.contains(guid)) {
                        out.add(guid);
                        log("found "+guid+" \""+text(c,"getGamepadName","getJoystickName","getName")+"\"");
                    }
                }
                if(!out.isEmpty())return out;
            }
        } catch(Throwable t) { log("game input lookup failed ("+t+")"); }
        try {
            Class<?> glfw=Class.forName("org.lwjgl.glfw.GLFW",false,loader);
            Method present=glfw.getMethod("glfwJoystickPresent",int.class), guid=glfw.getMethod("glfwGetJoystickGUID",int.class);
            for(int jid=0;jid<16;jid++) {
                if(!(Boolean)present.invoke(null,jid))continue;
                String g=(String)guid.invoke(null,jid);
                if(g!=null&&!out.contains(g)) { out.add(g); log("found "+g+" through GLFW joystick "+jid); }
            }
        } catch(Throwable t) { log("GLFW lookup failed ("+t+")"); }
        return out;
    }

    static void activate(ClassLoader loader,Class<?> jm,final Object manager,final String guid)throws Exception {
        final Method set=method(jm,"setControllerActive",String.class,boolean.class);
        final Method sync=method(jm,"syncActiveControllers");
        Runnable task=new Runnable() { public void run() {
            try {
                if(set!=null)set.invoke(manager,guid,true);
                else {
                    Object active=fieldValue(manager,"ActiveControllerGUIDs");
                    if(active instanceof Set) { @SuppressWarnings("unchecked") Set<String> s=(Set<String>)active; s.add(guid); }
                    if(sync!=null)sync.invoke(manager);
                }
                log("enabled "+guid+(set!=null?" (setControllerActive)":" (ActiveControllerGUIDs)"));
            } catch(Throwable t) { log("enable failed for "+guid+" ("+(t instanceof InvocationTargetException?t.getCause():t)+")"); }
        }};
        if(!onMainThread(loader,task))task.run();
    }

    /** Queue a task on the game's main thread, if it has a static
     * invoke-on-main-thread method taking a Runnable. */
    static boolean onMainThread(ClassLoader loader,Runnable task) {
        for(String cls:new String[]{"zombie.MainThread","zombie.GameWindow","zombie.core.opengl.RenderThread"}) {
            try {
                for(Method m:Class.forName(cls,false,loader).getDeclaredMethods()) {
                    String n=m.getName().toLowerCase(Locale.ROOT);
                    if(!Modifier.isStatic(m.getModifiers())||m.getParameterTypes().length!=1||m.getParameterTypes()[0]!=Runnable.class)continue;
                    if(!n.contains("invoke")||!n.contains("main"))continue;
                    m.setAccessible(true);
                    m.invoke(null,task);
                    log("queued on the main thread with "+cls+"."+m.getName());
                    return true;
                }
            } catch(Throwable ignored) {}
        }
        return false;
    }

    static Method method(Class<?> c,String name,Class<?>... args) {
        for(Class<?> k=c;k!=null;k=k.getSuperclass()) {
            try { Method m=k.getDeclaredMethod(name,args); m.setAccessible(true); return m; }
            catch(NoSuchMethodException e) {} catch(RuntimeException e) { return null; }
        }
        return null;
    }

    static String text(Object o,String... names) {
        for(String n:names) {
            Method m=method(o.getClass(),n);
            if(m==null)continue;
            try { Object v=m.invoke(o); if(v!=null)return v.toString(); } catch(Throwable ignored) {}
        }
        return null;
    }

    static Object staticValue(Class<?> c,String name) {
        try { Field f=c.getDeclaredField(name); f.setAccessible(true); return f.get(null); } catch(Throwable t) { return null; }
    }

    static Object fieldValue(Object o,String name) {
        try { Field f=o.getClass().getDeclaredField(name); f.setAccessible(true); return f.get(o); } catch(Throwable t) { return null; }
    }
}
