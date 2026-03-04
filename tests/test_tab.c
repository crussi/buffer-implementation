// test_tab.c
//
// Updated for the Vim-style undo tree refactor:
//
//   - Tab.mode field added (EditorMode enum); tabs start in MODE_NORMAL.
//   - EditorHistory now wraps UndoTree; no direct undo_stack/redo_stack access.
//   - Depth helpers replace old h->undo_stack->size / h->redo_stack->size checks.
//   - action_stack_is_empty() / peek_action() still work via the shim layer but
//     those checks are replaced with tree-depth helpers where possible.
//   - tabInsertText records action AND closes group (auto-group path in
//     history_record), so records_exactly_one_action is still testable via
//     undo_depth == 1.

#include "unity.h"
#include "tab.h"
#include "buffer.h"
#include "editor_history.h"
#include <string.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Tree-depth helpers (same as in test_editor_history.c)
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static Tab *make_tab_with_text(const char *text) {
    Tab *t = tab_new_empty();
    if (!t) return NULL;
    for (int i = 0; text[i] != '\0'; i++)
        tabInsertChar(t, 0, i, text[i]);
    return t;
}

static const char *row_text(Tab *t, int r) {
    return t->buf->rows[r].line;
}

// ---------------------------------------------------------------------------
// setUp / tearDown
// ---------------------------------------------------------------------------

void setUp(void)    {}
void tearDown(void) {}

// ---------------------------------------------------------------------------
// tab_new_empty
// ---------------------------------------------------------------------------

void test_tab_new_empty_not_null(void) {
    Tab *t = tab_new_empty();
    TEST_ASSERT_NOT_NULL(t);
    tab_free(t);
}

void test_tab_new_empty_has_buf_and_history(void) {
    Tab *t = tab_new_empty();
    TEST_ASSERT_NOT_NULL(t->buf);
    TEST_ASSERT_NOT_NULL(t->history);
    tab_free(t);
}

