/* SPDX-License-Identifier: GPL-3.0-or-later */
/* ProsperoPZ controller-first launcher UI for Project Zomboid. */
#include "pm_frontend.h"
#include <stdio.h>
#include <string.h>
#include "pm_font.h"
#include "pm_art.h"
#define TITLE 0xf3f0e6
#define TEXT 0xdedbd2
#define WARN 0xf2a58f
static int bound(int n, int lo, int hi)
{
    return n < lo ? lo : n > hi ? hi : n;
}
static int count(int n, int cap)
{
    return bound(n, 0, cap);
}
static int last(int n)
{
    return n > 0 ? n - 1 : 0;
}
static void copy(char *to, size_t n, const char *from)
{
    size_t k = 0;
    if (!n)
        return;
    for (; from && from[k] && k + 1 < n; k++)
        to[k] = from[k];
    to[k] = 0;
}
static const char *tabs[] = {"Home", "Profiles", "Games", "Mods & Saves", "Settings", "Server"};
static const char *kinds[] = {"mods", "saves"};
static const char *kind_labels[] = {"Mods", "Saves"};
static const int java_majors[] = {8, 17, 21, 25};
static const int resolution_heights[] = {720, 1080, 1440, 2160};
static const char *resolution_labels[] = {"1280 x 720", "1920 x 1080", "2560 x 1440",
                                          "3840 x 2160"};
static const char *debug_values[] = {"off", "on"};
static const char *debug_labels[] = {"Normal", "Debug mode (-debug)"};
static const char *input_values[] = {"native", "emulated"};
static const char *input_labels[] = {"Native gamepad (recommended)", "Emulated mouse / keyboard"};
static const char *theme_values[] = {"knox", "blood", "rosewood", "louisville", "muldraugh", "westpoint"};
static const char *theme_labels[] = {"Knox Green", "Blood Red", "Rosewood", "Louisville Night",
                                     "Muldraugh Amber", "West Point Teal"};
