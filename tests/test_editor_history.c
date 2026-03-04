// test_editor_history.c
//
// Updated for the Vim-style undo tree refactor:
//
//   - EditorHistory.tree  (UndoTree*)  replaces undo_stack / redo_stack
//   - history_record() now takes a third argument: cursor_after Position
//   - Depth helpers (undo_depth / redo_depth) count tree nodes so the tests
//     can still verify "how many steps can I undo/redo" without reaching into
//     the old ActionStack internals.
//   - Stack-size assertions that previously tested h->undo_stack->size /
//     h->redo_stack->size are replaced with depth helpers or removed where
//     the invariant is implicit in the buffer-state assertions.

#include "unity.h"
#include "editor_history.h"
#include "editor_cursor.h"
#include "buffer.h"
#include <stdlib.h>
#include <string.h>

// =============================================================================
// Fixtures
// =============================================================================

static EditorHistory *h;
static buffer        *buf;
static EditorCursor   cur;

void setUp(void) {
    h   = new_editor_history();
    buf = newBuf();
    cursor_init(&cur);
}

void tearDown(void) {
    free_editor_history(h);
    freeBuf(buf);
    h   = NULL;
    buf = NULL;
}

// =============================================================================
// Tree-depth helpers
//
// These replace the old h->undo_stack->size / h->redo_stack->size checks.
//
// undo_depth:  how many ancestors does tree->current have (excluding root)?
//              == how many times `u` can succeed.
// redo_depth:  how deep is the last_child chain from tree->current?
//              == how many times Ctrl-R can succeed.
// =============================================================================

static int undo_depth(EditorHistory *h) {
    if (!h || !h->tree) return 0;
    int d = 0;
    UndoNode *n = h->tree->current;
    while (n && n->parent) { d++; n = n->parent; }
    return d;
}

static int redo_depth(EditorHistory *h) {
    if (!h || !h->tree) return 0;
    int d = 0;
    UndoNode *n = h->tree->current->last_child;
    while (n) { d++; n = n->last_child; }
    return d;
}

// =============================================================================
// Helpers
// =============================================================================

static Position zero_pos(void) { Position p = {0, 0}; return p; }

static Action make_insert_char(int row, int col, char c) {
    Action a;
    a.type            = INSERT_CHAR;
    a.position.row    = row;
    a.position.col    = col;
    a.character       = c;
    a.text            = NULL;
    a.cursor_before   = zero_pos();
    a.cursor_after    = zero_pos();
    return a;
}

static Action make_delete_char(int row, int col, char c) {
    Action a;
    a.type            = DELETE_CHAR;
    a.position.row    = row;
    a.position.col    = col;
    a.character       = c;
    a.text            = NULL;
    a.cursor_before   = zero_pos();
    a.cursor_after    = zero_pos();
    return a;
}

static Action make_insert_cr(int row, int col) {
    Action a;
    a.type            = INSERT_CR;
    a.position.row    = row;
    a.position.col    = col;
    a.character       = '\0';
    a.text            = NULL;
    a.cursor_before   = zero_pos();
    a.cursor_after    = zero_pos();
    return a;
}

static Action make_delete_cr(int row, int col) {
    Action a;
    a.type            = DELETE_CR;
    a.position.row    = row;
    a.position.col    = col;
    a.character       = '\0';
    a.text            = NULL;
    a.cursor_before   = zero_pos();
    a.cursor_after    = zero_pos();
    return a;
}

// Build an INSERT_TEXT action whose text payload is heap-allocated.
// Ownership transfers to history via history_record.
static Action make_insert_text(int row, int col, const char *text) {
    Action a;
    a.type            = INSERT_TEXT;
    a.position.row    = row;
    a.position.col    = col;
    a.character       = '\0';
    a.text            = text ? strdup(text) : NULL;
    a.cursor_before   = zero_pos();
    a.cursor_after    = zero_pos();
    return a;
}

// Convenience: record an action with a zero cursor_after (sufficient for
// most buffer-state tests that don't check cursor placement).
static void record(EditorHistory *h_arg, Action a) {
    history_record(h_arg, a, zero_pos());
}

static void set_row(buffer *b, int row, const char *text) {
    int len = (int)strlen(text);
    for (int i = 0; i < len; i++)
        insertChar(&b->rows[row], i, text[i]);
}

// =============================================================================
// new_editor_history
// =============================================================================

void test_new_editor_history_returns_non_null(void) {
    TEST_ASSERT_NOT_NULL(h);
}

void test_new_editor_history_tree_not_null(void) {
    TEST_ASSERT_NOT_NULL(h->tree);
}

void test_new_editor_history_nothing_to_undo(void) {
    // No commits yet — undo must fail.
    TEST_ASSERT_FALSE(history_undo(h, buf, NULL));
}

void test_new_editor_history_nothing_to_redo(void) {
    TEST_ASSERT_FALSE(history_redo(h, buf, NULL));
}

// =============================================================================
// free_editor_history
// =============================================================================

void test_free_editor_history_null_does_not_crash(void) {
    free_editor_history(NULL);
}

// =============================================================================
// history_record
// =============================================================================

void test_record_makes_one_undo_step_available(void) {
    // Each record (with no open group → auto-group) creates one undo step.
    record(h, make_insert_char(0, 0, 'x'));
    TEST_ASSERT_EQUAL_INT(1, undo_depth(h));
}

void test_record_multiple_makes_multiple_undo_steps(void) {
    for (int i = 0; i < 5; i++)
        record(h, make_insert_char(0, i, (char)('a' + i)));
    TEST_ASSERT_EQUAL_INT(5, undo_depth(h));
}

