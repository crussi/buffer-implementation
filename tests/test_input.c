// test_input.c
//
// Updated for the Vim-modal input refactor.
//
// Key changes from the old tests:
//
//  1. input_handle_key() is now modal and takes (Tab*, EditorApp*, int).
//     App is passed as NULL for unit tests (no command-mode dispatch needed).
//
//  2. Printable characters are only inserted when the tab is in INSERT mode.
//     Normal mode uses 'i'/'a' etc. to enter Insert, and 'u'/Ctrl-R for
//     undo/redo.
//
//  3. Arrow keys (KEY_LEFT/UP/DOWN/RIGHT) work in both Normal and Insert mode.
//
//  4. Backspace, Delete, and Enter keys work in INSERT mode only.
//
//  5. Undo is 'u' in Normal mode; Ctrl-R is redo in Normal mode.
//     The old Ctrl-Z / Ctrl-Y bindings no longer exist in the new handler.
//
//  6. Vim change-group semantics: all chars typed in one Insert session are
//     one undo step.  Tests reflect this.
//
//  7. Mouse clicks return the tab to Normal mode (from Visual/Command).
//     Insert mode is NOT exited on a mouse click (matches Vim behaviour).

#include "unity.h"
#include "tab.h"
#include "editor_cursor.h"
#include "input.h"
#include <ncurses.h>
#include <string.h>
#include <stdlib.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Dispatch a single key through the modal handler with no app context.
static void send_key(Tab *t, int key) {
    input_handle_key(t, NULL, key);
}

// Put tab into Insert mode and type a string character by character.
static void insert_string(Tab *t, const char *s) {
    tab_enter_insert_mode(t);
    for (int i = 0; s[i]; i++)
        send_key(t, (unsigned char)s[i]);
}

// Leave Insert mode (simulates pressing Esc).
static void leave_insert(Tab *t) {
    send_key(t, 27);   // Escape
}

static Tab *make_tab_with_line(const char *text) {
    Tab *t = tab_new_empty();
    for (int i = 0; text[i]; i++)
        tabInsertChar(t, 0, i, text[i]);
    cursor_init(&t->cursor);
    return t;
}

static Tab *make_tab_with_lines(const char **lines, int count) {
    Tab *t = tab_new_empty();
    for (int i = 0; lines[0][i]; i++)
        tabInsertChar(t, 0, i, lines[0][i]);
    for (int r = 1; r < count; r++) {
        tabInsertCR(t, r - 1, t->buf->rows[r - 1].length);
        for (int i = 0; lines[r][i]; i++)
            tabInsertChar(t, r, i, lines[r][i]);
    }
    cursor_init(&t->cursor);
    return t;
}

void setUp(void)    {}
void tearDown(void) {}

// ---------------------------------------------------------------------------
// Arrow keys work in Normal mode
// ---------------------------------------------------------------------------

void test_key_left_normal_mode_moves_cursor_left(void) {
    Tab *t = make_tab_with_line("Hello");
    t->cursor.pos.col = 3;
    send_key(t, KEY_LEFT);
    TEST_ASSERT_EQUAL_INT(2, t->cursor.pos.col);
    tab_free(t);
}

void test_key_left_wraps_to_previous_row_normal(void) {
    const char *lines[] = { "Hello", "World" };
    Tab *t = make_tab_with_lines(lines, 2);
    t->cursor.pos.row = 1;
    t->cursor.pos.col = 0;
    send_key(t, KEY_LEFT);
    TEST_ASSERT_EQUAL_INT(0, t->cursor.pos.row);
    TEST_ASSERT_EQUAL_INT(5, t->cursor.pos.col);
    tab_free(t);
}

void test_key_left_at_origin_does_nothing(void) {
    Tab *t = make_tab_with_line("Hello");
    send_key(t, KEY_LEFT);
    TEST_ASSERT_EQUAL_INT(0, t->cursor.pos.row);
    TEST_ASSERT_EQUAL_INT(0, t->cursor.pos.col);
    tab_free(t);
}

