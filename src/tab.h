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
    MODE_NORMAL,    // Normal mode  – navigation, commands
    MODE_INSERT,    // Insert mode  – typing inserts characters
    MODE_VISUAL,    // Visual mode  – character-wise selection
    MODE_VISUAL_LINE, // Visual-line – whole-line selection
    MODE_REPLACE,   // Replace mode – overwrite characters
    MODE_COMMAND    // Command-line mode (`:` commands) – future work
} EditorMode;

// ---------------------------------------------------------------------------
// Tab
// ---------------------------------------------------------------------------
typedef struct {
    buffer        *buf;
    EditorHistory *history;
    EditorCursor   cursor;
    char          *filepath;
    bool           dirty;
    EditorMode     mode;        // current Vim editing mode
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
void tab_enter_replace_mode(Tab *t);
void tab_enter_normal_mode (Tab *t);   // generic "return to Normal"

// --- Editing operations (record into history) ---
void     tabInsertChar(Tab *t, int row, int col, char c);
void     tabDeleteChar(Tab *t, int row, int col);
void     tabInsertCR  (Tab *t, int row, int col);
void     tabDeleteCR  (Tab *t, int row);

// Insert `text` as a single undoable action.  Caller retains ownership of
// the pointer; the string is copied internally.  Returns the position
// immediately after the last inserted character for cursor placement.
Position tabInsertText(Tab *t, int row, int col, const char *text);

// --- Undo / Redo ---
bool tabUndo(Tab *t);
bool tabRedo(Tab *t);

// --- File I/O ---
bool tab_open   (Tab *t, const char *path);
bool tab_save   (Tab *t);
bool tab_save_as(Tab *t, const char *path);

void tabPrint(Tab *t);

#endif // TAB_H