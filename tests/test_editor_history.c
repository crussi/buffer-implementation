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
// Helpers
// =============================================================================

static Action make_insert_char(int row, int col, char c) {
    Action a;
    a.type         = INSERT_CHAR;
    a.position.row = row;
    a.position.col = col;
    a.character    = c;
    a.text         = NULL;
    return a;
}

static Action make_delete_char(int row, int col, char c) {
    Action a;
    a.type         = DELETE_CHAR;
    a.position.row = row;
    a.position.col = col;
    a.character    = c;
    a.text         = NULL;
    return a;
}

static Action make_insert_cr(int row, int col) {
    Action a;
    a.type         = INSERT_CR;
    a.position.row = row;
    a.position.col = col;
    a.character    = '\0';
    a.text         = NULL;
    return a;
}

static Action make_delete_cr(int row, int col) {
    Action a;
    a.type         = DELETE_CR;
    a.position.row = row;
    a.position.col = col;
    a.character    = '\0';
    a.text         = NULL;
    return a;
}

// Build an INSERT_TEXT action whose text payload is heap-allocated.
// The stack takes ownership when pushed via history_record.
static Action make_insert_text(int row, int col, const char *text) {
    Action a;
    a.type         = INSERT_TEXT;
    a.position.row = row;
    a.position.col = col;
    a.character    = '\0';
    a.text         = text ? strdup(text) : NULL;
    return a;
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

void test_new_editor_history_undo_stack_is_empty(void) {
    TEST_ASSERT_TRUE(action_stack_is_empty(h->undo_stack));
}

void test_new_editor_history_redo_stack_is_empty(void) {
    TEST_ASSERT_TRUE(action_stack_is_empty(h->redo_stack));
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

void test_record_pushes_onto_undo_stack(void) {
    Action a = make_insert_char(0, 0, 'x');
    history_record(h, a);
    TEST_ASSERT_EQUAL_size_t(1, h->undo_stack->size);
}

void test_record_multiple_grows_undo_stack(void) {
    for (int i = 0; i < 5; i++) {
        Action a = make_insert_char(0, i, (char)('a' + i));
        history_record(h, a);
    }
    TEST_ASSERT_EQUAL_size_t(5, h->undo_stack->size);
}

void test_record_clears_redo_stack(void) {
    Action a = make_insert_char(0, 0, 'x');
    push_action(h->redo_stack, a);
    TEST_ASSERT_EQUAL_size_t(1, h->redo_stack->size);
    Action b = make_insert_char(0, 1, 'y');
    history_record(h, b);
    TEST_ASSERT_EQUAL_size_t(0, h->redo_stack->size);
}

void test_record_does_not_crash_on_null_history(void) {
    Action a = make_insert_char(0, 0, 'x');
    history_record(NULL, a);
}

// =============================================================================
// history_undo — return value
// =============================================================================

void test_undo_returns_false_on_empty_undo_stack(void) {
    TEST_ASSERT_FALSE(history_undo(h, buf, NULL));
}

void test_undo_returns_false_on_null_history(void) {
    TEST_ASSERT_FALSE(history_undo(NULL, buf, NULL));
}

void test_undo_returns_false_on_null_buffer(void) {
    Action a = make_insert_char(0, 0, 'x');
    history_record(h, a);
    TEST_ASSERT_FALSE(history_undo(h, NULL, NULL));
}

void test_undo_returns_true_when_action_present(void) {
    set_row(buf, 0, "a");
    Action a = make_insert_char(0, 0, 'a');
    history_record(h, a);
    TEST_ASSERT_TRUE(history_undo(h, buf, NULL));
}

// =============================================================================
// history_redo — return value
// =============================================================================

void test_redo_returns_false_on_empty_redo_stack(void) {
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
    history_record(h, make_insert_char(0, 0, 'h'));
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(0, buf->rows[0].length);
    TEST_ASSERT_EQUAL_CHAR('\0', buf->rows[0].line[0]);
}

void test_undo_insert_char_decrements_undo_stack(void) {
    insertChar(&buf->rows[0], 0, 'h');
    history_record(h, make_insert_char(0, 0, 'h'));
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_size_t(0, h->undo_stack->size);
}

void test_undo_insert_char_pushes_onto_redo_stack(void) {
    insertChar(&buf->rows[0], 0, 'h');
    history_record(h, make_insert_char(0, 0, 'h'));
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_size_t(1, h->redo_stack->size);
}

void test_redo_insert_char_restores_character(void) {
    insertChar(&buf->rows[0], 0, 'h');
    history_record(h, make_insert_char(0, 0, 'h'));
    history_undo(h, buf, NULL);
    history_redo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(1, buf->rows[0].length);
    TEST_ASSERT_EQUAL_CHAR('h', buf->rows[0].line[0]);
}

void test_redo_insert_char_pushes_back_onto_undo_stack(void) {
    insertChar(&buf->rows[0], 0, 'h');
    history_record(h, make_insert_char(0, 0, 'h'));
    history_undo(h, buf, NULL);
    history_redo(h, buf, NULL);
    TEST_ASSERT_EQUAL_size_t(1, h->undo_stack->size);
    TEST_ASSERT_EQUAL_size_t(0, h->redo_stack->size);
}

void test_undo_redo_insert_char_multiple_times(void) {
    insertChar(&buf->rows[0], 0, 'h');
    history_record(h, make_insert_char(0, 0, 'h'));
    insertChar(&buf->rows[0], 1, 'i');
    history_record(h, make_insert_char(0, 1, 'i'));
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
    history_record(h, make_delete_char(0, 0, 'a'));
    deleteChar(buf, 0, 0);
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(2, buf->rows[0].length);
    TEST_ASSERT_EQUAL_CHAR('a', buf->rows[0].line[0]);
    TEST_ASSERT_EQUAL_CHAR('b', buf->rows[0].line[1]);
}

void test_undo_delete_char_pushes_onto_redo_stack(void) {
    set_row(buf, 0, "ab");
    history_record(h, make_delete_char(0, 0, 'a'));
    deleteChar(buf, 0, 0);
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_size_t(1, h->redo_stack->size);
}

void test_redo_delete_char_removes_character_again(void) {
    set_row(buf, 0, "ab");
    history_record(h, make_delete_char(0, 0, 'a'));
    deleteChar(buf, 0, 0);
    history_undo(h, buf, NULL);
    history_redo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(1, buf->rows[0].length);
    TEST_ASSERT_EQUAL_CHAR('b', buf->rows[0].line[0]);
}

void test_undo_redo_delete_char_cycle(void) {
    set_row(buf, 0, "abc");
    history_record(h, make_delete_char(0, 1, 'b'));
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
    history_record(h, make_insert_cr(0, 3));
    TEST_ASSERT_EQUAL_INT(2, buf->numrows);
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(1, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("hello", buf->rows[0].line);
}

void test_undo_insert_cr_pushes_onto_redo_stack(void) {
    set_row(buf, 0, "hello");
    insertCR(buf, 0, 3);
    history_record(h, make_insert_cr(0, 3));
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_size_t(1, h->redo_stack->size);
}

void test_redo_insert_cr_splits_row_again(void) {
    set_row(buf, 0, "hello");
    insertCR(buf, 0, 3);
    history_record(h, make_insert_cr(0, 3));
    history_undo(h, buf, NULL);
    history_redo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(2, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("hel", buf->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("lo",  buf->rows[1].line);
}

void test_undo_redo_insert_cr_cycle(void) {
    set_row(buf, 0, "abcd");
    insertCR(buf, 0, 2);
    history_record(h, make_insert_cr(0, 2));
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
    history_record(h, make_delete_cr(0, 3));
    deleteCR(buf, 1);
    TEST_ASSERT_EQUAL_INT(1, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("hello", buf->rows[0].line);
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(2, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("hel", buf->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("lo",  buf->rows[1].line);
}

void test_undo_delete_cr_pushes_onto_redo_stack(void) {
    set_row(buf, 0, "hel");
    insertCR(buf, 0, 3);
    set_row(buf, 1, "lo");
    history_record(h, make_delete_cr(0, 3));
    deleteCR(buf, 1);
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_size_t(1, h->redo_stack->size);
}

void test_redo_delete_cr_merges_rows_again(void) {
    set_row(buf, 0, "hel");
    insertCR(buf, 0, 3);
    set_row(buf, 1, "lo");
    history_record(h, make_delete_cr(0, 3));
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
    // Manually insert "hello" into the buffer, record INSERT_TEXT, then undo.
    insertText(buf, 0, 0, "hello");
    history_record(h, make_insert_text(0, 0, "hello"));
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(1, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("", buf->rows[0].line);
}

void test_undo_insert_text_removes_multiline(void) {
    insertText(buf, 0, 0, "foo\nbar");
    history_record(h, make_insert_text(0, 0, "foo\nbar"));
    TEST_ASSERT_EQUAL_INT(2, buf->numrows);
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(1, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("", buf->rows[0].line);
}

void test_undo_insert_text_decrements_undo_stack(void) {
    insertText(buf, 0, 0, "hello");
    history_record(h, make_insert_text(0, 0, "hello"));
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_size_t(0, h->undo_stack->size);
}

void test_undo_insert_text_pushes_onto_redo_stack(void) {
    insertText(buf, 0, 0, "hello");
    history_record(h, make_insert_text(0, 0, "hello"));
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_size_t(1, h->redo_stack->size);
}

void test_redo_insert_text_restores_single_line(void) {
    insertText(buf, 0, 0, "hello");
    history_record(h, make_insert_text(0, 0, "hello"));
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_STRING("", buf->rows[0].line);
    history_redo(h, buf, NULL);
    TEST_ASSERT_EQUAL_STRING("hello", buf->rows[0].line);
}

void test_redo_insert_text_restores_multiline(void) {
    insertText(buf, 0, 0, "foo\nbar");
    history_record(h, make_insert_text(0, 0, "foo\nbar"));
    history_undo(h, buf, NULL);
    history_redo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(2, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("foo", buf->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("bar", buf->rows[1].line);
}

void test_redo_insert_text_pushes_back_onto_undo_stack(void) {
    insertText(buf, 0, 0, "hello");
    history_record(h, make_insert_text(0, 0, "hello"));
    history_undo(h, buf, NULL);
    history_redo(h, buf, NULL);
    TEST_ASSERT_EQUAL_size_t(1, h->undo_stack->size);
    TEST_ASSERT_EQUAL_size_t(0, h->redo_stack->size);
}

void test_undo_redo_insert_text_cycle_multiple_times(void) {
    insertText(buf, 0, 0, "abc\ndef");
    history_record(h, make_insert_text(0, 0, "abc\ndef"));

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
    // Start with "XZ", insert "Y" at col 1 -> "XYZ", undo -> "XZ"
    set_row(buf, 0, "XZ");
    insertText(buf, 0, 1, "Y");
    history_record(h, make_insert_text(0, 1, "Y"));
    TEST_ASSERT_EQUAL_STRING("XYZ", buf->rows[0].line);
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_STRING("XZ", buf->rows[0].line);
}

void test_undo_insert_text_preserves_surrounding_rows(void) {
    // Buffer: ["before", "", "after"]
    // Insert "hello\nworld" at row 1, col 0 → ["before","hello","world","after"]
    // Undo → ["before", "", "after"]
    insertText(buf, 0, 0, "before");
    insertCR(buf, 0, 6);
    insertCR(buf, 1, 0);
    insertText(buf, 2, 0, "after");
    // Now insert the bulk text at row 1
    insertText(buf, 1, 0, "hello\nworld");
    history_record(h, make_insert_text(1, 0, "hello\nworld"));
    TEST_ASSERT_EQUAL_INT(4, buf->numrows);
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(3, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("before", buf->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("",       buf->rows[1].line);
    TEST_ASSERT_EQUAL_STRING("after",  buf->rows[2].line);
}

void test_undo_insert_text_with_trailing_newline(void) {
    insertText(buf, 0, 0, "hi\n");
    history_record(h, make_insert_text(0, 0, "hi\n"));
    TEST_ASSERT_EQUAL_INT(2, buf->numrows);
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(1, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("", buf->rows[0].line);
}

void test_insert_text_record_clears_redo(void) {
    // Do an insert, undo it so redo stack is populated, then record a new
    // INSERT_TEXT action — redo stack must be cleared.
    insertChar(&buf->rows[0], 0, 'A');
    history_record(h, make_insert_char(0, 0, 'A'));
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_size_t(1, h->redo_stack->size);

    insertText(buf, 0, 0, "new");
    history_record(h, make_insert_text(0, 0, "new"));
    TEST_ASSERT_EQUAL_size_t(0, h->redo_stack->size);
}

// =============================================================================
// INSERT_TEXT cursor tests
// =============================================================================

void test_undo_insert_text_restores_cursor_to_start(void) {
    insertText(buf, 0, 0, "hello");
    history_record(h, make_insert_text(0, 0, "hello"));
    cur.pos.row = 0;
    cur.pos.col = 5;
    history_undo(h, buf, &cur);
    TEST_ASSERT_EQUAL_INT(0, cur.pos.row);
    TEST_ASSERT_EQUAL_INT(0, cur.pos.col);
}

void test_undo_insert_text_multiline_restores_cursor_to_start(void) {
    insertText(buf, 0, 0, "foo\nbar");
    history_record(h, make_insert_text(0, 0, "foo\nbar"));
    cur.pos.row = 1;
    cur.pos.col = 3;
    history_undo(h, buf, &cur);
    TEST_ASSERT_EQUAL_INT(0, cur.pos.row);
    TEST_ASSERT_EQUAL_INT(0, cur.pos.col);
}

void test_redo_insert_text_restores_cursor_to_start_position(void) {
    insertText(buf, 0, 0, "hello");
    history_record(h, make_insert_text(0, 0, "hello"));
    history_undo(h, buf, &cur);
    cur.pos.row = 99;
    cur.pos.col = 99;
    history_redo(h, buf, &cur);
    // redo snaps cursor back to the recorded action position (0,0)
    TEST_ASSERT_EQUAL_INT(0, cur.pos.row);
    TEST_ASSERT_EQUAL_INT(0, cur.pos.col);
}

void test_undo_insert_text_syncs_desired_col(void) {
    insertText(buf, 0, 0, "hello");
    history_record(h, make_insert_text(0, 0, "hello"));
    cur.pos.col = 5;
    cur.desired_col = 5;
    history_undo(h, buf, &cur);
    TEST_ASSERT_EQUAL_INT(cur.pos.col, cur.desired_col);
}

// =============================================================================
// Mixed action sequences (now including INSERT_TEXT)
// =============================================================================

void test_undo_sequence_restores_in_reverse_order(void) {
    insertChar(&buf->rows[0], 0, 'h');
    history_record(h, make_insert_char(0, 0, 'h'));
    insertChar(&buf->rows[0], 1, 'i');
    history_record(h, make_insert_char(0, 1, 'i'));
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(1, buf->rows[0].length);
    TEST_ASSERT_EQUAL_CHAR('h', buf->rows[0].line[0]);
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_INT(0, buf->rows[0].length);
}

void test_new_edit_after_undo_clears_redo(void) {
    insertChar(&buf->rows[0], 0, 'a');
    history_record(h, make_insert_char(0, 0, 'a'));
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_size_t(1, h->redo_stack->size);
    insertChar(&buf->rows[0], 0, 'b');
    history_record(h, make_insert_char(0, 0, 'b'));
    TEST_ASSERT_EQUAL_size_t(0, h->redo_stack->size);
}

void test_redo_is_unavailable_after_new_edit(void) {
    insertChar(&buf->rows[0], 0, 'a');
    history_record(h, make_insert_char(0, 0, 'a'));
    history_undo(h, buf, NULL);
    insertChar(&buf->rows[0], 0, 'b');
    history_record(h, make_insert_char(0, 0, 'b'));
    TEST_ASSERT_FALSE(history_redo(h, buf, NULL));
}

void test_full_undo_redo_cycle_mixed_actions(void) {
    insertChar(&buf->rows[0], 0, 'a');
    history_record(h, make_insert_char(0, 0, 'a'));
    insertChar(&buf->rows[0], 1, 'b');
    history_record(h, make_insert_char(0, 1, 'b'));
    insertCR(buf, 0, 2);
    history_record(h, make_insert_cr(0, 2));
    insertChar(&buf->rows[1], 0, 'c');
    history_record(h, make_insert_char(1, 0, 'c'));

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
    // Record: INSERT_TEXT "hi", then INSERT_CHAR 'X'
    // Undo in reverse: 'X' removed first, then "hi"
    insertText(buf, 0, 0, "hi");
    history_record(h, make_insert_text(0, 0, "hi"));
    insertChar(&buf->rows[0], 2, 'X');
    history_record(h, make_insert_char(0, 2, 'X'));

    TEST_ASSERT_EQUAL_STRING("hiX", buf->rows[0].line);

    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_STRING("hi", buf->rows[0].line);

    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_STRING("", buf->rows[0].line);
}

void test_undo_beyond_history_returns_false(void) {
    insertChar(&buf->rows[0], 0, 'x');
    history_record(h, make_insert_char(0, 0, 'x'));
    history_undo(h, buf, NULL);
    TEST_ASSERT_FALSE(history_undo(h, buf, NULL));
    TEST_ASSERT_FALSE(history_undo(h, buf, NULL));
}

void test_redo_beyond_history_returns_false(void) {
    insertChar(&buf->rows[0], 0, 'x');
    history_record(h, make_insert_char(0, 0, 'x'));
    history_undo(h, buf, NULL);
    history_redo(h, buf, NULL);
    TEST_ASSERT_FALSE(history_redo(h, buf, NULL));
    TEST_ASSERT_FALSE(history_redo(h, buf, NULL));
}

void test_alternating_undo_redo_is_stable(void) {
    insertChar(&buf->rows[0], 0, 'z');
    history_record(h, make_insert_char(0, 0, 'z'));
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
    history_record(h, make_insert_text(0, 0, "hello\nworld"));
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
// Stack state invariants
// =============================================================================

void test_undo_reduces_undo_stack_by_one(void) {
    for (int i = 0; i < 3; i++) {
        Action a = make_insert_char(0, i, (char)('a' + i));
        insertChar(&buf->rows[0], i, (char)('a' + i));
        history_record(h, a);
    }
    TEST_ASSERT_EQUAL_size_t(3, h->undo_stack->size);
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_size_t(2, h->undo_stack->size);
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_size_t(1, h->undo_stack->size);
}

void test_redo_reduces_redo_stack_by_one(void) {
    for (int i = 0; i < 3; i++) {
        Action a = make_insert_char(0, i, (char)('a' + i));
        insertChar(&buf->rows[0], i, (char)('a' + i));
        history_record(h, a);
    }
    history_undo(h, buf, NULL);
    history_undo(h, buf, NULL);
    history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_size_t(3, h->redo_stack->size);
    history_redo(h, buf, NULL);
    TEST_ASSERT_EQUAL_size_t(2, h->redo_stack->size);
    history_redo(h, buf, NULL);
    TEST_ASSERT_EQUAL_size_t(1, h->redo_stack->size);
}

void test_undo_all_then_redo_all_stack_sizes(void) {
    for (int i = 0; i < 4; i++) {
        insertChar(&buf->rows[0], i, (char)('a' + i));
        history_record(h, make_insert_char(0, i, (char)('a' + i)));
    }
    for (int i = 0; i < 4; i++)
        history_undo(h, buf, NULL);
    TEST_ASSERT_EQUAL_size_t(0, h->undo_stack->size);
    TEST_ASSERT_EQUAL_size_t(4, h->redo_stack->size);
    for (int i = 0; i < 4; i++)
        history_redo(h, buf, NULL);
    TEST_ASSERT_EQUAL_size_t(4, h->undo_stack->size);
    TEST_ASSERT_EQUAL_size_t(0, h->redo_stack->size);
}

// =============================================================================
// Cursor -- NULL safety
// =============================================================================

void test_undo_with_null_cursor_does_not_crash(void) {
    insertChar(&buf->rows[0], 0, 'x');
    history_record(h, make_insert_char(0, 0, 'x'));
    TEST_ASSERT_TRUE(history_undo(h, buf, NULL));
}

void test_redo_with_null_cursor_does_not_crash(void) {
    insertChar(&buf->rows[0], 0, 'x');
    history_record(h, make_insert_char(0, 0, 'x'));
    history_undo(h, buf, NULL);
    TEST_ASSERT_TRUE(history_redo(h, buf, NULL));
}

void test_undo_insert_text_null_cursor_does_not_crash(void) {
    insertText(buf, 0, 0, "hello");
    history_record(h, make_insert_text(0, 0, "hello"));
    TEST_ASSERT_TRUE(history_undo(h, buf, NULL));
}

void test_redo_insert_text_null_cursor_does_not_crash(void) {
    insertText(buf, 0, 0, "hello");
    history_record(h, make_insert_text(0, 0, "hello"));
    history_undo(h, buf, NULL);
    TEST_ASSERT_TRUE(history_redo(h, buf, NULL));
}

// =============================================================================
// Cursor -- undo restores position (existing action types)
// =============================================================================

void test_undo_insert_char_restores_cursor_position(void) {
    insertChar(&buf->rows[0], 0, 'A');
    history_record(h, make_insert_char(0, 0, 'A'));
    cur.pos.row = 0;
    cur.pos.col = 1;
    history_undo(h, buf, &cur);
    TEST_ASSERT_EQUAL_INT(0, cur.pos.row);
    TEST_ASSERT_EQUAL_INT(0, cur.pos.col);
}

void test_undo_delete_char_restores_cursor_position(void) {
    set_row(buf, 0, "AB");
    history_record(h, make_delete_char(0, 1, 'B'));
    deleteChar(buf, 0, 1);
    cur.pos.row = 0;
    cur.pos.col = 1;
    history_undo(h, buf, &cur);
    TEST_ASSERT_EQUAL_INT(0, cur.pos.row);
    TEST_ASSERT_EQUAL_INT(1, cur.pos.col);
}

void test_undo_insert_cr_restores_cursor_to_split_point(void) {
    set_row(buf, 0, "hello");
    insertCR(buf, 0, 3);
    history_record(h, make_insert_cr(0, 3));
    cur.pos.row = 1;
    cur.pos.col = 0;
    history_undo(h, buf, &cur);
    TEST_ASSERT_EQUAL_INT(0, cur.pos.row);
    TEST_ASSERT_EQUAL_INT(3, cur.pos.col);
}

void test_undo_delete_cr_restores_cursor_to_join_point(void) {
    set_row(buf, 0, "hel");
    insertCR(buf, 0, 3);
    set_row(buf, 1, "lo");
    history_record(h, make_delete_cr(0, 3));
    deleteCR(buf, 1);
    cur.pos.row = 0;
    cur.pos.col = 5;
    history_undo(h, buf, &cur);
    TEST_ASSERT_EQUAL_INT(0, cur.pos.row);
    TEST_ASSERT_EQUAL_INT(3, cur.pos.col);
}

// =============================================================================
// Cursor -- clamping after undo/redo
// =============================================================================

void test_undo_clamps_cursor_when_line_shrinks(void) {
    insertChar(&buf->rows[0], 0, 'A');
    history_record(h, make_insert_char(0, 0, 'A'));
    insertChar(&buf->rows[0], 1, 'B');
    history_record(h, make_insert_char(0, 1, 'B'));
    insertChar(&buf->rows[0], 2, 'C');
    history_record(h, make_insert_char(0, 2, 'C'));
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
    history_record(h, make_insert_cr(0, 2));
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
    insertChar(&buf->rows[0], 0, 'A');
    history_record(h, make_insert_char(0, 0, 'A'));
    cur.pos.col     = 1;
    cur.desired_col = 1;
    history_undo(h, buf, &cur);
    TEST_ASSERT_EQUAL_INT(cur.pos.col, cur.desired_col);
}

void test_redo_syncs_desired_col_to_restored_position(void) {
    insertChar(&buf->rows[0], 0, 'A');
    history_record(h, make_insert_char(0, 0, 'A'));
    history_undo(h, buf, &cur);
    cur.desired_col = 99;
    history_redo(h, buf, &cur);
    TEST_ASSERT_EQUAL_INT(cur.pos.col, cur.desired_col);
}

// =============================================================================
// main
// =============================================================================

int main(void) {
    UNITY_BEGIN();

    // new / free
    RUN_TEST(test_new_editor_history_returns_non_null);
    RUN_TEST(test_new_editor_history_undo_stack_is_empty);
    RUN_TEST(test_new_editor_history_redo_stack_is_empty);
    RUN_TEST(test_free_editor_history_null_does_not_crash);

    // history_record
    RUN_TEST(test_record_pushes_onto_undo_stack);
    RUN_TEST(test_record_multiple_grows_undo_stack);
    RUN_TEST(test_record_clears_redo_stack);
    RUN_TEST(test_record_does_not_crash_on_null_history);

    // undo return values
    RUN_TEST(test_undo_returns_false_on_empty_undo_stack);
    RUN_TEST(test_undo_returns_false_on_null_history);
    RUN_TEST(test_undo_returns_false_on_null_buffer);
    RUN_TEST(test_undo_returns_true_when_action_present);

    // redo return values
    RUN_TEST(test_redo_returns_false_on_empty_redo_stack);
    RUN_TEST(test_redo_returns_false_on_null_history);
    RUN_TEST(test_redo_returns_false_on_null_buffer);

    // INSERT_CHAR
    RUN_TEST(test_undo_insert_char_removes_character);
    RUN_TEST(test_undo_insert_char_decrements_undo_stack);
    RUN_TEST(test_undo_insert_char_pushes_onto_redo_stack);
    RUN_TEST(test_redo_insert_char_restores_character);
    RUN_TEST(test_redo_insert_char_pushes_back_onto_undo_stack);
    RUN_TEST(test_undo_redo_insert_char_multiple_times);

    // DELETE_CHAR
    RUN_TEST(test_undo_delete_char_restores_character);
    RUN_TEST(test_undo_delete_char_pushes_onto_redo_stack);
    RUN_TEST(test_redo_delete_char_removes_character_again);
    RUN_TEST(test_undo_redo_delete_char_cycle);

    // INSERT_CR
    RUN_TEST(test_undo_insert_cr_merges_rows);
    RUN_TEST(test_undo_insert_cr_pushes_onto_redo_stack);
    RUN_TEST(test_redo_insert_cr_splits_row_again);
    RUN_TEST(test_undo_redo_insert_cr_cycle);

    // DELETE_CR
    RUN_TEST(test_undo_delete_cr_splits_rows);
    RUN_TEST(test_undo_delete_cr_pushes_onto_redo_stack);
    RUN_TEST(test_redo_delete_cr_merges_rows_again);

    // INSERT_TEXT undo/redo
    RUN_TEST(test_undo_insert_text_removes_single_line);
    RUN_TEST(test_undo_insert_text_removes_multiline);
    RUN_TEST(test_undo_insert_text_decrements_undo_stack);
    RUN_TEST(test_undo_insert_text_pushes_onto_redo_stack);
    RUN_TEST(test_redo_insert_text_restores_single_line);
    RUN_TEST(test_redo_insert_text_restores_multiline);
    RUN_TEST(test_redo_insert_text_pushes_back_onto_undo_stack);
    RUN_TEST(test_undo_redo_insert_text_cycle_multiple_times);
    RUN_TEST(test_undo_insert_text_mid_line_restores_context);
    RUN_TEST(test_undo_insert_text_preserves_surrounding_rows);
    RUN_TEST(test_undo_insert_text_with_trailing_newline);
    RUN_TEST(test_insert_text_record_clears_redo);

    // INSERT_TEXT cursor
    RUN_TEST(test_undo_insert_text_restores_cursor_to_start);
    RUN_TEST(test_undo_insert_text_multiline_restores_cursor_to_start);
    RUN_TEST(test_redo_insert_text_restores_cursor_to_start_position);
    RUN_TEST(test_undo_insert_text_syncs_desired_col);

    // Mixed sequences
    RUN_TEST(test_undo_sequence_restores_in_reverse_order);
    RUN_TEST(test_new_edit_after_undo_clears_redo);
    RUN_TEST(test_redo_is_unavailable_after_new_edit);
    RUN_TEST(test_full_undo_redo_cycle_mixed_actions);
    RUN_TEST(test_mixed_insert_text_and_insert_char_undo_order);
    RUN_TEST(test_undo_beyond_history_returns_false);
    RUN_TEST(test_redo_beyond_history_returns_false);
    RUN_TEST(test_alternating_undo_redo_is_stable);
    RUN_TEST(test_alternating_undo_redo_insert_text_is_stable);

    // Stack state invariants
    RUN_TEST(test_undo_reduces_undo_stack_by_one);
    RUN_TEST(test_redo_reduces_redo_stack_by_one);
    RUN_TEST(test_undo_all_then_redo_all_stack_sizes);

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

    return UNITY_END();
}