void test_key_right_normal_mode_moves_cursor_right(void) {
    Tab *t = make_tab_with_line("Hello");
    t->cursor.pos.col = 2;
    send_key(t, KEY_RIGHT);
    TEST_ASSERT_EQUAL_INT(3, t->cursor.pos.col);
    tab_free(t);
}

void test_key_right_wraps_to_next_row_normal(void) {
    const char *lines[] = { "Hi", "There" };
    Tab *t = make_tab_with_lines(lines, 2);
    t->cursor.pos.row = 0;
    t->cursor.pos.col = 2;
    send_key(t, KEY_RIGHT);
    TEST_ASSERT_EQUAL_INT(1, t->cursor.pos.row);
    TEST_ASSERT_EQUAL_INT(0, t->cursor.pos.col);
    tab_free(t);
}

void test_key_right_at_end_of_buffer_does_nothing(void) {
    Tab *t = make_tab_with_line("Hi");
    t->cursor.pos.col = 2;
    send_key(t, KEY_RIGHT);
    TEST_ASSERT_EQUAL_INT(0, t->cursor.pos.row);
    TEST_ASSERT_EQUAL_INT(2, t->cursor.pos.col);
    tab_free(t);
}

void test_key_up_moves_cursor_up(void) {
    const char *lines[] = { "Hello", "World" };
    Tab *t = make_tab_with_lines(lines, 2);
    t->cursor.pos.row     = 1;
    t->cursor.pos.col     = 3;
    t->cursor.desired_col = 3;
    send_key(t, KEY_UP);
    TEST_ASSERT_EQUAL_INT(0, t->cursor.pos.row);
    TEST_ASSERT_EQUAL_INT(3, t->cursor.pos.col);
    tab_free(t);
}

void test_key_up_at_first_row_does_nothing(void) {
    Tab *t = make_tab_with_line("Hello");
    t->cursor.pos.col = 2;
    send_key(t, KEY_UP);
    TEST_ASSERT_EQUAL_INT(0, t->cursor.pos.row);
    TEST_ASSERT_EQUAL_INT(2, t->cursor.pos.col);
    tab_free(t);
}

void test_key_down_moves_cursor_down(void) {
    const char *lines[] = { "Hello", "World" };
    Tab *t = make_tab_with_lines(lines, 2);
    t->cursor.pos.row     = 0;
    t->cursor.pos.col     = 3;
    t->cursor.desired_col = 3;
    send_key(t, KEY_DOWN);
    TEST_ASSERT_EQUAL_INT(1, t->cursor.pos.row);
    TEST_ASSERT_EQUAL_INT(3, t->cursor.pos.col);
    tab_free(t);
}

void test_key_down_at_last_row_does_nothing(void) {
    Tab *t = make_tab_with_line("Hello");
    t->cursor.pos.col = 2;
    send_key(t, KEY_DOWN);
    TEST_ASSERT_EQUAL_INT(0, t->cursor.pos.row);
    TEST_ASSERT_EQUAL_INT(2, t->cursor.pos.col);
    tab_free(t);
}

// ---------------------------------------------------------------------------
// Normal-mode navigation keys (hjkl, 0, $)
// ---------------------------------------------------------------------------

void test_h_moves_cursor_left_in_normal_mode(void) {
    Tab *t = make_tab_with_line("Hello");
    t->cursor.pos.col = 3;
    send_key(t, 'h');
    TEST_ASSERT_EQUAL_INT(2, t->cursor.pos.col);
    tab_free(t);
}

void test_l_moves_cursor_right_in_normal_mode(void) {
    Tab *t = make_tab_with_line("Hello");
    t->cursor.pos.col = 2;
    send_key(t, 'l');
    TEST_ASSERT_EQUAL_INT(3, t->cursor.pos.col);
    tab_free(t);
}

void test_zero_moves_to_start_of_line(void) {
    Tab *t = make_tab_with_line("Hello");
    t->cursor.pos.col = 4;
    send_key(t, '0');
    TEST_ASSERT_EQUAL_INT(0, t->cursor.pos.col);
    tab_free(t);
}

