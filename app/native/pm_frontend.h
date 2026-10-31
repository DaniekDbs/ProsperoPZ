/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PM_FRONTEND_H
#define PM_FRONTEND_H
#include "pw_present.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define PM_MAX_INSTANCES 64
#define PM_MAX_GAMES 32
#define PM_MAX_SOURCES 16
#define PM_MAX_MODS 128
#define PM_MAX_JAVA 16
#define PM_TEXT 192

    /* A profile: one game installation plus its own Zomboid folder. */
    typedef struct PmInstance
    {
        char id[PM_TEXT], name[PM_TEXT], version[PM_TEXT], loader[16];
        char game[64], server[64], password[32];
        char icon[512];
        int installed;
    } PmInstance;
    /* An imported Project Zomboid installation under /data/ProsperoPZ/games. */
    typedef struct PmGame
    {
        char id[64], name[PM_TEXT], detail[256];
        int build, java, valid;
    } PmGame;
    /* A mod (name = mod id) or a world (name = MODE/NAME). */
    typedef struct PmMod
    {
        char name[PM_TEXT], title[PM_TEXT], detail[256], icon[512];
        int enabled;
    } PmMod;
    typedef struct PmPack
    {
        char name[PM_TEXT], source[512];
    } PmPack;
    typedef struct PmJavaRuntime
    {
        int major, valid;
        char version[64], path[512], detail[PM_TEXT];
    } PmJavaRuntime;
    typedef struct PmView
    {
        char status[64], detail[512];
        PmInstance instances[PM_MAX_INSTANCES];
        int instance_count;
        PmGame games[PM_MAX_GAMES];
        int game_count;
        PmPack sources[PM_MAX_SOURCES];
        int source_count;
        PmMod mods[PM_MAX_MODS];
        int mod_count;
        char content_kind[24], content_instance[PM_TEXT];
        PmPack content_imports[PM_MAX_INSTANCES];
        int content_import_count;
        PmJavaRuntime java_runtimes[PM_MAX_JAVA];
        int java_count;
        int memory_mb, busy, progress_done, progress_total, resolution_height;
        char controller_mode[24];
        char theme_color[16];
        char vrr_mode[16];
        char debug_mode[8];
    } PmView;

    typedef enum PmScreen
    {
        PM_HOME,
        PM_LIBRARY,
        PM_GAMES,
        PM_MODS,
        PM_SETTINGS,
        PM_SERVER,
        PM_SCREEN_COUNT
    } PmScreen;
    typedef enum PmKey
    {
        PM_KEY_NONE,
        PM_KEY_UP,
        PM_KEY_DOWN,
        PM_KEY_LEFT,
        PM_KEY_RIGHT,
        PM_KEY_CONFIRM,
        PM_KEY_BACK,
        PM_KEY_PREVIOUS_TAB,
        PM_KEY_NEXT_TAB,
        PM_KEY_SECONDARY,
        PM_KEY_REFRESH
    } PmKey;
    /* Runtime serializes non-empty fields into request.properties. No raw shell input. */
    typedef struct PmAction
    {
        char command[32], id[PM_TEXT], name[PM_TEXT], game[64], mode[16];
        char source[512], mod[PM_TEXT], enabled[8];
        char content_kind[24];
        char controller_mode[24];
        char theme_color[16];
        char vrr_mode[16];
        char debug_mode[8];
        int memory_mb;
        int resolution_height;
        int java_major;
    } PmAction;
    typedef struct PmFrontend
    {
        PmScreen screen;
        int selected_instance, selected_game, selected_mod, selected_java, selected_server;
        int focus, memory_mb;
        int initialized_settings;
        int resolution_index, debug_index, input_index, theme_index, vrr_index;
        int pack_picker, selected_pack;
        int content_kind, header_focus, confirmation, confirmation_choice;
        int selector_popup, selector_choice;
        int rename_editor, rename_key, rename_replace, rename_lower;
        char rename_id[PM_TEXT], rename_text[97];
        char confirmation_id[PM_TEXT], confirmation_file[PM_TEXT];
        unsigned animation_ms, focus_ms, transition_ms;
        int transition_direction;
        int underline_x;
        int pointer_x, pointer_y, pointer_visible;
        char notice[PM_TEXT];
    } PmFrontend;

    /* RGBA pixels are borrowed from the runtime's bounded image cache. */
    typedef const uint8_t *(*PmIconLoader)(const char *path, unsigned *width, unsigned *height);
    void pm_frontend_set_icon_loader(PmIconLoader loader);

    void pm_frontend_init(PmFrontend *frontend);
    void pm_frontend_tick(PmFrontend *frontend, unsigned elapsed_ms);
    /* Printable USB keys and backspace feed the rename field. */
    void pm_frontend_text(PmFrontend *frontend, unsigned key);
    /* Returns 1 only for a backend command; caller invokes it and reloads PmView. */
    int pm_frontend_update(PmFrontend *frontend, const PmView *view, PmKey key, PmAction *action);
    /* Pointer coordinates use the same 1920x1080 design as rendering. Button is
     * a press edge: 1 left/select, 2 right/back, 0 motion or wheel only. */
    int pm_frontend_pointer(PmFrontend *frontend, const PmView *view, int x, int y, int button,
                            int wheel, PmAction *action);
    /* Native BGRX8 framebuffer. Any size is scaled from the 1920 x 1080 design. */
    void pm_frontend_render(const PmFrontend *frontend, const PmView *view,
                            const PwPresentTarget *target);

#ifdef __cplusplus
}
#endif
#endif
