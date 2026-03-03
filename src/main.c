//main.c

#include "editor_app.h"
#include "app_startup.h"
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

    app_init_from_args(app, argc, argv);

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