// ---------------------------------------------------------------------------
// Entering Insert mode via 'i'
// ---------------------------------------------------------------------------

void test_i_enters_insert_mode(void) {
    Tab *t = tab_new_empty();
    TEST_ASSERT_EQUAL_INT(MODE_NORMAL, t->mode);
    send_key(t, 'i');
    TEST_ASSERT_EQUAL_INT(MODE_INSERT, t->mode);
    tab_free(t);
}

void test_escape_leaves_insert_mode(void) {
    Tab *t = tab_new_empty();
    send_key(t, 'i');
    TEST_ASSERT_EQUAL_INT(MODE_INSERT, t->mode);
    send_key(t, 27);
    TEST_ASSERT_EQUAL_INT(MODE_NORMAL, t->mode);
    tab_free(t);
}

// ---------------------------------------------------------------------------
// Printable characters insert only in Insert mode
// ---------------------------------------------------------------------------

void test_printable_char_inserts_in_insert_mode(void) {
    Tab *t = tab_new_empty();
    send_key(t, 'i');   // enter Insert
    send_key(t, 'A');
    TEST_ASSERT_EQUAL_STRING("A", t->buf->rows[0].line);
    TEST_ASSERT_EQUAL_INT(1, t->cursor.pos.col);
    tab_free(t);
}

void test_printable_chars_build_word_in_insert_mode(void) {
    Tab *t = tab_new_empty();
    send_key(t, 'i');
    const char *word = "Hello";
    for (int i = 0; word[i]; i++)
        send_key(t, word[i]);
    TEST_ASSERT_EQUAL_STRING("Hello", t->buf->rows[0].line);
    TEST_ASSERT_EQUAL_INT(5, t->cursor.pos.col);
    tab_free(t);
}

void test_printable_char_ignored_in_normal_mode(void) {
    // 'Q' has no Normal-mode binding — buffer must remain unchanged.
    Tab *t = tab_new_empty();
    send_key(t, 'Q');
    TEST_ASSERT_EQUAL_STRING("", t->buf->rows[0].line);
    TEST_ASSERT_EQUAL_INT(0, t->cursor.pos.col);
    tab_free(t);
}

void test_non_printable_below_32_ignored_in_insert_mode(void) {
    Tab *t = tab_new_empty();
    send_key(t, 'i');
    send_key(t, 1);   // Ctrl-A — no binding
    TEST_ASSERT_EQUAL_STRING("", t->buf->rows[0].line);
    TEST_ASSERT_EQUAL_INT(0, t->cursor.pos.col);
    tab_free(t);
}

void test_del_127_is_backspace_in_insert_mode(void) {
    Tab *t = make_tab_with_line("Hi");
    send_key(t, 'i');   // enter Insert
    t->cursor.pos.col = 2;
    send_key(t, 127);
    TEST_ASSERT_EQUAL_STRING("H", t->buf->rows[0].line);
    tab_free(t);
}

// ---------------------------------------------------------------------------
// Backspace in Insert mode -- all three key codes
// ---------------------------------------------------------------------------

void test_backspace_KEY_BACKSPACE_deletes_left_insert(void) {
    Tab *t = make_tab_with_line("Hello");
    send_key(t, 'i');
    t->cursor.pos.col = 5;
    send_key(t, KEY_BACKSPACE);
    TEST_ASSERT_EQUAL_STRING("Hell", t->buf->rows[0].line);
    TEST_ASSERT_EQUAL_INT(4, t->cursor.pos.col);
    tab_free(t);
}

void test_backspace_ctrl_h_deletes_left_insert(void) {
    Tab *t = make_tab_with_line("Hello");
    send_key(t, 'i');
    t->cursor.pos.col = 5;
    send_key(t, '\b');
    TEST_ASSERT_EQUAL_STRING("Hell", t->buf->rows[0].line);
    TEST_ASSERT_EQUAL_INT(4, t->cursor.pos.col);
    tab_free(t);
}

