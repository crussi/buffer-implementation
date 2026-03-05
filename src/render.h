#ifndef RENDER_H
#define RENDER_H

#include "editor_app.h"
#include "tab.h"

// Render the entire screen: tab bar, buffer content, and status line.
// Call this after every event that may have changed state.
void render_screen(EditorApp *app);

// Render just the tab bar at row 0.
void render_tab_bar(EditorApp *app);

// Render the buffer content for the active tab.
void render_buffer(Tab *t, int top_row, int left_col,
                   int view_rows, int view_cols);

// Render the status/mode line at the bottom of the screen.
void render_status_line(Tab *t, const char *cmd_buf);

#endif // RENDER_H