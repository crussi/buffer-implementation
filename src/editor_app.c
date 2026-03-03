//editor_app.c

#include "editor_app.h"
#include <stdlib.h>
#include <string.h>

#define APP_INITIAL_CAPACITY 8

// ------------------------------------------------------------
// Internal helpers
// ------------------------------------------------------------

// Grow tabs[] if needed. Returns false on alloc failure.
static bool app_ensure_capacity(EditorApp *app) {
    if (app->count < app->capacity) return true;

    int new_cap = app->capacity ? app->capacity * 2 : APP_INITIAL_CAPACITY;
    Tab **temp  = realloc(app->tabs, new_cap * sizeof(Tab *));
    if (!temp) return false;

    app->tabs     = temp;
    app->capacity = new_cap;
    return true;
}

// ------------------------------------------------------------
// Lifecycle
// ------------------------------------------------------------

EditorApp *app_new(void) {
    EditorApp *app = malloc(sizeof(EditorApp));
    if (!app) return NULL;

    app->tabs     = NULL;
    app->count    = 0;
    app->capacity = 0;
    app->active   = 0;
    return app;
}

void app_free(EditorApp *app) {
    if (!app) return;
    for (int i = 0; i < app->count; i++)
        tab_free(app->tabs[i]);
    free(app->tabs);
    free(app);
}

// ------------------------------------------------------------
// Tab management
// ------------------------------------------------------------

// Create a new empty tab, add it to the app, and make it active.
Tab *app_new_tab(EditorApp *app) {
    if (!app) return NULL;
    if (!app_ensure_capacity(app)) return NULL;

    Tab *t = tab_new_empty();
    if (!t) return NULL;

    app->tabs[app->count++] = t;
    app->active = app->count - 1;
    return t;
}

// Open a file in a new tab.
// If the file is already open, switches to that tab and returns it instead
// of opening a duplicate. Returns NULL on failure.
Tab *app_open_tab(EditorApp *app, const char *path) {
    if (!app || !path) return NULL;

    // --- Duplicate detection: if path is already open, just switch to it ---
    for (int i = 0; i < app->count; i++) {
        if (app->tabs[i]->filepath &&
            strcmp(app->tabs[i]->filepath, path) == 0) {
            app->active = i;
            return app->tabs[i];
        }
    }

    if (!app_ensure_capacity(app)) return NULL;

    Tab *t = tab_new_empty();
    if (!t) return NULL;

    if (!tab_open(t, path)) {
        tab_free(t);
        return NULL;
    }

    app->tabs[app->count++] = t;
    app->active = app->count - 1;
    return t;
}

// Close the tab at index. If it is the active tab, move active left.
// Returns false if index is out of range.
bool app_close_tab(EditorApp *app, int index) {
    if (!app || index < 0 || index >= app->count) return false;

    tab_free(app->tabs[index]);

    // Shift remaining tabs left
    memmove(&app->tabs[index], &app->tabs[index + 1],
            (app->count - index - 1) * sizeof(Tab *));
    app->count--;

    // Keep active index valid
    if (app->count == 0) {
        app->active = 0;
    } else if (app->active >= app->count) {
        app->active = app->count - 1;
    } else if (index < app->active) {
        app->active--;
    }

    return true;
}

// Switch to the tab at index. Wraps around.
bool app_switch_tab(EditorApp *app, int index) {
    if (!app || app->count == 0) return false;

    // Wrap around in both directions
    if (index < 0)            index = app->count - 1;
    if (index >= app->count)  index = 0;

    app->active = index;
    return true;
}

// Return the currently active Tab, or NULL if no tabs are open.
Tab *app_active_tab(EditorApp *app) {
    if (!app || app->count == 0) return NULL;
    return app->tabs[app->active];
}

// ------------------------------------------------------------
// Save operations
// ------------------------------------------------------------

// Save the active tab.
// If the tab has no filepath (never saved), returns false — the caller
// should invoke app_save_active_as() to prompt the user for a path.
bool app_save_active(EditorApp *app) {
    Tab *t = app_active_tab(app);
    if (!t) return false;
    return tab_save(t);          // returns false when filepath == NULL
}

// Save the active tab under an explicit path (Save As).
// Updates the tab's filepath on success.
bool app_save_active_as(EditorApp *app, const char *path) {
    Tab *t = app_active_tab(app);
    if (!t || !path) return false;
    return tab_save_as(t, path);
}

// Save all open tabs. Returns true only if every save succeeded.
bool app_save_all(EditorApp *app) {
    if (!app) return false;
    bool all_ok = true;
    for (int i = 0; i < app->count; i++) {
        if (app->tabs[i]->dirty) {
            if (!tab_save(app->tabs[i]))
                all_ok = false;   // filepath == NULL tabs silently fail here;
                                  // the render layer should prompt save-as for them
        }
    }
    return all_ok;
}

// ------------------------------------------------------------
// Query
// ------------------------------------------------------------

bool app_any_dirty(EditorApp *app) {
    if (!app) return false;
    for (int i = 0; i < app->count; i++)
        if (app->tabs[i]->dirty) return true;
    return false;
}

int app_tab_count(EditorApp *app) {
    if (!app) return 0;
    return app->count;
}