void test_backspace_127_deletes_left_insert(void) {
    Tab *t = make_tab_with_line("Hello");
    send_key(t, 'i');
    t->cursor.pos.col = 5;
    send_key(t, 127);
    TEST_ASSERT_EQUAL_STRING("Hell", t->buf->rows[0].line);
    TEST_ASSERT_EQUAL_INT(4, t->cursor.pos.col);
    tab_free(t);
}

void test_backspace_at_col0_merges_rows_insert(void) {
    const char *lines[] = { "Hello", "World" };
    Tab *t = make_tab_with_lines(lines, 2);
    send_key(t, 'i');
    t->cursor.pos.row = 1;
    t->cursor.pos.col = 0;
    send_key(t, KEY_BACKSPACE);
    TEST_ASSERT_EQUAL_INT(1, t->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("HelloWorld", t->buf->rows[0].line);
    TEST_ASSERT_EQUAL_INT(0, t->cursor.pos.row);
    TEST_ASSERT_EQUAL_INT(5, t->cursor.pos.col);
    tab_free(t);
}

void test_backspace_at_origin_does_nothing_insert(void) {
    Tab *t = make_tab_with_line("Hello");
    send_key(t, 'i');
    // cursor at (0,0)
    send_key(t, KEY_BACKSPACE);
    TEST_ASSERT_EQUAL_STRING("Hello", t->buf->rows[0].line);
    TEST_ASSERT_EQUAL_INT(0, t->cursor.pos.row);
    TEST_ASSERT_EQUAL_INT(0, t->cursor.pos.col);
    tab_free(t);
}

// ---------------------------------------------------------------------------
// Delete key (KEY_DC) in Insert mode
// ---------------------------------------------------------------------------

void test_delete_removes_char_at_cursor_insert(void) {
    Tab *t = make_tab_with_line("Hello");
    send_key(t, 'i');
    t->cursor.pos.col = 0;
    send_key(t, KEY_DC);
    TEST_ASSERT_EQUAL_STRING("ello", t->buf->rows[0].line);
    TEST_ASSERT_EQUAL_INT(0, t->cursor.pos.col);
    tab_free(t);
}

void test_delete_mid_line_insert(void) {
    Tab *t = make_tab_with_line("Hello");
    send_key(t, 'i');
    t->cursor.pos.col = 2;
    send_key(t, KEY_DC);
    TEST_ASSERT_EQUAL_STRING("Helo", t->buf->rows[0].line);
    TEST_ASSERT_EQUAL_INT(2, t->cursor.pos.col);
    tab_free(t);
}

void test_delete_at_end_of_line_does_nothing_insert(void) {
    Tab *t = make_tab_with_line("Hi");
    send_key(t, 'i');
    t->cursor.pos.col = 2;
    send_key(t, KEY_DC);
    TEST_ASSERT_EQUAL_STRING("Hi", t->buf->rows[0].line);
    tab_free(t);
}

// ---------------------------------------------------------------------------
// Enter key in Insert mode -- all three codes
// ---------------------------------------------------------------------------

void test_enter_newline_splits_line_insert(void) {
    Tab *t = make_tab_with_line("Hello");
    send_key(t, 'i');
    t->cursor.pos.col = 3;
    send_key(t, '\n');
    TEST_ASSERT_EQUAL_INT(2, t->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("Hel", t->buf->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("lo",  t->buf->rows[1].line);
    TEST_ASSERT_EQUAL_INT(1, t->cursor.pos.row);
    TEST_ASSERT_EQUAL_INT(0, t->cursor.pos.col);
    tab_free(t);
}

void test_enter_KEY_ENTER_splits_line_insert(void) {
    Tab *t = make_tab_with_line("Hello");
    send_key(t, 'i');
    t->cursor.pos.col = 3;
    send_key(t, KEY_ENTER);
    TEST_ASSERT_EQUAL_INT(2, t->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("Hel", t->buf->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("lo",  t->buf->rows[1].line);
    tab_free(t);
}

void test_enter_cr_splits_line_insert(void) {
    Tab *t = make_tab_with_line("Hello");
    send_key(t, 'i');
    t->cursor.pos.col = 3;
    send_key(t, '\r');
    TEST_ASSERT_EQUAL_INT(2, t->buf->numrows);
    tab_free(t);
}

void test_enter_at_end_creates_empty_row_insert(void) {
    Tab *t = make_tab_with_line("Hello");
    send_key(t, 'i');
    t->cursor.pos.col = 5;
    send_key(t, '\n');
    TEST_ASSERT_EQUAL_INT(2, t->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("Hello", t->buf->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("",      t->buf->rows[1].line);
    TEST_ASSERT_EQUAL_INT(0, t->cursor.desired_col);
    tab_free(t);
}

void test_enter_at_start_creates_empty_first_row_insert(void) {
    Tab *t = make_tab_with_line("Hello");
    send_key(t, 'i');
    // cursor at (0,0)
    send_key(t, '\n');
    TEST_ASSERT_EQUAL_INT(2, t->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("",      t->buf->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("Hello", t->buf->rows[1].line);
    tab_free(t);
}

// ---------------------------------------------------------------------------
// Normal-mode 'x' -- delete char under cursor
// ---------------------------------------------------------------------------

void test_x_deletes_char_under_cursor(void) {
    Tab *t = make_tab_with_line("Hello");
    t->cursor.pos.col = 0;
    send_key(t, 'x');
    TEST_ASSERT_EQUAL_STRING("ello", t->buf->rows[0].line);
    tab_free(t);
}

void test_x_at_end_of_line_does_nothing(void) {
    Tab *t = make_tab_with_line("Hi");
    t->cursor.pos.col = 2;  // past last char
    send_key(t, 'x');
    TEST_ASSERT_EQUAL_STRING("Hi", t->buf->rows[0].line);
    tab_free(t);
}

// ---------------------------------------------------------------------------
// Undo ('u') and Redo (Ctrl-R) in Normal mode
// ---------------------------------------------------------------------------

void test_u_undoes_insert_session(void) {
    Tab *t = tab_new_empty();
    insert_string(t, "A");
    leave_insert(t);
    TEST_ASSERT_EQUAL_STRING("A", t->buf->rows[0].line);

    send_key(t, 'u');
    TEST_ASSERT_EQUAL_STRING("", t->buf->rows[0].line);
    tab_free(t);
}

void test_ctrl_r_redoes_insert_session(void) {
    Tab *t = tab_new_empty();
    insert_string(t, "B");
    leave_insert(t);
    send_key(t, 'u');              // undo
    send_key(t, 'r' & 0x1f);      // Ctrl-R redo
    TEST_ASSERT_EQUAL_STRING("B", t->buf->rows[0].line);
    tab_free(t);
}

void test_undo_redo_sequence_normal_mode(void) {
    Tab *t = tab_new_empty();

    // Three separate Insert sessions
    t->cursor.pos.col = 0;
    send_key(t, 'i');
    send_key(t, 'A');
    send_key(t, 27);   // Esc

    t->cursor.pos.col = t->buf->rows[0].length;
    send_key(t, 'i');
    send_key(t, 'B');
    send_key(t, 27);

    t->cursor.pos.col = t->buf->rows[0].length;
    send_key(t, 'i');
    send_key(t, 'C');
    send_key(t, 27);

    TEST_ASSERT_EQUAL_STRING("ABC", t->buf->rows[0].line);

    send_key(t, 'u');
    TEST_ASSERT_EQUAL_STRING("AB", t->buf->rows[0].line);

    send_key(t, 'u');
    TEST_ASSERT_EQUAL_STRING("A", t->buf->rows[0].line);

    send_key(t, 'r' & 0x1f);
    TEST_ASSERT_EQUAL_STRING("AB", t->buf->rows[0].line);

    send_key(t, 'r' & 0x1f);
    TEST_ASSERT_EQUAL_STRING("ABC", t->buf->rows[0].line);

    tab_free(t);
}

void test_whole_insert_session_undone_in_one_step(void) {
    Tab *t = tab_new_empty();
    insert_string(t, "Hello");
    leave_insert(t);
    TEST_ASSERT_EQUAL_STRING("Hello", t->buf->rows[0].line);

    send_key(t, 'u');
    TEST_ASSERT_EQUAL_STRING("", t->buf->rows[0].line);
    TEST_ASSERT_FALSE(tabUndo(t));
    tab_free(t);
}

void test_new_insert_after_undo_creates_new_branch(void) {
    Tab *t = tab_new_empty();
    insert_string(t, "A"); leave_insert(t);
    send_key(t, 'u');   // undo

    insert_string(t, "B"); leave_insert(t);
    // Redo on new branch should have no children.
    send_key(t, 'r' & 0x1f);   // should be a no-op
    TEST_ASSERT_EQUAL_STRING("B", t->buf->rows[0].line);
    tab_free(t);
}

// ---------------------------------------------------------------------------
// Mouse
// ---------------------------------------------------------------------------

void test_mouse_sets_cursor_position(void) {
    const char *lines[] = { "Hello", "World" };
    Tab *t = make_tab_with_lines(lines, 2);
    input_handle_mouse(t, 1, 3);
    TEST_ASSERT_EQUAL_INT(1, t->cursor.pos.row);
    TEST_ASSERT_EQUAL_INT(3, t->cursor.pos.col);
    TEST_ASSERT_EQUAL_INT(3, t->cursor.desired_col);
    tab_free(t);
}

void test_mouse_clamps_col_to_line_length(void) {
    Tab *t = make_tab_with_line("Hi");
    input_handle_mouse(t, 0, 99);
    TEST_ASSERT_EQUAL_INT(2, t->cursor.pos.col);
    TEST_ASSERT_EQUAL_INT(2, t->cursor.desired_col);
    tab_free(t);
}

void test_mouse_clamps_row_to_buffer_bounds(void) {
    Tab *t = make_tab_with_line("Hi");
    input_handle_mouse(t, 99, 0);
    TEST_ASSERT_EQUAL_INT(0, t->cursor.pos.row);
    tab_free(t);
}

void test_mouse_does_not_exit_insert_mode(void) {
    // Vim does NOT close Insert mode on a mouse click.
    Tab *t = make_tab_with_line("Hi");
    tab_enter_insert_mode(t);
    TEST_ASSERT_EQUAL_INT(MODE_INSERT, t->mode);
    input_handle_mouse(t, 0, 0);
    TEST_ASSERT_EQUAL_INT(MODE_INSERT, t->mode);
    tab_free(t);
}

void test_mouse_exits_visual_mode(void) {
    Tab *t = make_tab_with_line("Hi");
    tab_enter_visual_mode(t);
    TEST_ASSERT_EQUAL_INT(MODE_VISUAL, t->mode);
    input_handle_mouse(t, 0, 0);
    TEST_ASSERT_EQUAL_INT(MODE_NORMAL, t->mode);
    tab_free(t);
}

void test_mouse_then_arrow_uses_desired_col(void) {
    const char *lines[] = { "Hello", "Hi", "World" };
    Tab *t = make_tab_with_lines(lines, 3);

    input_handle_mouse(t, 0, 4);
    TEST_ASSERT_EQUAL_INT(4, t->cursor.desired_col);

    // Down to "Hi" (length 2) — col clamps but desired_col preserved.
    send_key(t, KEY_DOWN);
    TEST_ASSERT_EQUAL_INT(2, t->cursor.pos.col);
    TEST_ASSERT_EQUAL_INT(4, t->cursor.desired_col);

    // Down to "World" (length 5) — col restores to 4.
    send_key(t, KEY_DOWN);
    TEST_ASSERT_EQUAL_INT(4, t->cursor.pos.col);

    tab_free(t);
}

// ---------------------------------------------------------------------------
// Arrow keys in Insert mode (should still move cursor)
// ---------------------------------------------------------------------------

void test_arrow_keys_work_in_insert_mode(void) {
    const char *lines[] = { "Hello", "World" };
    Tab *t = make_tab_with_lines(lines, 2);
    t->cursor.pos.row = 1;
    t->cursor.pos.col = 3;
    t->cursor.desired_col = 3;

    send_key(t, 'i');   // enter Insert
    send_key(t, KEY_UP);
    TEST_ASSERT_EQUAL_INT(0, t->cursor.pos.row);
    send_key(t, KEY_RIGHT);
    TEST_ASSERT_EQUAL_INT(4, t->cursor.pos.col);
    tab_free(t);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(void) {
    UNITY_BEGIN();

    // Arrow keys (Normal mode)
    RUN_TEST(test_key_left_normal_mode_moves_cursor_left);
    RUN_TEST(test_key_left_wraps_to_previous_row_normal);
    RUN_TEST(test_key_left_at_origin_does_nothing);
    RUN_TEST(test_key_right_normal_mode_moves_cursor_right);
    RUN_TEST(test_key_right_wraps_to_next_row_normal);
    RUN_TEST(test_key_right_at_end_of_buffer_does_nothing);
    RUN_TEST(test_key_up_moves_cursor_up);
    RUN_TEST(test_key_up_at_first_row_does_nothing);
    RUN_TEST(test_key_down_moves_cursor_down);
    RUN_TEST(test_key_down_at_last_row_does_nothing);

    // Normal-mode navigation
    RUN_TEST(test_h_moves_cursor_left_in_normal_mode);
    RUN_TEST(test_l_moves_cursor_right_in_normal_mode);
    RUN_TEST(test_zero_moves_to_start_of_line);

    // Mode switching
    RUN_TEST(test_i_enters_insert_mode);
    RUN_TEST(test_escape_leaves_insert_mode);

    // Printable characters
    RUN_TEST(test_printable_char_inserts_in_insert_mode);
    RUN_TEST(test_printable_chars_build_word_in_insert_mode);
    RUN_TEST(test_printable_char_ignored_in_normal_mode);
    RUN_TEST(test_non_printable_below_32_ignored_in_insert_mode);
    RUN_TEST(test_del_127_is_backspace_in_insert_mode);

    // Backspace (Insert mode)
    RUN_TEST(test_backspace_KEY_BACKSPACE_deletes_left_insert);
    RUN_TEST(test_backspace_ctrl_h_deletes_left_insert);
    RUN_TEST(test_backspace_127_deletes_left_insert);
    RUN_TEST(test_backspace_at_col0_merges_rows_insert);
    RUN_TEST(test_backspace_at_origin_does_nothing_insert);

    // Delete key (Insert mode)
    RUN_TEST(test_delete_removes_char_at_cursor_insert);
    RUN_TEST(test_delete_mid_line_insert);
    RUN_TEST(test_delete_at_end_of_line_does_nothing_insert);

    // Enter (Insert mode)
    RUN_TEST(test_enter_newline_splits_line_insert);
    RUN_TEST(test_enter_KEY_ENTER_splits_line_insert);
    RUN_TEST(test_enter_cr_splits_line_insert);
    RUN_TEST(test_enter_at_end_creates_empty_row_insert);
    RUN_TEST(test_enter_at_start_creates_empty_first_row_insert);

    // Normal-mode 'x'
    RUN_TEST(test_x_deletes_char_under_cursor);
    RUN_TEST(test_x_at_end_of_line_does_nothing);

    // Undo / Redo (Normal mode)
    RUN_TEST(test_u_undoes_insert_session);
    RUN_TEST(test_ctrl_r_redoes_insert_session);
    RUN_TEST(test_undo_redo_sequence_normal_mode);
    RUN_TEST(test_whole_insert_session_undone_in_one_step);
    RUN_TEST(test_new_insert_after_undo_creates_new_branch);

    // Mouse
    RUN_TEST(test_mouse_sets_cursor_position);
    RUN_TEST(test_mouse_clamps_col_to_line_length);
    RUN_TEST(test_mouse_clamps_row_to_buffer_bounds);
    RUN_TEST(test_mouse_does_not_exit_insert_mode);
    RUN_TEST(test_mouse_exits_visual_mode);
    RUN_TEST(test_mouse_then_arrow_uses_desired_col);

    // Arrow keys in Insert mode
    RUN_TEST(test_arrow_keys_work_in_insert_mode);

    return UNITY_END();
}