void test_record_after_undo_creates_new_branch(void) {
    // After undo the redo path exists; a new record should create a new branch
    // (undo tree property) while the old branch remains reachable.
    insertChar(&buf->rows[0], 0, 'a');
    record(h, make_insert_char(0, 0, 'a'));
    history_undo(h, buf, NULL);
    // Now at root — redo depth should be 1 (the undone node).
    TEST_ASSERT_EQUAL_INT(1, redo_depth(h));
    // Make a new edit.
    insertChar(&buf->rows[0], 0, 'b');
    record(h, make_insert_char(0, 0, 'b'));
    // undo depth goes back to 1 (the new branch).
    TEST_ASSERT_EQUAL_INT(1, undo_depth(h));
}

void test_record_does_not_crash_on_null_history(void) {
    // Must not crash even with NULL.
    history_record(NULL, make_insert_char(0, 0, 'x'), zero_pos());
}

// =============================================================================
// history_undo — return value
// =============================================================================

void test_undo_returns_false_when_nothing_to_undo(void) {
    TEST_ASSERT_FALSE(history_undo(h, buf, NULL));
}

void test_undo_returns_false_on_null_history(void) {
    TEST_ASSERT_FALSE(history_undo(NULL, buf, NULL));
}

void test_undo_returns_false_on_null_buffer(void) {
    record(h, make_insert_char(0, 0, 'x'));
    TEST_ASSERT_FALSE(history_undo(h, NULL, NULL));
}

void test_undo_returns_true_when_action_present(void) {
    set_row(buf, 0, "a");
    record(h, make_insert_char(0, 0, 'a'));
    TEST_ASSERT_TRUE(history_undo(h, buf, NULL));
}

// =============================================================================
// history_redo — return value
// =============================================================================

void test_redo_returns_false_when_nothing_to_redo(void) {
    TEST_ASSERT_FALSE(history_redo(h, buf, NULL));
}

void test_redo_returns_false_on_null_history(void) {
    TEST_ASSERT_FALSE(history_redo(NULL, buf, NULL));
}

void test_redo_returns_false_on_null_buffer(void) {
    TEST_ASSERT_FALSE(history_redo(h, NULL, NULL));
}

// =============================================================================
// INSERT_CHAR undo / redo
// =============================================================================

void test_undo_insert_char_removes_character(void) {
    insertChar(&buf->rows[0], 0, 'h');
    record(h, make_insert_char(0, 0, 'h'));
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(0, buf->rows[0].length);
    TEST_ASSERT_EQUAL_CHAR('\0', buf->rows[0].line[0]);
}

void test_undo_insert_char_decrements_undo_depth(void) {
    insertChar(&buf->rows[0], 0, 'h');
    record(h, make_insert_char(0, 0, 'h'));
    TEST_ASSERT_EQUAL_INT(1, undo_depth(h));
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(0, undo_depth(h));
}

void test_undo_insert_char_makes_redo_available(void) {
    insertChar(&buf->rows[0], 0, 'h');
    record(h, make_insert_char(0, 0, 'h'));
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(1, redo_depth(h));
}

void test_redo_insert_char_restores_character(void) {
    insertChar(&buf->rows[0], 0, 'h');
    record(h, make_insert_char(0, 0, 'h'));
    history_undo(h, buf, NULL);
    history_redo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(1, buf->rows[0].length);
    TEST_ASSERT_EQUAL_CHAR('h', buf->rows[0].line[0]);
}

void test_redo_insert_char_restores_undo_depth(void) {
    insertChar(&buf->rows[0], 0, 'h');
    record(h, make_insert_char(0, 0, 'h'));
    history_undo(h, buf, NULL);
    history_redo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(1, undo_depth(h));
    TEST_ASSERT_EQUAL_INT(0, redo_depth(h));
}

void test_undo_redo_insert_char_multiple_times(void) {
    insertChar(&buf->rows[0], 0, 'h');
    record(h, make_insert_char(0, 0, 'h'));
    insertChar(&buf->rows[0], 1, 'i');
    record(h, make_insert_char(0, 1, 'i'));
    history_undo(h, buf, NULL);
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(0, buf->rows[0].length);
    history_redo(h, buf, NULL);
    history_redo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(2, buf->rows[0].length);
    TEST_ASSERT_EQUAL_CHAR('h', buf->rows[0].line[0]);
    TEST_ASSERT_EQUAL_CHAR('i', buf->rows[0].line[1]);
}

// =============================================================================
// DELETE_CHAR undo / redo
// =============================================================================

void test_undo_delete_char_restores_character(void) {
    set_row(buf, 0, "ab");
    record(h, make_delete_char(0, 0, 'a'));
    deleteChar(buf, 0, 0);
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(2, buf->rows[0].length);
    TEST_ASSERT_EQUAL_CHAR('a', buf->rows[0].line[0]);
    TEST_ASSERT_EQUAL_CHAR('b', buf->rows[0].line[1]);
}

void test_undo_delete_char_makes_redo_available(void) {
    set_row(buf, 0, "ab");
    record(h, make_delete_char(0, 0, 'a'));
    deleteChar(buf, 0, 0);
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(1, redo_depth(h));
}

void test_redo_delete_char_removes_character_again(void) {
    set_row(buf, 0, "ab");
    record(h, make_delete_char(0, 0, 'a'));
    deleteChar(buf, 0, 0);
    history_undo(h, buf, NULL);
    history_redo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(1, buf->rows[0].length);
    TEST_ASSERT_EQUAL_CHAR('b', buf->rows[0].line[0]);
}

