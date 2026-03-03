//app_startup.c

#include "app_startup.h"
#include <stdio.h>

void app_init_from_args(EditorApp *app, int argc, char *argv[]) {
    if (!app) return;

    if (argc > 1) {
        // Open each file passed on the command line as its own tab.
        // app_open_tab handles duplicate detection automatically.
        for (int i = 1; i < argc; i++) {
            Tab *t = app_open_tab(app, argv[i]);
            if (!t)
                fprintf(stderr, "Could not open file: %s\n", argv[i]);
        }
        // If no files opened successfully, fall back to one empty tab.
        if (app_tab_count(app) == 0)
            app_new_tab(app);
    } else {
        // No arguments -- start with one empty tab.
        app_new_tab(app);
    }
}