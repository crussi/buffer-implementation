#ifndef TAB_H
#define TAB_H

#include <stdbool.h>
#include <stdio.h>
#include "buffer.h"
#include "editor_history.h"
#include "editor_cursor.h"

// ---------------------------------------------------------------------------
// Vim editor modes
// ---------------------------------------------------------------------------
typedef enum {
    MODE_NORMAL,        // Normal mode  – navigation, commands
    MODE_INSERT,        // Insert mode  – typing inserts characters
    MODE_VISUAL,        // Visual mode  – character-wise selection
    MODE_VISUAL_LINE,   // Visual-line  – whole-line selection
    MODE_REPLACE,       // Replace mode – overwrite characters
    MODE_COMMAND        // Command-line mode  (`:` commands)
} EditorMode;

// Maximum length of the command-line buffer (`:` command string).
#define CMD_BUF_MAX 512

// ---------------------------------------------------------------------------
// Tab
// ---------------------------------------------------------------------------
typedef struct {
    buffer        *buf;
    EditorHistory *history;
    EditorCursor   cursor;
    char          *filepath;
    bool           dirty;
    EditorMode     mode;         // current Vim editing mode

    // Scroll offsets (maintained by render_screen).
    int scroll_top;
    int scroll_left;

    // Command-line input buffer, used while mode == MODE_COMMAND.
    // Null-terminated; never exceeds CMD_BUF_MAX-1 chars.
    char cmd_buf[CMD_BUF_MAX];
    int  cmd_len;                // strlen(cmd_buf)

    // Pending operator for two-key commands (e.g. 'd', 'y', 'c', 'g').
    // '\0' means no operator is pending.
    char pending_op;

    // Pending repeat count (0 = none / treat as 1).
    int  repeat_count;

    // Yank register (unnamed register, like Vim's `""`).
    char *yank_buf;              // heap-allocated, may be NULL
    bool  yank_is_line;          // true if yanked whole lines
} Tab;

// --- Lifecycle ---
Tab  *tab_new_empty(void);
Tab  *tab_new_from_file(FILE *f);
void  tab_free(Tab *t);

// --- Mode transitions ---
// These functions correctly open/close change groups in the history.
void tab_enter_insert_mode (Tab *t);
void tab_leave_insert_mode (Tab *t);   // returns to Normal
void tab_enter_visual_mode (Tab *t);
void tab_enter_visual_line_mode(Tab *t);
void tab_enter_replace_mode(Tab *t);
void tab_enter_normal_mode (Tab *t);   // generic "return to Normal"
void tab_enter_command_mode(Tab *t);

// --- Editing operations (record into history) ---
void     tabInsertChar(Tab *t, int row, int col, char c);
void     tabDeleteChar(Tab *t, int row, int col);
void     tabInsertCR  (Tab *t, int row, int col);
void     tabDeleteCR  (Tab *t, int row);

// Insert `text` as a single undoable action.  Caller retains ownership of
// the pointer; the string is copied internally.  Returns the position
// immediately after the last inserted character for cursor placement.
Position tabInsertText(Tab *t, int row, int col, const char *text);

// Delete a range of text.  start..end are inclusive buffer positions
// (character-wise, not line-wise).
void tabDeleteRange(Tab *t, Position start, Position end);

// Delete an entire line (dd).  row is clamped to valid range.
void tabDeleteLine(Tab *t, int row);

// Yank helpers.
void tab_yank_range(Tab *t, Position start, Position end, bool line_wise);
void tab_yank_line (Tab *t, int row);
void tab_put_after (Tab *t);   // p
void tab_put_before(Tab *t);   // P

// --- Undo / Redo ---
bool tabUndo(Tab *t);
bool tabRedo(Tab *t);

// --- File I/O ---
bool tab_open   (Tab *t, const char *path);
bool tab_save   (Tab *t);
bool tab_save_as(Tab *t, const char *path);

void tabPrint(Tab *t);

#endif // TAB_H