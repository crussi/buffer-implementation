#ifndef EDITOR_HISTORY_H
#define EDITOR_HISTORY_H

#include <stdbool.h>
#include "action_stack.h"
#include "buffer.h"

typedef struct {
    ActionStack *undo_stack;
    ActionStack *redo_stack;
} EditorHistory;

EditorHistory *new_editor_history(void);
void history_record(EditorHistory *h, Action a);
bool history_undo(EditorHistory *h, buffer *buf);
bool history_redo(EditorHistory *h, buffer *buf);
void free_editor_history(EditorHistory *h);

#endif