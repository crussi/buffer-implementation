#ifndef INPUT_H
#define INPUT_H

#include "editor_app.h"
#include "tab.h"

// Top-level event loop entry point -- takes the whole app so it can
// handle app-level keys (switch tab, close tab, save all) as well as
// dispatching per-tab edits to the active tab.
void input_handle_next_event(EditorApp *app);

// Per-tab handlers -- exported so unit tests can inject synthetic
// events without a real terminal.
void input_handle_key  (Tab *t, int key);
void input_handle_mouse(Tab *t, int y, int x);

#endif // INPUT_H