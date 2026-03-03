#ifndef TAB_H
#define TAB_H

#include <stdbool.h>
#include <stdio.h>
#include "buffer.h"
#include "editor_history.h"
#include "editor_cursor.h"

typedef struct {
    buffer        *buf;
    EditorHistory *history;
    EditorCursor   cursor;
    char          *filepath;
    bool           dirty;
} Tab;

Tab  *tab_new_empty(void);
Tab  *tab_new_from_file(FILE *f);
void  tab_free(Tab *t);

void     tabInsertChar(Tab *t, int row, int col, char c);
void     tabDeleteChar(Tab *t, int row, int col);
void     tabInsertCR  (Tab *t, int row, int col);
void     tabDeleteCR  (Tab *t, int row);

// Insert `text` as a single undoable action. Caller retains ownership of
// the pointer; the string is copied internally. Returns the position
// immediately after the last inserted character for cursor placement.
Position tabInsertText(Tab *t, int row, int col, const char *text);

bool  tabUndo(Tab *t);
bool  tabRedo(Tab *t);

bool  tab_open   (Tab *t, const char *path);
bool  tab_save   (Tab *t);
bool  tab_save_as(Tab *t, const char *path);

void  tabPrint(Tab *t);

#endif // TAB_H