void test_tab_new_empty_one_empty_row(void) {
    Tab *t = tab_new_empty();
    TEST_ASSERT_EQUAL_INT(1, t->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("", t->buf->rows[0].line);
    tab_free(t);
}

void test_tab_new_empty_filepath_is_null(void) {
    Tab *t = tab_new_empty();
    TEST_ASSERT_NULL(t->filepath);
    tab_free(t);
}

void test_tab_new_empty_not_dirty(void) {
    Tab *t = tab_new_empty();
    TEST_ASSERT_FALSE(t->dirty);
    tab_free(t);
}

void test_tab_new_empty_mode_is_normal(void) {
    Tab *t = tab_new_empty();
    TEST_ASSERT_EQUAL_INT(MODE_NORMAL, t->mode);
    tab_free(t);
}

// ---------------------------------------------------------------------------
// tab_new_from_file
// ---------------------------------------------------------------------------

void test_tab_new_from_file_reads_content(void) {
    FILE *f = tmpfile();
    fputs("hello\nworld\n", f);
    rewind(f);
    Tab *t = tab_new_from_file(f);
    fclose(f);
    TEST_ASSERT_NOT_NULL(t);
    TEST_ASSERT_EQUAL_INT(2, t->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("hello", row_text(t, 0));
    TEST_ASSERT_EQUAL_STRING("world", row_text(t, 1));
    tab_free(t);
}

void test_tab_new_from_file_null_returns_null(void) {
    Tab *t = tab_new_from_file(NULL);
    TEST_ASSERT_NULL(t);
}

void test_tab_new_from_file_filepath_is_null(void) {
    FILE *f = tmpfile();
    fputs("hello\n", f);
    rewind(f);
    Tab *t = tab_new_from_file(f);
    fclose(f);
    TEST_ASSERT_NULL(t->filepath);
    tab_free(t);
}

void test_tab_new_from_file_not_dirty(void) {
    FILE *f = tmpfile();
    fputs("hello\n", f);
    rewind(f);
    Tab *t = tab_new_from_file(f);
    fclose(f);
    TEST_ASSERT_FALSE(t->dirty);
    tab_free(t);
}

// ---------------------------------------------------------------------------
// tab_free
// ---------------------------------------------------------------------------

void test_tab_free_null_does_not_crash(void) {
    tab_free(NULL);
}

// ---------------------------------------------------------------------------
// Mode transitions
// ---------------------------------------------------------------------------

void test_tab_enter_insert_mode_sets_mode(void) {
    Tab *t = tab_new_empty();
    tab_enter_insert_mode(t);
    TEST_ASSERT_EQUAL_INT(MODE_INSERT, t->mode);
    tab_free(t);
}

void test_tab_leave_insert_mode_returns_to_normal(void) {
    Tab *t = tab_new_empty();
    tab_enter_insert_mode(t);
    tab_leave_insert_mode(t);
    TEST_ASSERT_EQUAL_INT(MODE_NORMAL, t->mode);
    tab_free(t);
}

void test_tab_enter_normal_mode_from_insert(void) {
    Tab *t = tab_new_empty();
    tab_enter_insert_mode(t);
    tab_enter_normal_mode(t);
    TEST_ASSERT_EQUAL_INT(MODE_NORMAL, t->mode);
    tab_free(t);
}

void test_tab_enter_visual_mode_sets_mode(void) {
    Tab *t = tab_new_empty();
    tab_enter_visual_mode(t);
    TEST_ASSERT_EQUAL_INT(MODE_VISUAL, t->mode);
    tab_free(t);
}

void test_tab_enter_replace_mode_sets_mode(void) {
    Tab *t = tab_new_empty();
    tab_enter_replace_mode(t);
    TEST_ASSERT_EQUAL_INT(MODE_REPLACE, t->mode);
    tab_free(t);
}

// ---------------------------------------------------------------------------
// tabInsertChar
// ---------------------------------------------------------------------------

void test_tabInsertChar_appends_single(void) {
    Tab *t = tab_new_empty();
    tabInsertChar(t, 0, 0, 'X');
    TEST_ASSERT_EQUAL_STRING("X", row_text(t, 0));
    tab_free(t);
}

void test_tabInsertChar_builds_word(void) {
    Tab *t = tab_new_empty();
    const char *word = "Hello";
    for (int i = 0; word[i]; i++)
        tabInsertChar(t, 0, i, word[i]);
    TEST_ASSERT_EQUAL_STRING("Hello", row_text(t, 0));
    tab_free(t);
}

void test_tabInsertChar_inserts_in_middle(void) {
    Tab *t = make_tab_with_text("AC");
    tabInsertChar(t, 0, 1, 'B');
    TEST_ASSERT_EQUAL_STRING("ABC", row_text(t, 0));
    tab_free(t);
}

void test_tabInsertChar_records_action(void) {
    Tab *t = tab_new_empty();
    tabInsertChar(t, 0, 0, 'Z');
    TEST_ASSERT_EQUAL_INT(1, undo_depth(t->history));
    tab_free(t);
}

void test_tabInsertChar_sets_dirty(void) {
    Tab *t = tab_new_empty();
    TEST_ASSERT_FALSE(t->dirty);
    tabInsertChar(t, 0, 0, 'A');
    TEST_ASSERT_TRUE(t->dirty);
    tab_free(t);
}

// ---------------------------------------------------------------------------
// tabDeleteChar
// ---------------------------------------------------------------------------

void test_tabDeleteChar_removes_char(void) {
    Tab *t = make_tab_with_text("ABC");
    tabDeleteChar(t, 0, 1);
    TEST_ASSERT_EQUAL_STRING("AC", row_text(t, 0));
    tab_free(t);
}

void test_tabDeleteChar_removes_first(void) {
    Tab *t = make_tab_with_text("AB");
    tabDeleteChar(t, 0, 0);
    TEST_ASSERT_EQUAL_STRING("B", row_text(t, 0));
    tab_free(t);
}

void test_tabDeleteChar_removes_last(void) {
    Tab *t = make_tab_with_text("AB");
    tabDeleteChar(t, 0, 1);
    TEST_ASSERT_EQUAL_STRING("A", row_text(t, 0));
    tab_free(t);
}

void test_tabDeleteChar_records_action(void) {
    Tab *t = make_tab_with_text("A");
    int depth_before = undo_depth(t->history);
    tabDeleteChar(t, 0, 0);
    TEST_ASSERT_EQUAL_INT(depth_before + 1, undo_depth(t->history));
    tab_free(t);
}

void test_tabDeleteChar_sets_dirty(void) {
    Tab *t = make_tab_with_text("AB");
    t->dirty = false;
    tabDeleteChar(t, 0, 0);
    TEST_ASSERT_TRUE(t->dirty);
    tab_free(t);
}

// ---------------------------------------------------------------------------
// tabInsertCR
// ---------------------------------------------------------------------------

void test_tabInsertCR_splits_row(void) {
    Tab *t = make_tab_with_text("Hello");
    tabInsertCR(t, 0, 2);
    TEST_ASSERT_EQUAL_INT(2, t->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("He",  row_text(t, 0));
    TEST_ASSERT_EQUAL_STRING("llo", row_text(t, 1));
    tab_free(t);
}

void test_tabInsertCR_at_beginning(void) {
    Tab *t = make_tab_with_text("Hi");
    tabInsertCR(t, 0, 0);
    TEST_ASSERT_EQUAL_INT(2, t->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("",   row_text(t, 0));
    TEST_ASSERT_EQUAL_STRING("Hi", row_text(t, 1));
    tab_free(t);
}

void test_tabInsertCR_at_end(void) {
    Tab *t = make_tab_with_text("Hi");
    tabInsertCR(t, 0, 2);
    TEST_ASSERT_EQUAL_INT(2, t->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("Hi", row_text(t, 0));
    TEST_ASSERT_EQUAL_STRING("",   row_text(t, 1));
    tab_free(t);
}

void test_tabInsertCR_records_action(void) {
    Tab *t = make_tab_with_text("AB");
    int depth_before = undo_depth(t->history);
    tabInsertCR(t, 0, 1);
    TEST_ASSERT_EQUAL_INT(depth_before + 1, undo_depth(t->history));
    tab_free(t);
}

void test_tabInsertCR_sets_dirty(void) {
    Tab *t = make_tab_with_text("Hello");
    t->dirty = false;
    tabInsertCR(t, 0, 2);
    TEST_ASSERT_TRUE(t->dirty);
    tab_free(t);
}

// ---------------------------------------------------------------------------
// tabDeleteCR
// ---------------------------------------------------------------------------

void test_tabDeleteCR_merges_rows(void) {
    Tab *t = make_tab_with_text("Hello");
    tabInsertCR(t, 0, 2);
    tabDeleteCR(t, 1);
    TEST_ASSERT_EQUAL_INT(1, t->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("Hello", row_text(t, 0));
    tab_free(t);
}

void test_tabDeleteCR_records_action(void) {
    Tab *t = make_tab_with_text("AB");
    tabInsertCR(t, 0, 1);
    int depth_before = undo_depth(t->history);
    tabDeleteCR(t, 1);
    TEST_ASSERT_EQUAL_INT(depth_before + 1, undo_depth(t->history));
    tab_free(t);
}

void test_tabDeleteCR_sets_dirty(void) {
    Tab *t = make_tab_with_text("Hello");
    tabInsertCR(t, 0, 2);
    t->dirty = false;
    tabDeleteCR(t, 1);
    TEST_ASSERT_TRUE(t->dirty);
    tab_free(t);
}

// ---------------------------------------------------------------------------
// tabInsertText — basic behaviour
// ---------------------------------------------------------------------------

void test_tabInsertText_returns_end_position_single_line(void) {
    Tab *t = tab_new_empty();
    Position end = tabInsertText(t, 0, 0, "hello");
    TEST_ASSERT_EQUAL_INT(0, end.row);
    TEST_ASSERT_EQUAL_INT(5, end.col);
    tab_free(t);
}

void test_tabInsertText_inserts_single_line(void) {
    Tab *t = tab_new_empty();
    tabInsertText(t, 0, 0, "hello");
    TEST_ASSERT_EQUAL_INT(1, t->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("hello", row_text(t, 0));
    tab_free(t);
}

void test_tabInsertText_inserts_multiline(void) {
    Tab *t = tab_new_empty();
    tabInsertText(t, 0, 0, "foo\nbar");
    TEST_ASSERT_EQUAL_INT(2, t->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("foo", row_text(t, 0));
    TEST_ASSERT_EQUAL_STRING("bar", row_text(t, 1));
    tab_free(t);
}

void test_tabInsertText_returns_end_position_multiline(void) {
    Tab *t = tab_new_empty();
    Position end = tabInsertText(t, 0, 0, "foo\nbar");
    TEST_ASSERT_EQUAL_INT(1, end.row);
    TEST_ASSERT_EQUAL_INT(3, end.col);
    tab_free(t);
}

void test_tabInsertText_inserts_mid_line(void) {
    Tab *t = make_tab_with_text("AC");
    tabInsertText(t, 0, 1, "B");
    TEST_ASSERT_EQUAL_STRING("ABC", row_text(t, 0));
    tab_free(t);
}

void test_tabInsertText_with_trailing_newline(void) {
    Tab *t = tab_new_empty();
    tabInsertText(t, 0, 0, "hi\n");
    TEST_ASSERT_EQUAL_INT(2, t->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("hi", row_text(t, 0));
    TEST_ASSERT_EQUAL_STRING("",   row_text(t, 1));
    tab_free(t);
}

void test_tabInsertText_multiple_embedded_newlines(void) {
    Tab *t = tab_new_empty();
    tabInsertText(t, 0, 0, "a\nb\nc");
    TEST_ASSERT_EQUAL_INT(3, t->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("a", row_text(t, 0));
    TEST_ASSERT_EQUAL_STRING("b", row_text(t, 1));
    TEST_ASSERT_EQUAL_STRING("c", row_text(t, 2));
    tab_free(t);
}

void test_tabInsertText_empty_string_is_noop(void) {
    Tab *t = make_tab_with_text("hello");
    int depth_before = undo_depth(t->history);
    bool dirty_before = t->dirty;

    Position end = tabInsertText(t, 0, 3, "");

    TEST_ASSERT_EQUAL_STRING("hello", row_text(t, 0));
    TEST_ASSERT_EQUAL_INT(depth_before, undo_depth(t->history));
    TEST_ASSERT_EQUAL(dirty_before, t->dirty);
    TEST_ASSERT_EQUAL_INT(0, end.row);
    TEST_ASSERT_EQUAL_INT(3, end.col);
    tab_free(t);
}

void test_tabInsertText_null_text_is_safe(void) {
    Tab *t = tab_new_empty();
    Position end = tabInsertText(t, 0, 0, NULL);
    TEST_ASSERT_EQUAL_INT(0, end.row);
    TEST_ASSERT_EQUAL_INT(0, end.col);
    TEST_ASSERT_EQUAL_STRING("", row_text(t, 0));
    tab_free(t);
}

// ---------------------------------------------------------------------------
// tabInsertText — records a single undoable action
// ---------------------------------------------------------------------------

void test_tabInsertText_records_exactly_one_undo_step(void) {
    Tab *t = tab_new_empty();
    TEST_ASSERT_EQUAL_INT(0, undo_depth(t->history));
    tabInsertText(t, 0, 0, "hello\nworld");
    // Bulk insert must be one undo step, not one per character.
    TEST_ASSERT_EQUAL_INT(1, undo_depth(t->history));
    tab_free(t);
}

void test_tabInsertText_sets_dirty(void) {
    Tab *t = tab_new_empty();
    TEST_ASSERT_FALSE(t->dirty);
    tabInsertText(t, 0, 0, "hello");
    TEST_ASSERT_TRUE(t->dirty);
    tab_free(t);
}

void test_tabInsertText_clears_redo_path(void) {
    // Undo something so there is a redo path, then insert — redo depth of
    // the NEW branch is 0 (the old branch still exists in the tree but is
    // not the default redo target).
    Tab *t = tab_new_empty();
    tabInsertChar(t, 0, 0, 'A');
    tabUndo(t);
    TEST_ASSERT_EQUAL_INT(1, redo_depth(t->history));
    tabInsertText(t, 0, 0, "new");
    // The new branch has no children yet.
    TEST_ASSERT_EQUAL_INT(0, redo_depth(t->history));
    tab_free(t);
}

// ---------------------------------------------------------------------------
// tabInsertText — undo restores original state
// ---------------------------------------------------------------------------

void test_tabInsertText_undo_removes_single_line(void) {
    Tab *t = tab_new_empty();
    tabInsertText(t, 0, 0, "hello");
    tabUndo(t);
    TEST_ASSERT_EQUAL_INT(1, t->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("", row_text(t, 0));
    tab_free(t);
}

void test_tabInsertText_undo_removes_multiline(void) {
    Tab *t = tab_new_empty();
    tabInsertText(t, 0, 0, "foo\nbar");
    tabUndo(t);
    TEST_ASSERT_EQUAL_INT(1, t->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("", row_text(t, 0));
    tab_free(t);
}

void test_tabInsertText_undo_removes_mid_line_insert(void) {
    Tab *t = make_tab_with_text("XZ");
    tabInsertText(t, 0, 1, "Y");
    TEST_ASSERT_EQUAL_STRING("XYZ", row_text(t, 0));
    tabUndo(t);
    TEST_ASSERT_EQUAL_STRING("XZ", row_text(t, 0));
    tab_free(t);
}

void test_tabInsertText_undo_is_single_step(void) {
    Tab *t = tab_new_empty();
    tabInsertText(t, 0, 0, "helloworld");
    tabUndo(t);
    TEST_ASSERT_EQUAL_STRING("", row_text(t, 0));
    // Second undo should find nothing to do.
    TEST_ASSERT_FALSE(tabUndo(t));
    tab_free(t);
}

void test_tabInsertText_undo_multiline_is_single_step(void) {
    Tab *t = tab_new_empty();
    tabInsertText(t, 0, 0, "line1\nline2\nline3");
    TEST_ASSERT_EQUAL_INT(3, t->buf->numrows);
    tabUndo(t);
    TEST_ASSERT_EQUAL_INT(1, t->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("", row_text(t, 0));
    TEST_ASSERT_FALSE(tabUndo(t));
    tab_free(t);
}

void test_tabInsertText_undo_sets_dirty(void) {
    Tab *t = tab_new_empty();
    tabInsertText(t, 0, 0, "hello");
    t->dirty = false;
    tabUndo(t);
    TEST_ASSERT_TRUE(t->dirty);
    tab_free(t);
}

// ---------------------------------------------------------------------------
// tabInsertText — redo replays the insertion
// ---------------------------------------------------------------------------

void test_tabInsertText_redo_restores_single_line(void) {
    Tab *t = tab_new_empty();
    tabInsertText(t, 0, 0, "hello");
    tabUndo(t);
    tabRedo(t);
    TEST_ASSERT_EQUAL_STRING("hello", row_text(t, 0));
    tab_free(t);
}

void test_tabInsertText_redo_restores_multiline(void) {
    Tab *t = tab_new_empty();
    tabInsertText(t, 0, 0, "foo\nbar");
    tabUndo(t);
    tabRedo(t);
    TEST_ASSERT_EQUAL_INT(2, t->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("foo", row_text(t, 0));
    TEST_ASSERT_EQUAL_STRING("bar", row_text(t, 1));
    tab_free(t);
}

void test_tabInsertText_undo_redo_cycle_is_stable(void) {
    Tab *t = tab_new_empty();
    tabInsertText(t, 0, 0, "hello\nworld");
    for (int i = 0; i < 5; i++) {
        tabUndo(t);
        TEST_ASSERT_EQUAL_INT(1, t->buf->numrows);
        TEST_ASSERT_EQUAL_STRING("", row_text(t, 0));
        tabRedo(t);
        TEST_ASSERT_EQUAL_INT(2, t->buf->numrows);
        TEST_ASSERT_EQUAL_STRING("hello", row_text(t, 0));
        TEST_ASSERT_EQUAL_STRING("world", row_text(t, 1));
    }
    tab_free(t);
}

// ---------------------------------------------------------------------------
// tabInsertText — cursor placement
// ---------------------------------------------------------------------------

void test_tabInsertText_cursor_positioned_at_end(void) {
    Tab *t = tab_new_empty();
    Position end = tabInsertText(t, 0, 0, "hello");
    TEST_ASSERT_EQUAL_INT(0, end.row);
    TEST_ASSERT_EQUAL_INT(5, end.col);
    tab_free(t);
}

void test_tabInsertText_cursor_positioned_after_multiline(void) {
    Tab *t = tab_new_empty();
    Position end = tabInsertText(t, 0, 0, "ab\ncd\nef");
    TEST_ASSERT_EQUAL_INT(2, end.row);
    TEST_ASSERT_EQUAL_INT(2, end.col);
    tab_free(t);
}

// ---------------------------------------------------------------------------
// tabInsertText — interaction with other edit operations
// ---------------------------------------------------------------------------

void test_tabInsertText_followed_by_insert_char_undo_order(void) {
    Tab *t = tab_new_empty();
    tabInsertText(t, 0, 0, "hi");
    tabInsertChar(t, 0, 2, 'X');
    TEST_ASSERT_EQUAL_STRING("hiX", row_text(t, 0));
    tabUndo(t);
    TEST_ASSERT_EQUAL_STRING("hi", row_text(t, 0));
    tabUndo(t);
    TEST_ASSERT_EQUAL_STRING("", row_text(t, 0));
    tab_free(t);
}

void test_tabInsertText_after_insert_cr_undo_order(void) {
    Tab *t = make_tab_with_text("Hello");
    tabInsertCR(t, 0, 5);
    tabInsertText(t, 1, 0, "World");
    TEST_ASSERT_EQUAL_STRING("World", row_text(t, 1));
    tabUndo(t);
    TEST_ASSERT_EQUAL_STRING("", row_text(t, 1));
    tabUndo(t);
    TEST_ASSERT_EQUAL_INT(1, t->buf->numrows);
    tab_free(t);
}

void test_new_edit_after_insert_text_undo_creates_new_branch(void) {
    Tab *t = tab_new_empty();
    tabInsertText(t, 0, 0, "hello");
    tabUndo(t);
    TEST_ASSERT_EQUAL_INT(1, redo_depth(t->history));
    tabInsertChar(t, 0, 0, 'X');
    // New branch is current; old redo is no longer default.
    TEST_ASSERT_EQUAL_INT(0, redo_depth(t->history));
    tab_free(t);
}

// ---------------------------------------------------------------------------
// tabUndo -- INSERT_CHAR (existing)
// ---------------------------------------------------------------------------

void test_tabUndo_insert_char(void) {
    Tab *t = tab_new_empty();
    tabInsertChar(t, 0, 0, 'A');
    bool ok = tabUndo(t);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("", row_text(t, 0));
    tab_free(t);
}

void test_tabUndo_multiple_insert_chars(void) {
    Tab *t = tab_new_empty();
    tabInsertChar(t, 0, 0, 'A');
    tabInsertChar(t, 0, 1, 'B');
    tabInsertChar(t, 0, 2, 'C');
    tabUndo(t);
    TEST_ASSERT_EQUAL_STRING("AB", row_text(t, 0));
    tabUndo(t);
    TEST_ASSERT_EQUAL_STRING("A", row_text(t, 0));
    tabUndo(t);
    TEST_ASSERT_EQUAL_STRING("", row_text(t, 0));
    tab_free(t);
}

void test_tabUndo_delete_char_restores(void) {
    Tab *t = make_tab_with_text("AB");
    tabDeleteChar(t, 0, 1);
    TEST_ASSERT_EQUAL_STRING("A", row_text(t, 0));
    tabUndo(t);
    TEST_ASSERT_EQUAL_STRING("AB", row_text(t, 0));
    tab_free(t);
}

void test_tabUndo_insert_cr_merges_back(void) {
    Tab *t = make_tab_with_text("Hello");
    tabInsertCR(t, 0, 2);
    tabUndo(t);
    TEST_ASSERT_EQUAL_INT(1, t->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("Hello", row_text(t, 0));
    tab_free(t);
}

void test_tabUndo_delete_cr_restores_split(void) {
    Tab *t = make_tab_with_text("Hello");
    tabInsertCR(t, 0, 2);
    tabDeleteCR(t, 1);
    tabUndo(t);
    TEST_ASSERT_EQUAL_INT(2, t->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("He",  row_text(t, 0));
    TEST_ASSERT_EQUAL_STRING("llo", row_text(t, 1));
    tab_free(t);
}

void test_tabUndo_empty_history_returns_false(void) {
    Tab *t = tab_new_empty();
    bool ok = tabUndo(t);
    TEST_ASSERT_FALSE(ok);
    tab_free(t);
}

void test_tabUndo_sets_dirty(void) {
    Tab *t = tab_new_empty();
    tabInsertChar(t, 0, 0, 'A');
    t->dirty = false;
    tabUndo(t);
    TEST_ASSERT_TRUE(t->dirty);
    tab_free(t);
}

// ---------------------------------------------------------------------------
// tabRedo -- basic
// ---------------------------------------------------------------------------

void test_tabRedo_after_undo_insert_char(void) {
    Tab *t = tab_new_empty();
    tabInsertChar(t, 0, 0, 'A');
    tabUndo(t);
    bool ok = tabRedo(t);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("A", row_text(t, 0));
    tab_free(t);
}

void test_tabRedo_after_undo_delete_char(void) {
    Tab *t = make_tab_with_text("AB");
    tabDeleteChar(t, 0, 1);
    tabUndo(t);
    tabRedo(t);
    TEST_ASSERT_EQUAL_STRING("A", row_text(t, 0));
    tab_free(t);
}

void test_tabRedo_after_undo_insert_cr(void) {
    Tab *t = make_tab_with_text("Hello");
    tabInsertCR(t, 0, 2);
    tabUndo(t);
    tabRedo(t);
    TEST_ASSERT_EQUAL_INT(2, t->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("He",  row_text(t, 0));
    TEST_ASSERT_EQUAL_STRING("llo", row_text(t, 1));
    tab_free(t);
}

void test_tabRedo_after_undo_delete_cr(void) {
    Tab *t = make_tab_with_text("Hello");
    tabInsertCR(t, 0, 2);
    tabDeleteCR(t, 1);
    tabUndo(t);
    tabRedo(t);
    TEST_ASSERT_EQUAL_INT(1, t->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("Hello", row_text(t, 0));
    tab_free(t);
}

void test_tabRedo_empty_redo_path_returns_false(void) {
    Tab *t = tab_new_empty();
    TEST_ASSERT_FALSE(tabRedo(t));
    tab_free(t);
}

void test_tabRedo_sets_dirty(void) {
    Tab *t = tab_new_empty();
    tabInsertChar(t, 0, 0, 'A');
    tabUndo(t);
    t->dirty = false;
    tabRedo(t);
    TEST_ASSERT_TRUE(t->dirty);
    tab_free(t);
}

// ---------------------------------------------------------------------------
// New edit after undo (undo tree: creates new branch)
// ---------------------------------------------------------------------------

void test_new_edit_after_undo_creates_branch_not_erase(void) {
    // In an undo tree, the old redo path is NOT erased; a new branch is
    // created.  The test verifies that redo_depth on the new branch is 0
    // (the new branch has no children) while undo_depth is 1.
    Tab *t = tab_new_empty();
    tabInsertChar(t, 0, 0, 'A');
    tabUndo(t);
    tabInsertChar(t, 0, 0, 'B');
    TEST_ASSERT_EQUAL_INT(1, undo_depth(t->history));
    TEST_ASSERT_EQUAL_INT(0, redo_depth(t->history));
    tab_free(t);
}

// ---------------------------------------------------------------------------
// Multi-step undo/redo round-trip
// ---------------------------------------------------------------------------

void test_full_undo_redo_round_trip(void) {
    Tab *t = tab_new_empty();
    tabInsertChar(t, 0, 0, 'A');
    tabInsertChar(t, 0, 1, 'B');
    tabInsertChar(t, 0, 2, 'C');
    TEST_ASSERT_EQUAL_STRING("ABC", row_text(t, 0));
    tabUndo(t); tabUndo(t); tabUndo(t);
    TEST_ASSERT_EQUAL_STRING("", row_text(t, 0));
    tabRedo(t); tabRedo(t); tabRedo(t);
    TEST_ASSERT_EQUAL_STRING("ABC", row_text(t, 0));
    tab_free(t);
}

void test_undo_redo_with_cr(void) {
    Tab *t = make_tab_with_text("HelloWorld");
    tabInsertCR(t, 0, 5);
    TEST_ASSERT_EQUAL_INT(2, t->buf->numrows);
    tabUndo(t);
    TEST_ASSERT_EQUAL_INT(1, t->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("HelloWorld", row_text(t, 0));
    tabRedo(t);
    TEST_ASSERT_EQUAL_INT(2, t->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("Hello", row_text(t, 0));
    TEST_ASSERT_EQUAL_STRING("World", row_text(t, 1));
    tab_free(t);
}

// ---------------------------------------------------------------------------
// Vim change-group via mode transitions
// ---------------------------------------------------------------------------

void test_insert_mode_groups_chars_as_one_undo_step(void) {
    // Enter Insert, type three chars, leave Insert → one undo step.
    Tab *t = tab_new_empty();
    tab_enter_insert_mode(t);
    tabInsertChar(t, 0, 0, 'a');
    tabInsertChar(t, 0, 1, 'b');
    tabInsertChar(t, 0, 2, 'c');
    tab_leave_insert_mode(t);

    TEST_ASSERT_EQUAL_STRING("abc", row_text(t, 0));
    TEST_ASSERT_EQUAL_INT(1, undo_depth(t->history));

    tabUndo(t);
    TEST_ASSERT_EQUAL_STRING("", row_text(t, 0));
    TEST_ASSERT_FALSE(tabUndo(t));  // nothing more to undo
    tab_free(t);
}

void test_two_insert_sessions_two_undo_steps(void) {
    Tab *t = tab_new_empty();

    tab_enter_insert_mode(t);
    tabInsertChar(t, 0, 0, 'A');
    tab_leave_insert_mode(t);

    tab_enter_insert_mode(t);
    tabInsertChar(t, 0, 1, 'B');
    tab_leave_insert_mode(t);

    TEST_ASSERT_EQUAL_INT(2, undo_depth(t->history));
    tabUndo(t);
    TEST_ASSERT_EQUAL_STRING("A", row_text(t, 0));
    tabUndo(t);
    TEST_ASSERT_EQUAL_STRING("", row_text(t, 0));
    tab_free(t);
}

void test_undo_during_insert_mode_closes_group_first(void) {
    // tabUndo when in Insert mode should close the open group then undo it.
    Tab *t = tab_new_empty();
    tab_enter_insert_mode(t);
    tabInsertChar(t, 0, 0, 'X');
    tabInsertChar(t, 0, 1, 'Y');
    // Undo while still in Insert mode.
    tabUndo(t);
    // Both chars must be gone (the group was closed then undone).
    TEST_ASSERT_EQUAL_STRING("", row_text(t, 0));
    TEST_ASSERT_EQUAL_INT(MODE_NORMAL, t->mode);
    tab_free(t);
}

// ---------------------------------------------------------------------------
// tab_open
// ---------------------------------------------------------------------------

void test_tab_open_loads_file_content(void) {
    const char *path = "test_tab_open_tmp.txt";
    FILE *f = fopen(path, "w");
    fputs("line1\nline2\n", f);
    fclose(f);
    Tab *t = tab_new_empty();
    bool ok = tab_open(t, path);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_INT(2, t->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("line1", row_text(t, 0));
    TEST_ASSERT_EQUAL_STRING("line2", row_text(t, 1));
    TEST_ASSERT_EQUAL_STRING(path, t->filepath);
    TEST_ASSERT_FALSE(t->dirty);
    tab_free(t);
    remove(path);
}

void test_tab_open_resets_history(void) {
    const char *path = "test_tab_open_hist_tmp.txt";
    FILE *f = fopen(path, "w");
    fputs("hello\n", f);
    fclose(f);
    Tab *t = tab_new_empty();
    tabInsertChar(t, 0, 0, 'X');
    tab_open(t, path);
    // After open, undo tree should be reset (nothing to undo).
    TEST_ASSERT_FALSE(tabUndo(t));
    TEST_ASSERT_FALSE(t->dirty);
    tab_free(t);
    remove(path);
}

void test_tab_open_resets_mode_to_normal(void) {
    const char *path = "test_tab_open_mode_tmp.txt";
    FILE *f = fopen(path, "w");
    fputs("hi\n", f);
    fclose(f);
    Tab *t = tab_new_empty();
    tab_enter_insert_mode(t);
    tab_open(t, path);
    TEST_ASSERT_EQUAL_INT(MODE_NORMAL, t->mode);
    tab_free(t);
    remove(path);
}

void test_tab_open_null_path_returns_false(void) {
    Tab *t = tab_new_empty();
    TEST_ASSERT_FALSE(tab_open(t, NULL));
    tab_free(t);
}

void test_tab_open_bad_path_returns_false(void) {
    Tab *t = tab_new_empty();
    TEST_ASSERT_FALSE(tab_open(t, "/nonexistent/path/that/does/not/exist.txt"));
    tab_free(t);
}

// ---------------------------------------------------------------------------
// tab_save / tab_save_as
// ---------------------------------------------------------------------------

void test_tab_save_as_writes_file(void) {
    const char *path = "test_tab_save_tmp.txt";
    Tab *t = make_tab_with_text("Hello");
    bool ok = tab_save_as(t, path);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_FALSE(t->dirty);
    TEST_ASSERT_EQUAL_STRING(path, t->filepath);
    FILE *f = fopen(path, "r");
    TEST_ASSERT_NOT_NULL(f);
    char buf[64] = {0};
    fgets(buf, sizeof(buf), f);
    fclose(f);
    TEST_ASSERT_EQUAL_STRING("Hello\n", buf);
    tab_free(t);
    remove(path);
}

void test_tab_save_as_clears_dirty(void) {
    const char *path = "test_tab_save_dirty_tmp.txt";
    Tab *t = make_tab_with_text("Hi");
    TEST_ASSERT_TRUE(t->dirty);
    tab_save_as(t, path);
    TEST_ASSERT_FALSE(t->dirty);
    tab_free(t);
    remove(path);
}

void test_tab_save_as_updates_filepath(void) {
    const char *path = "test_tab_save_path_tmp.txt";
    Tab *t = tab_new_empty();
    TEST_ASSERT_NULL(t->filepath);
    tab_save_as(t, path);
    TEST_ASSERT_EQUAL_STRING(path, t->filepath);
    tab_free(t);
    remove(path);
}

void test_tab_save_uses_existing_filepath(void) {
    const char *path = "test_tab_save_existing_tmp.txt";
    Tab *t = make_tab_with_text("World");
    tab_save_as(t, path);
    tabInsertChar(t, 0, 5, '!');
    TEST_ASSERT_TRUE(t->dirty);
    bool ok = tab_save(t);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_FALSE(t->dirty);
    FILE *f = fopen(path, "r");
    char buf[64] = {0};
    fgets(buf, sizeof(buf), f);
    fclose(f);
    TEST_ASSERT_EQUAL_STRING("World!\n", buf);
    tab_free(t);
    remove(path);
}

void test_tab_save_null_filepath_returns_false(void) {
    Tab *t = tab_new_empty();
    TEST_ASSERT_NULL(t->filepath);
    TEST_ASSERT_FALSE(tab_save(t));
    tab_free(t);
}

void test_tab_save_as_null_path_returns_false(void) {
    Tab *t = tab_new_empty();
    TEST_ASSERT_FALSE(tab_save_as(t, NULL));
    tab_free(t);
}

// ---------------------------------------------------------------------------
// Round-trip: save then open
// ---------------------------------------------------------------------------

void test_save_open_round_trip(void) {
    const char *path = "test_tab_roundtrip_tmp.txt";
    Tab *t = tab_new_empty();
    tabInsertChar(t, 0, 0, 'H');
    tabInsertChar(t, 0, 1, 'i');
    tabInsertCR(t, 0, 2);
    tabInsertChar(t, 1, 0, 'B');
    tabInsertChar(t, 1, 1, 'y');
    tabInsertChar(t, 1, 2, 'e');
    tab_save_as(t, path);
    tab_free(t);
    Tab *t2 = tab_new_empty();
    tab_open(t2, path);
    TEST_ASSERT_EQUAL_INT(2, t2->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("Hi",  row_text(t2, 0));
    TEST_ASSERT_EQUAL_STRING("Bye", row_text(t2, 1));
    TEST_ASSERT_FALSE(t2->dirty);
    tab_free(t2);
    remove(path);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(void) {
    UNITY_BEGIN();

    // tab_new_empty
    RUN_TEST(test_tab_new_empty_not_null);
    RUN_TEST(test_tab_new_empty_has_buf_and_history);
    RUN_TEST(test_tab_new_empty_one_empty_row);
    RUN_TEST(test_tab_new_empty_filepath_is_null);
    RUN_TEST(test_tab_new_empty_not_dirty);
    RUN_TEST(test_tab_new_empty_mode_is_normal);

    // tab_new_from_file
    RUN_TEST(test_tab_new_from_file_reads_content);
    RUN_TEST(test_tab_new_from_file_null_returns_null);
    RUN_TEST(test_tab_new_from_file_filepath_is_null);
    RUN_TEST(test_tab_new_from_file_not_dirty);

    // tab_free
    RUN_TEST(test_tab_free_null_does_not_crash);

    // Mode transitions
    RUN_TEST(test_tab_enter_insert_mode_sets_mode);
    RUN_TEST(test_tab_leave_insert_mode_returns_to_normal);
    RUN_TEST(test_tab_enter_normal_mode_from_insert);
    RUN_TEST(test_tab_enter_visual_mode_sets_mode);
    RUN_TEST(test_tab_enter_replace_mode_sets_mode);

    // tabInsertChar
    RUN_TEST(test_tabInsertChar_appends_single);
    RUN_TEST(test_tabInsertChar_builds_word);
    RUN_TEST(test_tabInsertChar_inserts_in_middle);
    RUN_TEST(test_tabInsertChar_records_action);
    RUN_TEST(test_tabInsertChar_sets_dirty);

    // tabDeleteChar
    RUN_TEST(test_tabDeleteChar_removes_char);
    RUN_TEST(test_tabDeleteChar_removes_first);
    RUN_TEST(test_tabDeleteChar_removes_last);
    RUN_TEST(test_tabDeleteChar_records_action);
    RUN_TEST(test_tabDeleteChar_sets_dirty);

    // tabInsertCR
    RUN_TEST(test_tabInsertCR_splits_row);
    RUN_TEST(test_tabInsertCR_at_beginning);
    RUN_TEST(test_tabInsertCR_at_end);
    RUN_TEST(test_tabInsertCR_records_action);
    RUN_TEST(test_tabInsertCR_sets_dirty);

    // tabDeleteCR
    RUN_TEST(test_tabDeleteCR_merges_rows);
    RUN_TEST(test_tabDeleteCR_records_action);
    RUN_TEST(test_tabDeleteCR_sets_dirty);

    // tabInsertText — basic behaviour
    RUN_TEST(test_tabInsertText_returns_end_position_single_line);
    RUN_TEST(test_tabInsertText_inserts_single_line);
    RUN_TEST(test_tabInsertText_inserts_multiline);
    RUN_TEST(test_tabInsertText_returns_end_position_multiline);
    RUN_TEST(test_tabInsertText_inserts_mid_line);
    RUN_TEST(test_tabInsertText_with_trailing_newline);
    RUN_TEST(test_tabInsertText_multiple_embedded_newlines);
    RUN_TEST(test_tabInsertText_empty_string_is_noop);
    RUN_TEST(test_tabInsertText_null_text_is_safe);

    // tabInsertText — single undoable step
    RUN_TEST(test_tabInsertText_records_exactly_one_undo_step);
    RUN_TEST(test_tabInsertText_sets_dirty);
    RUN_TEST(test_tabInsertText_clears_redo_path);

    // tabInsertText — undo
    RUN_TEST(test_tabInsertText_undo_removes_single_line);
    RUN_TEST(test_tabInsertText_undo_removes_multiline);
    RUN_TEST(test_tabInsertText_undo_removes_mid_line_insert);
    RUN_TEST(test_tabInsertText_undo_is_single_step);
    RUN_TEST(test_tabInsertText_undo_multiline_is_single_step);
    RUN_TEST(test_tabInsertText_undo_sets_dirty);

    // tabInsertText — redo
    RUN_TEST(test_tabInsertText_redo_restores_single_line);
    RUN_TEST(test_tabInsertText_redo_restores_multiline);
    RUN_TEST(test_tabInsertText_undo_redo_cycle_is_stable);

    // tabInsertText — cursor
    RUN_TEST(test_tabInsertText_cursor_positioned_at_end);
    RUN_TEST(test_tabInsertText_cursor_positioned_after_multiline);

    // tabInsertText — interactions
    RUN_TEST(test_tabInsertText_followed_by_insert_char_undo_order);
    RUN_TEST(test_tabInsertText_after_insert_cr_undo_order);
    RUN_TEST(test_new_edit_after_insert_text_undo_creates_new_branch);

    // tabUndo (existing action types)
    RUN_TEST(test_tabUndo_insert_char);
    RUN_TEST(test_tabUndo_multiple_insert_chars);
    RUN_TEST(test_tabUndo_delete_char_restores);
    RUN_TEST(test_tabUndo_insert_cr_merges_back);
    RUN_TEST(test_tabUndo_delete_cr_restores_split);
    RUN_TEST(test_tabUndo_empty_history_returns_false);
    RUN_TEST(test_tabUndo_sets_dirty);

    // tabRedo
    RUN_TEST(test_tabRedo_after_undo_insert_char);
    RUN_TEST(test_tabRedo_after_undo_delete_char);
    RUN_TEST(test_tabRedo_after_undo_insert_cr);
    RUN_TEST(test_tabRedo_after_undo_delete_cr);
    RUN_TEST(test_tabRedo_empty_redo_path_returns_false);
    RUN_TEST(test_tabRedo_sets_dirty);

    // New edit creates branch (undo tree)
    RUN_TEST(test_new_edit_after_undo_creates_branch_not_erase);

    // Round-trip undo/redo
    RUN_TEST(test_full_undo_redo_round_trip);
    RUN_TEST(test_undo_redo_with_cr);

    // Vim change-group via mode transitions
    RUN_TEST(test_insert_mode_groups_chars_as_one_undo_step);
    RUN_TEST(test_two_insert_sessions_two_undo_steps);
    RUN_TEST(test_undo_during_insert_mode_closes_group_first);

    // tab_open
    RUN_TEST(test_tab_open_loads_file_content);
    RUN_TEST(test_tab_open_resets_history);
    RUN_TEST(test_tab_open_resets_mode_to_normal);
    RUN_TEST(test_tab_open_null_path_returns_false);
    RUN_TEST(test_tab_open_bad_path_returns_false);

    // tab_save / tab_save_as
    RUN_TEST(test_tab_save_as_writes_file);
    RUN_TEST(test_tab_save_as_clears_dirty);
    RUN_TEST(test_tab_save_as_updates_filepath);
    RUN_TEST(test_tab_save_uses_existing_filepath);
    RUN_TEST(test_tab_save_null_filepath_returns_false);
    RUN_TEST(test_tab_save_as_null_path_returns_false);

    // Round-trip save/open
    RUN_TEST(test_save_open_round_trip);

    return UNITY_END();
}