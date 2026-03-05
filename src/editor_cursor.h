#ifndef EDITOR_CURSOR_H
#define EDITOR_CURSOR_H

#include <stdbool.h>
#include "action_stack.h"
#include "buffer.h"

typedef struct {
    Position pos;         // current cursor position in the buffer
    Position anchor;      // selection start (valid only when selecting == true)
    bool     selecting;   // true while shift+arrow or click-drag is active
    int      desired_col; // remembered col for vertical movement
} EditorCursor;

void cursor_init           (EditorCursor *c);
void cursor_clamp          (EditorCursor *c, buffer *buf);

// In Normal mode the cursor must not rest on the newline (len-1 max).
// In Insert mode the cursor may sit at position len (after last char).
void cursor_clamp_normal   (EditorCursor *c, buffer *buf);

void cursor_move_left      (EditorCursor *c, buffer *buf);
void cursor_move_right     (EditorCursor *c, buffer *buf);
void cursor_move_up        (EditorCursor *c, buffer *buf);
void cursor_move_down      (EditorCursor *c, buffer *buf);

// Normal-mode variants that stop one before the newline.
void cursor_move_left_normal (EditorCursor *c, buffer *buf);
void cursor_move_right_normal(EditorCursor *c, buffer *buf);

// Word motions (Normal mode).
void cursor_move_word_forward (EditorCursor *c, buffer *buf); // w
void cursor_move_word_backward(EditorCursor *c, buffer *buf); // b
void cursor_move_word_end     (EditorCursor *c, buffer *buf); // e

void cursor_start_selection(EditorCursor *c);
void cursor_clear_selection(EditorCursor *c);
bool cursor_has_selection  (const EditorCursor *c);

#endif // EDITOR_CURSOR_H