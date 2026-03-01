#include "unity.h"
#include "editor.h"
#include "buffer.h"
#include "editor_history.h"
#include <string.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Build an Editor whose row 0 contains the given string.
static Editor *make_editor_with_text(const char *text) {
    Editor *e = editor_new_empty();
    if (!e) return NULL;
    for (int i = 0; text[i] != '\0'; i++)
        editorInsertChar(e, 0, i, text[i]);
    return e;
}

// Convenience: return a copy of row `r`'s text (caller must NOT free — valid
// until the next buffer mutation).
static const char *row_text(Editor *e, int r) {
    return e->buf->rows[r].line;
}

// ---------------------------------------------------------------------------
// setUp / tearDown
// ---------------------------------------------------------------------------

void setUp(void)    {}
void tearDown(void) {}

// ---------------------------------------------------------------------------
// editor_new_empty
// ---------------------------------------------------------------------------

void test_editor_new_empty_not_null(void) {
    Editor *e = editor_new_empty();
    TEST_ASSERT_NOT_NULL(e);
    editor_free(e);
}

void test_editor_new_empty_has_buf_and_history(void) {
    Editor *e = editor_new_empty();
    TEST_ASSERT_NOT_NULL(e->buf);
    TEST_ASSERT_NOT_NULL(e->history);
    editor_free(e);
}

