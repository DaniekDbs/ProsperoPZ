/* SPDX-License-Identifier: GPL-3.0-or-later */
#define _POSIX_C_SOURCE 200809L
#include "pm_bridge.h"
#include "prospero_win.h"
#include <ctype.h>
#include <errno.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

static void copy(char *dst, size_t cap, const char *src)
{
    size_t n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n); dst[n] = 0;
}
static int path(char *out, size_t cap, const char *root, const char *file)
{
    int n = snprintf(out, cap, "%s/%s", root, file);
    return n > 0 && (size_t)n < cap ? 0 : -1;
}
static int make_dir(const char *name)
{
    struct stat st;
    if (!stat(name, &st)) return S_ISDIR(st.st_mode) ? 0 : -1;
    return mkdir(name, 0700);
}
static int regular(const char *name)
{
    struct stat st;
    return !stat(name, &st) && S_ISREG(st.st_mode);
}
static int windows(char *out, size_t cap, const char *unix_path)
{
    size_t n = strlen(unix_path);
    if (n + 3 > cap || unix_path[0] != '/' || strchr(unix_path, '"')) return -1;
    out[0] = 'Z'; out[1] = ':';
    for (size_t i = 0; i <= n; i++) out[i+2] = unix_path[i] == '/' ? '\\' : unix_path[i];
    return 0;
}
static int copy_file(const char *source, const char *target)
{
    char tmp[768], buffer[16384];
    FILE *input = fopen(source, "rb"), *output;
    size_t n; int failed = 0;
    if (!input) return -1;
    if (snprintf(tmp, sizeof(tmp), "%s.tmp", target) >= (int)sizeof(tmp)) { fclose(input); return -1; }
    output = fopen(tmp, "wb");
    if (!output) { fclose(input); return -1; }
    while ((n = fread(buffer, 1, sizeof(buffer), input)))
        if (fwrite(buffer, 1, n, output) != n) { failed = 1; break; }
    failed |= ferror(input) || ferror(output);
    fclose(input); failed |= fclose(output) != 0;
    if (failed || rename(tmp, target)) { unlink(tmp); return -1; }
    chmod(target, 0600); return 0;
}
static int same_file_contents(const char *source, const char *target)
{
    unsigned char a[16384], b[16384];
    FILE *input = fopen(source, "rb"), *existing;
    int same = 0;
    if (!input) return 0;
    existing = fopen(target, "rb");
    if (existing) {
        for (;;) {
            size_t na = fread(a, 1, sizeof(a), input);
            size_t nb = fread(b, 1, sizeof(b), existing);
            if (na != nb || memcmp(a, b, na)) break;
            if (na < sizeof(a)) { same = !ferror(input) && !ferror(existing); break; }
        }
        fclose(existing);
    }
    fclose(input);
    return same;
}
int pm_bridge_initialize(const char *root, const char *app0)
{
    static const char *const dirs[] = {"profiles", "launcher", "runtimes", "instances", "games", "backups",
                                       "imports", "imports/mods", "imports/saves"};
    char name[768], source[768];
    if (make_dir(root)) return -1;
    for (size_t i = 0; i < sizeof(dirs)/sizeof(dirs[0]); i++) {
        if (path(name, sizeof(name), root, dirs[i]) || make_dir(name)) return -1;
    }
    if (path(name, sizeof(name), root, "launcher/prosperopz-launcher.jar") ||
        path(source, sizeof(source), app0, "launcher/prosperopz-launcher.jar")) return -1;
    /* Refresh when the app is upgraded, independent of the selected game JVM. */
    /* A pre-staged matching JAR needs no rename over a loader-owned file. */
    if (same_file_contents(source, name)) return 0;
    /* Homebrew loaders may expose runtime modules without the app's extra
     * files. Deployment also installs this verified JAR in the data root. */
    FILE *packaged=fopen(source,"rb");
    if (!packaged) {
        FILE *staged=fopen(name,"rb");
        if (staged) {
            int a=fgetc(staged), b=fgetc(staged); fclose(staged);
            if (a=='P' && b=='K') return 0;
        }
        return -1;
    }
    fclose(packaged);
    return copy_file(source, name);
}

