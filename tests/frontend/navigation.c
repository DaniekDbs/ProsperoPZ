/* Controller, pointer and framebuffer checks for the ProsperoPZ launcher UI. */
#include "pm_frontend.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static PmView v;
static PmFrontend f;
static PmAction a;
static uint8_t image_pixels[16] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
static int key(PmKey k)
{
    return pm_frontend_update(&f, &v, k, &a);
}
static int pointer(int x, int y, int b, int wheel)
{
    return pm_frontend_pointer(&f, &v, x, y, b, wheel, &a);
}
static void fixture(void)
{
    memset(&v, 0, sizeof(v));
    pm_frontend_init(&f);
    v.instance_count = 16;
    v.memory_mb = 6144;
    v.resolution_height = 1440;
    for (int j = 0; j < 16; j++)
    {
        snprintf(v.instances[j].id, PM_TEXT, "profile-%d", j);
        snprintf(v.instances[j].name, PM_TEXT, "Profile %d", j);
        strcpy(v.instances[j].version, "Project Zomboid Build 41");
        strcpy(v.instances[j].loader, j == 5 || j == 9 ? "server" : "client");
        strcpy(v.instances[j].game, "build41");
        v.instances[j].installed = 1;
    }
    strcpy(v.instances[5].server, "profile-5");
    strcpy(v.instances[5].password, "pw5");
    v.game_count = 2;
    strcpy(v.games[0].id, "build41");
    v.games[0].build = 41;
    v.games[0].valid = 1;
    strcpy(v.games[1].id, "build42");
    v.games[1].build = 42;
    v.games[1].valid = 0;
}
static void content_fixture(const char *id, const char *kind)
{
    strcpy(v.content_instance, id);
    strcpy(v.content_kind, kind);
    v.mod_count = 2;
    strcpy(v.mods[0].name, !strcmp(kind, "mods") ? "tsarslib" : "Sandbox/Riverside");
    v.mods[0].enabled = 1;
    strcpy(v.mods[1].name, !strcmp(kind, "mods") ? "BedfordFalls" : "Survival/Day 9");
    v.mods[1].enabled = 0;
}
static void home_and_profiles(void)
{
    fixture();
    assert(key(PM_KEY_CONFIRM));
    assert(!strcmp(a.command, "prepare-launch") && !strcmp(a.id, "profile-0"));
    assert(!key(PM_KEY_DOWN) && f.selected_instance == 4);
    assert(!key(PM_KEY_RIGHT) && f.selected_instance == 5);
    assert(key(PM_KEY_CONFIRM) && !strcmp(a.command, "prepare-launch") && !strcmp(a.id, "profile-5"));
    v.instances[5].installed = 0;
    assert(!key(PM_KEY_CONFIRM) && !a.command[0] && strstr(f.notice, "missing"));
    v.instances[5].installed = 1;
    v.busy = 1;
    assert(!key(PM_KEY_CONFIRM) && !a.command[0]);
    v.busy = 0;
    /* Square opens Games to create a profile. */
    assert(!key(PM_KEY_SECONDARY) && f.screen == PM_GAMES);
    /* Tabs wrap in both directions and Mods lists the selected profile. */
    f.screen = PM_HOME;
    assert(!key(PM_KEY_PREVIOUS_TAB) && f.screen == PM_SERVER);
    assert(!key(PM_KEY_NEXT_TAB) && f.screen == PM_HOME);
    f.screen = PM_SETTINGS;
    f.selected_instance = 0;
    assert(key(PM_KEY_PREVIOUS_TAB) && f.screen == PM_MODS);
    f.screen = PM_LIBRARY;
    assert(key(PM_KEY_NEXT_TAB) == 0 && f.screen == PM_GAMES);
    assert(key(PM_KEY_NEXT_TAB) && f.screen == PM_MODS && !strcmp(a.command, "content-list") &&
           !strcmp(a.content_kind, "mods") && !strcmp(a.id, "profile-0"));
    /* Profiles: list on the left, actions on the right. */
    fixture();
    f.screen = PM_LIBRARY;
    assert(!key(PM_KEY_DOWN) && f.selected_instance == 1);
    assert(!key(PM_KEY_RIGHT) && f.focus == 1);
    assert(key(PM_KEY_CONFIRM) && !strcmp(a.command, "prepare-launch") && !strcmp(a.id, "profile-1"));
    assert(!key(PM_KEY_DOWN) && f.focus == 2);
    assert(key(PM_KEY_CONFIRM) && f.screen == PM_MODS && !strcmp(a.command, "content-list"));
    f.screen = PM_LIBRARY;
    f.focus = 4;
    assert(key(PM_KEY_CONFIRM) && !strcmp(a.command, "clone") && !strcmp(a.id, "profile-1"));
    f.focus = 5;
    assert(!key(PM_KEY_CONFIRM) && f.confirmation == 1);
    assert(!key(PM_KEY_CONFIRM) && !f.confirmation && !a.command[0]);
    assert(!key(PM_KEY_CONFIRM) && f.confirmation == 1);
    assert(!key(PM_KEY_RIGHT));
    assert(key(PM_KEY_CONFIRM) && !strcmp(a.command, "delete-instance") && !strcmp(a.id, "profile-1"));
    /* A changed selection never deletes a different profile. */
    assert(!key(PM_KEY_CONFIRM) && f.confirmation == 1);
    f.selected_instance = 2;
    assert(!key(PM_KEY_RIGHT) && !key(PM_KEY_CONFIRM) && !a.command[0]);
    /* Pointer: a row selects, the right panel's first button plays. */
    fixture();
    f.screen = PM_LIBRARY;
    assert(!pointer(300, 282 + 83 * 2 + 20, 1, 0) && f.selected_instance == 2);
    assert(pointer(1300, 540, 1, 0) && !strcmp(a.command, "prepare-launch") && !strcmp(a.id, "profile-2"));
    f.screen = PM_HOME;
    assert(!pointer(120 + 432, 300, 1, 0) && f.selected_instance == 1);
    assert(pointer(120 + 432, 300, 1, 0) && !strcmp(a.command, "prepare-launch"));
    assert(!pointer(1600, 200, 1, 0) && f.screen == PM_GAMES);
}
static void games(void)
{
    fixture();
    f.screen = PM_GAMES;
    assert(!key(PM_KEY_RIGHT) && f.focus == 1);
    assert(key(PM_KEY_CONFIRM) && !strcmp(a.command, "create") && !strcmp(a.game, "build41") &&
           !strcmp(a.mode, "client") && strstr(a.name, "Build 41"));
    assert(!key(PM_KEY_DOWN) && f.focus == 2);
    assert(key(PM_KEY_CONFIRM) && !strcmp(a.mode, "server"));
    /* Incomplete installations cannot get profiles. */
    f.focus = 0;
    assert(!key(PM_KEY_DOWN) && f.selected_game == 1);
    f.focus = 1;
    assert(!key(PM_KEY_CONFIRM) && !a.command[0] && strstr(f.notice, "incomplete"));
    /* Import opens a list of game folders reported by the backend. */
    f.focus = 3;
    assert(key(PM_KEY_CONFIRM) && f.pack_picker && !strcmp(a.command, "catalog"));
    assert(!key(PM_KEY_CONFIRM) && !a.command[0] && f.notice[0]);
    v.source_count = 2;
    strcpy(v.sources[1].name, "ProjectZomboid");
    strcpy(v.sources[1].source, "Z:\\mnt\\usb0\\ProjectZomboid");
    assert(!key(PM_KEY_DOWN) && f.selected_pack == 1);
    assert(key(PM_KEY_CONFIRM) && !strcmp(a.command, "import-game") && !strcmp(a.source, "Z:\\mnt\\usb0\\ProjectZomboid"));
    assert(!f.pack_picker);
    f.focus = 4;
    assert(key(PM_KEY_CONFIRM) && !strcmp(a.command, "catalog"));
    /* Deleting game files names the selected installation. */
    f.focus = 5;
    f.selected_game = 0;
    assert(!key(PM_KEY_CONFIRM) && f.confirmation == 4);
    assert(!key(PM_KEY_RIGHT) && key(PM_KEY_CONFIRM) && !strcmp(a.command, "delete-game") && !strcmp(a.game, "build41"));
    /* With no game, only import and rescan exist. */
    fixture();
    v.game_count = 0;
    f.screen = PM_GAMES;
    assert(!key(PM_KEY_RIGHT) && f.focus == 3);
    assert(!key(PM_KEY_UP) && f.focus == 3);
    assert(!key(PM_KEY_DOWN) && f.focus == 4);
    assert(!key(PM_KEY_DOWN) && f.focus == 4);
    f.focus = 0;
    assert(key(PM_KEY_CONFIRM) && f.pack_picker);
    assert(!key(PM_KEY_BACK) && !f.pack_picker);
    assert(key(PM_KEY_SECONDARY) && f.pack_picker);
}
static void mods_and_saves(void)
{
    fixture();
    f.screen = PM_MODS;
    content_fixture("profile-0", "mods");
    f.focus = 2;
    assert(key(PM_KEY_CONFIRM) && !strcmp(a.command, "content-toggle") && !strcmp(a.mod, "tsarslib") &&
           !strcmp(a.enabled, "false") && !strcmp(a.content_kind, "mods") && !strcmp(a.id, "profile-0"));
    assert(!key(PM_KEY_DOWN) && f.selected_mod == 1);
    assert(key(PM_KEY_CONFIRM) && !strcmp(a.mod, "BedfordFalls") && !strcmp(a.enabled, "true"));
    assert(!key(PM_KEY_RIGHT) && f.focus == 3);
    assert(key(PM_KEY_CONFIRM) && f.pack_picker && !strcmp(a.command, "scan-content-imports"));
    v.content_import_count = 1;
    strcpy(v.content_imports[0].name, "2392709985");
    strcpy(v.content_imports[0].source, "Z:\\data\\ProsperoPZ\\imports\\mods\\2392709985");
    assert(key(PM_KEY_CONFIRM) && !strcmp(a.command, "content-import") && strstr(a.source, "2392709985") &&
           !strcmp(a.content_kind, "mods"));
    f.focus = 4;
    assert(!key(PM_KEY_CONFIRM) && f.confirmation == 3 && !strcmp(f.confirmation_file, "BedfordFalls"));
    assert(!key(PM_KEY_RIGHT) && key(PM_KEY_CONFIRM) && !strcmp(a.command, "content-delete") &&
           !strcmp(a.mod, "BedfordFalls"));
    /* Switching to saves reloads; selecting a world backs it up. */
    f.focus = 1;
    assert(key(PM_KEY_RIGHT) && f.content_kind == 1 && !strcmp(a.content_kind, "saves") && !strcmp(a.command, "content-list"));
    content_fixture("profile-0", "saves");
    f.focus = 2;
    f.selected_mod = 0;
    assert(key(PM_KEY_CONFIRM) && !strcmp(a.command, "content-backup") && !strcmp(a.mod, "Sandbox/Riverside"));
    /* Stale lists for another profile are reloaded before acting. */
    f.focus = 0;
    assert(key(PM_KEY_RIGHT) && f.selected_instance == 1 && !strcmp(a.id, "profile-1"));
    f.focus = 2;
    assert(key(PM_KEY_CONFIRM) && !strcmp(a.command, "content-list"));
    /* Pointer: kind tabs, rows and buttons. */
    fixture();
    f.screen = PM_MODS;
    content_fixture("profile-0", "mods");
    assert(pointer(150, 310, 1, 0) && f.content_kind == 0 && !strcmp(a.command, "content-list"));
    assert(pointer(300, 365 + 81 + 20, 1, 0) && !strcmp(a.mod, "BedfordFalls"));
    assert(pointer(1400, 380, 1, 0) && f.pack_picker);
}
static void settings_and_close(void)
{
    fixture();
    f.screen = PM_SETTINGS;
    assert(!key(PM_KEY_CONFIRM) && f.selector_popup == 1);
    assert(!key(PM_KEY_DOWN) && !key(PM_KEY_CONFIRM) && f.memory_mb == 6656);
    f.focus = 8;
    assert(!key(PM_KEY_CONFIRM) && f.selector_popup == 3);
    assert(!key(PM_KEY_DOWN) && !key(PM_KEY_CONFIRM) && f.debug_index == 1);
    f.focus = 9;
    assert(!key(PM_KEY_CONFIRM) && f.selector_popup == 4 && !f.selector_choice);
    assert(!key(PM_KEY_DOWN) && !key(PM_KEY_CONFIRM) && f.input_index == 1);
    f.focus = 6;
    assert(key(PM_KEY_CONFIRM) && !strcmp(a.command, "settings") && a.memory_mb == 6656 &&
           a.resolution_height == 1440 && !strcmp(a.debug_mode, "on") && !strcmp(a.controller_mode, "emulated") &&
           !strcmp(a.theme_color, "knox") && !strcmp(a.vrr_mode, "auto"));
    f.focus = 10;
    assert(!key(PM_KEY_CONFIRM) && f.selector_popup == 5);
    assert(!key(PM_KEY_DOWN) && key(PM_KEY_CONFIRM) && !strcmp(a.theme_color, "blood"));
    f.focus = 2;
    assert(key(PM_KEY_CONFIRM) && !strcmp(a.command, "install-java") && a.java_major == 17);
    f.focus = 5;
    assert(key(PM_KEY_CONFIRM) && !strcmp(a.command, "detect-java"));
    assert(key(PM_KEY_REFRESH) && !strcmp(a.command, "detect-java"));
    /* Memory never leaves the supported range. */
    f.focus = 0;
    assert(!key(PM_KEY_CONFIRM));
    for (int j = 0; j < 40; j++)
        key(PM_KEY_DOWN);
    assert(!key(PM_KEY_CONFIRM) && f.memory_mb == 8192);
    /* Settings read from the view initialize the selectors once. */
    fixture();
    strcpy(v.controller_mode, "emulated");
    strcpy(v.debug_mode, "on");
    strcpy(v.theme_color, "westpoint");
    f.screen = PM_SETTINGS;
    f.focus = 6;
    assert(key(PM_KEY_CONFIRM) && a.memory_mb == 6144 && !strcmp(a.controller_mode, "emulated") &&
           !strcmp(a.debug_mode, "on") && !strcmp(a.theme_color, "westpoint"));
    /* VRR saves immediately; pointer reaches the save button. */
    f.focus = 11;
    assert(key(PM_KEY_CONFIRM) && !strcmp(a.vrr_mode, "stable"));
    assert(pointer(1300, 860, 1, 0) && !strcmp(a.command, "settings"));
    /* Close asks first. */
    fixture();
    assert(!pointer(1750, 70, 1, 0) && f.confirmation == 2);
    assert(pointer(1100, 650, 1, 0) && !strcmp(a.command, "quit"));
    f.header_focus = PM_SCREEN_COUNT;
    assert(!key(PM_KEY_CONFIRM) && f.confirmation == 2);
    assert(!key(PM_KEY_BACK) && !f.confirmation);
}
static void server(void)
{
    fixture();
    f.screen = PM_SERVER;
    assert(!key(PM_KEY_RIGHT) && f.focus == 1);
    assert(key(PM_KEY_CONFIRM) && !strcmp(a.command, "prepare-launch") && !strcmp(a.id, "profile-5"));
    assert(!key(PM_KEY_DOWN) && f.focus == 2);
    assert(key(PM_KEY_CONFIRM) && !strcmp(a.command, "server-password") && !strcmp(a.id, "profile-5"));
    f.focus = 0;
    assert(!key(PM_KEY_DOWN) && f.selected_server == 1);
    assert(!key(PM_KEY_DOWN) && f.selected_server == 1);
    f.focus = 4;
    assert(key(PM_KEY_CONFIRM) && f.screen == PM_MODS && f.selected_instance == 9 && !strcmp(a.id, "profile-9"));
    f.screen = PM_SERVER;
    f.focus = 3;
    assert(key(PM_KEY_CONFIRM) && !strcmp(a.command, "create") && !strcmp(a.mode, "server"));
    assert(key(PM_KEY_SECONDARY) && !strcmp(a.mode, "server"));
    /* No server profiles: Cross creates one. */
    fixture();
    strcpy(v.instances[5].loader, "client");
    strcpy(v.instances[9].loader, "client");
    f.screen = PM_SERVER;
    assert(!key(PM_KEY_RIGHT) && f.focus == 3);
    assert(key(PM_KEY_CONFIRM) && !strcmp(a.command, "create") && !strcmp(a.mode, "server"));
    v.game_count = 0;
    assert(!key(PM_KEY_CONFIRM) && strstr(f.notice, "Import"));
}
static void renaming(void)
{
    fixture();
    f.screen = PM_LIBRARY;
    f.focus = 3;
    assert(!key(PM_KEY_CONFIRM) && f.rename_editor && f.rename_replace);
    assert(!key(PM_KEY_CONFIRM) && !strcmp(f.rename_text, "A"));
    f.rename_key = 42;
    assert(!key(PM_KEY_CONFIRM) && f.rename_lower);
    f.rename_key = 1;
    assert(!key(PM_KEY_CONFIRM) && !strcmp(f.rename_text, "Ab"));
    pm_frontend_text(&f, '.');
    pm_frontend_text(&f, '2');
    assert(!key(PM_KEY_SECONDARY) && !strcmp(f.rename_text, "Ab."));
    assert(key(PM_KEY_REFRESH) && !f.rename_editor && !strcmp(a.command, "rename-instance") &&
           !strcmp(a.id, "profile-0") && !strcmp(a.name, "Ab."));
    assert(!key(PM_KEY_CONFIRM) && f.rename_editor);
    f.rename_key = 41;
    assert(!key(PM_KEY_CONFIRM) && !f.rename_text[0]);
    assert(!key(PM_KEY_REFRESH) && f.rename_editor && !a.command[0] && f.notice[0]);
    pm_frontend_text(&f, 'X');
    v.busy = 1;
    assert(!key(PM_KEY_REFRESH) && f.rename_editor && !a.command[0]);
    v.busy = 0;
    for (int j = 0; j < 150; j++)
        pm_frontend_text(&f, 'D');
    assert(strlen(f.rename_text) == 96);
    f.rename_key = 44;
    assert(!key(PM_KEY_CONFIRM) && !f.rename_editor && !a.command[0]);
}
static const uint8_t *test_icon(const char *path, unsigned *w, unsigned *h)
{
    if (strcmp(path, "test-icon"))
        return NULL;
    *w = *h = 2;
    return image_pixels;
}
static void render_bounds(void)
{
    fixture();
    strcpy(v.instances[0].icon, "test-icon");
    pm_frontend_set_icon_loader(test_icon);
    unsigned char *bytes = malloc(1920 * 1080 * 4 + 32);
    assert(bytes);
    memset(bytes, 0x5a, 1920 * 1080 * 4 + 32);
    PwPresentTarget t = {0};
    t.pixels = bytes + 16;
    t.width = 1920;
    t.height = 1080;
    t.stride = 1920 * 4;
    t.bytes = 1920 * 1080 * 4;
    pm_frontend_render(&f, &v, &t);
    unsigned char *p = t.pixels + 350 * t.stride + 160 * 4;
    assert(p[0] == 0 && p[1] == 0 && p[2] == 255);
    v.instance_count = PM_MAX_INSTANCES + 99;
    v.game_count = PM_MAX_GAMES + 99;
    v.mod_count = PM_MAX_MODS + 99;
    v.source_count = PM_MAX_SOURCES + 99;
    v.java_count = PM_MAX_JAVA + 99;
    for (int screen = 0; screen < PM_SCREEN_COUNT; screen++)
        for (int picker = 0; picker < 2; picker++)
        {
            pm_frontend_init(&f);
            f.screen = (PmScreen)screen;
            f.pack_picker = picker;
            f.selected_instance = f.selected_game = f.selected_mod = f.selected_server = 99999;
            f.selected_pack = 99999;
            f.focus = 9999;
            for (int j = 0; j < 40; j++)
                key((PmKey)(PM_KEY_UP + j % 4));
            assert(f.selected_instance < PM_MAX_INSTANCES && f.selected_game < PM_MAX_GAMES &&
                   f.selected_mod < PM_MAX_MODS);
            pm_frontend_render(&f, &v, &t);
        }
    for (int j = 0; j < 16; j++)
    {
        assert(bytes[j] == 0x5a);
        assert(bytes[1920 * 1080 * 4 + 16 + j] == 0x5a);
    }
    /* Smaller and odd framebuffers scale without writing outside. */
    t.width = 1279;
    t.height = 719;
    t.stride = 1279 * 4;
    t.bytes = (uint64_t)t.stride * t.height;
    pm_frontend_render(&f, &v, &t);
    memset(bytes + 16, 0x5a, 1920 * 1080 * 4);
    t.bytes = 1;
    pm_frontend_render(&f, &v, &t);
    assert(bytes[16] == 0x5a);
    t.bytes = 1920 * 1080 * 4;
    t.stride = 1;
    pm_frontend_render(&f, &v, &t);
    assert(bytes[16] == 0x5a);
    free(bytes);
    pm_frontend_set_icon_loader(NULL);
}
static void full_palettes_and_transparent_icons(void)
{
    fixture();
    const char *names[] = {"knox", "blood", "rosewood", "louisville", "muldraugh", "westpoint"};
    uint8_t *pixels = calloc(1, 1920 * 1080 * 4);
    assert(pixels);
    PwPresentTarget target = {pixels, 1920, 1080, 1920 * 4, 1920 * 1080 * 4};
    uint32_t backgrounds[6], surfaces[6];
    pm_frontend_set_icon_loader(test_icon);
    image_pixels[15] = 0;
    strcpy(v.instances[0].icon, "test-icon");
    for (int j = 0; j < 6; j++)
    {
        pm_frontend_init(&f);
        strcpy(v.theme_color, names[j]);
        f.header_focus = PM_HOME;
        pm_frontend_render(&f, &v, &target);
        memcpy(&backgrounds[j], pixels + (180 * 1920 + 20) * 4, 4);
        memcpy(&surfaces[j], pixels + (325 * 1920 + 218) * 4, 4);
        /* The transparent icon corner shows the card surface, not an opaque plate. */
        assert(!memcmp(pixels + (377 * 1920 + 193) * 4, pixels + (377 * 1920 + 218) * 4, 4));
        for (int k = 0; k < j; k++)
        {
            assert(backgrounds[j] != backgrounds[k]);
            assert(surfaces[j] != surfaces[k]);
        }
    }
    image_pixels[15] = 255;
    free(pixels);
    pm_frontend_set_icon_loader(NULL);
    puts("FULL_PALETTE_AND_ICON_ALPHA_PASS six backgrounds and surfaces");
}
static void motion(void)
{
    fixture();
    assert(!key(PM_KEY_NEXT_TAB) && f.transition_ms == 220 && f.transition_direction == 1);
    for (int j = 0; j < 15; j++)
        pm_frontend_tick(&f, 16);
    assert(!f.transition_ms);
}
int main(void)
{
    home_and_profiles();
    games();
    mods_and_saves();
    settings_and_close();
    server();
    renaming();
    render_bounds();
    full_palettes_and_transparent_icons();
    motion();
    puts("Launcher controller, pointer, games, mods, saves, server, close and framebuffer checks passed.");
    return 0;
}
