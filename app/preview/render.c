/* Native framebuffer preview: no browser, CSS or parallel mock UI. */
#include "pm_frontend.h"
#include "pm_icons.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static PmView view;
static char icon_root[512];
#define SET(field, value) snprintf(field, sizeof(field), "%s", value)
static void fixture(void)
{
    SET(view.status, "idle");
    SET(view.detail, "2 game installations ready");
    view.memory_mb = 4096;
    view.resolution_height = 1080;
    SET(view.controller_mode, "native");
    view.game_count = 2;
    SET(view.games[0].id, "build41");
    SET(view.games[0].name, "Project Zomboid 41.78.16");
    SET(view.games[0].detail, "Ready. Uses the game's bundled Java 17");
    view.games[0].build = 41;
    view.games[0].java = 17;
    view.games[0].valid = 1;
    SET(view.games[1].id, "build42");
    SET(view.games[1].name, "Project Zomboid Build 42");
    SET(view.games[1].detail, "Ready. Uses the game's bundled Java 17");
    view.games[1].build = 42;
    view.games[1].java = 17;
    view.games[1].valid = 1;
    const char *names[] = {"Riverside Run", "Muldraugh Long Haul", "Louisville or Bust", "Co-op Night",
                           "PS5 Server", "Build 42 Testing", "Hardcore Apocalypse", "Rosewood Fire Station"};
    const char *modes[] = {"client", "client", "client", "client", "server", "client", "client", "client"};
    view.instance_count = 8;
    for (int j = 0; j < 8; j++)
    {
        PmInstance *i = &view.instances[j];
        snprintf(i->id, sizeof(i->id), "profile-%04d", j + 1);
        SET(i->name, names[j]);
        SET(i->loader, modes[j]);
        SET(i->version, j == 5 ? view.games[1].name : view.games[0].name);
        SET(i->game, j == 5 ? "build42" : "build41");
        i->installed = 1;
        if (!strcmp(modes[j], "server"))
        {
            SET(i->server, "ps5server");
            SET(i->password, "k7mq2xr9tb");
        }
    }
    snprintf(view.instances[0].icon, sizeof(view.instances[0].icon), "%.490s/custom.png", icon_root);
    strcpy(view.content_instance, view.instances[0].id);
    strcpy(view.content_kind, "mods");
    view.mod_count = 5;
    const char *mods[][3] = {{"tsarslib", "Tsar's Common Library", ""},
                             {"BedfordFalls", "Bedford Falls", "Requires tsarslib  /  Map: Bedford Falls, KY"},
                             {"Brita", "Brita's Weapon Pack", "Adds hundreds of firearms and attachments."},
                             {"Hydrocraft", "Hydrocraft", "Crafting overhaul."},
                             {"ModManager", "Mod Manager", "Sort and filter mods in game."}};
    for (int j = 0; j < 5; j++)
    {
        SET(view.mods[j].name, mods[j][0]);
        SET(view.mods[j].title, mods[j][1]);
        SET(view.mods[j].detail, mods[j][2]);
        view.mods[j].enabled = j != 3;
    }
    view.java_count = 2;
    view.java_runtimes[0].major = 8;
    view.java_runtimes[0].valid = 1;
    SET(view.java_runtimes[0].version, "1.8.0_462");
    view.java_runtimes[1].major = 17;
    view.java_runtimes[1].valid = 1;
    SET(view.java_runtimes[1].version, "17.0.16");
}
/* Same key=value format the native bridge reads from launcher-view.properties. */
static void load(const char *path)
{
    FILE *fp = fopen(path, "rb");
    char line[1024];
    if (!fp)
    {
        perror(path);
        exit(2);
    }
    memset(&view, 0, sizeof(view));
    while (fgets(line, sizeof(line), fp))
    {
        char *value = strchr(line, '=');
        int n;
        char prop[100];
        if (!value || line[0] == '#')
            continue;
        *value++ = 0;
        value[strcspn(value, "\r\n")] = 0;
        if (!strcmp(line, "status")) SET(view.status, value);
        else if (!strcmp(line, "detail")) SET(view.detail, value);
        else if (!strcmp(line, "instance.count")) view.instance_count = atoi(value);
        else if (!strcmp(line, "game.count")) view.game_count = atoi(value);
        else if (!strcmp(line, "source.count")) view.source_count = atoi(value);
        else if (!strcmp(line, "mod.count")) view.mod_count = atoi(value);
        else if (!strcmp(line, "java.count")) view.java_count = atoi(value);
        else if (!strcmp(line, "settings.memoryMb")) view.memory_mb = atoi(value);
        else if (!strcmp(line, "content.kind")) SET(view.content_kind, value);
        else if (!strcmp(line, "content.instance")) SET(view.content_instance, value);
        else if (!strcmp(line, "settings.controllerMode")) SET(view.controller_mode, value);
        else if (!strcmp(line, "settings.themeColor")) SET(view.theme_color, value);
        else if (!strcmp(line, "settings.vrrMode")) SET(view.vrr_mode, value);
        else if (!strcmp(line, "settings.debugMode")) SET(view.debug_mode, value);
        else if (sscanf(line, "instance.%d.%99s", &n, prop) == 2 && n >= 0 && n < PM_MAX_INSTANCES)
        {
            PmInstance *i = &view.instances[n];
            if (!strcmp(prop, "id")) SET(i->id, value);
            if (!strcmp(prop, "name")) SET(i->name, value);
            if (!strcmp(prop, "version")) SET(i->version, value);
            if (!strcmp(prop, "loader")) SET(i->loader, value);
            if (!strcmp(prop, "game")) SET(i->game, value);
            if (!strcmp(prop, "server")) SET(i->server, value);
            if (!strcmp(prop, "password")) SET(i->password, value);
            if (!strcmp(prop, "icon")) SET(i->icon, value);
            if (!strcmp(prop, "installed")) i->installed = !strcmp(value, "true");
        }
        else if (sscanf(line, "game.%d.%99s", &n, prop) == 2 && n >= 0 && n < PM_MAX_GAMES)
        {
            PmGame *g = &view.games[n];
            if (!strcmp(prop, "id")) SET(g->id, value);
            if (!strcmp(prop, "name")) SET(g->name, value);
            if (!strcmp(prop, "detail")) SET(g->detail, value);
            if (!strcmp(prop, "build")) g->build = atoi(value);
            if (!strcmp(prop, "java")) g->java = atoi(value);
            if (!strcmp(prop, "valid")) g->valid = !strcmp(value, "true");
        }
        else if (sscanf(line, "source.%d.%99s", &n, prop) == 2 && n >= 0 && n < PM_MAX_SOURCES)
        {
            if (!strcmp(prop, "name")) SET(view.sources[n].name, value);
            if (!strcmp(prop, "path")) SET(view.sources[n].source, value);
        }
        else if (sscanf(line, "mod.%d.%99s", &n, prop) == 2 && n >= 0 && n < PM_MAX_MODS)
        {
            PmMod *m = &view.mods[n];
            if (!strcmp(prop, "name")) SET(m->name, value);
            if (!strcmp(prop, "title")) SET(m->title, value);
            if (!strcmp(prop, "detail")) SET(m->detail, value);
            if (!strcmp(prop, "icon")) SET(m->icon, value);
            if (!strcmp(prop, "enabled")) m->enabled = !strcmp(value, "true");
        }
        else if (sscanf(line, "java.%d.%99s", &n, prop) == 2 && n >= 0 && n < PM_MAX_JAVA)
        {
            PmJavaRuntime *runtime = &view.java_runtimes[n];
            if (!strcmp(prop, "major")) runtime->major = atoi(value);
            if (!strcmp(prop, "valid")) runtime->valid = !strcmp(value, "true") || !strcmp(value, "1");
            if (!strcmp(prop, "version")) SET(runtime->version, value);
            if (!strcmp(prop, "path")) SET(runtime->path, value);
            if (!strcmp(prop, "detail")) SET(runtime->detail, value);
        }
    }
    fclose(fp);
}
static int option(int argc, char **argv, const char *name)
{
    return argc > 3 && !strcmp(argv[3], name);
}
int main(int argc, char **argv)
{
    PmFrontend f;
    PwPresentTarget t;
    FILE *fp;
    int x, y;
    if (argc < 3)
    {
        fprintf(stderr, "Usage: render output.ppm screen-number [launcher-view.properties|--empty|--OPTION]\n");
        return 2;
    }
    char cwd[450];
    if (!getcwd(cwd, sizeof(cwd)))
        return 2;
    snprintf(icon_root, sizeof(icon_root), "%s/artifacts/frontend/icons", cwd);
    pm_icons_init(icon_root);
    pm_frontend_set_icon_loader(pm_icon_pixels);
    fixture();
    if (argc > 3)
    {
        if (!strcmp(argv[3], "--empty"))
            memset(&view, 0, sizeof(view));
        else if (strncmp(argv[3], "--", 2))
            load(argv[3]);
    }
    pm_frontend_init(&f);
    f.screen = (PmScreen)atoi(argv[2]);
    if (argc > 3 && !strncmp(argv[3], "--palette=", 10))
        SET(view.theme_color, argv[3] + 10);
    if (option(argc, argv, "--rename"))
    {
        f.rename_editor = f.rename_replace = 1;
        strcpy(f.rename_text, "Riverside Run");
        f.rename_key = 43;
    }
    if (option(argc, argv, "--saves"))
    {
        f.content_kind = 1;
        SET(view.content_kind, "saves");
        view.mod_count = 3;
        const char *worlds[][3] = {{"Sandbox/Riverside", "Riverside", "Sandbox  /  last played 2026-10-30 21:14"},
                                   {"Survival/Day 182", "Day 182", "Survival  /  last played 2026-10-12 23:51"},
                                   {"Apocalypse/First Week", "First Week", "Apocalypse  /  last played 2026-09-02 18:03"}};
        for (int j = 0; j < 3; j++)
        {
            SET(view.mods[j].name, worlds[j][0]);
            SET(view.mods[j].title, worlds[j][1]);
            SET(view.mods[j].detail, worlds[j][2]);
            view.mods[j].icon[0] = 0;
        }
    }
    if (option(argc, argv, "--import"))
    {
        f.pack_picker = 1;
        view.source_count = 2;
        SET(view.sources[0].name, "ProjectZomboid");
        SET(view.sources[0].source, "Z:\\data\\ProsperoPZ\\imports\\ProjectZomboid");
        SET(view.sources[1].name, "ProjectZomboid");
        SET(view.sources[1].source, "Z:\\mnt\\usb0\\ProjectZomboid");
    }
    if (option(argc, argv, "--input"))
    {
        f.selector_popup = 4;
        f.selector_choice = 0;
    }
    if (option(argc, argv, "--memory"))
    {
        f.selector_popup = 1;
        f.selector_choice = 6;
    }
    if (option(argc, argv, "--theme"))
    {
        strcpy(view.theme_color, "louisville");
        f.selector_popup = 5;
        f.selector_choice = 3;
    }
    if (option(argc, argv, "--close"))
        f.confirmation = 2;
    if (option(argc, argv, "--resolution"))
    {
        f.selector_popup = 2;
        f.selector_choice = 1;
    }
    for (int frame = 0; frame < 10; frame++)
        pm_frontend_tick(&f, 100);
    if (option(argc, argv, "--motion"))
    {
        f.transition_ms = argc > 4 ? atoi(argv[4]) : 140;
        f.transition_direction = 1;
    }
    if (option(argc, argv, "--server"))
        f.focus = 1;
    t.width = 1920;
    t.height = 1080;
    t.stride = t.width * 4;
    t.bytes = (uint64_t)t.stride * t.height;
    t.pixels = (unsigned char *)calloc(1, (size_t)t.bytes);
    if (!t.pixels)
        return 2;
    for (int frame = 0; frame < 16; frame++)
    {
        pm_frontend_render(&f, &view, &t);
        pm_icons_tick();
    }
    fp = fopen(argv[1], "wb");
    if (!fp)
    {
        perror(argv[1]);
        return 2;
    }
    fprintf(fp, "P6\n%d %d\n255\n", t.width, t.height);
    for (y = 0; y < (int)t.height; y++)
        for (x = 0; x < (int)t.width; x++)
        {
            const unsigned char *p = t.pixels + y * t.stride + x * 4;
            fputc(p[2], fp);
            fputc(p[1], fp);
            fputc(p[0], fp);
        }
    fclose(fp);
    free(t.pixels);
    return 0;
}