static const uint32_t theme_colors[] = {0x9fd36b, 0xe0574f, 0xe39a5b, 0x7da7ff, 0xf0c35a, 0x5fd0c0};
static const uint32_t theme_backgrounds[] = {0x0b0e0a, 0x140909, 0x140d08, 0x090d18, 0x15110a, 0x07120f};
#define MEMORY_MIN 1024
#define MEMORY_MAX 8192
#define MEMORY_STEP 512
#define MEMORY_CHOICES ((MEMORY_MAX - MEMORY_MIN) / MEMORY_STEP)
static PmIconLoader icon_loader;
void pm_frontend_set_icon_loader(PmIconLoader loader)
{
    icon_loader = loader;
}
static int text_width(const char *s, int size);
static const PmInstance *selected_instance(const PmFrontend *f, const PmView *v)
{
    int n = count(v->instance_count, PM_MAX_INSTANCES);
    return n ? &v->instances[bound(f->selected_instance, 0, n - 1)] : NULL;
}
static const PmGame *selected_game(const PmFrontend *f, const PmView *v)
{
    int n = count(v->game_count, PM_MAX_GAMES);
    return n ? &v->games[bound(f->selected_game, 0, n - 1)] : NULL;
}
static int is_server(const PmInstance *i)
{
    return i && !strcmp(i->loader, "server");
}
static int server_count(const PmView *v)
{
    int n = 0;
    for (int j = 0; j < count(v->instance_count, PM_MAX_INSTANCES); j++)
        n += is_server(&v->instances[j]);
    return n;
}
/* Index into v->instances of the k-th server profile, or -1. */
static int server_index(const PmView *v, int k)
{
    for (int j = 0; j < count(v->instance_count, PM_MAX_INSTANCES); j++)
        if (is_server(&v->instances[j]) && !k--)
            return j;
    return -1;
}
static const PmInstance *selected_server(const PmFrontend *f, const PmView *v)
{
    int j = server_index(v, f->selected_server);
    return j >= 0 ? &v->instances[j] : NULL;
}
static int list_start(int selection, int rows)
{
    return selection / rows * rows;
}
static int content_matches(const PmFrontend *f, const PmView *v)
{
    const PmInstance *i = selected_instance(f, v);
    return i && !strcmp(i->id, v->content_instance) &&
           !strcmp(kinds[f->content_kind], v->content_kind);
}
void pm_frontend_init(PmFrontend *f)
{
    if (!f)
        return;
    memset(f, 0, sizeof(*f));
    f->memory_mb = 4096;
    f->header_focus = -1;
    f->underline_x = 478;
    f->pointer_x = 960;
    f->pointer_y = 540;
    f->resolution_index = 1;
}
void pm_frontend_tick(PmFrontend *f, unsigned ms)
{
    if (!f)
        return;
    ms = ms > 100 ? 100 : ms;
    f->animation_ms += ms;
    f->focus_ms = bound((int)f->focus_ms + (int)ms, 0, 180);
    f->transition_ms = f->transition_ms > ms ? f->transition_ms - ms : 0;
    int x = 470;
    for (int j = 0; j < (int)f->screen; j++)
        x += text_width(tabs[j], 24) + 43;
    int distance = x - f->underline_x;
    int step = distance * (int)ms / 80;
    if (!step && distance && ms) step = distance > 0 ? 1 : -1;
    f->underline_x += distance > 0 ? bound(step, 0, distance) : bound(step, distance, 0);
}
static void normalize(PmFrontend *f, const PmView *v)
{
    f->screen = (PmScreen)bound(f->screen, 0, PM_SCREEN_COUNT - 1);
    f->selected_instance = bound(f->selected_instance, 0, last(count(v->instance_count, PM_MAX_INSTANCES)));
    f->selected_game = bound(f->selected_game, 0, last(count(v->game_count, PM_MAX_GAMES)));
    f->selected_mod = bound(f->selected_mod, 0, last(count(v->mod_count, PM_MAX_MODS)));
    f->selected_java = bound(f->selected_java, 0, last(count(v->java_count, PM_MAX_JAVA)));
    f->selected_server = bound(f->selected_server, 0, last(server_count(v)));
    f->content_kind = bound(f->content_kind, 0, 1);
    f->header_focus = bound(f->header_focus, -1, PM_SCREEN_COUNT);
    f->focus = bound(f->focus, 0,
                     f->screen == PM_SETTINGS  ? 11
                     : f->screen == PM_MODS    ? 4
                     : f->screen == PM_LIBRARY ? 5
                     : f->screen == PM_GAMES   ? 5
                     : f->screen == PM_SERVER  ? 4
                                               : 1);
    f->memory_mb = bound(f->memory_mb, MEMORY_MIN, MEMORY_MAX);
    f->resolution_index = bound(f->resolution_index, 0, 3);
    f->debug_index = bound(f->debug_index, 0, 1);
    f->input_index = bound(f->input_index, 0, 1);
    f->theme_index = bound(f->theme_index, 0, 5);
    f->vrr_index = bound(f->vrr_index, 0, 1);
    if (!f->initialized_settings)
    {
        f->memory_mb = v->memory_mb ? bound(v->memory_mb, MEMORY_MIN, MEMORY_MAX) : 4096;
        f->debug_index = !strcmp(v->debug_mode, "on");
        f->input_index = !strcmp(v->controller_mode, "emulated");
        f->vrr_index = !strcmp(v->vrr_mode, "stable");
        for (int j = 0; j < 6; j++)
            if (!strcmp(v->theme_color, theme_values[j])) f->theme_index = j;
        for (int j = 0; j < 4; j++)
            if (v->resolution_height == resolution_heights[j])
                f->resolution_index = j;
        f->initialized_settings = 1;
    }
}
static int action(PmAction *a, const char *command, const PmInstance *i)
{
    copy(a->command, sizeof(a->command), command);
    if (i)
        copy(a->id, sizeof(a->id), i->id);
    return 1;
}
static int unavailable(PmFrontend *f, const char *reason)
{
    copy(f->notice, sizeof(f->notice), reason);
    return 0;
}
static int content_action(PmFrontend *f, PmAction *a, const char *command, const PmInstance *i)
{
    copy(a->content_kind, sizeof(a->content_kind), kinds[f->content_kind]);
    return action(a, command, i);
}
static int switch_screen(PmFrontend *f, const PmView *v, PmScreen screen, PmAction *a)
{
    f->transition_direction = screen < f->screen ? -1 : 1;
    f->transition_ms = 220;
    f->screen = screen;
    f->focus = 0;
    f->header_focus = -1;
    f->pack_picker = 0;
    f->notice[0] = 0;
    if (screen == PM_MODS && selected_instance(f, v) && !v->busy)
        return content_action(f, a, "content-list", selected_instance(f, v));
    return 0;
}
static const char rename_keys[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -._";
void pm_frontend_text(PmFrontend *f, unsigned key)
{
    if (!f || !f->rename_editor)
        return;
    char *buffer = f->rename_text;
    size_t capacity = sizeof(f->rename_text);
    if (f->rename_replace && (key == 8 || (key >= 32 && key <= 126)))
    {
        buffer[0] = 0;
        f->rename_replace = 0;
    }
    size_t n = strlen(buffer);
    if (key == 8)
    {
        if (n)
            buffer[n - 1] = 0;
    }
    else if (key >= 32 && key <= 126 && n < capacity - 1)
    {
        buffer[n] = (char)key;
        buffer[n + 1] = 0;
    }
}
static void confirm(PmFrontend *f, const PmView *v, int kind)
{
    f->confirmation = kind;
    f->confirmation_choice = 0;
    const PmInstance *i = selected_instance(f, v);
    const PmGame *g = selected_game(f, v);
    copy(f->confirmation_id, sizeof(f->confirmation_id), kind == 4 ? (g ? g->id : "") : i ? i->id : "");
    copy(f->confirmation_file, sizeof(f->confirmation_file),
         count(v->mod_count, PM_MAX_MODS) ? v->mods[f->selected_mod].name : "");
}
static int play(PmFrontend *f, const PmView *v, const PmInstance *i, PmAction *a)
{
    if (!i)
        return switch_screen(f, v, PM_GAMES, a);
    if (v->busy)
        return unavailable(f, "Wait for the current operation to finish.");
    if (!i->installed)
        return unavailable(f, "This profile's game is missing. Import Project Zomboid in Games.");
    return action(a, "prepare-launch", i);
}
static void start_rename(PmFrontend *f, const PmInstance *i)
{
    f->rename_editor = 1;
    f->rename_key = f->rename_lower = 0;
    f->rename_replace = 1;
    copy(f->rename_id, sizeof(f->rename_id), i->id);
    copy(f->rename_text, sizeof(f->rename_text), i->name[0] ? i->name : i->id);
}
static int create_profile(PmFrontend *f, const PmView *v, const char *mode, PmAction *a)
{
    const PmGame *g = selected_game(f, v);
    if (!g)
        return unavailable(f, "Import Project Zomboid first.");
    if (!g->valid)
        return unavailable(f, "This game folder is incomplete. Copy the whole Windows game folder again.");
    if (v->busy)
        return unavailable(f, "Wait for the current operation to finish.");
    copy(a->game, sizeof(a->game), g->id);
    copy(a->mode, sizeof(a->mode), mode);
    snprintf(a->name, sizeof(a->name), "%s - Build %d", strcmp(mode, "server") ? "Survivor" : "Server",
             g->build);
    return action(a, "create", NULL);
}
static int open_import(PmFrontend *f, const PmView *v, PmAction *a)
{
    if (v->busy)
        return unavailable(f, "Wait for the current operation to finish.");
    f->pack_picker = 1;
    f->selected_pack = 0;
    if (f->screen == PM_MODS)
        return content_action(f, a, "scan-content-imports", selected_instance(f, v));
    return action(a, "catalog", NULL);
}
static int update_settings_screen(PmFrontend *f, const PmView *v, PmKey key, int delta, PmAction *a)
{
    if (key == PM_KEY_SECONDARY)
    {
        if (v->java_count)
            f->selected_java = (f->selected_java + 1) % count(v->java_count, PM_MAX_JAVA);
        return 0;
    }
    if (key == PM_KEY_UP)
    {
        if (f->focus == 0 || (f->focus >= 7 && f->focus <= 9))
            f->header_focus = PM_SETTINGS;
        else if (f->focus == 10 || f->focus == 11)
            f->focus = f->focus == 10 ? 0 : 8;
        else if (f->focus >= 1 && f->focus <= 4)
            f->focus = f->focus <= 2 ? 10 : 11;
        else
            f->focus = f->focus == 5 ? 1 : 3;
    }
    if (key == PM_KEY_DOWN)
    {
        if (f->focus == 10 || f->focus == 11)
            f->focus = f->focus == 10 ? 1 : 3;
        else if (f->focus == 0 || f->focus >= 7)
            f->focus = f->focus == 0 || f->focus == 7 ? 10 : 11;
        else if (f->focus >= 1 && f->focus <= 4)
            f->focus = f->focus <= 2 ? 5 : 6;
    }
    if (key == PM_KEY_LEFT || key == PM_KEY_RIGHT)
    {
        if (f->focus == 10 || f->focus == 11)
            f->focus = bound(f->focus + delta, 10, 11);
        else if (f->focus == 0 || f->focus >= 7)
        {
            int column = f->focus == 0 ? 0 : f->focus - 6;
            column = bound(column + delta, 0, 3);
            f->focus = column == 0 ? 0 : column + 6;
        }
        else if (f->focus <= 4)
            f->focus = bound(f->focus + delta, 1, 4);
        else
            f->focus = bound(f->focus + delta, 5, 6);
    }
    if (key == PM_KEY_CONFIRM && !v->busy)
    {
        if (f->focus == 11)
        {
            f->vrr_index = !f->vrr_index;
            copy(a->vrr_mode, sizeof(a->vrr_mode), f->vrr_index ? "stable" : "auto");
            return action(a, "settings", NULL);
        }
        if (f->focus == 0 || f->focus >= 7)
        {
            f->selector_popup = f->focus == 10 ? 5 : f->focus == 0 ? 1 : f->focus == 7 ? 2 : f->focus == 8 ? 3 : 4;
            f->selector_choice = f->focus == 0    ? (f->memory_mb - MEMORY_MIN) / MEMORY_STEP
                                 : f->focus == 7  ? f->resolution_index
                                 : f->focus == 8  ? f->debug_index
                                 : f->focus == 10 ? f->theme_index
                                                  : f->input_index;
            return 0;
        }
        if (f->focus <= 4)
        {
            a->java_major = java_majors[f->focus - 1];
            return action(a, "install-java", NULL);
        }
        if (f->focus == 5)
            return action(a, "detect-java", NULL);
        a->memory_mb = f->memory_mb;
        a->resolution_height = resolution_heights[f->resolution_index];
        copy(a->debug_mode, sizeof(a->debug_mode), debug_values[f->debug_index]);
        copy(a->controller_mode, sizeof(a->controller_mode), input_values[f->input_index]);
        copy(a->theme_color, sizeof(a->theme_color), theme_values[f->theme_index]);
        copy(a->vrr_mode, sizeof(a->vrr_mode), f->vrr_index ? "stable" : "auto");
        return action(a, "settings", NULL);
    }
    return 0;
}
int pm_frontend_update(PmFrontend *f, const PmView *v, PmKey key, PmAction *a)
{
    if (!f || !v || !a)
        return 0;
    memset(a, 0, sizeof(*a));
    normalize(f, v);
    const PmInstance *i = selected_instance(f, v);
    int n = count(v->instance_count, PM_MAX_INSTANCES),
        delta = key == PM_KEY_DOWN || key == PM_KEY_RIGHT ? 1 : -1;
    if (key == PM_KEY_NONE)
        return 0;
    f->focus_ms = 0;
    if (f->rename_editor)
    {
        if (key == PM_KEY_BACK) { f->rename_editor = 0; return 0; }
        if (key == PM_KEY_LEFT || key == PM_KEY_RIGHT)
            f->rename_key = bound(f->rename_key + delta, 0, 44);
        if (key == PM_KEY_UP || key == PM_KEY_DOWN)
            f->rename_key = bound(f->rename_key + delta * 9, 0, 44);
        if (key == PM_KEY_SECONDARY) pm_frontend_text(f, 8);
        if (key == PM_KEY_REFRESH || (key == PM_KEY_CONFIRM && f->rename_key == 43))
        {
            const PmInstance *target = NULL;
            for (int j = 0; j < n; j++)
                if (!strcmp(v->instances[j].id, f->rename_id)) target = &v->instances[j];
            if (v->busy || !target)
                return unavailable(f, "Wait for the current operation before renaming.");
            if (!f->rename_text[0]) return unavailable(f, "Enter a profile name.");
            copy(a->name, sizeof(a->name), f->rename_text);
            f->rename_editor = 0;
            return action(a, "rename-instance", target);
        }
        if (key == PM_KEY_CONFIRM)
        {
            if (f->rename_key < 40)
            {
                unsigned letter = (unsigned char)rename_keys[f->rename_key];
                if (f->rename_lower && letter >= 'A' && letter <= 'Z') letter += 'a' - 'A';
                pm_frontend_text(f, letter);
            }
            else if (f->rename_key == 40) pm_frontend_text(f, 8);
            else if (f->rename_key == 41) { f->rename_text[0] = 0; f->rename_replace = 0; }
            else if (f->rename_key == 42) f->rename_lower = !f->rename_lower;
            else if (f->rename_key == 44) f->rename_editor = 0;
        }
        return 0;
    }
    if (f->confirmation)
    {
        if (key == PM_KEY_BACK)
        {
            f->confirmation = 0;
            return 0;
        }
        if (key == PM_KEY_LEFT)
            f->confirmation_choice = 0;
        if (key == PM_KEY_RIGHT)
            f->confirmation_choice = 1;
        if (key == PM_KEY_CONFIRM)
        {
            int kind = f->confirmation;
            f->confirmation = 0;
            if (!f->confirmation_choice)
                return 0;
            if (kind == 2)
                return action(a, "quit", NULL);
            if (v->busy)
                return unavailable(f, "Wait for the current operation before deleting.");
            if (kind == 4)
            {
                const PmGame *g = selected_game(f, v);
                if (!g || strcmp(g->id, f->confirmation_id))
                    return 0;
                copy(a->game, sizeof(a->game), g->id);
                return action(a, "delete-game", NULL);
            }
            if (!i || strcmp(i->id, f->confirmation_id))
                return 0;
            if (kind == 1)
                return action(a, "delete-instance", i);
            if (!count(v->mod_count, PM_MAX_MODS) || !content_matches(f, v))
                return 0;
            copy(a->mod, sizeof(a->mod), f->confirmation_file);
            return content_action(f, a, "content-delete", i);
        }
        return 0;
    }
    if (f->selector_popup)
    {
        int cap = f->selector_popup == 1 ? MEMORY_CHOICES : f->selector_popup == 2 ? 3 : f->selector_popup == 5 ? 5 : 1;
        if (key == PM_KEY_BACK)
        {
            f->selector_popup = 0;
            return 0;
        }
        if (key == PM_KEY_UP || key == PM_KEY_DOWN)
            f->selector_choice = bound(f->selector_choice + delta, 0, cap);
        if (key == PM_KEY_CONFIRM)
        {
            if (f->selector_popup == 1)
                f->memory_mb = MEMORY_MIN + f->selector_choice * MEMORY_STEP;
            else if (f->selector_popup == 2)
                f->resolution_index = f->selector_choice;
            else if (f->selector_popup == 3)
                f->debug_index = f->selector_choice;
            else if (f->selector_popup == 5)
            {
                f->theme_index = f->selector_choice;
                copy(a->theme_color, sizeof(a->theme_color), theme_values[f->theme_index]);
                f->selector_popup = 0;
                return action(a, "settings", NULL);
            }
            else
                f->input_index = f->selector_choice;
            f->selector_popup = 0;
        }
        return 0;
    }
    if (key == PM_KEY_PREVIOUS_TAB || key == PM_KEY_NEXT_TAB)
        return switch_screen(
            f, v,
            (PmScreen)((f->screen + (key == PM_KEY_NEXT_TAB ? 1 : PM_SCREEN_COUNT - 1)) %
                       PM_SCREEN_COUNT),
            a);
    if (f->header_focus >= 0)
    {
        if (key == PM_KEY_LEFT || key == PM_KEY_RIGHT)
            f->header_focus = bound(f->header_focus + delta, 0, PM_SCREEN_COUNT);
        if (key == PM_KEY_DOWN || key == PM_KEY_BACK)
            f->header_focus = -1;
        if (key == PM_KEY_CONFIRM)
        {
            if (f->header_focus == PM_SCREEN_COUNT)
            {
                confirm(f, v, 2);
                return 0;
            }
            return switch_screen(f, v, (PmScreen)f->header_focus, a);
        }
        return 0;
    }
    if (f->pack_picker)
    {
        int content = f->screen == PM_MODS;
        int m = count(content ? v->content_import_count : v->source_count,
                      content ? PM_MAX_INSTANCES : PM_MAX_SOURCES);
        f->selected_pack = bound(f->selected_pack, 0, last(m));
        if (key == PM_KEY_BACK || key == PM_KEY_SECONDARY)
        {
            f->pack_picker = 0;
            return 0;
        }
        if (key == PM_KEY_UP || key == PM_KEY_DOWN)
            f->selected_pack = bound(f->selected_pack + delta, 0, last(m));
        if (v->busy)
            return 0;
        if (key == PM_KEY_REFRESH)
            return content ? content_action(f, a, "scan-content-imports", i) : action(a, "catalog", NULL);
        if (key == PM_KEY_CONFIRM)
        {
            if (!m)
                return unavailable(f, "Copy files to the imports folder shown here, then refresh.");
            copy(a->source, sizeof(a->source),
                 (content ? v->content_imports : v->sources)[f->selected_pack].source);
            f->pack_picker = 0;
            return content ? content_action(f, a, "content-import", i) : action(a, "import-game", NULL);
        }
        return 0;
    }
    if (key == PM_KEY_BACK)
        return f->screen == PM_HOME ? 0 : switch_screen(f, v, PM_HOME, a);
    if (key == PM_KEY_REFRESH && !v->busy)
        return f->screen == PM_MODS       ? content_action(f, a, "content-list", i)
               : f->screen == PM_SETTINGS ? action(a, "detect-java", NULL)
                                          : action(a, "catalog", NULL);
    f->notice[0] = 0;
    switch (f->screen)
    {
    case PM_HOME:
        if (key == PM_KEY_UP && (!n || f->selected_instance < 4))
        {
            f->header_focus = PM_HOME;
            break;
        }
        if (key == PM_KEY_SECONDARY)
            return switch_screen(f, v, PM_GAMES, a);
        if (key == PM_KEY_LEFT || key == PM_KEY_RIGHT)
            f->selected_instance = bound(f->selected_instance + delta, 0, last(n));
        if (key == PM_KEY_UP || key == PM_KEY_DOWN)
            f->selected_instance = bound(f->selected_instance + delta * 4, 0, last(n));
        if (key == PM_KEY_CONFIRM)
            return play(f, v, i, a);
        break;
    case PM_LIBRARY:
        if (key == PM_KEY_LEFT)
            f->focus = 0;
        if (key == PM_KEY_RIGHT && n)
            f->focus = bound(f->focus, 1, 5);
        if (key == PM_KEY_UP || key == PM_KEY_DOWN)
        {
            if (!f->focus)
            {
                if (key == PM_KEY_UP && !f->selected_instance)
                {
                    f->header_focus = PM_LIBRARY;
                    break;
                }
                f->selected_instance = bound(f->selected_instance + delta, 0, last(n));
            }
            else
                f->focus = bound(f->focus + delta, 1, 5);
        }
        if (key == PM_KEY_SECONDARY)
            return switch_screen(f, v, PM_GAMES, a);
        if (key == PM_KEY_CONFIRM)
        {
            if (!i)
                return switch_screen(f, v, PM_GAMES, a);
            if (v->busy)
                return unavailable(f, "Wait for the current operation to finish.");
            if (f->focus <= 1)
                return play(f, v, i, a);
            if (f->focus == 2)
                return switch_screen(f, v, PM_MODS, a);
            if (f->focus == 3)
            {
                start_rename(f, i);
                break;
            }
            if (f->focus == 4)
                return action(a, "clone", i);
            confirm(f, v, 1);
        }
        break;
    case PM_GAMES:
    {
        int g = count(v->game_count, PM_MAX_GAMES);
        if (key == PM_KEY_LEFT)
            f->focus = 0;
        if (key == PM_KEY_RIGHT)
            f->focus = bound(f->focus, g ? 1 : 3, 5);
        if (key == PM_KEY_UP || key == PM_KEY_DOWN)
        {
            if (!f->focus)
            {
                if (key == PM_KEY_UP && !f->selected_game)
                {
                    f->header_focus = PM_GAMES;
                    break;
                }
                f->selected_game = bound(f->selected_game + delta, 0, last(g));
            }
            else
                f->focus = bound(f->focus + delta, g ? 1 : 3, g ? 5 : 4);
        }
        if (key == PM_KEY_SECONDARY)
            return open_import(f, v, a);
        if (key == PM_KEY_CONFIRM)
        {
            if (!g && f->focus < 3)
                f->focus = 3;
            if (f->focus <= 1)
                return create_profile(f, v, "client", a);
            if (f->focus == 2)
                return create_profile(f, v, "server", a);
            if (f->focus == 3)
                return open_import(f, v, a);
            if (f->focus == 4)
                return v->busy ? 0 : action(a, "catalog", NULL);
            if (g)
                confirm(f, v, 4);
        }
        break;
    }
    case PM_MODS:
    {
        int m = content_matches(f, v) ? count(v->mod_count, PM_MAX_MODS) : 0;
        if (!i)
        {
            if (key == PM_KEY_CONFIRM)
                return switch_screen(f, v, PM_GAMES, a);
            break;
        }
        if (key == PM_KEY_UP)
        {
            if (f->focus == 0)
                f->header_focus = PM_MODS;
            else if (f->focus == 2 && f->selected_mod > 0)
                f->selected_mod--;
            else if (f->focus == 4)
                f->focus = 3;
            else
                f->focus = f->focus == 3 ? 1 : f->focus - 1;
        }
        if (key == PM_KEY_DOWN)
        {
            if (f->focus == 2)
                f->selected_mod = bound(f->selected_mod + 1, 0, last(m));
            else if (f->focus == 3)
                f->focus = 4;
            else if (f->focus < 2)
                f->focus++;
        }
        if (key == PM_KEY_LEFT || key == PM_KEY_RIGHT)
        {
            if (f->focus == 0 && n)
            {
                f->selected_instance = (f->selected_instance + delta + n) % n;
                f->selected_mod = 0;
                return v->busy ? 0 : content_action(f, a, "content-list", selected_instance(f, v));
            }
            if (f->focus == 1)
            {
                f->content_kind = !f->content_kind;
                f->selected_mod = 0;
                return v->busy ? 0 : content_action(f, a, "content-list", i);
            }
            f->focus = key == PM_KEY_RIGHT ? (f->focus == 2 ? 3 : f->focus) : 2;
        }
        if (key == PM_KEY_SECONDARY)
            return open_import(f, v, a);
        if (key == PM_KEY_CONFIRM && !v->busy)
        {
            if (f->focus == 0)
            {
                f->selected_instance = (f->selected_instance + 1) % n;
                f->selected_mod = 0;
                return content_action(f, a, "content-list", selected_instance(f, v));
            }
            if (f->focus == 1)
            {
                f->content_kind = !f->content_kind;
                f->selected_mod = 0;
                return content_action(f, a, "content-list", i);
            }
            if (f->focus == 3)
                return open_import(f, v, a);
            if (!content_matches(f, v))
                return content_action(f, a, "content-list", i);
            if (!m)
                return unavailable(f, f->content_kind ? "No worlds yet. Play once, or import a world."
                                                      : "No mods yet. Import mod folders first.");
            if (f->focus == 4)
            {
                confirm(f, v, 3);
                break;
            }
            copy(a->mod, sizeof(a->mod), v->mods[f->selected_mod].name);
            if (f->content_kind)
                return content_action(f, a, "content-backup", i);
            copy(a->enabled, sizeof(a->enabled), v->mods[f->selected_mod].enabled ? "false" : "true");
            return content_action(f, a, "content-toggle", i);
        }
        break;
    }
    case PM_SETTINGS:
        return update_settings_screen(f, v, key, delta, a);
    case PM_SERVER:
    {
        int s = server_count(v);
        const PmInstance *server = selected_server(f, v);
        if (key == PM_KEY_LEFT)
            f->focus = 0;
        if (key == PM_KEY_RIGHT)
            f->focus = bound(f->focus, s ? 1 : 3, s ? 4 : 3);
        if (key == PM_KEY_UP || key == PM_KEY_DOWN)
        {
            if (!f->focus)
            {
                if (key == PM_KEY_UP && !f->selected_server)
                {
                    f->header_focus = PM_SERVER;
                    break;
                }
                f->selected_server = bound(f->selected_server + delta, 0, last(s));
            }
            else
                f->focus = bound(f->focus + delta, s ? 1 : 3, s ? 4 : 3);
        }
        if (key == PM_KEY_SECONDARY || (key == PM_KEY_CONFIRM && (f->focus == 3 || !s)))
        {
            if (!count(v->game_count, PM_MAX_GAMES))
                return unavailable(f, "Import Project Zomboid in Games first.");
            return create_profile(f, v, "server", a);
        }
        if (key == PM_KEY_CONFIRM && server)
        {
            if (f->focus <= 1)
                return play(f, v, server, a);
            if (v->busy)
                return unavailable(f, "Wait for the current operation to finish.");
            if (f->focus == 2)
                return action(a, "server-password", server);
            f->selected_instance = server_index(v, f->selected_server);
            return switch_screen(f, v, PM_MODS, a);
        }
        break;
    }
    default:
        break;
    }
    return 0;
}
static int inside(int x, int y, int left, int top, int w, int h)
{
    return x >= left && x < left + w && y >= top && y < top + h;
}
int pm_frontend_pointer(PmFrontend *f, const PmView *v, int x, int y, int button, int wheel,
                        PmAction *a)
{
    if (!f || !v || !a)
        return 0;
    memset(a, 0, sizeof(*a));
    normalize(f, v);
    f->pointer_x = bound(x, 0, 1919);
    f->pointer_y = bound(y, 0, 1079);
    f->pointer_visible = 1;
    x = f->pointer_x;
    y = f->pointer_y;
    if (button == 2)
        return pm_frontend_update(f, v, PM_KEY_BACK, a);
    if (f->rename_editor)
    {
        if (button == 1 && inside(x, y, 140, 295, 1640, 64)) f->rename_replace = 1;
        for (int j = 0; j < 45; j++)
            if (inside(x, y, 140 + (j % 9) * 184, 380 + (j / 9) * 82, 170, 66))
            {
                f->rename_key = j;
                if (button == 1) return pm_frontend_update(f, v, PM_KEY_CONFIRM, a);
            }
        return 0;
    }
    if (f->confirmation)
    {
        for (int j = 0; j < 2; j++)
            if (button == 1 && inside(x, y, 630 + j * 350, 625, 310, 66))
            {
                f->confirmation_choice = j;
                return pm_frontend_update(f, v, PM_KEY_CONFIRM, a);
            }
        return 0;
    }
    if (f->selector_popup)
    {
        int rows = f->selector_popup == 5 ? 6 : f->selector_popup == 1 ? 7 : f->selector_popup == 2 ? 4 : 2,
            start = f->selector_popup == 1 ? list_start(f->selector_choice, 7) : 0;
        if (f->selector_popup == 1)
            rows = bound(MEMORY_CHOICES + 1 - start, 0, rows);
        if (wheel)
            return pm_frontend_update(f, v, wheel > 0 ? PM_KEY_UP : PM_KEY_DOWN, a);
        for (int j = 0; j < rows; j++)
            if (button == 1 && inside(x, y, 665, 320 + j * 70, 590, 60))
            {
                f->selector_choice = start + j;
                return pm_frontend_update(f, v, PM_KEY_CONFIRM, a);
            }
        return 0;
    }
    if (button == 1 && inside(x, y, 1702, 52, 150, 54))
    {
        confirm(f, v, 2);
        return 0;
    }
    int tx = 470;
    for (int j = 0; j < PM_SCREEN_COUNT; j++)
    {
        int w = text_width(tabs[j], 24) + 30;
        if (button == 1 && inside(x, y, tx - 12, 52, w, 55))
            return switch_screen(f, v, (PmScreen)j, a);
        tx += w + 13;
    }
    if (f->pack_picker)
    {
        int content = f->screen == PM_MODS,
            m = count(content ? v->content_import_count : v->source_count,
                      content ? PM_MAX_INSTANCES : PM_MAX_SOURCES),
            start = list_start(f->selected_pack, 6);
        if (wheel)
            return pm_frontend_update(f, v, wheel > 0 ? PM_KEY_UP : PM_KEY_DOWN, a);
        for (int j = start; j < m && j < start + 6; j++)
            if (button == 1 && inside(x, y, 110, 315 + (j - start) * 86, 1700, 76))
            {
                f->selected_pack = j;
                return pm_frontend_update(f, v, PM_KEY_CONFIRM, a);
            }
        return 0;
    }
    if (button == 1 && inside(x, y, 1080, 985, 300, 48))
        return pm_frontend_update(f, v, PM_KEY_REFRESH, a);
    if (wheel)
    {
        if (f->screen == PM_HOME || f->screen == PM_LIBRARY || f->screen == PM_GAMES || f->screen == PM_SERVER)
            f->focus = 0;
        else if (f->screen == PM_MODS)
            f->focus = 2;
        else
            return 0;
        return pm_frontend_update(f, v, wheel > 0 ? PM_KEY_UP : PM_KEY_DOWN, a);
    }
    int n = count(v->instance_count, PM_MAX_INSTANCES), start, hit = -1;
    switch (f->screen)
    {
    case PM_HOME:
        if (button == 1 && inside(x, y, 1530, 185, 280, 66))
            return switch_screen(f, v, PM_GAMES, a);
        start = list_start(f->selected_instance, 12);
        for (int j = start; j < n && j < start + 12; j++)
        {
            int k = j - start;
            if (button == 1 && inside(x, y, 110 + (k % 4) * 432, 290 + (k / 4) * 181, 408, 161))
            {
                if (f->selected_instance == j)
                    return pm_frontend_update(f, v, PM_KEY_CONFIRM, a);
                f->selected_instance = j;
                f->focus = 0;
                return 0;
            }
        }
        if (button == 1 && inside(x, y, 1520, 847, 290, 66))
            f->selected_instance = bound(start + 12, 0, last(n));
        if (button == 1 && inside(x, y, 1200, 847, 290, 66))
            f->selected_instance = bound(start - 12, 0, last(n));
        return 0;
    case PM_LIBRARY:
    case PM_GAMES:
    case PM_SERVER:
    {
        int rows = f->screen == PM_GAMES ? count(v->game_count, PM_MAX_GAMES)
                   : f->screen == PM_SERVER ? server_count(v) : n;
        int *selection = f->screen == PM_GAMES    ? &f->selected_game
                         : f->screen == PM_SERVER ? &f->selected_server
                                                  : &f->selected_instance;
        start = list_start(*selection, 7);
        for (int j = start; j < rows && j < start + 7; j++)
            if (button == 1 && inside(x, y, 110, 282 + (j - start) * 83, 870, 74))
            {
                *selection = j;
                f->focus = 0;
                return 0;
            }
        int first = f->screen == PM_SERVER ? 590 : 520, buttons = f->screen == PM_SERVER ? 4 : 5;
        for (int j = 0; j < buttons; j++)
            if (inside(x, y, 1050, first + j * 70, 730, 66))
                hit = j + 1;
        break;
    }
    case PM_MODS:
        if (inside(x, y, 110, 187, 1050, 66))
            hit = 0;
        for (int j = 0; j < 2; j++)
            if (button == 1 && inside(x, y, 110 + j * 290, 292, 274, 50))
            {
                f->content_kind = j;
                f->selected_mod = 0;
                f->focus = 2;
                return v->busy ? 0 : content_action(f, a, "content-list", selected_instance(f, v));
            }
        start = list_start(f->selected_mod, 6);
        for (int j = start; j < count(v->mod_count, PM_MAX_MODS) && j < start + 6; j++)
            if (button == 1 && inside(x, y, 110, 365 + (j - start) * 81, 1120, 72))
            {
                f->selected_mod = j;
                f->focus = 2;
                return pm_frontend_update(f, v, PM_KEY_CONFIRM, a);
            }
        for (int j = 0; j < 2; j++)
            if (inside(x, y, 1290, 365 + j * 94, 490, 66))
                hit = j + 3;
        break;
    case PM_SETTINGS:
        if (inside(x, y, 110, 430, 840, 110))
            hit = 10;
        if (inside(x, y, 974, 430, 840, 110))
            hit = 11;
        if (inside(x, y, 110, 280, 408, 134))
            hit = 0;
        for (int j = 0; j < 3; j++)
            if (inside(x, y, 542 + j * 432, 280, 408, 134))
                hit = 7 + j;
        for (int j = 0; j < 4; j++)
            if (inside(x, y, 110 + j * 432, 600, 408, 185))
                hit = j + 1;
        if (inside(x, y, 110, 835, 840, 66))
            hit = 5;
        if (inside(x, y, 974, 835, 840, 66))
            hit = 6;
        break;
    default:
        break;
    }
    if (hit >= 0)
    {
        f->focus = hit;
        f->header_focus = -1;
        if (button == 1)
            return pm_frontend_update(f, v, PM_KEY_CONFIRM, a);
    }
    return 0;
}
typedef struct Canvas
{
    const PwPresentTarget *t;
    unsigned focus_alpha;
    unsigned opacity;
    int offset_x;
    uint32_t accent, focus_rgb, base, panel, row, edge, muted, faint;
} Canvas;
static uint32_t mix(uint32_t a, uint32_t b, unsigned alpha)
{
    unsigned r = ((a >> 16 & 255) * (255 - alpha) + (b >> 16 & 255) * alpha) / 255;
    unsigned g = ((a >> 8 & 255) * (255 - alpha) + (b >> 8 & 255) * alpha) / 255;
    unsigned bl = ((a & 255) * (255 - alpha) + (b & 255) * alpha) / 255;
    return (r << 16) | (g << 8) | bl;
}
static void px(Canvas *c, int x, int y, uint32_t rgb, unsigned alpha)
{
    uint8_t *p;
    x += c->offset_x;
    alpha = alpha * c->opacity / 255;
    if (x < 0 || y < 0 || x >= (int)c->t->width || y >= (int)c->t->height)
        return;
    p = c->t->pixels + (size_t)y * c->t->stride + (size_t)x * 4;
    if (alpha < 255)
        rgb = mix(((uint32_t)p[2] << 16) | ((uint32_t)p[1] << 8) | p[0], rgb, alpha);
    p[0] = (uint8_t)rgb;
    p[1] = (uint8_t)(rgb >> 8);
    p[2] = (uint8_t)(rgb >> 16);
    p[3] = 255;
}
static int sx(Canvas *c, int x)
{
    return (int)((int64_t)x * c->t->width / 1920);
}
static int sy(Canvas *c, int y)
{
    return (int)((int64_t)y * c->t->height / 1080);
}
static void box(Canvas *c, int x, int y, int w, int h, uint32_t rgb)
{
    int left = bound(sx(c, x) + c->offset_x, 0, c->t->width), top = bound(sy(c, y), 0, c->t->height);
    int right = bound(sx(c, x + w) + c->offset_x, 0, c->t->width), bottom = bound(sy(c, y + h), 0, c->t->height),
        xx, yy;
    if (c->opacity < 255) rgb = mix(c->base, rgb, c->opacity);
    uint32_t pixel = rgb | 0xff000000u;
    for (yy = top; yy < bottom; yy++)
    {
        uint8_t *p = c->t->pixels + (size_t)yy * c->t->stride + (size_t)left * 4;
        for (xx = left; xx < right; xx++, p += 4)
            memcpy(p, &pixel, 4);
    }
}
static void roundbox(Canvas *c, int x, int y, int w, int h, int rad, uint32_t rgb)
{
    int xx, yy, left = sx(c, x) + c->offset_x, top = sy(c, y), right = sx(c, x + w) + c->offset_x,
                        bottom = sy(c, y + h);
    int rx = sx(c, rad), ry = sy(c, rad);
    if (rx < 1 || ry < 1)
    {
        box(c, x, y, w, h, rgb);
        return;
    }
    if (c->opacity < 255) rgb = mix(c->base, rgb, c->opacity);
    uint32_t pixel = rgb | 0xff000000u;
    for (yy = bound(top, 0, c->t->height); yy < bound(bottom, 0, c->t->height); yy++)
    {
        int dy = yy < top + ry ? top + ry - yy : yy >= bottom - ry ? yy - (bottom - ry - 1) : 0;
        int inset = 0;
        while (dy && inset < rx &&
               (int64_t)(rx-inset)*(rx-inset)*ry*ry + (int64_t)dy*dy*rx*rx > (int64_t)rx*rx*ry*ry) inset++;
        int begin = bound(left + inset, 0, c->t->width), end = bound(right - inset, 0, c->t->width);
        uint8_t *p = c->t->pixels + (size_t)yy*c->t->stride + (size_t)begin*4;
        for (xx=begin;xx<end;xx++,p+=4) memcpy(p,&pixel,4);
    }
}
static void panel(Canvas *c, int x, int y, int w, int h, int focused)
{
    roundbox(c, x, y + 5, w, h, 4, c->base);
    roundbox(c, x, y, w, h, 4, focused ? mix(c->edge, c->accent, c->focus_alpha) : c->edge);
    roundbox(c, x + 2, y + 2, w - 4, h - 4, 2, focused ? c->focus_rgb : c->panel);
    /* A recessed inventory edge, with a clear controller focus outline. */
    box(c, x + 7, y + 2, w - 14, 1, focused ? c->accent : mix(c->edge,c->accent,35));
    if (focused)
        box(c, x + 2, y + 9, 4, h - 18, c->accent);
}
static int text_width(const char *s, int size)
{
    int width = 0;
    while (*s)
    {
        unsigned ch = (unsigned char)*s++;
        if (ch < 32 || ch > 126)
            ch = '?';
        width += pm_glyphs[ch - 32].advance * size / 48;
    }
    return width;
}
static void text(Canvas *c, const char *s, int x, int y, int size, uint32_t rgb, int max_width)
{
    int pen = x;
    if (!s)
        return;
    while (*s)
    {
        unsigned ch = (unsigned char)*s++;
        const PmGlyph *g;
        int l, t, w, h, xx, yy, gx, gy;
        if (ch < 32 || ch > 126)
            ch = '?';
        g = &pm_glyphs[ch - 32];
        if (max_width > 0 && pen + g->advance * size / 48 > x + max_width)
            break;
        l = sx(c, pen + g->left * size / 48);
        t = sy(c, y + (46 + g->top) * size / 48);
        w = sx(c, g->width * size / 48);
        h = sy(c, g->height * size / 48);
        if (w && h)
            for (yy = 0; yy < h; yy++)
                for (xx = 0; xx < w; xx++)
                {
                    gx = xx * g->width / w;
                    gy = yy * g->height / h;
                    px(c, l + xx, t + yy, rgb, pm_font_pixels[g->offset + gy * g->width + gx]);
                }
        pen += g->advance * size / 48;
    }
}
static void small(Canvas *c, const char *s, int x, int y, uint32_t color, int maxw)
{
    text(c, s, x, y, 24, color, maxw);
}
static int instance_name(Canvas *c, const char *name, int x, int y, int width)
{
    if (text_width(name, 24) <= width)
    {
        text(c, name, x, y, 24, TITLE, width);
        return 0;
    }
    char first[PM_TEXT];
    size_t cut = 0, space = 0;
    for (size_t j = 0; name[j] && j < sizeof(first) - 1; j++)
    {
        first[j] = name[j];
        first[j + 1] = 0;
        if (text_width(first, 24) > width)
            break;
        cut = j + 1;
        if (name[j] == ' ')
            space = j;
    }
    if (space)
        cut = space;
    first[cut] = 0;
    text(c, first, x, y, 24, TITLE, width);
    const char *rest = name + cut;
    while (*rest == ' ')
        rest++;
    text(c, rest, x, y + 27, 24, TITLE, width);
    return 1;
}
static float segment_distance_squared(float x, float y, float ax, float ay, float bx, float by)
{
    float dx = bx - ax, dy = by - ay, t = ((x - ax) * dx + (y - ay) * dy) / (dx * dx + dy * dy);
    if (t < 0)
        t = 0;
    if (t > 1)
        t = 1;
    dx = x - ax - t * dx;
    dy = y - ay - t * dy;
    return dx * dx + dy * dy;
}
/* Shared size, stroke, color and antialiasing for every controller symbol. */
static void controller_symbol(Canvas *c, char shape, int x, int y)
{
    int w = sx(c, 26), h = sy(c, 26), left = sx(c, x), top = sy(c, y);
    if (w <= 0 || h <= 0)
        return;
    for (int yy = 0; yy < h; yy++)
        for (int xx = 0; xx < w; xx++)
        {
            unsigned hits = 0;
            for (int j = 0; j < 4; j++)
                for (int i = 0; i < 4; i++)
                {
                    float px0 = (xx + (i + .5f) / 4) * 26 / w, py0 = (yy + (j + .5f) / 4) * 26 / h,
                          d = 1000;
#define EDGE_DISTANCE(ax, ay, bx, by)                                                              \
    do                                                                                             \
    {                                                                                              \
        float v = segment_distance_squared(px0, py0, ax, ay, bx, by);                              \
        if (v < d)                                                                                 \
            d = v;                                                                                 \
    } while (0)
                    if (shape == 'X')
                    {
                        EDGE_DISTANCE(5, 5, 21, 21);
                        EDGE_DISTANCE(5, 21, 21, 5);
                    }
                    else if (shape == 'O')
                    {
                        float dx = px0 - 13, dy = py0 - 13, r = dx * dx + dy * dy;
                        d = (r >= 81 && r <= 121) ? 0 : 1000;
                    }
                    else if (shape == 'T')
                    {
                        EDGE_DISTANCE(13, 2, 1, 24);
                        EDGE_DISTANCE(1, 24, 25, 24);
                        EDGE_DISTANCE(25, 24, 13, 2);
                    }
                    else
                    {
                        EDGE_DISTANCE(3, 3, 23, 3);
                        EDGE_DISTANCE(23, 3, 23, 23);
                        EDGE_DISTANCE(23, 23, 3, 23);
                        EDGE_DISTANCE(3, 23, 3, 3);
                    }
#undef EDGE_DISTANCE
                    if (d <= 1)
                        hits++;
                }
            if (hits)
                px(c, left + xx, top + yy, TEXT, hits * 255 / 16);
        }
}
static void button(Canvas *c, const char *s, int x, int y, int w, int focused, int primary)
{
    roundbox(c, x, y + 4, w, 66, 4, c->base);
    roundbox(c, x, y, w, 66, 4,
             focused   ? mix(c->edge, c->accent, c->focus_alpha)
             : primary ? c->accent
                       : c->edge);
    if (!primary)
        roundbox(c, x + 2, y + 2, w - 4, 62, 2, focused ? c->focus_rgb : c->row);
    else
        box(c, x + 5, y + 3, w - 10, 2, mix(c->accent, TITLE, 55));
    text(c, s, x + (w - text_width(s, 24)) / 2, y + 17, 24,
         primary   ? 0x111b14
         : focused ? TITLE
                   : TEXT,
         w - 20);
}
static void chip(Canvas *c, const char *s, int x, int y, int accent)
{
    int w = text_width(s, 20) + 30;
    roundbox(c, x, y, w, 38, 10, accent ? c->focus_rgb : c->row);
    text(c, s, x + 15, y + 9, 20, accent ? c->accent : c->muted, w - 22);
}

static void logo(Canvas *c, int x, int y, int size)
{
    int xx, yy, left = sx(c, x), top = sy(c, y), right = sx(c, x + size), bottom = sy(c, y + size);
    if (right <= left || bottom <= top)
        return;
    for (yy = bound(top, 0, c->t->height); yy < bound(bottom, 0, c->t->height); yy++)
    {
        unsigned row = (unsigned)((int64_t)(yy - top) * PM_LOGO_H / (bottom - top));
        for (xx = bound(left, 0, c->t->width); xx < bound(right, 0, c->t->width); xx++)
        {
            unsigned col = (unsigned)((int64_t)(xx - left) * PM_LOGO_W / (right - left));
            uint16_t v = pm_logo_pixels[row * PM_LOGO_W + col];
            unsigned r = v >> 11, g = (v >> 5) & 63, b = v & 31;
            px(c, xx, yy, ((r * 255 / 31) << 16) | ((g * 255 / 63) << 8) | (b * 255 / 31), 255);
        }
    }
}

/* 16 x 16 pixel-art symbols drawn from boxes; no game artwork is used. */
static void pixel_icon(Canvas *c, const char *kind, int x, int y, int size)
{
    int s = size / 16;
#define P(col, row, w, h, rgb) box(c, x + (col) * s, y + (row) * s, (w) * s, (h) * s, rgb)
    if (!strcmp(kind, "archive"))
    {
        P(1, 3, 14, 11, 0x59452d);
        P(2, 4, 12, 9, 0xa87b40);
        P(2, 7, 12, 1, 0x5c462d);
        P(7, 6, 2, 4, 0xebd49d);
        return;
    }
    if (!strcmp(kind, "server"))
    {
        P(2, 1, 12, 14, 0x2b3033);
        for (int j = 0; j < 3; j++)
        {
            P(3, 2 + j * 4, 10, 3, 0x454c50);
            P(4, 3 + j * 4, 5, 1, 0x1b1e20);
            P(11, 3 + j * 4, 1, 1, j == 1 ? 0xf0c35a : 0x9fd36b);
        }
        return;
    }
    if (!strcmp(kind, "game"))
    {
        /* A Kentucky house: roof, walls, windows and a door. */
        for (int row = 0; row < 6; row++)
            P(7 - row, 1 + row, 2 + row * 2, 1, row % 2 ? 0x8a2f2b : 0xa63a33);
        P(2, 7, 12, 8, 0xd8ccb0);
        P(3, 9, 3, 3, 0x3b5566);
        P(10, 9, 3, 3, 0x3b5566);
        P(7, 10, 2, 5, 0x5a3b26);
        P(1, 15, 14, 1, 0x4f6b37);
        return;
    }
    if (!strcmp(kind, "mod"))
    {
        P(2, 4, 10, 10, 0x6d8f4e);
        P(5, 2, 4, 2, 0x6d8f4e);
        P(12, 7, 2, 4, 0x6d8f4e);
        P(3, 5, 8, 8, 0x8db66a);
        return;
    }
    if (!strcmp(kind, "save"))
    {
        P(2, 2, 12, 12, 0x34506b);
        P(4, 2, 8, 4, 0xc7ccd1);
        P(9, 3, 2, 2, 0x34506b);
        P(4, 8, 8, 6, 0xe6e2d6);
        P(5, 10, 6, 1, 0x9aa0a6);
        return;
    }
    if (!strcmp(kind, "server-off"))
    {
        P(2, 1, 12, 14, 0x2b3033);
        return;
    }
    /* Default: a survivor's axe over a field-green background. */
    P(1, 1, 14, 14, 0x2c3a26);
    for (int j = 0; j < 10; j++)
        P(4 + j, 13 - j, 1, 1, 0x8a5a33);
    P(9, 2, 4, 5, 0xb9c0c4);
    P(12, 2, 2, 3, 0x7d858a);
    P(10, 7, 2, 1, 0x7a2a26);
#undef P
}
static void icon(Canvas *c, const char *path, const char *kind, int x, int y, int size)
{
    unsigned width = 0, height = 0;
    const uint8_t *pixels =
        path && path[0] && icon_loader ? icon_loader(path, &width, &height) : NULL;
    if (!pixels || !width || !height || width > 2048 || height > 2048)
    {
        pixel_icon(c, kind ? kind : "client", x, y, size);
        return;
    }
    int left = sx(c, x), top = sy(c, y), right = sx(c, x + size), bottom = sy(c, y + size);
    for (int yy = top; yy < bottom; yy++)
        for (int xx = left; xx < right; xx++)
        {
            unsigned col = (unsigned)((int64_t)(xx - left) * width / (right - left)),
                     row = (unsigned)((int64_t)(yy - top) * height / (bottom - top));
            const uint8_t *p = pixels + ((size_t)row * width + col) * 4;
            px(c, xx, yy, ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2], p[3]);
        }
}
static void heading(Canvas *c, const char *title, const char *subtitle)
{
    text(c, title, 110, 181, 39, TITLE, 1400);
    small(c, subtitle, 112, 237, c->faint, 1630);
}
static void empty(Canvas *c, const char *title, const char *body)
{
    text(c, title, 152, 405, 34, TITLE, 1520);
    small(c, body, 154, 465, c->muted, 1500);
}
/* Word-wrapped text; returns the y below the last line. */
static int paragraph(Canvas *c, const char *s, int x, int y, int size, uint32_t rgb, int width, int lines)
{
    char line[256];
    while (*s && lines-- > 0)
    {
        size_t n = 0, cut = 0;
        while (s[n] && n < sizeof(line) - 1)
        {
            line[n] = s[n];
            line[n + 1] = 0;
            if (text_width(line, size) > width)
                break;
            n++;
            if (s[n] == ' ' || !s[n])
                cut = n;
        }
        if (!s[n])
            cut = n;
        if (!cut)
            cut = n ? n : 1;
        memcpy(line, s, cut);
        line[cut] = 0;
        text(c, line, x, y, size, rgb, width);
        s += cut;
        while (*s == ' ')
            s++;
        y += size + size / 3;
    }
    return y;
}
static const char *profile_kind(const PmInstance *i)
{
    return is_server(i) ? "server" : "client";
}
static void header(Canvas *c, const PmFrontend *f, const PmView *v)
{
    logo(c, 64, 43, 61);
    text(c, "PROSPERO PZ", 141, 45, 29, TITLE, 315);
    small(c, "PROJECT ZOMBOID / PS5", 142, 86, c->faint, 315);
    int x = 470;
    for (int j = 0; j < PM_SCREEN_COUNT; j++)
    {
        int w = text_width(tabs[j], 24) + 30;
        if (f->screen == (PmScreen)j)
            roundbox(c, x - 12, 52, w, 50, 4, c->row);
        if (f->header_focus == j)
            panel(c, x - 12, 52, w, 50, 1);
        text(c, tabs[j], x + 3, 66, 24, f->screen == (PmScreen)j ? TITLE : c->muted, w - 8);
        if (f->screen == (PmScreen)j)
        {
            box(c, f->underline_x, 110, w - 26, 3, c->accent);
            box(c, f->underline_x, 113, 12, 3, mix(c->base, c->accent, 120));
        }
        x += w + 13;
    }
    roundbox(c, 1702, 52, 150, 54, 12, f->header_focus == PM_SCREEN_COUNT ? c->focus_rgb : c->row);
    small(c, "Close  X", 1730, 66, TEXT, 116);
    box(c, 65, 137, 1790, 1, c->edge);
    roundbox(c, 1813, 166, 10, 10, 5, v->busy ? WARN : strstr(v->status, "error") ? WARN : c->accent);
}
static void footer(Canvas *c, const PmFrontend *f, const PmView *v)
{
    box(c, 65, 967, 1790, 1, c->edge);
    small(c, "L1 / R1  Navigate", 80, 993, c->muted, 300);
    controller_symbol(c, 'X', 438, 992);
    small(c,
          f->confirmation        ? "Choose"
          : f->pack_picker       ? "Import"
          : f->screen == PM_HOME ? "Play selected"
                                 : "Select",
          478, 992, TEXT, 330);
    controller_symbol(c, 'O', 813, 992);
    small(c, "Back", 853, 992, TEXT, 170);
    controller_symbol(c, 'T', 1080, 992);
    small(c, f->screen == PM_SETTINGS ? "Detect Java" : "Refresh", 1120, 992, TEXT, 250);
    if (f->screen != PM_SETTINGS || v->java_count)
    {
        controller_symbol(c, 'S', 1480, 992);
        small(c,
              f->pack_picker              ? "Close list"
              : f->screen == PM_SETTINGS  ? "Runtime info"
              : f->screen == PM_GAMES     ? "Import game"
              : f->screen == PM_MODS      ? "Import"
              : f->screen == PM_SERVER    ? "New server"
                                          : "New profile",
              1520, 992, TEXT, 300);
    }
    const char *detail = f->notice[0] ? f->notice : v->detail;
    if (detail[0])
        text(c, detail, 110, 919, 21, f->notice[0] ? WARN : v->busy ? c->accent : c->muted, 1700);
    if (v->busy)
    {
        box(c, 110, 955, 1700, 3, c->edge);
        if (v->progress_total > 0)
            box(c, 110, 955,
                (int)((int64_t)1700 * bound(v->progress_done, 0, v->progress_total) /
                      v->progress_total),
                3, c->accent);
        else
            box(c, 110 + (int)(f->animation_ms % 1600) * 1400 / 1600, 955, 300, 3, c->accent);
    }
}
static void home(Canvas *c, const PmFrontend *f, const PmView *v)
{
    char b[256];
    int n = count(v->instance_count, PM_MAX_INSTANCES),
        start = list_start(f->selected_instance, 12);
    heading(c, "This is how you died",
            "...unless you pick a profile and keep surviving Knox County.");
    button(c, "+ NEW PROFILE", 1530, 185, 280, 0, 1);
    if (!n)
    {
        panel(c, 110, 300, 1700, 480, 0);
        if (!count(v->game_count, PM_MAX_GAMES))
        {
            empty(c, "Bring your copy of Project Zomboid",
                  "Copy your Windows game folder to /data/ProsperoPZ/imports over FTP or a USB drive.");
            small(c, "Then open Games and choose Import Game. Saves and mods stay with each profile.", 154,
                  515, c->faint, 1500);
        }
        else
            empty(c, "No profiles yet", "Open Games and create a profile, or a dedicated server.");
        return;
    }
    for (int j = start; j < n && j < start + 12; j++)
    {
        int k = j - start, x = 110 + (k % 4) * 432, y = 290 + (k / 4) * 181;
        const PmInstance *i = &v->instances[j];
        panel(c, x, y, 408, 161, j == f->selected_instance && f->header_focus < 0);
        icon(c, i->icon, profile_kind(i), x + 23, y + 26, 80);
        int two_lines = instance_name(c, i->name[0] ? i->name : i->id, x + 118, y + 24, 265);
        text(c, i->version, x + 119, y + (two_lines ? 86 : 67), 19, c->muted, 265);
        chip(c, !i->installed ? "GAME MISSING" : is_server(i) ? "DEDICATED SERVER" : "READY TO PLAY",
             x + 119, y + (two_lines ? 118 : 105), i->installed);
    }
    snprintf(b, sizeof(b), "%d profiles  /  page %d of %d", n, start / 12 + 1, (n + 11) / 12);
    small(c, b, 110, 860, c->faint, 900);
    if (start)
        button(c, "< PREVIOUS", 1200, 847, 290, 0, 0);
    if (start + 12 < n)
        button(c, "NEXT >", 1520, 847, 290, 0, 0);
}
static void list_rows(Canvas *c, const PmFrontend *f, const PmView *v, int rows, int selected,
                      int which)
{
    char b[256];
    int start = list_start(selected, 7);
    for (int j = start; j < rows && j < start + 7; j++)
    {
        int y = 282 + (j - start) * 83;
        panel(c, 110, y, 870, 74, j == selected && !f->focus && f->header_focus < 0);
        if (which == PM_GAMES)
        {
            const PmGame *g = &v->games[j];
            icon(c, NULL, "game", 126, y + 8, 56);
            text(c, g->name, 204, y + 7, 25, TITLE, 720);
            snprintf(b, sizeof(b), "Build %d  /  Java %d  /  %s", g->build, g->java,
                     g->valid ? "ready" : "needs attention");
            text(c, b, 205, y + 43, 18, g->valid ? c->faint : WARN, 710);
            continue;
        }
        const PmInstance *item = which == PM_SERVER ? &v->instances[server_index(v, j)] : &v->instances[j];
        icon(c, item->icon, profile_kind(item), 126, y + 8, 56);
        text(c, item->name[0] ? item->name : item->id, 204, y + 7, 25, TITLE, 720);
        snprintf(b, sizeof(b), "%s  /  %s", item->version, is_server(item) ? "dedicated server" : "single player & co-op client");
        text(c, b, 205, y + 43, 18, c->faint, 710);
    }
}
static void library(Canvas *c, const PmFrontend *f, const PmView *v)
{
    int n = count(v->instance_count, PM_MAX_INSTANCES);
    const PmInstance *i = selected_instance(f, v);
    heading(c, "Profiles", "Each profile keeps its own worlds, mods and game options.");
    list_rows(c, f, v, n, f->selected_instance, PM_LIBRARY);
    panel(c, 1018, 282, 790, 586, 0);
    if (!i)
    {
        empty(c, "No profiles yet", "Open Games to create one from your imported game.");
        return;
    }
    icon(c, i->icon, profile_kind(i), 1050, 314, 100);
    text(c, i->name[0] ? i->name : i->id, 1178, 320, 31, TITLE, 580);
    small(c, i->version, 1180, 367, c->muted, 570);
    chip(c, is_server(i) ? "Dedicated server" : "Client", 1050, 446, 1);
    chip(c, i->installed ? "Game ready" : "Game missing", 1290, 446, 0);
    button(c, is_server(i) ? "START SERVER" : "PLAY", 1050, 520, 730, f->focus == 1, 1);
    button(c, "MODS & SAVES", 1050, 590, 730, f->focus == 2, 0);
    button(c, "RENAME PROFILE", 1050, 660, 730, f->focus == 3, 0);
    button(c, "DUPLICATE PROFILE", 1050, 730, 730, f->focus == 4, 0);
    button(c, "DELETE PROFILE", 1050, 800, 730, f->focus == 5, 0);
}
static void games(Canvas *c, const PmFrontend *f, const PmView *v)
{
    char b[256];
    int g = count(v->game_count, PM_MAX_GAMES);
    const PmGame *game = selected_game(f, v);
    heading(c, "Your game files", "Project Zomboid copied from your PC (Steam or GOG, Windows version).");
    list_rows(c, f, v, g, f->selected_game, PM_GAMES);
    if (!g)
    {
        panel(c, 110, 282, 870, 586, 0);
        text(c, "No game imported yet", 150, 320, 31, TITLE, 800);
        int y = paragraph(c, "1. On your PC, open Steam > Project Zomboid > Manage > Browse local files.",
                          150, 390, 22, c->muted, 800, 2);
        y = paragraph(c, "2. Copy that whole folder to /data/ProsperoPZ/imports on the PS5 (FTP), or to a "
                         "USB drive. scripts/import-zomboid.py can do it for you.",
                      150, y + 10, 22, c->muted, 800, 3);
        y = paragraph(c, "3. Choose Import Game. Uploads already on the PS5 are moved, not copied, so "
                         "importing is instant.",
                      150, y + 10, 22, c->muted, 800, 3);
        paragraph(c, "Mods: copy workshop/content/108600/ITEM folders to imports/mods.", 150, y + 10, 20,
                  c->faint, 800, 2);
    }
    panel(c, 1018, 282, 790, 586, 0);
    if (game)
    {
        icon(c, NULL, "game", 1050, 314, 100);
        text(c, game->name, 1178, 320, 31, TITLE, 580);
        snprintf(b, sizeof(b), "Build %d  /  Java %d", game->build, game->java);
        small(c, b, 1180, 367, c->muted, 570);
        paragraph(c, game->detail, 1050, 432, 20, game->valid ? c->faint : WARN, 730, 3);
    }
    else
    {
        text(c, "Import Project Zomboid", 1050, 320, 31, TITLE, 730);
        snprintf(b, sizeof(b), "%d folder%s ready to import", count(v->source_count, PM_MAX_SOURCES),
                 v->source_count == 1 ? "" : "s");
        small(c, b, 1052, 372, c->muted, 730);
    }
    if (g)
    {
        button(c, "NEW PROFILE", 1050, 520, 730, f->focus == 1, 1);
        button(c, "NEW SERVER PROFILE", 1050, 590, 730, f->focus == 2, 0);
    }
    button(c, "IMPORT GAME", 1050, 660, 730, f->focus == 3, !g);
    button(c, "RESCAN", 1050, 730, 730, f->focus == 4, 0);
    if (g)
        button(c, "DELETE GAME FILES", 1050, 800, 730, f->focus == 5, 0);
}
static void content(Canvas *c, const PmFrontend *f, const PmView *v)
{
    const PmInstance *i = selected_instance(f, v);
    char b[256];
    if (!i)
    {
        heading(c, "Mods & Saves", "Manage mods and worlds for each profile.");
        empty(c, "Choose a profile first", "Create a profile in Games, then come back here.");
        return;
    }
    snprintf(b, sizeof(b), "<  %s  >", i->name[0] ? i->name : i->id);
    button(c, b, 110, 187, 1050, f->focus == 0, 0);
    small(c, i->version, 1190, 203, c->faint, 600);
    for (int j = 0; j < 2; j++)
    {
        int x = 110 + j * 290;
        roundbox(c, x, 292, 274, 50, 10, f->content_kind == j ? c->focus_rgb : c->row);
        text(c, kind_labels[j], x + 20, 307, 22, f->content_kind == j ? c->accent : c->muted, 235);
        if (f->focus == 1 && f->content_kind == j)
            box(c, x + 18, 338, 235, 2, c->accent);
    }
    int n = content_matches(f, v) ? count(v->mod_count, PM_MAX_MODS) : 0,
        start = list_start(f->selected_mod, 6);
    if (!n)
    {
        panel(c, 110, 365, 1120, 477, 0);
        text(c, !content_matches(f, v) ? "Loading this profile..."
                : f->content_kind      ? "No worlds yet"
                                       : "No mods yet",
             150, 400, 31, TITLE, 1000);
        paragraph(c, f->content_kind
                         ? "Worlds appear after you play. Import a world (.zip or folder) from "
                           "/data/ProsperoPZ/imports/saves."
                         : "Copy mod folders or .zip files to /data/ProsperoPZ/imports/mods, then choose "
                           "Import. Steam Workshop folders (workshop/content/108600/ID) work as they are.",
                  150, 455, 22, c->muted, 1000, 3);
    }
    for (int j = start; j < n && j < start + 6; j++)
    {
        int y = 365 + (j - start) * 81;
        const PmMod *m = &v->mods[j];
        panel(c, 110, y, 1120, 72, j == f->selected_mod && f->focus == 2);
        icon(c, m->icon, f->content_kind ? "save" : "mod", 126, y + 8, 56);
        text(c, m->title[0] ? m->title : m->name, 200, y + 8, 24, TITLE, 820);
        text(c, m->detail, 201, y + 42, 17, c->faint, 820);
        if (!f->content_kind)
        {
            roundbox(c, 1040, y + 29, 14, 14, 7, m->enabled ? c->accent : c->faint);
            text(c, m->enabled ? "Enabled" : "Disabled", 1070, y + 24, 19, m->enabled ? c->accent : c->faint, 135);
        }
    }
    button(c, f->content_kind ? "IMPORT WORLD" : "IMPORT MODS", 1290, 365, 490, f->focus == 3, 0);
    button(c, f->content_kind ? "MOVE TO BACKUPS" : "DELETE MOD", 1290, 459, 490, f->focus == 4, 0);
    if (n)
    {
        small(c, f->content_kind ? "SELECTED WORLD" : "SELECTED MOD", 1290, 560, c->faint, 490);
        text(c, v->mods[f->selected_mod].name, 1290, 599, 21, TITLE, 490);
    }
    paragraph(c, f->content_kind ? "Select a world to back it up as a .zip in /data/ProsperoPZ/backups."
                                 : "Select a mod to enable or disable it. Required mods are enabled too.",
              1290, 660, 20, c->muted, 490, 3);
    paragraph(c, f->content_kind ? "Deleted worlds are moved to backups, never erased."
                                 : "Server profiles get the same list in their server .ini.",
              1290, 770, 19, c->faint, 490, 2);
}
static const PmJavaRuntime *runtime_for_major(const PmView *v, int major)
{
    for (int j = 0; j < count(v->java_count, PM_MAX_JAVA); j++)
        if (v->java_runtimes[j].major == major && v->java_runtimes[j].valid)
            return &v->java_runtimes[j];
    return NULL;
}
static void settings(Canvas *c, const PmFrontend *f, const PmView *v)
{
    char b[256];
    heading(c, "Make it yours",
            "Memory, display and controller settings apply to every profile when you save.");
    panel(c, 110, 280, 408, 134, !f->focus);
    text(c, "GAME MEMORY", 140, 302, 21, c->faint, 348);
    snprintf(b, sizeof(b), "%d MB   v", f->memory_mb);
    text(c, b, 140, 350, 29, TITLE, 348);
    panel(c, 542, 280, 408, 134, f->focus == 7);
    text(c, "GAME RESOLUTION", 572, 302, 21, c->faint, 348);
    snprintf(b, sizeof(b), "%s   v", resolution_labels[f->resolution_index]);
    text(c, b, 572, 350, 28, TITLE, 348);
    panel(c, 974, 280, 408, 134, f->focus == 8);
    text(c, "LAUNCH MODE", 1004, 302, 21, c->faint, 348);
    snprintf(b, sizeof(b), "%s  v", debug_labels[f->debug_index]);
    text(c, b, 1004, 352, 21, TITLE, 348);
    panel(c, 1406, 280, 408, 134, f->focus == 9);
    text(c, "CONTROLLER INPUT", 1436, 302, 21, c->faint, 348);
    snprintf(b, sizeof(b), "%s  v", f->input_index ? "Mouse / keyboard" : "Native gamepad");
    text(c, b, 1436, 352, 23, TITLE, 348);
    panel(c, 110, 430, 840, 110, f->focus == 10);
    small(c, "APPEARANCE", 140, 449, c->faint, 380);
    snprintf(b, sizeof(b), "%s  v", theme_labels[f->theme_index]);
    text(c, b, 140, 486, 29, TITLE, 380);
    roundbox(c, 850, 482, 42, 30, 6, c->accent);
    text(c, "Theme saves immediately", 535, 492, 20, c->muted, 310);
    panel(c, 974, 430, 840, 110, f->focus == 11);
    small(c, "DISPLAY PACING / VRR", 1004, 449, c->faint, 650);
    text(c, f->vrr_index ? "Stable refresh" : "Auto", 1004, 486, 29, TITLE, 330);
    text(c, "Stable refresh if VRR flickers", 1374, 492, 20, c->muted, 390);
    small(c, "JAVA RUNTIMES  (the game's own jre64 is used when present)", 110, 565, c->faint, 1200);
    for (int j = 0; j < 4; j++)
    {
        int x = 110 + j * 432;
        const PmJavaRuntime *rt = runtime_for_major(v, java_majors[j]);
        panel(c, x, 600, 408, 185, f->focus == j + 1);
        snprintf(b, sizeof(b), "Java %d", java_majors[j]);
        text(c, b, x + 28, 615, 34, TITLE, 350);
        chip(c, rt ? "INSTALLED" : "NOT INSTALLED", x + 28, 660, rt != NULL);
        text(c, rt ? rt->version : j == 0 ? "Runs this launcher" : "Fallback for the game", x + 29, 709, 20,
             c->muted, 350);
        text(c, rt ? "VERIFY >" : "DOWNLOAD >", x + 29, 750, 20, c->accent, 350);
    }
    if (v->java_count)
    {
        const PmJavaRuntime *rt =
            &v->java_runtimes[bound(f->selected_java, 0, count(v->java_count, PM_MAX_JAVA) - 1)];
        snprintf(b, sizeof(b), "Detected runtime: Java %d / %s / %s", rt->major, rt->version,
                 rt->valid ? "Ready" : "Needs attention");
        text(c, b, 110, 797, 21, c->faint, 1700);
    }
    button(c, "REFRESH JAVA RUNTIMES", 110, 835, 840, f->focus == 5, 0);
    button(c, "SAVE SETTINGS", 974, 835, 840, f->focus == 6, 1);
}
static void server(Canvas *c, const PmFrontend *f, const PmView *v)
{
    char b[256];
    int s = server_count(v);
    const PmInstance *i = selected_server(f, v);
    heading(c, "Dedicated server", "Host a world on your PS5 for friends on your network. No PC needed.");
    list_rows(c, f, v, s, f->selected_server, PM_SERVER);
    if (!s)
    {
        panel(c, 110, 282, 870, 586, 0);
        icon(c, NULL, "server-off", 150, 320, 96);
        text(c, "No server profile yet", 270, 330, 31, TITLE, 680);
        paragraph(c, "A server profile runs Project Zomboid's headless dedicated server with "
                     "its own world, mods and settings. It pauses when nobody is connected.",
                  150, 450, 22, c->muted, 790, 4);
    }
    panel(c, 1018, 282, 790, 586, 0);
    if (i)
    {
        text(c, i->name[0] ? i->name : i->id, 1050, 312, 31, TITLE, 730);
        small(c, i->version, 1052, 358, c->muted, 730);
        snprintf(b, sizeof(b), "Server name: %s", i->server);
        small(c, b, 1052, 400, TEXT, 730);
        small(c, "ADMIN PASSWORD (user: admin)", 1052, 440, c->faint, 730);
        text(c, i->password[0] ? i->password : "(set on first start)", 1050, 470, 34, c->accent, 730);
        paragraph(c, "Ports UDP 16261 and 16262. Friends start Project Zomboid with -nosteam and join "
                     "this PS5's IP address.",
                  1052, 524, 19, c->faint, 730, 2);
        button(c, "START SERVER", 1050, 590, 730, f->focus == 1, 1);
        button(c, "NEW ADMIN PASSWORD", 1050, 660, 730, f->focus == 2, 0);
    }
    else
        paragraph(c, "Create a server profile to get a world name and an admin password.", 1050, 330, 24,
                  c->muted, 730, 3);
    button(c, "CREATE SERVER PROFILE", 1050, 730, 730, f->focus == 3 || (!s && f->focus), !s);
    if (i)
        button(c, "MODS & SAVES", 1050, 800, 730, f->focus == 4, 0);
}
static void packs(Canvas *c, const PmFrontend *f, const PmView *v)
{
    int content = f->screen == PM_MODS,
        n = count(content ? v->content_import_count : v->source_count, content ? PM_MAX_INSTANCES : PM_MAX_SOURCES),
        start = list_start(f->selected_pack, 6);
    char b[512];
    if (content)
        snprintf(b, sizeof(b), "Copy folders or .zip files to /data/ProsperoPZ/imports/%s, then refresh.",
                 kinds[f->content_kind]);
    else
        copy(b, sizeof(b), "Game folders found in /data/ProsperoPZ/imports and on USB drives.");
    heading(c, content ? (f->content_kind ? "Import a world" : "Import mods") : "Import Project Zomboid", b);
    if (!n)
    {
        panel(c, 110, 315, 1700, 516, 0);
        empty(c, "Nothing to import yet",
              content ? (f->content_kind ? "A world folder contains map_ver.bin or map_t.bin."
                                         : "Each mod has a mod.info file. Workshop item folders work as they are.")
                      : "Copy the whole Windows game folder (with ProjectZomboid64.json), then refresh.");
    }
    for (int j = start; j < n && j < start + 6; j++)
    {
        int y = 315 + (j - start) * 86;
        const PmPack *item = &(content ? v->content_imports : v->sources)[j];
        panel(c, 110, y, 1700, 76, j == f->selected_pack);
        icon(c, NULL, content ? (f->content_kind ? "save" : "mod") : "game", 129, y + 9, 56);
        text(c, item->name, 214, y + 10, 27, TITLE, 1545);
        text(c, item->source, 215, y + 46, 17, c->faint, 1545);
    }
}
static void overlay(Canvas *c, const PmFrontend *f, const PmView *v)
{
    if (!f->confirmation && !f->selector_popup && !f->rename_editor)
        return;
    for (int y = 0; y < (int)c->t->height; y++)
        for (int x = 0; x < (int)c->t->width; x++)
            px(c, x, y, c->base, 190);
    if (f->rename_editor)
    {
        panel(c, 110, 210, 1700, 690, 0);
        text(c, "Rename profile", 140, 238, 31, TITLE, 1300);
        roundbox(c, 140, 295, 1640, 64, 10, f->rename_replace ? c->focus_rgb : c->row);
        text(c, f->rename_text[0] ? f->rename_text : "Enter a name...", 161, 310, 27,
             f->rename_replace ? c->accent : TITLE, 1590);
        for (int j = 0; j < 45; j++)
        {
            char label[32];
            if (j < 40)
            {
                char letter = rename_keys[j];
                if (f->rename_lower && letter >= 'A' && letter <= 'Z') letter += 'a' - 'A';
                if (letter == ' ') copy(label, sizeof(label), "SPACE");
                else { label[0] = letter; label[1] = 0; }
            }
            else copy(label, sizeof(label), j == 40 ? "ERASE" : j == 41 ? "CLEAR" :
                      j == 42 ? (f->rename_lower ? "a > A" : "A > a") : j == 43 ? "SAVE" : "CANCEL");
            button(c, label, 140 + (j % 9) * 184, 380 + (j / 9) * 82, 170,
                   f->rename_key == j, j == 43);
        }
        small(c, "Square: erase  /  Triangle or Enter: save  /  Circle: cancel", 140, 821, c->muted, 1620);
        text(c, f->notice[0] ? f->notice : "Typing replaces the selected name. Worlds and mods stay in this profile.",
             140, 858, 20, f->notice[0] ? WARN : c->faint, 1620);
        return;
    }
    if (f->selector_popup)
    {
        panel(c, 630, 233, 660, 626, 0);
        text(c,
             f->selector_popup == 5   ? "Theme color"
             : f->selector_popup == 1 ? "Game memory"
             : f->selector_popup == 2 ? "Game resolution"
             : f->selector_popup == 3 ? "Launch mode"
                                      : "Controller input",
             666, 264, 31, TITLE, 580);
        int rows = f->selector_popup == 5 ? 6 : f->selector_popup == 1 ? 7 : f->selector_popup == 2 ? 4 : 2,
            start = f->selector_popup == 1 ? list_start(f->selector_choice, 7) : 0;
        if (f->selector_popup == 1)
            rows = bound(MEMORY_CHOICES + 1 - start, 0, rows);
        for (int j = 0; j < rows; j++)
        {
            char b[64];
            if (f->selector_popup == 5)
                copy(b, sizeof(b), theme_labels[j]);
            else if (f->selector_popup == 1)
                snprintf(b, sizeof(b), "%d MB%s", MEMORY_MIN + (start + j) * MEMORY_STEP,
                         MEMORY_MIN + (start + j) * MEMORY_STEP == 4096 ? "  (recommended)" : "");
            else if (f->selector_popup == 2)
                copy(b, sizeof(b), resolution_labels[j]);
            else if (f->selector_popup == 3)
                copy(b, sizeof(b), debug_labels[j]);
            else
                copy(b, sizeof(b), input_labels[j]);
            if (start + j == f->selector_choice)
                roundbox(c, 665, 320 + j * 70, 590, 60, 10, c->focus_rgb);
            if (f->selector_popup == 5)
                roundbox(c, 1200, 338 + j * 70, 25, 25, 4, theme_colors[j]);
            text(c, b, 694, 336 + j * 70, 26, start + j == f->selector_choice ? c->accent : TEXT, 535);
        }
        return;
    }
    panel(c, 580, 330, 760, 412, 0);
    text(c,
         f->confirmation == 2   ? "Close ProsperoPZ?"
         : f->confirmation == 1 ? "Delete this profile?"
         : f->confirmation == 4 ? "Delete these game files?"
         : f->content_kind      ? "Move this world to backups?"
                                : "Delete this mod?",
         630, 376, 35, TITLE, 650);
    const PmInstance *i = selected_instance(f, v);
    text(c,
         f->confirmation == 2   ? "Return to the PS5 home screen."
         : f->confirmation == 3 ? f->confirmation_file
         : f->confirmation == 4 ? f->confirmation_id
         : i                    ? (i->name[0] ? i->name : i->id)
                                : "",
         630, 444, 25, c->muted, 650);
    text(c,
         f->confirmation == 1   ? "Its worlds and mods move to /data/ProsperoPZ/backups."
         : f->confirmation == 4 ? "Copy them again from your PC to play this build."
         : f->confirmation == 3 ? (f->content_kind ? "It can be restored from backups." : "The mod folder is removed.")
         : v->busy              ? "The current operation will stop."
                                : "",
         630, 494, 23, WARN, 650);
    button(c, "CANCEL", 630, 625, 310, !f->confirmation_choice, 0);
    button(c, f->confirmation == 2 ? "CLOSE" : f->confirmation == 3 && f->content_kind ? "MOVE" : "DELETE",
           980, 625, 310, f->confirmation_choice, 1);
}
void pm_frontend_render(const PmFrontend *frontend, const PmView *v, const PwPresentTarget *target)
{
    if (!frontend || !v || !target || !target->pixels || !target->width || !target->height ||
        target->width > 16384 || target->height > 16384)
        return;
    if (target->stride < (uint64_t)target->width * 4 ||
        target->bytes < (uint64_t)target->stride * target->height)
        return;
    PmFrontend f = *frontend;
    normalize(&f, v);
    Canvas c;
    c.t = target;
    c.accent = theme_colors[f.theme_index];
    c.base = theme_backgrounds[f.theme_index];
    c.muted = mix(0xbcb8ae, c.accent, 28);
    c.faint = mix(0x8e8a80, c.accent, 22);
    c.panel = mix(c.base, c.accent, 14);
    c.row = mix(c.base, c.accent, 22);
    c.edge = mix(c.base, c.accent, 45);
    c.focus_rgb = mix(c.panel, c.accent, 26);
    c.opacity = 255;
    c.offset_x = 0;
    c.focus_alpha = 140 + bound((int)f.focus_ms, 0, 180) * 115 / 180;
    for (int y = 0; y < 1080; y++)
        box(&c, 0, y, 1920, 1, mix(c.base, c.accent, (unsigned)(y * 9 / 1080)));
    /* A quiet Kentucky town skyline: houses, a water tower and dark windows. */
    static const unsigned char heights[] = {3, 5, 4, 7, 6, 4, 3, 9, 8, 5, 4, 6, 3, 4, 7, 5, 4, 3, 6, 8,
                                            5, 4, 3, 5, 6, 4, 7, 5, 3, 4};
    for (unsigned j = 0; j < sizeof(heights); j++)
    {
        int top = 1010 - heights[j] * 28, x0 = (int)j * 64;
        box(&c, x0 + 4, top, 58, 1080 - top, mix(c.base, c.accent, 5));
        box(&c, x0 + 4, top, 58, 3, mix(c.base, c.accent, 10));
        for (int w = top + 18; w < 1000; w += 34)
            if ((j * 7 + (unsigned)w) % 5 == 0)
                box(&c, x0 + 18 + (int)(j % 3) * 10, w, 10, 12, mix(c.base, c.accent, 18));
    }
    box(&c, 1540, 820, 6, 190, mix(c.base, c.accent, 7));
    box(&c, 1590, 820, 6, 190, mix(c.base, c.accent, 7));
    roundbox(&c, 1520, 760, 96, 70, 30, mix(c.base, c.accent, 8));
    header(&c, &f, v);
    unsigned remaining = bound((int)f.transition_ms, 0, 220);
    unsigned eased = remaining * remaining * remaining / (220u * 220u);
    c.offset_x = f.transition_direction * sx(&c, (int)(18u * eased / 220u));
    c.opacity = 255u - 55u * eased / 220u;
    if (f.pack_picker)
        packs(&c, &f, v);
    else
        switch (f.screen)
        {
        case PM_HOME:
            home(&c, &f, v);
            break;
        case PM_LIBRARY:
            library(&c, &f, v);
            break;
        case PM_GAMES:
            games(&c, &f, v);
            break;
        case PM_MODS:
            content(&c, &f, v);
            break;
        case PM_SETTINGS:
            settings(&c, &f, v);
            break;
        case PM_SERVER:
            server(&c, &f, v);
            break;
        default:
            break;
        }
    c.opacity = 255;
    c.offset_x = 0;
    footer(&c, &f, v);
    overlay(&c, &f, v);
    if (f.pointer_visible)
    {
        for (int j = 0; j < 20; j++)
            box(&c, f.pointer_x, f.pointer_y + j, j / 2 + 2, 1, c.base);
        for (int j = 1; j < 17; j++)
            box(&c, f.pointer_x + 1, f.pointer_y + j, j / 2, 1, TITLE);
    }
}
