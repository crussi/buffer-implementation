#include "editor_app.h"
#include "tab.h"
#include <stdio.h>

int main(int argc, char *argv[]) {
    EditorApp *app = app_new();
    if (!app) {
        fprintf(stderr, "Failed to create editor app\n");
        return 1;
    }

    if (argc > 1) {
        // Open each file passed on the command line as its own tab
        for (int i = 1; i < argc; i++) {
            Tab *t = app_open_tab(app, argv[i]);
            if (!t) {
                fprintf(stderr, "Could not open file: %s\n", argv[i]);
            }
        }
        // If no files opened successfully, start with an empty tab
        if (app_tab_count(app) == 0)
            app_new_tab(app);
    } else {
        // No arguments -- start with one empty tab
        app_new_tab(app);
    }

    // Print the active tab (temporary test -- replace with render loop)
    Tab *t = app_active_tab(app);
    if (t) tabPrint(t);

    app_free(app);
    return 0;
}