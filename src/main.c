// main.c

#include "editor_app.h"
#include "app_startup.h"
#include "input.h"
#include "render.h"
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

    // -------------------------------------------------------------------------
    // ncurses initialisation
    // -------------------------------------------------------------------------
    initscr();
    raw();                  // Pass keys directly, no line buffering
    noecho();               // Don't echo typed characters
    keypad(stdscr, TRUE);   // Enable arrow keys, function keys, etc.
    curs_set(1);            // Show the cursor
    mousemask(BUTTON1_CLICKED | BUTTON1_PRESSED, NULL);

    // -------------------------------------------------------------------------
    // Main event loop
    // -------------------------------------------------------------------------
    // render_screen is called at the top of each input_handle_next_event call,
    // so the display is always fresh before waiting for the next key.
    while (input_handle_next_event(app))
        ;

    // Final render so the user sees the last state before ncurses tears down.
    render_screen(app);

    // -------------------------------------------------------------------------
    // Teardown
    // -------------------------------------------------------------------------
    endwin();
    app_free(app);
    return 0;
}