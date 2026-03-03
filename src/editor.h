#ifndef EDITOR_H
#define EDITOR_H

#include <stdbool.h>
#include "buffer.h"
#include "editor_history.h"
#include "editor_cursor.h"

// The Editor owns a buffer and its undo/redo history.
typedef struct {
    buffer        *buf;
    EditorHistory *history;
    EditorCursor   cursor;
} Editor;

// Construction / destruction
Editor *editor_new_empty(void);
Editor *editor_new_from_file(FILE *f);
void    editor_free(Editor *e);

// High-level editing API (the only functions the user should call)
void editorInsertChar(Editor *e, int row, int col, char c);
void editorDeleteChar(Editor *e, int row, int col);
void editorInsertCR(Editor *e, int row, int col);
void editorDeleteCR(Editor *e, int row);

// Undo / Redo
bool editorUndo(Editor *e);
bool editorRedo(Editor *e);

// Convenience
void editorPrint(Editor *e);

#endif