typedef void (*Property)(void *, const char *, const char *);
static int properties(const char *name, Property callback, void *context)
{
    char line[4096]; FILE *file = fopen(name, "rb");
    if (!file) return -1;
    while (fgets(line, sizeof(line), file)) {
        char *equal, *end;
        if (!strchr(line, '\n') && !feof(file)) { fclose(file); return -1; }
        end = line + strlen(line);
        while (end > line && (end[-1] == '\n' || end[-1] == '\r')) *--end = 0;
        if (!line[0] || line[0] == '#' || !(equal = strchr(line, '='))) continue;
        *equal++ = 0; callback(context, line, equal);
    }
    fclose(file); return 0;
}
static int integer(const char *s, int max)
{
    unsigned value = 0;
    for (; *s; s++) { if (*s < '0' || *s > '9' || value > (unsigned)max / 10) return 0; value = value*10 + (unsigned)(*s-'0'); }
    return value <= (unsigned)max ? (int)value : 0;
}
static int truth(const char *value) { return !strcmp(value,"true") || !strcmp(value,"1"); }
static const char *resolution_name(int height)
{
    switch(height) {
    case 720:return "1280x720"; case 1080:return "1920x1080";
    case 1440:return "2560x1440"; case 2160:return "3840x2160";
    default:return NULL;
    }
}
static int capped(const char *value, int cap) { int n=integer(value,2147483647); return n>cap?cap:n; }
static void view_property(void *context, const char *key, const char *value)
{
    PmView *view = context;
    unsigned i; int chars = 0;
#define TEXT(k,f) if (!strcmp(key,k)) { copy(view->f, sizeof(view->f), value); return; }
    TEXT("status", status); TEXT("detail", detail);
    TEXT("controllerMode", controller_mode); TEXT("settings.controllerMode", controller_mode);
    TEXT("themeColor", theme_color); TEXT("settings.themeColor", theme_color);
    TEXT("vrrMode", vrr_mode); TEXT("settings.vrrMode", vrr_mode);
    TEXT("debugMode", debug_mode); TEXT("settings.debugMode", debug_mode);
    TEXT("content.kind", content_kind); TEXT("content.instance", content_instance);
#undef TEXT
    if (!strcmp(key,"instance.count")) view->instance_count=capped(value,PM_MAX_INSTANCES);
    else if (!strcmp(key,"game.count")) view->game_count=capped(value,PM_MAX_GAMES);
    else if (!strcmp(key,"source.count")) view->source_count=capped(value,PM_MAX_SOURCES);
    else if (!strcmp(key,"mod.count")) view->mod_count=capped(value,PM_MAX_MODS);
    else if (!strcmp(key,"java.count")) view->java_count=capped(value,PM_MAX_JAVA);
    else if (!strcmp(key,"memoryMb") || !strcmp(key,"settings.memoryMb")) view->memory_mb = integer(value,32768);
    else if (!strcmp(key,"resolution") || !strcmp(key,"settings.resolution")) view->resolution_height=!strcmp(value,"3840x2160")?2160:!strcmp(value,"2560x1440")?1440:!strcmp(value,"1280x720")?720:1080;
    else if (!strcmp(key,"done")) view->progress_done = integer(value,2147483647);
    else if (!strcmp(key,"total")) view->progress_total = integer(value,2147483647);
    else if (sscanf(key,"instance.%u.%n", &i, &chars) == 1 && chars && i < PM_MAX_INSTANCES) {
        PmInstance *item = &view->instances[i]; const char *field = key + chars;
        if (!strcmp(field,"id")) copy(item->id,sizeof(item->id),value);
        else if (!strcmp(field,"name")) copy(item->name,sizeof(item->name),value);
        else if (!strcmp(field,"version")) copy(item->version,sizeof(item->version),value);
        else if (!strcmp(field,"loader")) copy(item->loader,sizeof(item->loader),value);
        else if (!strcmp(field,"game")) copy(item->game,sizeof(item->game),value);
        else if (!strcmp(field,"server")) copy(item->server,sizeof(item->server),value);
        else if (!strcmp(field,"password")) copy(item->password,sizeof(item->password),value);
        else if (!strcmp(field,"icon")) copy(item->icon,sizeof(item->icon),value);
        else if (!strcmp(field,"installed")) item->installed = truth(value);
    } else if (sscanf(key,"game.%u.%n", &i, &chars) == 1 && chars && i < PM_MAX_GAMES) {
        PmGame *item = &view->games[i]; const char *field = key + chars;
        if (!strcmp(field,"id")) copy(item->id,sizeof(item->id),value);
        else if (!strcmp(field,"name")) copy(item->name,sizeof(item->name),value);
        else if (!strcmp(field,"detail")) copy(item->detail,sizeof(item->detail),value);
        else if (!strcmp(field,"build")) item->build = integer(value,99);
        else if (!strcmp(field,"java")) item->java = integer(value,99);
        else if (!strcmp(field,"valid")) item->valid = truth(value);
    } else if (sscanf(key,"source.%u.%n", &i, &chars) == 1 && chars && i < PM_MAX_SOURCES) {
        PmPack *item = &view->sources[i]; const char *field = key + chars;
        if (!strcmp(field,"name")) copy(item->name,sizeof(item->name),value);
        else if (!strcmp(field,"path")) copy(item->source,sizeof(item->source),value);
    } else if (sscanf(key,"mod.%u.%n", &i, &chars) == 1 && chars && i < PM_MAX_MODS) {
        PmMod *item = &view->mods[i]; const char *field = key + chars;
        if (!strcmp(field,"name")) copy(item->name,sizeof(item->name),value);
        else if (!strcmp(field,"title")) copy(item->title,sizeof(item->title),value);
        else if (!strcmp(field,"detail")) copy(item->detail,sizeof(item->detail),value);
        else if (!strcmp(field,"icon")) copy(item->icon,sizeof(item->icon),value);
        else if (!strcmp(field,"enabled")) item->enabled = truth(value);
    } else if (sscanf(key,"java.%u.%n", &i, &chars) == 1 && chars && i < PM_MAX_JAVA) {
        PmJavaRuntime *item=&view->java_runtimes[i]; const char *field=key+chars;
        if (!strcmp(field,"major")) item->major=integer(value,99);
        else if (!strcmp(field,"valid")) item->valid=truth(value);
        else if (!strcmp(field,"version")) copy(item->version,sizeof(item->version),value);
        else if (!strcmp(field,"path")) copy(item->path,sizeof(item->path),value);
        else if (!strcmp(field,"detail")) copy(item->detail,sizeof(item->detail),value);
    }
}
static int content_kind_safe(const char *kind)
{ return !strcmp(kind,"mods") || !strcmp(kind,"saves"); }
/* Mods and worlds can be folders or .zip archives copied over FTP. */
static int scan_content_imports(const char *root,PmView *view)
{
    char folder[768],full[1024];DIR *dir;struct dirent *entry;
    view->content_import_count=0;
    if(!content_kind_safe(view->content_kind) ||
       snprintf(folder,sizeof(folder),"%s/imports/%s",root,view->content_kind)>=(int)sizeof(folder) ||
       !(dir=opendir(folder)))return 1;
    while((entry=readdir(dir))) {
        size_t n=strlen(entry->d_name);struct stat st;
        if(!n || entry->d_name[0]=='.' || n>=PM_TEXT || strchr(entry->d_name,'\n') || strchr(entry->d_name,'\r') ||
           view->content_import_count>=PM_MAX_INSTANCES ||
           snprintf(full,sizeof(full),"%s/%s",folder,entry->d_name)>=(int)sizeof(full) ||
           lstat(full,&st))continue;
        int zip=S_ISREG(st.st_mode) && n>4 && !strcasecmp(entry->d_name+n-4,".zip");
        if(!zip && !S_ISDIR(st.st_mode))continue;
        PmPack *pack=&view->content_imports[view->content_import_count];
        if(windows(pack->source,sizeof(pack->source),full))continue;
        copy(pack->name,sizeof(pack->name),entry->d_name);view->content_import_count++;
    }
    closedir(dir);
    for(int i=1;i<view->content_import_count;i++) {
        PmPack item=view->content_imports[i];int j=i;
        while(j>0 && strcmp(view->content_imports[j-1].name,item.name)>0){view->content_imports[j]=view->content_imports[j-1];j--;}
        view->content_imports[j]=item;
    }
    return 1;
}
int pm_bridge_view(const char *root, PmView *view)
{
    char name[768]; int status;
    memset(view, 0, sizeof(*view)); view->memory_mb = 4096; view->resolution_height = 1080;
    copy(view->controller_mode,sizeof(view->controller_mode),"native");
    copy(view->debug_mode,sizeof(view->debug_mode),"off");
    copy(view->status,sizeof(view->status),"Ready");
    copy(view->detail,sizeof(view->detail),"Copy your Windows Project Zomboid folder to /data/ProsperoPZ/imports, then open Games.");
    path(name,sizeof(name),root,"launcher-view.properties");
    status = properties(name,view_property,view);
    path(name,sizeof(name),root,"progress.properties");
    (void)properties(name,view_property,view);
    path(name,sizeof(name),root,"runtime-notice.properties");
    (void)properties(name,view_property,view);
    path(name,sizeof(name),root,"settings.properties");
    (void)properties(name,view_property,view);
    (void)scan_content_imports(root,view);
    return status;
}
static int value_safe(const char *value) { return !strchr(value,'\n') && !strchr(value,'\r'); }
static int write_property(FILE *file, const char *key, const char *value)
{
    if (!value_safe(value)) return -1;
    return value[0] && fprintf(file,"%s=%s\n",key,value) < 0 ? -1 : 0;
}
int pm_bridge_request(const char *root, const PmAction *action)
{
    static const char *const commands[] = {"catalog","scan-games","import-game","delete-game","create","install",
        "prepare-launch","settings","install-java","detect-java","clone","rename-instance","server-password",
        "delete-instance","content-list","content-import","content-toggle","content-delete","content-backup"};
    char name[768], temporary[768]; FILE *file; int allowed = 0, failed = 0;
    for (size_t i=0; i<sizeof(commands)/sizeof(commands[0]); i++) allowed |= !strcmp(action->command,commands[i]);
    if (!allowed || action->java_major<0 || action->java_major>99 ||
        (action->resolution_height && !resolution_name(action->resolution_height)) ||
        path(name,sizeof(name),root,"request.properties") ||
        path(temporary,sizeof(temporary),root,"request.properties.tmp")) return -1;
    path(name,sizeof(name),root,"response.properties"); unlink(name);
    path(name,sizeof(name),root,"runtime-notice.properties"); unlink(name);
    path(name,sizeof(name),root,"pending-launch"); unlink(name);
    if (!(file=fopen(temporary,"wb"))) return -1;
#define FIELD(k,f) failed |= write_property(file,k,action->f) != 0
    FIELD("command",command); FIELD("id",id); FIELD("name",name); FIELD("game",game); FIELD("mode",mode);
    FIELD("source",source); FIELD("mod",mod); FIELD("enabled",enabled);
    FIELD("content_kind",content_kind);
    FIELD("controllerMode",controller_mode);
    FIELD("themeColor",theme_color);
    FIELD("vrrMode",vrr_mode);
    FIELD("debugMode",debug_mode);
#undef FIELD
    if (action->memory_mb) failed |= fprintf(file,"memoryMb=%d\n",action->memory_mb) < 0;
    if(action->resolution_height)failed |= fprintf(file,"resolution=%s\n",resolution_name(action->resolution_height))<0;
    if (action->java_major) failed |= fprintf(file,"major=%d\n",action->java_major) < 0;
    if (!strcmp(action->command,"install-java")) failed |= write_property(file,"repair","true") != 0;
    failed |= fclose(file) != 0;
    if (failed) { unlink(temporary); return -1; }
    if (!strcmp(action->command,"prepare-launch")) {
        char prepared[768];
        path(prepared,sizeof(prepared),root,"profiles/launch.profile"); unlink(prepared);
        path(name,sizeof(name),root,"pending-launch");
        if (!(file=fopen(name,"wb"))) { unlink(temporary); return -1; }
        failed = fputs("prepare-launch\n",file) < 0; failed |= fclose(file) != 0;
        if (failed) { unlink(temporary); return -1; }
        chmod(name,0600);
    }
    path(name,sizeof(name),root,"request.properties");
    if (rename(temporary,name)) { unlink(temporary); return -1; }
    chmod(name,0600);
    return 0;
}
int pm_bridge_backend(const char *root, PwGameProfile *profile)
{
    char exe[768], jar[768], request[768], winroot[768], winjar[768], winrequest[768];
    memset(profile,0,sizeof(*profile)); pw_game_input_init(&profile->input);
    if (path(exe,sizeof(exe),root,"runtimes/java8/bin/java.exe") || !regular(exe) ||
        path(jar,sizeof(jar),root,"launcher/prosperopz-launcher.jar") || !regular(jar) ||
        path(request,sizeof(request),root,"request.properties") ||
        windows(profile->app.executable,sizeof(profile->app.executable),exe) ||
        windows(winroot,sizeof(winroot),root) || windows(winjar,sizeof(winjar),jar) ||
        windows(winrequest,sizeof(winrequest),request)) return -1;
    copy(profile->app.id,sizeof(profile->app.id),"launcher");
    copy(profile->app.name,sizeof(profile->app.name),"ProsperoPZ");
    copy(profile->app.prefix,sizeof(profile->app.prefix),"zomboid");
    copy(profile->app.runtime,sizeof(profile->app.runtime),"wine-wow64");
    copy(profile->app.working_directory,sizeof(profile->app.working_directory),winroot);
    profile->app.architecture=PW_APP_ARCH_PE64; profile->app.graphics=PW_APP_GRAPHICS_GDI;
    profile->runtime.thread_scheduling=1;
    return snprintf(profile->app.arguments,sizeof(profile->app.arguments),
      "-Xms32M -Xmx256M -XX:-UseCompressedOops -XX:-UseCompressedClassPointers -XX:ReservedCodeCacheSize=64M -Djava.awt.headless=true -jar \"%s\" --root \"%s\" --serve",winjar,winroot) < (int)sizeof(profile->app.arguments) ? 0 : -1;
}
static int component_safe(const char *name)
{
    if (!name[0] || !strcmp(name,".") || !strcmp(name,"..")) return 0;
    for (const unsigned char *p=(const unsigned char *)name; *p; p++)
        if (!(isalnum(*p) || *p=='_' || *p=='-' || *p=='.' || *p=='+')) return 0;
    return 1;
}
/* Game JVMs: root\runtimes\javaN\bin\java.exe or a game's own
 * root\games\ID\jre64\bin\java.exe. Nothing else can be started. */
