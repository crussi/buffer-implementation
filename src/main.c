//main.c

#include "editor_app.h"
#include "input.h"
#include "tab.h"
#include <stdio.h>
#include <ncurses.h>

int main(int argc, char *argv[]) {
    EditorApp *app = app_new();
    if (!app) {
        fprintf(stderr, "Failed to create editor app\n");
        return 1;
    }

    if (argc > 1) {
        // Open each file passed on the command line as its own tab.
        // app_open_tab handles duplicate detection automatically.
        for (int i = 1; i < argc; i++) {
            Tab *t = app_open_tab(app, argv[i]);
            if (!t)
                fprintf(stderr, "Could not open file: %s\n", argv[i]);
        }
        // If no files opened successfully, start with one empty tab
        if (app_tab_count(app) == 0)
            app_new_tab(app);
    } else {
        // No arguments -- start with one empty tab
        app_new_tab(app);
    }

    // ---------------------------------------------------------------------------
    // ncurses initialisation
    // ---------------------------------------------------------------------------
    initscr();
    raw();                  // Pass keys directly, no line buffering
    noecho();               // Don't echo typed characters
    keypad(stdscr, TRUE);   // Enable arrow keys, function keys, etc.
    curs_set(1);            // Show the cursor
    mousemask(BUTTON1_CLICKED | BUTTON1_PRESSED, NULL);

    // ---------------------------------------------------------------------------
    // Main event loop
    // ---------------------------------------------------------------------------
    // input_handle_next_event returns false when the user confirms quit (Ctrl-Q).
    while (input_handle_next_event(app))
        ;

    // ---------------------------------------------------------------------------
    // Teardown
    // ---------------------------------------------------------------------------
    endwin();
    app_free(app);
    return 0;
}