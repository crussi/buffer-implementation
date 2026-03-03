#ifndef TAB_H
#define TAB_H

#include <stdbool.h>
#include <stdio.h>
#include "buffer.h"
#include "editor_history.h"
#include "editor_cursor.h"

// A Tab owns one file's worth of state: buffer, undo/redo history,
// cursor position, and file metadata.
typedef struct {
    buffer        *buf;
    EditorHistory *history;
    EditorCursor   cursor;
    char          *filepath;  // heap-allocated path, NULL if never saved
    bool           dirty;     // true if modified since last save/open
} Tab;

// Construction / destruction
Tab  *tab_new_empty(void);
Tab  *tab_new_from_file(FILE *f);
void  tab_free(Tab *t);

// High-level editing API
void  tabInsertChar(Tab *t, int row, int col, char c);
void  tabDeleteChar(Tab *t, int row, int col);
void  tabInsertCR  (Tab *t, int row, int col);
void  tabDeleteCR  (Tab *t, int row);

// Undo / Redo
bool  tabUndo(Tab *t);
bool  tabRedo(Tab *t);

// Save / load
bool  tab_open   (Tab *t, const char *path);
bool  tab_save   (Tab *t);
bool  tab_save_as(Tab *t, const char *path);

// Convenience
void  tabPrint(Tab *t);

#endif // TAB_H