static int allowed_java(const char *root, const char *exe)
{
    char base[768], id[128]; const char *suffix, *end; size_t n;
    if (windows(base,sizeof(base),root)) return 0;
    n=strlen(base);
    if (strncmp(exe,base,n)) return 0;
    if (!strncmp(exe+n,"\\runtimes\\java",14)) {
        suffix=exe+n+14;
        if (!isdigit((unsigned char)*suffix)) return 0;
        while (isdigit((unsigned char)*suffix)) suffix++;
        return !strcmp(suffix,"\\bin\\java.exe");
    }
    if (strncmp(exe+n,"\\games\\",7)) return 0;
    suffix=exe+n+7;
    if (!(end=strchr(suffix,'\\')) || (size_t)(end-suffix)>=sizeof(id)) return 0;
    memcpy(id,suffix,(size_t)(end-suffix)); id[end-suffix]=0;
    return component_safe(id) && !strcmp(end,"\\jre64\\bin\\java.exe");
}
int pm_bridge_game(const char *root, PwGameProfile *profile)
{
    char name[768]; static uint8_t content[PW_APP_PROFILE_MAX_BYTES]; FILE *file; size_t n;
    if (path(name,sizeof(name),root,"profiles/launch.profile") || !(file=fopen(name,"rb"))) return -1;
    n=fread(content,1,sizeof(content),file);
    if (ferror(file) || fgetc(file)!=EOF) { fclose(file); return -1; } fclose(file);
    if (pw_game_profile_parse(content,n,profile)!=PW_OK || strcmp(profile->app.id,"zomboid") ||
        !allowed_java(root,profile->app.executable) || profile->app.architecture!=PW_APP_ARCH_PE64 ||
        (profile->app.graphics!=PW_APP_GRAPHICS_OPENGL && profile->app.graphics!=PW_APP_GRAPHICS_ZINK && profile->app.graphics!=PW_APP_GRAPHICS_AUTO) ||
        strcmp(profile->app.prefix,"zomboid")) return -1;
    profile->runtime.thread_scheduling=1;
    /* Project Zomboid renders through OpenGL on Zink (Vulkan). */
    if (profile->app.graphics==PW_APP_GRAPHICS_OPENGL)
        profile->app.graphics=PW_APP_GRAPHICS_ZINK;
    return 0;
}
typedef struct Response { int ok, command; } Response;
static void response_property(void *context, const char *key, const char *value)
{
    Response *response=context;
    if (!strcmp(key,"ok")) response->ok=!strcmp(value,"true");
    else if (!strcmp(key,"command")) response->command=!strcmp(value,"prepare-launch");
}
int pm_bridge_consume_launch(const char *root)
{
    char name[768]; Response response={0,0};
    path(name,sizeof(name),root,"pending-launch");
    if (!regular(name)) return 0;
    unlink(name); /* Consume exactly once, including failed requests. */
    path(name,sizeof(name),root,"response.properties");
    return !properties(name,response_property,&response) && response.ok && response.command;
}
int pm_bridge_frontend_save(const char *root, const PmFrontend *frontend)
{
    char name[768],temporary[768]; FILE *file; int failed;
    if (path(name,sizeof(name),root,"frontend-state.properties") ||
        path(temporary,sizeof(temporary),root,"frontend-state.properties.tmp") || !(file=fopen(temporary,"wb"))) return -1;
    failed=fprintf(file,"screen=%d\ninstance=%d\ngame=%d\nmod=%d\njava=%d\nserver=%d\nfocus=%d\ncontent_kind=%d\n",(int)frontend->screen,
       frontend->selected_instance,frontend->selected_game,frontend->selected_mod,frontend->selected_java,frontend->selected_server,
       frontend->focus,frontend->content_kind)<0;
    failed |= fclose(file)!=0;
    if (failed || rename(temporary,name)) { unlink(temporary); return -1; }
    return 0;
}
static void frontend_property(void *context, const char *key, const char *value)
{
    PmFrontend *frontend=context;
    if (!strcmp(key,"screen")) frontend->screen=(PmScreen)integer(value,PM_SCREEN_COUNT-1);
    else if (!strcmp(key,"instance")) frontend->selected_instance=integer(value,PM_MAX_INSTANCES-1);
    else if (!strcmp(key,"game")) frontend->selected_game=integer(value,PM_MAX_GAMES-1);
    else if (!strcmp(key,"mod")) frontend->selected_mod=integer(value,PM_MAX_MODS-1);
    else if (!strcmp(key,"java")) frontend->selected_java=integer(value,PM_MAX_JAVA-1);
    else if (!strcmp(key,"server")) frontend->selected_server=integer(value,PM_MAX_INSTANCES-1);
    else if (!strcmp(key,"focus")) frontend->focus=integer(value,11);
    else if (!strcmp(key,"content_kind")) frontend->content_kind=integer(value,1);
}
void pm_bridge_frontend_load(const char *root, PmFrontend *frontend)
{
    frontend->header_focus=-1;frontend->confirmation=0;frontend->confirmation_choice=0;
    char name[768]; if (!path(name,sizeof(name),root,"frontend-state.properties")) (void)properties(name,frontend_property,frontend);
}
void pm_bridge_notice(const char *root, const char *message)
{
    char name[768],temporary[768]; FILE *file; int failed;
    if (path(name,sizeof(name),root,"runtime-notice.properties") ||
        path(temporary,sizeof(temporary),root,"runtime-notice.properties.tmp") || !(file=fopen(temporary,"wb"))) return;
    failed=write_property(file,"status","error") || write_property(file,"detail",message);
    failed |= fclose(file)!=0;
    if (failed || rename(temporary,name)) unlink(temporary);
}
int pm_bridge_auto_catalog(const char *root, const PmView *view)
{
    char name[768]; FILE *file; int failed;
    if (view->game_count>0 || path(name,sizeof(name),root,"catalog-auto.attempted") || regular(name)) return 0;
    if (!(file=fopen(name,"wb"))) return 0;
    failed=fputs("catalog\n",file)<0; failed |= fclose(file)!=0;
    if (failed) { unlink(name); return 0; }
    return 1;
}
int pm_bridge_response_ready(const char *root)
{
    char name[768];
    return !path(name,sizeof(name),root,"response.properties") && regular(name);
}
static int local_settings(const char *root, const PmAction *action, PmView *view)
{
    static const char *const themes[]={"knox","blood","rosewood","louisville","muldraugh","westpoint"};
    char name[768], temporary[768], line[4096]; FILE *input, *output; int failed=0, theme_ok=!action->theme_color[0];
    for (size_t i=0;i<sizeof(themes)/sizeof(themes[0]);i++) theme_ok |= !strcmp(action->theme_color,themes[i]);
    if ((action->memory_mb && (action->memory_mb<1024 || action->memory_mb>8192)) ||
        (action->resolution_height && !resolution_name(action->resolution_height)) ||
        (action->controller_mode[0] && strcmp(action->controller_mode,"emulated") && strcmp(action->controller_mode,"native")) ||
        !theme_ok ||
        (action->vrr_mode[0] && strcmp(action->vrr_mode,"auto") && strcmp(action->vrr_mode,"stable")) ||
        (action->debug_mode[0] && strcmp(action->debug_mode,"on") && strcmp(action->debug_mode,"off")) ||
        path(name,sizeof(name),root,"settings.properties") ||
        path(temporary,sizeof(temporary),root,"settings.properties.tmp")) { errno=EINVAL; return -1; }
    input=fopen(name,"rb");
    if (!input && errno!=ENOENT) return -1;
    output=fopen(temporary,"wb");
    if (!output) { if (input) fclose(input); return -1; }
    if (input) {
        while (fgets(line,sizeof(line),input)) {
            if (!strchr(line,'\n') && !feof(input)) { failed=1; break; }
            if ((action->memory_mb && !strncmp(line,"memoryMb=",9)) ||
                (action->resolution_height && !strncmp(line,"resolution=",11)) ||
                (action->controller_mode[0] && !strncmp(line,"controllerMode=",15)) ||
                (action->theme_color[0] && !strncmp(line,"themeColor=",11)) ||
                (action->vrr_mode[0] && !strncmp(line,"vrrMode=",8)) ||
                (action->debug_mode[0] && !strncmp(line,"debugMode=",10))) continue;
            if (fputs(line,output)<0) { failed=1; break; }
            size_t n=strlen(line);
            if (n && line[n-1]!='\n' && fputc('\n',output)==EOF) failed=1;
        }
        failed |= ferror(input)!=0; fclose(input);
    }
    if (action->memory_mb) failed |= fprintf(output,"memoryMb=%d\n",action->memory_mb)<0;
    if(action->resolution_height)failed |= fprintf(output,"resolution=%s\n",resolution_name(action->resolution_height))<0;
    failed |= write_property(output,"controllerMode",action->controller_mode)!=0;
    failed |= write_property(output,"themeColor",action->theme_color)!=0;
    failed |= write_property(output,"vrrMode",action->vrr_mode)!=0;
    failed |= write_property(output,"debugMode",action->debug_mode)!=0;
    failed |= fclose(output)!=0;
    if (failed || rename(temporary,name)) { int saved=errno; unlink(temporary); errno=saved; return -1; }
    (void)properties(name,view_property,view);
    copy(view->status,sizeof(view->status),"ready");
    copy(view->detail,sizeof(view->detail),"Launcher settings saved");
    return 1;
}
int pm_bridge_local(const char *root, const PmAction *action, PmView *view)
{
    if(!strcmp(action->command,"scan-content-imports")) {
        if(!component_safe(action->id)||!content_kind_safe(action->content_kind)) {errno=EINVAL;return -1;}
        copy(view->content_kind,sizeof(view->content_kind),action->content_kind);
        copy(view->content_instance,sizeof(view->content_instance),action->id);
        int result=scan_content_imports(root,view);
        copy(view->status,sizeof(view->status),"ready");
        snprintf(view->detail,sizeof(view->detail),"Found %d items in /data/ProsperoPZ/imports/%s",view->content_import_count,view->content_kind);
        return result;
    }
    if (!strcmp(action->command,"settings")) return local_settings(root,action,view);
    return 0;
}
int pm_bridge_prepare_prefix(const char *root)
{
    static const char *const folders[]={"prefixes","prefixes/zomboid","prefixes/zomboid/drive_c","prefixes/zomboid/dosdevices"};
    static const char table[]="c:\t../drive_c\np:\t/data/ProsperoPZ\nz:\t/\n";
    static const char original[]="c:\t../drive_c\nz:\t/\n";
    char name[768], temporary[768], present[128], normalized[128];
    for (size_t i=0;i<sizeof(folders)/sizeof(folders[0]);i++)
        if (path(name,sizeof(name),root,folders[i]) || make_dir(name)) return -1;
    if (path(name,sizeof(name),root,"prefixes/zomboid/dosdevices/.pw-symlinks") ||
        path(temporary,sizeof(temporary),root,"prefixes/zomboid/dosdevices/.pw-symlinks.tmp")) return -1;
    FILE *input=fopen(name,"rb");
    if (input) {
        size_t n=fread(present,1,sizeof(present),input), kept=0;
        int failed=ferror(input); fclose(input);
        if (failed || n>=sizeof(present)) {errno=EINVAL;return -1;}
        if (n==sizeof(table)-1 && !memcmp(present,table,n)) return 0;
        for (size_t i=0;i<n;i++) if (present[i]!='\r') normalized[kept++]=present[i];
        if (!((kept==sizeof(table)-1 && !memcmp(normalized,table,kept)) ||
              (kept==sizeof(original)-1 && !memcmp(normalized,original,kept)))) {errno=EINVAL;return -1;}
    } else if (errno!=ENOENT) return -1;
    FILE *output=fopen(temporary,"wb"); if (!output) return -1;
    int failed=fwrite(table,1,sizeof(table)-1,output)!=sizeof(table)-1;
    failed |= fclose(output)!=0;
    if (failed || rename(temporary,name)) {unlink(temporary);return -1;}
    return 0;
}
