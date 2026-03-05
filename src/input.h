#ifndef INPUT_H
#define INPUT_H

#include "editor_app.h"
#include "tab.h"

// Top-level event loop entry point.
// Returns false when the editor should exit.
bool input_handle_next_event(EditorApp *app);

// Per-tab modal key dispatcher.
// app is passed so that command-mode handlers can manipulate tabs.
void input_handle_key  (Tab *t, EditorApp *app, int key);
void input_handle_mouse(Tab *t, int y, int x);

#endif // INPUT_H