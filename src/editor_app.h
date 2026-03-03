#ifndef EDITOR_APP_H
#define EDITOR_APP_H

#include <stdbool.h>
#include "tab.h"

// EditorApp is the application controller.
// It owns the list of open tabs and tracks which one is active.
typedef struct {
    Tab   **tabs;      // dynamic array of Tab pointers
    int     count;     // number of open tabs
    int     capacity;  // allocated slots in tabs[]
    int     active;    // index of the currently visible tab
} EditorApp;

// Lifecycle
EditorApp *app_new (void);
void       app_free(EditorApp *app);

// Tab management
Tab  *app_new_tab   (EditorApp *app);
Tab  *app_open_tab  (EditorApp *app, const char *path);
bool  app_close_tab (EditorApp *app, int index);
bool  app_switch_tab(EditorApp *app, int index);
Tab  *app_active_tab(EditorApp *app);

// Save operations
bool  app_save_active   (EditorApp *app);
bool  app_save_active_as(EditorApp *app, const char *path);
bool  app_save_all      (EditorApp *app);

// Query
bool  app_any_dirty  (EditorApp *app);
int   app_tab_count  (EditorApp *app);

#endif // EDITOR_APP_H