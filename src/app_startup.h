#ifndef APP_STARTUP_H
#define APP_STARTUP_H

#include "editor_app.h"

// app_init_from_args processes command-line arguments and populates `app`
// with the initial set of tabs:
//
//   - argc == 1 (no file arguments): opens one empty tab.
//   - argc  > 1: attempts to open each argv[1..argc-1] as a file tab.
//                Duplicate paths are silently collapsed (see app_open_tab).
//                If every path fails to open, falls back to one empty tab.
//
// This function is pure application logic with no ncurses dependency, which
// makes it directly unit-testable.
void app_init_from_args(EditorApp *app, int argc, char *argv[]);

#endif // APP_STARTUP_H