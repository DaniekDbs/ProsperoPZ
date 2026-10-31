/* SPDX-License-Identifier: GPL-3.0-or-later */
#define _POSIX_C_SOURCE 200809L
#include "pm_bridge.h"
#include "pw_wine_launch.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static char content[65536];
static void write_file(const char *name,const char *text)
{ FILE *f=fopen(name,"wb"); assert(f); assert(fputs(text,f)>=0); assert(!fclose(f)); }
static const char *read_file(const char *name)
{
    FILE *f=fopen(name,"rb"); assert(f);
    size_t n=fread(content,1,sizeof(content)-1,f); fclose(f); content[n]=0; return content;
}
static const char *read_request(const char *root)
{
    char name[1024]; snprintf(name,sizeof(name),"%s/request.properties",root); return read_file(name);
}
/* A launch profile whose executable is EXE (relative to the data root, Windows separators). */
static void write_profile(const char *root,const char *exe,const char *id,const char *prefix)
{
    char name[1024], text[4096], winroot[1024];
    snprintf(winroot,sizeof(winroot),"Z:%s",root);
    for(char *p=winroot;*p;p++) if(*p=='/') *p='\\';
    snprintf(text,sizeof(text),"[application]\nid=%s\nname=Project Zomboid\nexecutable=%s\\%s\nworking_directory=%s\\games\\build41\n"
             "arguments=-Xmx4096m -cp a;b;c org.prosperopz.Main\nprefix=%s\nruntime=wine-wow64\narchitecture=pe64\ngraphics=zink\n"
             "[input]\nmode=xinput\nmouse=none\n",id,winroot,exe,winroot,prefix);
    snprintf(name,sizeof(name),"%s/profiles/launch.profile",root); write_file(name,text);
}
int main(void)
{
    char root[]="/tmp/prosperopz-bridge-test.XXXXXX", name[1024];
    static PmView view; static PwGameProfile game;
    PmAction action={0}; PwWineLaunch launch; PmFrontend frontend={0},loaded={0};
    PwWineApp app={"zomboid","Project Zomboid","Java","Z:\\data\\ProsperoPZ\\games\\build41\\jre64\\bin\\java.exe"};
    assert(mkdtemp(root));

    /* View parsing is bounded and maps every Project Zomboid field. */
    snprintf(name,sizeof(name),"%s/launcher-view.properties",root);
    write_file(name,"instance.count=99\ninstance.0.id=survivor\ninstance.0.installed=true\ninstance.0.loader=server\n"
               "instance.0.server=ps5\ninstance.0.password=abc123\ninstance.0.version=Project Zomboid Build 41\n"
               "game.count=40\ngame.0.id=build41\ngame.0.build=41\ngame.0.java=17\ngame.0.valid=true\ngame.0.detail=Ready\n"
               "source.count=2\nsource.0.name=ProjectZomboid\nsource.0.path=Z:\\data\\ProsperoPZ\\imports\\ProjectZomboid\n"
               "mod.count=129\nmod.0.name=tsarslib\nmod.0.title=Tsar Library\nmod.0.enabled=true\nmod.0.detail=Library\n"
               "java.count=20\njava.0.major=8\njava.0.valid=true\njava.0.version=1.8.0_504\njava.1.major=17\njava.1.valid=false\n"
               "settings.memoryMb=6144\nsettings.debugMode=on\ncontent.kind=mods\ncontent.instance=survivor\n");
    assert(!pm_bridge_view(root,&view));
    assert(view.instance_count==PM_MAX_INSTANCES && view.game_count==PM_MAX_GAMES && view.mod_count==PM_MAX_MODS);
    assert(view.java_count==PM_MAX_JAVA && view.source_count==2 && view.memory_mb==6144);
    assert(view.instances[0].installed && !strcmp(view.instances[0].loader,"server"));
    assert(!strcmp(view.instances[0].server,"ps5") && !strcmp(view.instances[0].password,"abc123"));
    assert(!strcmp(view.games[0].id,"build41") && view.games[0].build==41 && view.games[0].java==17 && view.games[0].valid);
    assert(strstr(view.sources[0].source,"imports\\ProjectZomboid"));
    assert(!strcmp(view.mods[0].title,"Tsar Library") && view.mods[0].enabled);
    assert(!strcmp(view.debug_mode,"on") && !strcmp(view.content_kind,"mods"));
    assert(view.java_runtimes[0].valid && view.java_runtimes[1].major==17 && !view.java_runtimes[1].valid);
    assert(!pm_bridge_auto_catalog(root,&view));
    view.game_count=0; assert(pm_bridge_auto_catalog(root,&view)); assert(!pm_bridge_auto_catalog(root,&view));

    /* Requests: only listed commands, no line injection, every field serialized. */
    strcpy(action.command,"install-java"); action.java_major=17; assert(!pm_bridge_request(root,&action));
    assert(strstr(read_request(root),"command=install-java\n") && strstr(content,"major=17\n") && strstr(content,"repair=true\n"));
    action.java_major=-1; assert(pm_bridge_request(root,&action)); action.java_major=0;
    memset(&action,0,sizeof(action));
    strcpy(action.command,"create"); strcpy(action.game,"build41"); strcpy(action.mode,"server"); strcpy(action.name,"PS5 Server");
    assert(!pm_bridge_request(root,&action));
    assert(strstr(read_request(root),"game=build41\n") && strstr(content,"mode=server\n") && strstr(content,"name=PS5 Server\n"));
    memset(&action,0,sizeof(action));
    strcpy(action.command,"content-toggle"); strcpy(action.id,"survivor"); strcpy(action.content_kind,"mods");
    strcpy(action.mod,"tsarslib"); strcpy(action.enabled,"false"); assert(!pm_bridge_request(root,&action));
    assert(strstr(read_request(root),"mod=tsarslib\n") && strstr(content,"enabled=false\n") && strstr(content,"content_kind=mods\n"));
    strcpy(action.command,"content-backup"); strcpy(action.content_kind,"saves"); strcpy(action.mod,"Sandbox/My World");
    assert(!pm_bridge_request(root,&action) && strstr(read_request(root),"mod=Sandbox/My World\n"));
    memset(&action,0,sizeof(action));
    strcpy(action.command,"import-game"); strcpy(action.source,view.sources[0].source); assert(!pm_bridge_request(root,&action));
    strcpy(action.command,"catalog"); strcpy(action.name,"bad\ncommand=delete-game"); assert(pm_bridge_request(root,&action));
    action.name[0]=0; strcpy(action.command,"run-exe"); assert(pm_bridge_request(root,&action));
    strcpy(action.command,"pack-search"); assert(pm_bridge_request(root,&action));
    strcpy(action.command,"server-password"); strcpy(action.id,"survivor"); assert(!pm_bridge_request(root,&action));

    /* prepare-launch is consumed once, and only after a successful response. */
    snprintf(name,sizeof(name),"%s/profiles",root); assert(!mkdir(name,0700));
    strcpy(action.command,"prepare-launch"); assert(!pm_bridge_request(root,&action));
    snprintf(name,sizeof(name),"%s/response.properties",root); write_file(name,"ok=false\ncommand=prepare-launch\n");
    assert(!pm_bridge_consume_launch(root)); assert(!pm_bridge_consume_launch(root));
    assert(!pm_bridge_request(root,&action)); write_file(name,"ok=true\ncommand=prepare-launch\n");
    assert(pm_bridge_consume_launch(root)); assert(!pm_bridge_consume_launch(root));

    /* Launch arguments never accept a raw executable path. */
    char *raw[]={"path=Z:\\random.exe"}; assert(!pw_wine_launch_parse(1,raw,&app,1,&launch)); assert(launch.refused);
    char *override[]={"profile=zomboid","path=Z:\\random.exe"};
    assert(!pw_wine_launch_parse(2,override,&app,1,&launch)); assert(launch.refused);
    char *valid[]={"profile=zomboid"}; assert(!pw_wine_launch_parse(1,valid,&app,1,&launch)); assert(launch.mode==PW_WINE_LAUNCH_GAME);

    /* The game JVM must be the game's own jre64 or a managed runtime. */
    write_profile(root,"games\\build41\\jre64\\bin\\java.exe","zomboid","zomboid");
    assert(!pm_bridge_game(root,&game));
    assert(game.app.graphics==PW_APP_GRAPHICS_ZINK && game.input.mode==PW_GAME_INPUT_XINPUT);
    write_profile(root,"runtimes\\java17\\bin\\java.exe","zomboid","zomboid"); assert(!pm_bridge_game(root,&game));
    write_profile(root,"games\\..\\jre64\\bin\\java.exe","zomboid","zomboid"); assert(pm_bridge_game(root,&game));
    write_profile(root,"games\\build41\\ProjectZomboid64.exe","zomboid","zomboid"); assert(pm_bridge_game(root,&game));
    write_profile(root,"games\\build41\\jre64\\bin\\java.exe","minecraft","zomboid"); assert(pm_bridge_game(root,&game));
    write_profile(root,"games\\build41\\jre64\\bin\\java.exe","zomboid","other"); assert(pm_bridge_game(root,&game));
    write_profile(root,"imports\\evil\\jre64\\bin\\java.exe","zomboid","zomboid"); assert(pm_bridge_game(root,&game));

    /* Frontend position survives restarts; transient dialogs do not. */
    frontend.screen=PM_SERVER; frontend.selected_instance=3; frontend.selected_game=1; frontend.selected_server=2;
    frontend.content_kind=1; frontend.header_focus=2; frontend.confirmation=1; frontend.focus=4;
    assert(!pm_bridge_frontend_save(root,&frontend)); pm_bridge_frontend_load(root,&loaded);
    assert(loaded.screen==PM_SERVER && loaded.selected_instance==3 && loaded.selected_game==1 && loaded.selected_server==2);
    assert(loaded.content_kind==1 && loaded.focus==4 && loaded.header_focus==-1 && !loaded.confirmation);

    /* The packaged launcher JAR is refreshed only when it changes. */
    char initialized[]="/tmp/prosperopz-init-test.XXXXXX", package[]="/tmp/prosperopz-package-test.XXXXXX";
    assert(mkdtemp(initialized) && mkdtemp(package));
    snprintf(name,sizeof(name),"%s/launcher",package); assert(!mkdir(name,0700));
    snprintf(name,sizeof(name),"%s/launcher/prosperopz-launcher.jar",package); write_file(name,"old jar");
    assert(!pm_bridge_initialize(initialized,package));
    char destination[1024]; struct stat before, after;
    snprintf(destination,sizeof(destination),"%s/launcher/prosperopz-launcher.jar",initialized);
    assert(!stat(destination,&before));
    assert(!pm_bridge_initialize(initialized,package)); assert(!stat(destination,&after));
    assert(before.st_ino==after.st_ino);
    write_file(name,"new jar"); assert(!pm_bridge_initialize(initialized,package));
    assert(!strcmp(read_file(destination),"new jar"));
    snprintf(name,sizeof(name),"%s/games",initialized); assert(!stat(name,&after) && S_ISDIR(after.st_mode));
    snprintf(name,sizeof(name),"%s/imports/saves",initialized); assert(!stat(name,&after) && S_ISDIR(after.st_mode));
    assert(pm_bridge_initialize(initialized,"/nonexistent/prosperopz-package"));
    write_file(destination,"PK staged jar");
    assert(!pm_bridge_initialize(initialized,"/nonexistent/prosperopz-package"));

    /* Local settings merge and validate without the Java backend. */
    snprintf(name,sizeof(name),"%s/settings.properties",root);
    write_file(name,"java8=Z:\\data\\ProsperoPZ\\runtimes\\java8\\bin\\java.exe\nmemoryMb=2048\n");
    memset(&action,0,sizeof(action)); strcpy(action.command,"settings"); action.memory_mb=6144; action.resolution_height=1440;
    strcpy(action.theme_color,"blood"); strcpy(action.controller_mode,"emulated"); strcpy(action.debug_mode,"on");
    assert(pm_bridge_local(root,&action,&view)==1 && view.memory_mb==6144 && view.resolution_height==1440);
    assert(!pm_bridge_view(root,&view) && !strcmp(view.theme_color,"blood") && !strcmp(view.controller_mode,"emulated"));
    assert(strstr(read_file(name),"java8=Z:") && strstr(content,"debugMode=on\n") && !strstr(content,"memoryMb=2048"));
    assert(!pm_bridge_request(root,&action) && strstr(read_request(root),"themeColor=blood\n") && strstr(content,"resolution=2560x1440\n"));
    action.memory_mb=512; assert(pm_bridge_local(root,&action,&view)==-1); action.memory_mb=0;
    strcpy(action.theme_color,"grass"); assert(pm_bridge_local(root,&action,&view)==-1); action.theme_color[0]=0;
    strcpy(action.debug_mode,"maybe"); assert(pm_bridge_local(root,&action,&view)==-1); action.debug_mode[0]=0;
    action.resolution_height=900; assert(pm_bridge_local(root,&action,&view)==-1); assert(pm_bridge_request(root,&action));
    action.resolution_height=0;
    strcpy(action.command,"catalog"); assert(pm_bridge_local(root,&action,&view)==0);

    /* Import lists: folders and .zip files, never symlinks or other files. */
    snprintf(name,sizeof(name),"%s/imports",root); assert(!mkdir(name,0700));
    snprintf(name,sizeof(name),"%s/imports/mods",root); assert(!mkdir(name,0700));
    snprintf(name,sizeof(name),"%s/imports/mods/2392709985",root); assert(!mkdir(name,0700));
    snprintf(name,sizeof(name),"%s/imports/mods/Brita.zip",root); write_file(name,"PK");
    snprintf(name,sizeof(name),"%s/imports/mods/readme.txt",root); write_file(name,"text");
    snprintf(name,sizeof(name),"%s/imports/mods/alias.zip",root); assert(!symlink("/etc/passwd",name));
    memset(&action,0,sizeof(action)); strcpy(action.command,"scan-content-imports"); strcpy(action.id,"survivor");
    strcpy(action.content_kind,"mods");
    assert(pm_bridge_local(root,&action,&view)==1 && view.content_import_count==2);
    assert(!strcmp(view.content_imports[0].name,"2392709985") && !strcmp(view.content_imports[1].name,"Brita.zip"));
    assert(strstr(view.content_imports[1].source,"Z:"));
    strcpy(action.content_kind,"resourcepacks"); assert(pm_bridge_local(root,&action,&view)==-1);
    strcpy(action.content_kind,"saves"); strcpy(action.id,"../x"); assert(pm_bridge_local(root,&action,&view)==-1);

    /* Wine drive table for the zomboid prefix. */
    assert(!pm_bridge_prepare_prefix(root)); assert(!pm_bridge_prepare_prefix(root));
    snprintf(name,sizeof(name),"%s/prefixes/zomboid/dosdevices/.pw-symlinks",root);
    assert(!strcmp(read_file(name),"c:\t../drive_c\np:\t/data/ProsperoPZ\nz:\t/\n"));
    write_file(name,"c:\t../drive_c\r\nz:\t/\r\n"); assert(!pm_bridge_prepare_prefix(root));
    assert(!strcmp(read_file(name),"c:\t../drive_c\np:\t/data/ProsperoPZ\nz:\t/\n"));
    write_file(name,"c:\t../drive_c\np:\t/data/OtherApp\nz:\t/\n"); assert(pm_bridge_prepare_prefix(root));
    puts("Project Zomboid runtime bridge tests passed");
    return 0;
}