void test_undo_redo_delete_char_cycle(void) {
    set_row(buf, 0, "abc");
    record(h, make_delete_char(0, 1, 'b'));
    deleteChar(buf, 0, 1);
    TEST_ASSERT_EQUAL_STRING("ac", buf->rows[0].line);
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_STRING("abc", buf->rows[0].line);
    history_redo(h, buf, NULL);
    TEST_ASSERT_EQUAL_STRING("ac", buf->rows[0].line);
}

// =============================================================================
// INSERT_CR undo / redo
// =============================================================================

void test_undo_insert_cr_merges_rows(void) {
    set_row(buf, 0, "hello");
    insertCR(buf, 0, 3);
    record(h, make_insert_cr(0, 3));
    TEST_ASSERT_EQUAL_INT(2, buf->numrows);
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(1, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("hello", buf->rows[0].line);
}

void test_undo_insert_cr_makes_redo_available(void) {
    set_row(buf, 0, "hello");
    insertCR(buf, 0, 3);
    record(h, make_insert_cr(0, 3));
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(1, redo_depth(h));
}

void test_redo_insert_cr_splits_row_again(void) {
    set_row(buf, 0, "hello");
    insertCR(buf, 0, 3);
    record(h, make_insert_cr(0, 3));
    history_undo(h, buf, NULL);
    history_redo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(2, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("hel", buf->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("lo",  buf->rows[1].line);
}

void test_undo_redo_insert_cr_cycle(void) {
    set_row(buf, 0, "abcd");
    insertCR(buf, 0, 2);
    record(h, make_insert_cr(0, 2));
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(1,         buf->numrows);
    TEST_ASSERT_EQUAL_STRING("abcd", buf->rows[0].line);
    history_redo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(2,       buf->numrows);
    TEST_ASSERT_EQUAL_STRING("ab", buf->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("cd", buf->rows[1].line);
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(1,         buf->numrows);
    TEST_ASSERT_EQUAL_STRING("abcd", buf->rows[0].line);
}

// =============================================================================
// DELETE_CR undo / redo
// =============================================================================

void test_undo_delete_cr_splits_rows(void) {
    set_row(buf, 0, "hel");
    insertCR(buf, 0, 3);
    set_row(buf, 1, "lo");
    record(h, make_delete_cr(0, 3));
    deleteCR(buf, 1);
    TEST_ASSERT_EQUAL_INT(1, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("hello", buf->rows[0].line);
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(2, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("hel", buf->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("lo",  buf->rows[1].line);
}

void test_undo_delete_cr_makes_redo_available(void) {
    set_row(buf, 0, "hel");
    insertCR(buf, 0, 3);
    set_row(buf, 1, "lo");
    record(h, make_delete_cr(0, 3));
    deleteCR(buf, 1);
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(1, redo_depth(h));
}

void test_redo_delete_cr_merges_rows_again(void) {
    set_row(buf, 0, "hel");
    insertCR(buf, 0, 3);
    set_row(buf, 1, "lo");
    record(h, make_delete_cr(0, 3));
    deleteCR(buf, 1);
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(2, buf->numrows);
    history_redo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(1, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("hello", buf->rows[0].line);
}

// =============================================================================
// INSERT_TEXT undo / redo
// =============================================================================

void test_undo_insert_text_removes_single_line(void) {
    insertText(buf, 0, 0, "hello");
    record(h, make_insert_text(0, 0, "hello"));
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(1, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("", buf->rows[0].line);
}

void test_undo_insert_text_removes_multiline(void) {
    insertText(buf, 0, 0, "foo\nbar");
    record(h, make_insert_text(0, 0, "foo\nbar"));
    TEST_ASSERT_EQUAL_INT(2, buf->numrows);
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(1, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("", buf->rows[0].line);
}

void test_undo_insert_text_decrements_undo_depth(void) {
    insertText(buf, 0, 0, "hello");
    record(h, make_insert_text(0, 0, "hello"));
    TEST_ASSERT_EQUAL_INT(1, undo_depth(h));
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(0, undo_depth(h));
}

void test_undo_insert_text_makes_redo_available(void) {
    insertText(buf, 0, 0, "hello");
    record(h, make_insert_text(0, 0, "hello"));
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(1, redo_depth(h));
}

void test_redo_insert_text_restores_single_line(void) {
    insertText(buf, 0, 0, "hello");
    record(h, make_insert_text(0, 0, "hello"));
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_STRING("", buf->rows[0].line);
    history_redo(h, buf, NULL);
    TEST_ASSERT_EQUAL_STRING("hello", buf->rows[0].line);
}

void test_redo_insert_text_restores_multiline(void) {
    insertText(buf, 0, 0, "foo\nbar");
    record(h, make_insert_text(0, 0, "foo\nbar"));
    history_undo(h, buf, NULL);
    history_redo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(2, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("foo", buf->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("bar", buf->rows[1].line);
}

void test_redo_insert_text_restores_undo_depth(void) {
    insertText(buf, 0, 0, "hello");
    record(h, make_insert_text(0, 0, "hello"));
    history_undo(h, buf, NULL);
    history_redo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(1, undo_depth(h));
    TEST_ASSERT_EQUAL_INT(0, redo_depth(h));
}

void test_undo_redo_insert_text_cycle_multiple_times(void) {
    insertText(buf, 0, 0, "abc\ndef");
    record(h, make_insert_text(0, 0, "abc\ndef"));

    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(1, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("", buf->rows[0].line);

    history_redo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(2, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("abc", buf->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("def", buf->rows[1].line);

    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(1, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("", buf->rows[0].line);
}

void test_undo_insert_text_mid_line_restores_context(void) {
    set_row(buf, 0, "XZ");
    insertText(buf, 0, 1, "Y");
    record(h, make_insert_text(0, 1, "Y"));
    TEST_ASSERT_EQUAL_STRING("XYZ", buf->rows[0].line);
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_STRING("XZ", buf->rows[0].line);
}

void test_undo_insert_text_preserves_surrounding_rows(void) {
    insertText(buf, 0, 0, "before");
    insertCR(buf, 0, 6);
    insertCR(buf, 1, 0);
    insertText(buf, 2, 0, "after");
    insertText(buf, 1, 0, "hello\nworld");
    record(h, make_insert_text(1, 0, "hello\nworld"));
    TEST_ASSERT_EQUAL_INT(4, buf->numrows);
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(3, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("before", buf->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("",       buf->rows[1].line);
    TEST_ASSERT_EQUAL_STRING("after",  buf->rows[2].line);
}

void test_undo_insert_text_with_trailing_newline(void) {
    insertText(buf, 0, 0, "hi\n");
    record(h, make_insert_text(0, 0, "hi\n"));
    TEST_ASSERT_EQUAL_INT(2, buf->numrows);
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(1, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("", buf->rows[0].line);
}

void test_insert_text_record_after_undo_creates_new_branch(void) {
    // Do an insert, undo it so a redo path exists, then record a new
    // INSERT_TEXT action — the tree creates a new branch (undo tree property).
    // The old redo path is no longer the default, but undo_depth should be 1.
    insertChar(&buf->rows[0], 0, 'A');
    record(h, make_insert_char(0, 0, 'A'));
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(1, redo_depth(h));

    insertText(buf, 0, 0, "new");
    record(h, make_insert_text(0, 0, "new"));
    // New branch is now current; undo depth is 1 again.
    TEST_ASSERT_EQUAL_INT(1, undo_depth(h));
}

// =============================================================================
// INSERT_TEXT cursor tests
// =============================================================================

void test_undo_insert_text_restores_cursor_to_start(void) {
    Position after = {0, 5};
    insertText(buf, 0, 0, "hello");
    history_record(h, make_insert_text(0, 0, "hello"), after);
    cur.pos.row = 0;
    cur.pos.col = 5;
    history_undo(h, buf, &cur);
    TEST_ASSERT_EQUAL_INT(0, cur.pos.row);
    TEST_ASSERT_EQUAL_INT(0, cur.pos.col);
}

void test_undo_insert_text_multiline_restores_cursor_to_start(void) {
    Position after = {1, 3};
    insertText(buf, 0, 0, "foo\nbar");
    history_record(h, make_insert_text(0, 0, "foo\nbar"), after);
    cur.pos.row = 1;
    cur.pos.col = 3;
    history_undo(h, buf, &cur);
    TEST_ASSERT_EQUAL_INT(0, cur.pos.row);
    TEST_ASSERT_EQUAL_INT(0, cur.pos.col);
}

void test_redo_insert_text_restores_cursor_to_recorded_after_position(void) {
    Position after = {0, 5};
    insertText(buf, 0, 0, "hello");
    history_record(h, make_insert_text(0, 0, "hello"), after);
    history_undo(h, buf, &cur);
    cur.pos.row = 99;
    cur.pos.col = 99;
    history_redo(h, buf, &cur);
    // redo restores cursor to the position recorded at commit time (0,5)
    TEST_ASSERT_EQUAL_INT(0, cur.pos.row);
    TEST_ASSERT_EQUAL_INT(5, cur.pos.col);
}

void test_undo_insert_text_syncs_desired_col(void) {
    Position after = {0, 5};
    insertText(buf, 0, 0, "hello");
    history_record(h, make_insert_text(0, 0, "hello"), after);
    cur.pos.col     = 5;
    cur.desired_col = 5;
    history_undo(h, buf, &cur);
    TEST_ASSERT_EQUAL_INT(cur.pos.col, cur.desired_col);
}

// =============================================================================
// Mixed action sequences
// =============================================================================

void test_undo_sequence_restores_in_reverse_order(void) {
    insertChar(&buf->rows[0], 0, 'h');
    record(h, make_insert_char(0, 0, 'h'));
    insertChar(&buf->rows[0], 1, 'i');
    record(h, make_insert_char(0, 1, 'i'));
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(1, buf->rows[0].length);
    TEST_ASSERT_EQUAL_CHAR('h', buf->rows[0].line[0]);
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(0, buf->rows[0].length);
}

void test_new_edit_after_undo_creates_branch(void) {
    // Undo tree: new edit after undo creates a new child, not erasing old.
    insertChar(&buf->rows[0], 0, 'a');
    record(h, make_insert_char(0, 0, 'a'));
    history_undo(h, buf, NULL);
    // Old redo path exists.
    TEST_ASSERT_EQUAL_INT(1, redo_depth(h));
    // New edit.
    insertChar(&buf->rows[0], 0, 'b');
    record(h, make_insert_char(0, 0, 'b'));
    // Current branch has depth 1.
    TEST_ASSERT_EQUAL_INT(1, undo_depth(h));
}

void test_redo_follows_most_recent_branch(void) {
    // After undo+new_edit, redo follows the most recently created child.
    insertChar(&buf->rows[0], 0, 'a');
    record(h, make_insert_char(0, 0, 'a'));
    history_undo(h, buf, NULL);
    // Make new edit 'b'.
    insertChar(&buf->rows[0], 0, 'b');
    record(h, make_insert_char(0, 0, 'b'));
    history_undo(h, buf, NULL);
    // Redo should replay 'b', not 'a'.
    history_redo(h, buf, NULL);
    TEST_ASSERT_EQUAL_CHAR('b', buf->rows[0].line[0]);
}

void test_redo_is_unavailable_after_new_edit_on_fresh_history(void) {
    // Simple case: record, undo, record new edit, redo on new branch returns false.
    insertChar(&buf->rows[0], 0, 'a');
    record(h, make_insert_char(0, 0, 'a'));
    history_undo(h, buf, NULL);
    deleteChar(buf, 0, 0); // reset buf
    insertChar(&buf->rows[0], 0, 'b');
    record(h, make_insert_char(0, 0, 'b'));
    // 'b' branch has no children yet.
    TEST_ASSERT_FALSE(history_redo(h, buf, NULL));
}

void test_full_undo_redo_cycle_mixed_actions(void) {
    insertChar(&buf->rows[0], 0, 'a');
    record(h, make_insert_char(0, 0, 'a'));
    insertChar(&buf->rows[0], 1, 'b');
    record(h, make_insert_char(0, 1, 'b'));
    insertCR(buf, 0, 2);
    record(h, make_insert_cr(0, 2));
    insertChar(&buf->rows[1], 0, 'c');
    record(h, make_insert_char(1, 0, 'c'));

    TEST_ASSERT_EQUAL_INT(2, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("ab", buf->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("c",  buf->rows[1].line);

    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_STRING("", buf->rows[1].line);
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(1, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("ab", buf->rows[0].line);
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_STRING("a", buf->rows[0].line);
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(0, buf->rows[0].length);

    history_redo(h, buf, NULL);
    history_redo(h, buf, NULL);
    history_redo(h, buf, NULL);
    history_redo(h, buf, NULL);

    TEST_ASSERT_EQUAL_INT(2, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("ab", buf->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("c",  buf->rows[1].line);
}

void test_mixed_insert_text_and_insert_char_undo_order(void) {
    insertText(buf, 0, 0, "hi");
    record(h, make_insert_text(0, 0, "hi"));
    insertChar(&buf->rows[0], 2, 'X');
    record(h, make_insert_char(0, 2, 'X'));

    TEST_ASSERT_EQUAL_STRING("hiX", buf->rows[0].line);

    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_STRING("hi", buf->rows[0].line);

    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_STRING("", buf->rows[0].line);
}

void test_undo_beyond_history_returns_false(void) {
    insertChar(&buf->rows[0], 0, 'x');
    record(h, make_insert_char(0, 0, 'x'));
    history_undo(h, buf, NULL);
    TEST_ASSERT_FALSE(history_undo(h, buf, NULL));
    TEST_ASSERT_FALSE(history_undo(h, buf, NULL));
}

void test_redo_beyond_history_returns_false(void) {
    insertChar(&buf->rows[0], 0, 'x');
    record(h, make_insert_char(0, 0, 'x'));
    history_undo(h, buf, NULL);
    history_redo(h, buf, NULL);
    TEST_ASSERT_FALSE(history_redo(h, buf, NULL));
    TEST_ASSERT_FALSE(history_redo(h, buf, NULL));
}

void test_alternating_undo_redo_is_stable(void) {
    insertChar(&buf->rows[0], 0, 'z');
    record(h, make_insert_char(0, 0, 'z'));
    for (int i = 0; i < 10; i++) {
        history_undo(h, buf, NULL);
        TEST_ASSERT_EQUAL_INT(0, buf->rows[0].length);
        history_redo(h, buf, NULL);
        TEST_ASSERT_EQUAL_INT(1, buf->rows[0].length);
        TEST_ASSERT_EQUAL_CHAR('z', buf->rows[0].line[0]);
    }
}

void test_alternating_undo_redo_insert_text_is_stable(void) {
    insertText(buf, 0, 0, "hello\nworld");
    record(h, make_insert_text(0, 0, "hello\nworld"));
    for (int i = 0; i < 5; i++) {
        history_undo(h, buf, NULL);
        TEST_ASSERT_EQUAL_INT(1, buf->numrows);
        TEST_ASSERT_EQUAL_STRING("", buf->rows[0].line);
        history_redo(h, buf, NULL);
        TEST_ASSERT_EQUAL_INT(2, buf->numrows);
        TEST_ASSERT_EQUAL_STRING("hello", buf->rows[0].line);
        TEST_ASSERT_EQUAL_STRING("world", buf->rows[1].line);
    }
}

// =============================================================================
// Depth invariants
// =============================================================================

void test_undo_reduces_undo_depth_by_one(void) {
    for (int i = 0; i < 3; i++) {
        insertChar(&buf->rows[0], i, (char)('a' + i));
        record(h, make_insert_char(0, i, (char)('a' + i)));
    }
    TEST_ASSERT_EQUAL_INT(3, undo_depth(h));
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(2, undo_depth(h));
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(1, undo_depth(h));
}

void test_redo_reduces_redo_depth_by_one(void) {
    for (int i = 0; i < 3; i++) {
        insertChar(&buf->rows[0], i, (char)('a' + i));
        record(h, make_insert_char(0, i, (char)('a' + i)));
    }
    history_undo(h, buf, NULL);
    history_undo(h, buf, NULL);
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(3, redo_depth(h));
    history_redo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(2, redo_depth(h));
    history_redo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(1, redo_depth(h));
}

void test_undo_all_then_redo_all_depths(void) {
    for (int i = 0; i < 4; i++) {
        insertChar(&buf->rows[0], i, (char)('a' + i));
        record(h, make_insert_char(0, i, (char)('a' + i)));
    }
    for (int i = 0; i < 4; i++)
        history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(0, undo_depth(h));
    TEST_ASSERT_EQUAL_INT(4, redo_depth(h));
    for (int i = 0; i < 4; i++)
        history_redo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(4, undo_depth(h));
    TEST_ASSERT_EQUAL_INT(0, redo_depth(h));
}

// =============================================================================
// Cursor -- NULL safety
// =============================================================================

void test_undo_with_null_cursor_does_not_crash(void) {
    insertChar(&buf->rows[0], 0, 'x');
    record(h, make_insert_char(0, 0, 'x'));
    TEST_ASSERT_TRUE(history_undo(h, buf, NULL));
}

void test_redo_with_null_cursor_does_not_crash(void) {
    insertChar(&buf->rows[0], 0, 'x');
    record(h, make_insert_char(0, 0, 'x'));
    history_undo(h, buf, NULL);
    TEST_ASSERT_TRUE(history_redo(h, buf, NULL));
}

void test_undo_insert_text_null_cursor_does_not_crash(void) {
    insertText(buf, 0, 0, "hello");
    record(h, make_insert_text(0, 0, "hello"));
    TEST_ASSERT_TRUE(history_undo(h, buf, NULL));
}

void test_redo_insert_text_null_cursor_does_not_crash(void) {
    insertText(buf, 0, 0, "hello");
    record(h, make_insert_text(0, 0, "hello"));
    history_undo(h, buf, NULL);
    TEST_ASSERT_TRUE(history_redo(h, buf, NULL));
}

// =============================================================================
// Cursor -- undo restores position
// =============================================================================

void test_undo_insert_char_restores_cursor_position(void) {
    Position after = {0, 1};
    insertChar(&buf->rows[0], 0, 'A');
    history_record(h, make_insert_char(0, 0, 'A'), after);
    cur.pos.row = 0;
    cur.pos.col = 1;
    history_undo(h, buf, &cur);
    TEST_ASSERT_EQUAL_INT(0, cur.pos.row);
    TEST_ASSERT_EQUAL_INT(0, cur.pos.col);
}

void test_undo_delete_char_restores_cursor_position(void) {
    // Establish a prior commit so the parent node carries cursor {0,1}.
    // That is: insert 'A', commit with cursor_after={0,1}, then delete 'B'.
    // Undoing the delete must restore the cursor to {0,1}.
    set_row(buf, 0, "AB");
    Position pre_delete = {0, 1};
    history_begin_group(h, zero_pos());
    history_record(h, make_insert_char(0, 0, 'A'), pre_delete);
    history_end_group(h, pre_delete);   // parent now has cursor_after={0,1}

    // Now record the delete as its own group.
    Position after_delete = {0, 1};
    history_begin_group(h, pre_delete);
    history_record(h, make_delete_char(0, 1, 'B'), after_delete);
    history_end_group(h, after_delete);
    deleteChar(buf, 0, 1);

    cur.pos.row = 0;
    cur.pos.col = 1;
    history_undo(h, buf, &cur);
    // Undo returns parent's cursor_after = {0,1}.
    TEST_ASSERT_EQUAL_INT(0, cur.pos.row);
    TEST_ASSERT_EQUAL_INT(1, cur.pos.col);
}

void test_undo_insert_cr_restores_cursor_to_split_point(void) {
    // Establish a prior commit with cursor_after = {0,3} (the split point).
    set_row(buf, 0, "hello");
    Position pre_cr = {0, 3};
    history_begin_group(h, zero_pos());
    history_record(h, make_insert_char(0, 0, 'h'), pre_cr);
    history_end_group(h, pre_cr);   // parent has cursor_after={0,3}

    // Record the CR as its own group.
    insertCR(buf, 0, 3);
    Position after_cr = {1, 0};
    history_begin_group(h, pre_cr);
    history_record(h, make_insert_cr(0, 3), after_cr);
    history_end_group(h, after_cr);

    cur.pos.row = 1;
    cur.pos.col = 0;
    history_undo(h, buf, &cur);
    // Undo returns parent's cursor_after = {0,3}.
    TEST_ASSERT_EQUAL_INT(0, cur.pos.row);
    TEST_ASSERT_EQUAL_INT(3, cur.pos.col);
}

void test_undo_delete_cr_restores_cursor_to_join_point(void) {
    // Establish a prior commit with cursor_after = {0,3} (the join point).
    set_row(buf, 0, "hel");
    insertCR(buf, 0, 3);
    set_row(buf, 1, "lo");
    Position pre_del_cr = {0, 3};
    history_begin_group(h, zero_pos());
    history_record(h, make_insert_char(0, 0, 'h'), pre_del_cr);
    history_end_group(h, pre_del_cr);   // parent has cursor_after={0,3}

    // Record the delete-CR as its own group.
    Position after_del_cr = {0, 5};
    history_begin_group(h, pre_del_cr);
    history_record(h, make_delete_cr(0, 3), after_del_cr);
    history_end_group(h, after_del_cr);
    deleteCR(buf, 1);

    cur.pos.row = 0;
    cur.pos.col = 5;
    history_undo(h, buf, &cur);
    // Undo returns parent's cursor_after = {0,3}.
    TEST_ASSERT_EQUAL_INT(0, cur.pos.row);
    TEST_ASSERT_EQUAL_INT(3, cur.pos.col);
}

// =============================================================================
// Cursor -- clamping after undo/redo
// =============================================================================

void test_undo_clamps_cursor_when_line_shrinks(void) {
    for (int i = 0; i < 3; i++) {
        insertChar(&buf->rows[0], i, (char)('A' + i));
        record(h, make_insert_char(0, i, (char)('A' + i)));
    }
    cur.pos.col = 3;
    history_undo(h, buf, &cur);
    TEST_ASSERT_TRUE(cur.pos.col <= buf->rows[cur.pos.row].length);
    history_undo(h, buf, &cur);
    TEST_ASSERT_TRUE(cur.pos.col <= buf->rows[cur.pos.row].length);
    history_undo(h, buf, &cur);
    TEST_ASSERT_TRUE(cur.pos.col <= buf->rows[cur.pos.row].length);
}

void test_undo_clamps_cursor_row_when_row_removed(void) {
    set_row(buf, 0, "hi");
    insertCR(buf, 0, 2);
    record(h, make_insert_cr(0, 2));
    cur.pos.row = 1;
    cur.pos.col = 0;
    history_undo(h, buf, &cur);
    TEST_ASSERT_TRUE(cur.pos.row < buf->numrows);
    TEST_ASSERT_TRUE(cur.pos.col <= buf->rows[cur.pos.row].length);
}

// =============================================================================
// Cursor -- desired_col sync
// =============================================================================

void test_undo_syncs_desired_col_to_restored_position(void) {
    Position after = {0, 1};
    insertChar(&buf->rows[0], 0, 'A');
    history_record(h, make_insert_char(0, 0, 'A'), after);
    cur.pos.col     = 1;
    cur.desired_col = 1;
    history_undo(h, buf, &cur);
    TEST_ASSERT_EQUAL_INT(cur.pos.col, cur.desired_col);
}

void test_redo_syncs_desired_col_to_restored_position(void) {
    Position after = {0, 1};
    insertChar(&buf->rows[0], 0, 'A');
    history_record(h, make_insert_char(0, 0, 'A'), after);
    history_undo(h, buf, &cur);
    cur.desired_col = 99;
    history_redo(h, buf, &cur);
    TEST_ASSERT_EQUAL_INT(cur.pos.col, cur.desired_col);
}

// =============================================================================
// Change-group API (history_begin_group / history_end_group)
// =============================================================================

void test_group_multiple_actions_undo_as_one_step(void) {
    // Open a group, push three actions, close it — undo must revert all three.
    Position before = {0, 0};
    Position after  = {0, 3};
    history_begin_group(h, before);
    insertChar(&buf->rows[0], 0, 'a');
    history_record(h, make_insert_char(0, 0, 'a'), (Position){0,1});
    insertChar(&buf->rows[0], 1, 'b');
    history_record(h, make_insert_char(0, 1, 'b'), (Position){0,2});
    insertChar(&buf->rows[0], 2, 'c');
    history_record(h, make_insert_char(0, 2, 'c'), (Position){0,3});
    history_end_group(h, after);

    TEST_ASSERT_EQUAL_STRING("abc", buf->rows[0].line);
    TEST_ASSERT_EQUAL_INT(1, undo_depth(h));

    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_STRING("", buf->rows[0].line);
    TEST_ASSERT_EQUAL_INT(0, undo_depth(h));
}

void test_group_redo_replays_all_actions(void) {
    Position before = {0, 0};
    Position after  = {0, 3};
    history_begin_group(h, before);
    insertChar(&buf->rows[0], 0, 'x');
    history_record(h, make_insert_char(0, 0, 'x'), (Position){0,1});
    insertChar(&buf->rows[0], 1, 'y');
    history_record(h, make_insert_char(0, 1, 'y'), (Position){0,2});
    insertChar(&buf->rows[0], 2, 'z');
    history_record(h, make_insert_char(0, 2, 'z'), (Position){0,3});
    history_end_group(h, after);

    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_STRING("", buf->rows[0].line);

    history_redo(h, buf, NULL);
    TEST_ASSERT_EQUAL_STRING("xyz", buf->rows[0].line);
}

void test_empty_group_is_discarded(void) {
    // An empty group must not create a new undo step.
    history_begin_group(h, zero_pos());
    history_end_group(h, zero_pos());
    TEST_ASSERT_EQUAL_INT(0, undo_depth(h));
    TEST_ASSERT_FALSE(history_undo(h, buf, NULL));
}

void test_two_groups_two_undo_steps(void) {
    // Two separate groups → two separate undo steps.
    history_begin_group(h, zero_pos());
    insertChar(&buf->rows[0], 0, 'A');
    history_record(h, make_insert_char(0, 0, 'A'), (Position){0,1});
    history_end_group(h, (Position){0,1});

    history_begin_group(h, (Position){0,1});
    insertChar(&buf->rows[0], 1, 'B');
    history_record(h, make_insert_char(0, 1, 'B'), (Position){0,2});
    history_end_group(h, (Position){0,2});

    TEST_ASSERT_EQUAL_INT(2, undo_depth(h));
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_STRING("A", buf->rows[0].line);
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_STRING("", buf->rows[0].line);
}

// =============================================================================
// main
// =============================================================================

int main(void) {
    UNITY_BEGIN();

    // new / free
    RUN_TEST(test_new_editor_history_returns_non_null);
    RUN_TEST(test_new_editor_history_tree_not_null);
    RUN_TEST(test_new_editor_history_nothing_to_undo);
    RUN_TEST(test_new_editor_history_nothing_to_redo);
    RUN_TEST(test_free_editor_history_null_does_not_crash);

    // history_record
    RUN_TEST(test_record_makes_one_undo_step_available);
    RUN_TEST(test_record_multiple_makes_multiple_undo_steps);
    RUN_TEST(test_record_after_undo_creates_new_branch);
    RUN_TEST(test_record_does_not_crash_on_null_history);

    // undo return values
    RUN_TEST(test_undo_returns_false_when_nothing_to_undo);
    RUN_TEST(test_undo_returns_false_on_null_history);
    RUN_TEST(test_undo_returns_false_on_null_buffer);
    RUN_TEST(test_undo_returns_true_when_action_present);

    // redo return values
    RUN_TEST(test_redo_returns_false_when_nothing_to_redo);
    RUN_TEST(test_redo_returns_false_on_null_history);
    RUN_TEST(test_redo_returns_false_on_null_buffer);

    // INSERT_CHAR
    RUN_TEST(test_undo_insert_char_removes_character);
    RUN_TEST(test_undo_insert_char_decrements_undo_depth);
    RUN_TEST(test_undo_insert_char_makes_redo_available);
    RUN_TEST(test_redo_insert_char_restores_character);
    RUN_TEST(test_redo_insert_char_restores_undo_depth);
    RUN_TEST(test_undo_redo_insert_char_multiple_times);

    // DELETE_CHAR
    RUN_TEST(test_undo_delete_char_restores_character);
    RUN_TEST(test_undo_delete_char_makes_redo_available);
    RUN_TEST(test_redo_delete_char_removes_character_again);
    RUN_TEST(test_undo_redo_delete_char_cycle);

    // INSERT_CR
    RUN_TEST(test_undo_insert_cr_merges_rows);
    RUN_TEST(test_undo_insert_cr_makes_redo_available);
    RUN_TEST(test_redo_insert_cr_splits_row_again);
    RUN_TEST(test_undo_redo_insert_cr_cycle);

    // DELETE_CR
    RUN_TEST(test_undo_delete_cr_splits_rows);
    RUN_TEST(test_undo_delete_cr_makes_redo_available);
    RUN_TEST(test_redo_delete_cr_merges_rows_again);

    // INSERT_TEXT undo/redo
    RUN_TEST(test_undo_insert_text_removes_single_line);
    RUN_TEST(test_undo_insert_text_removes_multiline);
    RUN_TEST(test_undo_insert_text_decrements_undo_depth);
    RUN_TEST(test_undo_insert_text_makes_redo_available);
    RUN_TEST(test_redo_insert_text_restores_single_line);
    RUN_TEST(test_redo_insert_text_restores_multiline);
    RUN_TEST(test_redo_insert_text_restores_undo_depth);
    RUN_TEST(test_undo_redo_insert_text_cycle_multiple_times);
    RUN_TEST(test_undo_insert_text_mid_line_restores_context);
    RUN_TEST(test_undo_insert_text_preserves_surrounding_rows);
    RUN_TEST(test_undo_insert_text_with_trailing_newline);
    RUN_TEST(test_insert_text_record_after_undo_creates_new_branch);

    // INSERT_TEXT cursor
    RUN_TEST(test_undo_insert_text_restores_cursor_to_start);
    RUN_TEST(test_undo_insert_text_multiline_restores_cursor_to_start);
    RUN_TEST(test_redo_insert_text_restores_cursor_to_recorded_after_position);
    RUN_TEST(test_undo_insert_text_syncs_desired_col);

    // Mixed sequences
    RUN_TEST(test_undo_sequence_restores_in_reverse_order);
    RUN_TEST(test_new_edit_after_undo_creates_branch);
    RUN_TEST(test_redo_follows_most_recent_branch);
    RUN_TEST(test_redo_is_unavailable_after_new_edit_on_fresh_history);
    RUN_TEST(test_full_undo_redo_cycle_mixed_actions);
    RUN_TEST(test_mixed_insert_text_and_insert_char_undo_order);
    RUN_TEST(test_undo_beyond_history_returns_false);
    RUN_TEST(test_redo_beyond_history_returns_false);
    RUN_TEST(test_alternating_undo_redo_is_stable);
    RUN_TEST(test_alternating_undo_redo_insert_text_is_stable);

    // Depth invariants
    RUN_TEST(test_undo_reduces_undo_depth_by_one);
    RUN_TEST(test_redo_reduces_redo_depth_by_one);
    RUN_TEST(test_undo_all_then_redo_all_depths);

    // Cursor -- NULL safety
    RUN_TEST(test_undo_with_null_cursor_does_not_crash);
    RUN_TEST(test_redo_with_null_cursor_does_not_crash);
    RUN_TEST(test_undo_insert_text_null_cursor_does_not_crash);
    RUN_TEST(test_redo_insert_text_null_cursor_does_not_crash);

    // Cursor -- undo restores position
    RUN_TEST(test_undo_insert_char_restores_cursor_position);
    RUN_TEST(test_undo_delete_char_restores_cursor_position);
    RUN_TEST(test_undo_insert_cr_restores_cursor_to_split_point);
    RUN_TEST(test_undo_delete_cr_restores_cursor_to_join_point);

    // Cursor -- clamping
    RUN_TEST(test_undo_clamps_cursor_when_line_shrinks);
    RUN_TEST(test_undo_clamps_cursor_row_when_row_removed);

    // Cursor -- desired_col sync
    RUN_TEST(test_undo_syncs_desired_col_to_restored_position);
    RUN_TEST(test_redo_syncs_desired_col_to_restored_position);

    // Change-group API
    RUN_TEST(test_group_multiple_actions_undo_as_one_step);
    RUN_TEST(test_group_redo_replays_all_actions);
    RUN_TEST(test_empty_group_is_discarded);
    RUN_TEST(test_two_groups_two_undo_steps);

    return UNITY_END();
}