void test_editor_new_empty_one_empty_row(void) {
    Editor *e = editor_new_empty();
    TEST_ASSERT_EQUAL_INT(1, e->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("", e->buf->rows[0].line);
    editor_free(e);
}

// ---------------------------------------------------------------------------
// editor_new_from_file
// ---------------------------------------------------------------------------

void test_editor_new_from_file_reads_content(void) {
    FILE *f = tmpfile();
    fputs("hello\nworld\n", f);
    rewind(f);

    Editor *e = editor_new_from_file(f);
    fclose(f);

    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_INT(2, e->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("hello", row_text(e, 0));
    TEST_ASSERT_EQUAL_STRING("world", row_text(e, 1));
    editor_free(e);
}

void test_editor_new_from_file_null_returns_null(void) {
    // fileToBuf returns NULL for NULL file; editor_new_from_file should
    // propagate that and clean up.
    Editor *e = editor_new_from_file(NULL);
    TEST_ASSERT_NULL(e);
}

// ---------------------------------------------------------------------------
// editor_free
// ---------------------------------------------------------------------------

void test_editor_free_null_does_not_crash(void) {
    editor_free(NULL); // must be a safe no-op
}

// ---------------------------------------------------------------------------
// editorInsertChar
// ---------------------------------------------------------------------------

void test_editorInsertChar_appends_single(void) {
    Editor *e = editor_new_empty();
    editorInsertChar(e, 0, 0, 'X');
    TEST_ASSERT_EQUAL_STRING("X", row_text(e, 0));
    editor_free(e);
}

void test_editorInsertChar_builds_word(void) {
    Editor *e = editor_new_empty();
    const char *word = "Hello";
    for (int i = 0; word[i]; i++)
        editorInsertChar(e, 0, i, word[i]);
    TEST_ASSERT_EQUAL_STRING("Hello", row_text(e, 0));
    editor_free(e);
}

void test_editorInsertChar_inserts_in_middle(void) {
    Editor *e = make_editor_with_text("AC");
    editorInsertChar(e, 0, 1, 'B');
    TEST_ASSERT_EQUAL_STRING("ABC", row_text(e, 0));
    editor_free(e);
}

void test_editorInsertChar_records_action(void) {
    Editor *e = editor_new_empty();
    editorInsertChar(e, 0, 0, 'Z');
    TEST_ASSERT_FALSE(action_stack_is_empty(e->history->undo_stack));
    editor_free(e);
}

// ---------------------------------------------------------------------------
// editorDeleteChar
// ---------------------------------------------------------------------------

void test_editorDeleteChar_removes_char(void) {
    Editor *e = make_editor_with_text("ABC");
    editorDeleteChar(e, 0, 1); // remove 'B'
    TEST_ASSERT_EQUAL_STRING("AC", row_text(e, 0));
    editor_free(e);
}

void test_editorDeleteChar_removes_first(void) {
    Editor *e = make_editor_with_text("AB");
    editorDeleteChar(e, 0, 0);
    TEST_ASSERT_EQUAL_STRING("B", row_text(e, 0));
    editor_free(e);
}

void test_editorDeleteChar_removes_last(void) {
    Editor *e = make_editor_with_text("AB");
    editorDeleteChar(e, 0, 1);
    TEST_ASSERT_EQUAL_STRING("A", row_text(e, 0));
    editor_free(e);
}

void test_editorDeleteChar_records_action(void) {
    Editor *e = make_editor_with_text("A");
    editorDeleteChar(e, 0, 0);
    TEST_ASSERT_FALSE(action_stack_is_empty(e->history->undo_stack));
    editor_free(e);
}

// ---------------------------------------------------------------------------
// editorInsertCR
// ---------------------------------------------------------------------------

void test_editorInsertCR_splits_row(void) {
    Editor *e = make_editor_with_text("Hello");
    editorInsertCR(e, 0, 2); // ["He", "llo"]
    TEST_ASSERT_EQUAL_INT(2, e->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("He",  row_text(e, 0));
    TEST_ASSERT_EQUAL_STRING("llo", row_text(e, 1));
    editor_free(e);
}

void test_editorInsertCR_at_beginning(void) {
    Editor *e = make_editor_with_text("Hi");
    editorInsertCR(e, 0, 0);
    TEST_ASSERT_EQUAL_INT(2, e->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("",   row_text(e, 0));
    TEST_ASSERT_EQUAL_STRING("Hi", row_text(e, 1));
    editor_free(e);
}

void test_editorInsertCR_at_end(void) {
    Editor *e = make_editor_with_text("Hi");
    editorInsertCR(e, 0, 2);
    TEST_ASSERT_EQUAL_INT(2, e->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("Hi", row_text(e, 0));
    TEST_ASSERT_EQUAL_STRING("",   row_text(e, 1));
    editor_free(e);
}

void test_editorInsertCR_records_action(void) {
    Editor *e = make_editor_with_text("AB");
    editorInsertCR(e, 0, 1);
    TEST_ASSERT_FALSE(action_stack_is_empty(e->history->undo_stack));
    editor_free(e);
}

// ---------------------------------------------------------------------------
// editorDeleteCR
// ---------------------------------------------------------------------------

void test_editorDeleteCR_merges_rows(void) {
    Editor *e = make_editor_with_text("Hello");
    editorInsertCR(e, 0, 2); // ["He", "llo"]
    editorDeleteCR(e, 1);    // merge back
    TEST_ASSERT_EQUAL_INT(1, e->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("Hello", row_text(e, 0));
    editor_free(e);
}

void test_editorDeleteCR_records_action(void) {
    Editor *e = make_editor_with_text("AB");
    editorInsertCR(e, 0, 1);
    size_t before = e->history->undo_stack->size;
    editorDeleteCR(e, 1);
    TEST_ASSERT_GREATER_THAN(before, e->history->undo_stack->size);
    editor_free(e);
}

// ---------------------------------------------------------------------------
// editorUndo — INSERT_CHAR
// ---------------------------------------------------------------------------

void test_editorUndo_insert_char(void) {
    Editor *e = editor_new_empty();
    editorInsertChar(e, 0, 0, 'A');
    bool ok = editorUndo(e);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("", row_text(e, 0));
    editor_free(e);
}

void test_editorUndo_multiple_insert_chars(void) {
    Editor *e = editor_new_empty();
    editorInsertChar(e, 0, 0, 'A');
    editorInsertChar(e, 0, 1, 'B');
    editorInsertChar(e, 0, 2, 'C');
    editorUndo(e); // undo 'C'
    TEST_ASSERT_EQUAL_STRING("AB", row_text(e, 0));
    editorUndo(e); // undo 'B'
    TEST_ASSERT_EQUAL_STRING("A", row_text(e, 0));
    editorUndo(e); // undo 'A'
    TEST_ASSERT_EQUAL_STRING("", row_text(e, 0));
    editor_free(e);
}

// ---------------------------------------------------------------------------
// editorUndo — DELETE_CHAR
// ---------------------------------------------------------------------------

void test_editorUndo_delete_char_restores(void) {
    Editor *e = make_editor_with_text("AB");
    editorDeleteChar(e, 0, 1); // delete 'B'
    TEST_ASSERT_EQUAL_STRING("A", row_text(e, 0));
    editorUndo(e);
    TEST_ASSERT_EQUAL_STRING("AB", row_text(e, 0));
    editor_free(e);
}

// ---------------------------------------------------------------------------
// editorUndo — INSERT_CR
// ---------------------------------------------------------------------------

void test_editorUndo_insert_cr_merges_back(void) {
    Editor *e = make_editor_with_text("Hello");
    editorInsertCR(e, 0, 2); // ["He", "llo"]
    editorUndo(e);
    TEST_ASSERT_EQUAL_INT(1, e->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("Hello", row_text(e, 0));
    editor_free(e);
}

// ---------------------------------------------------------------------------
// editorUndo — DELETE_CR
// ---------------------------------------------------------------------------

void test_editorUndo_delete_cr_restores_split(void) {
    Editor *e = make_editor_with_text("Hello");
    editorInsertCR(e, 0, 2); // ["He", "llo"]
    editorDeleteCR(e, 1);    // merge → "Hello"
    editorUndo(e);            // should restore split → ["He", "llo"]
    TEST_ASSERT_EQUAL_INT(2, e->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("He",  row_text(e, 0));
    TEST_ASSERT_EQUAL_STRING("llo", row_text(e, 1));
    editor_free(e);
}

// ---------------------------------------------------------------------------
// editorUndo on empty history
// ---------------------------------------------------------------------------

void test_editorUndo_empty_history_returns_false(void) {
    Editor *e = editor_new_empty();
    bool ok = editorUndo(e);
    TEST_ASSERT_FALSE(ok);
    editor_free(e);
}

// ---------------------------------------------------------------------------
// editorRedo — basic
// ---------------------------------------------------------------------------

void test_editorRedo_after_undo_insert_char(void) {
    Editor *e = editor_new_empty();
    editorInsertChar(e, 0, 0, 'A');
    editorUndo(e);
    TEST_ASSERT_EQUAL_STRING("", row_text(e, 0));
    bool ok = editorRedo(e);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("A", row_text(e, 0));
    editor_free(e);
}

void test_editorRedo_after_undo_delete_char(void) {
    Editor *e = make_editor_with_text("AB");
    editorDeleteChar(e, 0, 1); // delete 'B' → "A"
    editorUndo(e);              // restore → "AB"
    editorRedo(e);              // re-delete → "A"
    TEST_ASSERT_EQUAL_STRING("A", row_text(e, 0));
    editor_free(e);
}

void test_editorRedo_after_undo_insert_cr(void) {
    Editor *e = make_editor_with_text("Hello");
    editorInsertCR(e, 0, 2);
    editorUndo(e);
    editorRedo(e);
    TEST_ASSERT_EQUAL_INT(2, e->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("He",  row_text(e, 0));
    TEST_ASSERT_EQUAL_STRING("llo", row_text(e, 1));
    editor_free(e);
}

void test_editorRedo_after_undo_delete_cr(void) {
    Editor *e = make_editor_with_text("Hello");
    editorInsertCR(e, 0, 2); // ["He", "llo"]
    editorDeleteCR(e, 1);    // "Hello"
    editorUndo(e);            // ["He", "llo"]
    editorRedo(e);            // "Hello"
    TEST_ASSERT_EQUAL_INT(1, e->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("Hello", row_text(e, 0));
    editor_free(e);
}

void test_editorRedo_empty_redo_stack_returns_false(void) {
    Editor *e = editor_new_empty();
    bool ok = editorRedo(e);
    TEST_ASSERT_FALSE(ok);
    editor_free(e);
}

// ---------------------------------------------------------------------------
// Redo stack cleared on new edit
// ---------------------------------------------------------------------------

void test_new_edit_clears_redo_stack(void) {
    Editor *e = editor_new_empty();
    editorInsertChar(e, 0, 0, 'A');
    editorUndo(e); // redo stack now has one entry
    editorInsertChar(e, 0, 0, 'B'); // new edit should wipe redo stack
    bool ok = editorRedo(e);
    TEST_ASSERT_FALSE(ok); // nothing to redo
    editor_free(e);
}

// ---------------------------------------------------------------------------
// Multi-step undo/redo round-trip
// ---------------------------------------------------------------------------

void test_full_undo_redo_round_trip(void) {
    Editor *e = editor_new_empty();

    // Build "ABC" character by character
    editorInsertChar(e, 0, 0, 'A');
    editorInsertChar(e, 0, 1, 'B');
    editorInsertChar(e, 0, 2, 'C');
    TEST_ASSERT_EQUAL_STRING("ABC", row_text(e, 0));

    // Undo all three
    editorUndo(e);
    editorUndo(e);
    editorUndo(e);
    TEST_ASSERT_EQUAL_STRING("", row_text(e, 0));

    // Redo all three
    editorRedo(e);
    editorRedo(e);
    editorRedo(e);
    TEST_ASSERT_EQUAL_STRING("ABC", row_text(e, 0));

    editor_free(e);
}

void test_undo_redo_with_cr(void) {
    Editor *e = make_editor_with_text("HelloWorld");

    editorInsertCR(e, 0, 5); // ["Hello", "World"]
    TEST_ASSERT_EQUAL_INT(2, e->buf->numrows);

    editorUndo(e); // back to "HelloWorld"
    TEST_ASSERT_EQUAL_INT(1, e->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("HelloWorld", row_text(e, 0));

    editorRedo(e); // ["Hello", "World"] again
    TEST_ASSERT_EQUAL_INT(2, e->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("Hello", row_text(e, 0));
    TEST_ASSERT_EQUAL_STRING("World", row_text(e, 1));

    editor_free(e);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_editor_new_empty_not_null);
    RUN_TEST(test_editor_new_empty_has_buf_and_history);
    RUN_TEST(test_editor_new_empty_one_empty_row);

    RUN_TEST(test_editor_new_from_file_reads_content);
    RUN_TEST(test_editor_new_from_file_null_returns_null);

    RUN_TEST(test_editor_free_null_does_not_crash);

    RUN_TEST(test_editorInsertChar_appends_single);
    RUN_TEST(test_editorInsertChar_builds_word);
    RUN_TEST(test_editorInsertChar_inserts_in_middle);
    RUN_TEST(test_editorInsertChar_records_action);

    RUN_TEST(test_editorDeleteChar_removes_char);
    RUN_TEST(test_editorDeleteChar_removes_first);
    RUN_TEST(test_editorDeleteChar_removes_last);
    RUN_TEST(test_editorDeleteChar_records_action);

    RUN_TEST(test_editorInsertCR_splits_row);
    RUN_TEST(test_editorInsertCR_at_beginning);
    RUN_TEST(test_editorInsertCR_at_end);
    RUN_TEST(test_editorInsertCR_records_action);

    RUN_TEST(test_editorDeleteCR_merges_rows);
    RUN_TEST(test_editorDeleteCR_records_action);

    RUN_TEST(test_editorUndo_insert_char);
    RUN_TEST(test_editorUndo_multiple_insert_chars);
    RUN_TEST(test_editorUndo_delete_char_restores);
    RUN_TEST(test_editorUndo_insert_cr_merges_back);
    RUN_TEST(test_editorUndo_delete_cr_restores_split);
    RUN_TEST(test_editorUndo_empty_history_returns_false);

    RUN_TEST(test_editorRedo_after_undo_insert_char);
    RUN_TEST(test_editorRedo_after_undo_delete_char);
    RUN_TEST(test_editorRedo_after_undo_insert_cr);
    RUN_TEST(test_editorRedo_after_undo_delete_cr);
    RUN_TEST(test_editorRedo_empty_redo_stack_returns_false);

    RUN_TEST(test_new_edit_clears_redo_stack);

    RUN_TEST(test_full_undo_redo_round_trip);
    RUN_TEST(test_undo_redo_with_cr);

    return UNITY_END();
}