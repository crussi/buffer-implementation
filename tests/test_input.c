#include "unity.h"
#include "tab.h"
#include "editor_cursor.h"
#include "input.h"
#include <ncurses.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

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
// Arrow keys
// ---------------------------------------------------------------------------

void test_key_left_moves_cursor_left(void) {
    Tab *t = make_tab_with_line("Hello");
    t->cursor.pos.col = 3;
    input_handle_key(t, KEY_LEFT);
    TEST_ASSERT_EQUAL_INT(2, t->cursor.pos.col);
    tab_free(t);
}

void test_key_left_wraps_to_previous_row(void) {
    const char *lines[] = { "Hello", "World" };
    Tab *t = make_tab_with_lines(lines, 2);
    t->cursor.pos.row = 1;
    t->cursor.pos.col = 0;
    input_handle_key(t, KEY_LEFT);
    TEST_ASSERT_EQUAL_INT(0, t->cursor.pos.row);
    TEST_ASSERT_EQUAL_INT(5, t->cursor.pos.col);
    tab_free(t);
}

void test_key_left_at_origin_does_nothing(void) {
    Tab *t = make_tab_with_line("Hello");
    input_handle_key(t, KEY_LEFT);
    TEST_ASSERT_EQUAL_INT(0, t->cursor.pos.row);
    TEST_ASSERT_EQUAL_INT(0, t->cursor.pos.col);
    tab_free(t);
}

void test_key_right_moves_cursor_right(void) {
    Tab *t = make_tab_with_line("Hello");
    t->cursor.pos.col = 2;
    input_handle_key(t, KEY_RIGHT);
    TEST_ASSERT_EQUAL_INT(3, t->cursor.pos.col);
    tab_free(t);
}

void test_key_right_wraps_to_next_row(void) {
    const char *lines[] = { "Hi", "There" };
    Tab *t = make_tab_with_lines(lines, 2);
    t->cursor.pos.row = 0;
    t->cursor.pos.col = 2;   // end of "Hi"
    input_handle_key(t, KEY_RIGHT);
    TEST_ASSERT_EQUAL_INT(1, t->cursor.pos.row);
    TEST_ASSERT_EQUAL_INT(0, t->cursor.pos.col);
    tab_free(t);
}

void test_key_right_at_end_of_buffer_does_nothing(void) {
    Tab *t = make_tab_with_line("Hi");
    t->cursor.pos.col = 2;
    input_handle_key(t, KEY_RIGHT);
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
    input_handle_key(t, KEY_UP);
    TEST_ASSERT_EQUAL_INT(0, t->cursor.pos.row);
    TEST_ASSERT_EQUAL_INT(3, t->cursor.pos.col);
    tab_free(t);
}

void test_key_up_at_first_row_does_nothing(void) {
    Tab *t = make_tab_with_line("Hello");
    t->cursor.pos.col = 2;
    input_handle_key(t, KEY_UP);
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
    input_handle_key(t, KEY_DOWN);
    TEST_ASSERT_EQUAL_INT(1, t->cursor.pos.row);
    TEST_ASSERT_EQUAL_INT(3, t->cursor.pos.col);
    tab_free(t);
}

void test_key_down_at_last_row_does_nothing(void) {
    Tab *t = make_tab_with_line("Hello");
    t->cursor.pos.col = 2;
    input_handle_key(t, KEY_DOWN);
    TEST_ASSERT_EQUAL_INT(0, t->cursor.pos.row);
    TEST_ASSERT_EQUAL_INT(2, t->cursor.pos.col);
    tab_free(t);
}

// ---------------------------------------------------------------------------
// Printable characters
// ---------------------------------------------------------------------------

void test_printable_char_inserts_at_cursor(void) {
    Tab *t = tab_new_empty();
    input_handle_key(t, 'A');
    TEST_ASSERT_EQUAL_STRING("A", t->buf->rows[0].line);
    TEST_ASSERT_EQUAL_INT(1, t->cursor.pos.col);
    tab_free(t);
}

void test_printable_chars_build_word(void) {
    Tab *t = tab_new_empty();
    const char *word = "Hello";
    for (int i = 0; word[i]; i++)
        input_handle_key(t, word[i]);
    TEST_ASSERT_EQUAL_STRING("Hello", t->buf->rows[0].line);
    TEST_ASSERT_EQUAL_INT(5, t->cursor.pos.col);
    tab_free(t);
}

void test_printable_char_inserts_mid_line(void) {
    Tab *t = make_tab_with_line("Helo");
    t->cursor.pos.col = 3;
    input_handle_key(t, 'l');
    TEST_ASSERT_EQUAL_STRING("Hello", t->buf->rows[0].line);
    TEST_ASSERT_EQUAL_INT(4, t->cursor.pos.col);
    tab_free(t);
}

void test_non_printable_below_32_ignored(void) {
    Tab *t = tab_new_empty();
    input_handle_key(t, 1);    // Ctrl-A (not a bound key in our handler)
    TEST_ASSERT_EQUAL_STRING("", t->buf->rows[0].line);
    TEST_ASSERT_EQUAL_INT(0, t->cursor.pos.col);
    tab_free(t);
}

void test_del_127_ignored_as_printable(void) {
    // 127 is routed to backspace logic, not inserted as a character
    Tab *t = make_tab_with_line("Hi");
    t->cursor.pos.col = 2;
    input_handle_key(t, 127);
    // Should delete 'i', not insert char 127
    TEST_ASSERT_EQUAL_STRING("H", t->buf->rows[0].line);
    tab_free(t);
}

// ---------------------------------------------------------------------------
// Backspace -- all three key codes
// ---------------------------------------------------------------------------

void test_backspace_KEY_BACKSPACE_deletes_left(void) {
    Tab *t = make_tab_with_line("Hello");
    t->cursor.pos.col = 5;
    input_handle_key(t, KEY_BACKSPACE);
    TEST_ASSERT_EQUAL_STRING("Hell", t->buf->rows[0].line);
    TEST_ASSERT_EQUAL_INT(4, t->cursor.pos.col);
    tab_free(t);
}

void test_backspace_ctrl_h_deletes_left(void) {
    Tab *t = make_tab_with_line("Hello");
    t->cursor.pos.col = 5;
    input_handle_key(t, '\b');
    TEST_ASSERT_EQUAL_STRING("Hell", t->buf->rows[0].line);
    TEST_ASSERT_EQUAL_INT(4, t->cursor.pos.col);
    tab_free(t);
}

void test_backspace_127_deletes_left(void) {
    Tab *t = make_tab_with_line("Hello");
    t->cursor.pos.col = 5;
    input_handle_key(t, 127);
    TEST_ASSERT_EQUAL_STRING("Hell", t->buf->rows[0].line);
    TEST_ASSERT_EQUAL_INT(4, t->cursor.pos.col);
    tab_free(t);
}

void test_backspace_at_col0_merges_rows(void) {
    const char *lines[] = { "Hello", "World" };
    Tab *t = make_tab_with_lines(lines, 2);
    t->cursor.pos.row = 1;
    t->cursor.pos.col = 0;
    input_handle_key(t, KEY_BACKSPACE);
    TEST_ASSERT_EQUAL_INT(1, t->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("HelloWorld", t->buf->rows[0].line);
    TEST_ASSERT_EQUAL_INT(0, t->cursor.pos.row);
    TEST_ASSERT_EQUAL_INT(5, t->cursor.pos.col);
    tab_free(t);
}

void test_backspace_at_origin_does_nothing(void) {
    Tab *t = make_tab_with_line("Hello");
    // cursor already at (0,0)
    input_handle_key(t, KEY_BACKSPACE);
    TEST_ASSERT_EQUAL_STRING("Hello", t->buf->rows[0].line);
    TEST_ASSERT_EQUAL_INT(0, t->cursor.pos.row);
    TEST_ASSERT_EQUAL_INT(0, t->cursor.pos.col);
    tab_free(t);
}

// ---------------------------------------------------------------------------
// Delete key (KEY_DC)
// ---------------------------------------------------------------------------

void test_delete_removes_char_at_cursor(void) {
    Tab *t = make_tab_with_line("Hello");
    t->cursor.pos.col = 0;
    input_handle_key(t, KEY_DC);
    TEST_ASSERT_EQUAL_STRING("ello", t->buf->rows[0].line);
    TEST_ASSERT_EQUAL_INT(0, t->cursor.pos.col);
    tab_free(t);
}

void test_delete_mid_line(void) {
    Tab *t = make_tab_with_line("Hello");
    t->cursor.pos.col = 2;
    input_handle_key(t, KEY_DC);
    TEST_ASSERT_EQUAL_STRING("Helo", t->buf->rows[0].line);
    TEST_ASSERT_EQUAL_INT(2, t->cursor.pos.col);
    tab_free(t);
}

void test_delete_at_end_of_line_does_nothing(void) {
    Tab *t = make_tab_with_line("Hi");
    t->cursor.pos.col = 2;   // past last char
    input_handle_key(t, KEY_DC);
    TEST_ASSERT_EQUAL_STRING("Hi", t->buf->rows[0].line);
    tab_free(t);
}

// ---------------------------------------------------------------------------
// Enter key -- all three codes
// ---------------------------------------------------------------------------

void test_enter_newline_splits_line(void) {
    Tab *t = make_tab_with_line("Hello");
    t->cursor.pos.col = 3;
    input_handle_key(t, '\n');
    TEST_ASSERT_EQUAL_INT(2, t->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("Hel", t->buf->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("lo",  t->buf->rows[1].line);
    TEST_ASSERT_EQUAL_INT(1, t->cursor.pos.row);
    TEST_ASSERT_EQUAL_INT(0, t->cursor.pos.col);
    tab_free(t);
}

void test_enter_KEY_ENTER_splits_line(void) {
    Tab *t = make_tab_with_line("Hello");
    t->cursor.pos.col = 3;
    input_handle_key(t, KEY_ENTER);
    TEST_ASSERT_EQUAL_INT(2, t->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("Hel", t->buf->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("lo",  t->buf->rows[1].line);
    tab_free(t);
}

void test_enter_cr_splits_line(void) {
    Tab *t = make_tab_with_line("Hello");
    t->cursor.pos.col = 3;
    input_handle_key(t, '\r');
    TEST_ASSERT_EQUAL_INT(2, t->buf->numrows);
    tab_free(t);
}

void test_enter_at_end_creates_empty_row(void) {
    Tab *t = make_tab_with_line("Hello");
    t->cursor.pos.col = 5;
    input_handle_key(t, '\n');
    TEST_ASSERT_EQUAL_INT(2, t->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("Hello", t->buf->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("",      t->buf->rows[1].line);
    TEST_ASSERT_EQUAL_INT(0, t->cursor.desired_col);
    tab_free(t);
}

void test_enter_at_start_creates_empty_first_row(void) {
    Tab *t = make_tab_with_line("Hello");
    // cursor at (0,0)
    input_handle_key(t, '\n');
    TEST_ASSERT_EQUAL_INT(2, t->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("",      t->buf->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("Hello", t->buf->rows[1].line);
    tab_free(t);
}

// ---------------------------------------------------------------------------
// Undo (Ctrl-Z) and Redo (Ctrl-Y)
// ---------------------------------------------------------------------------

void test_ctrl_z_undoes_insert(void) {
    Tab *t = tab_new_empty();
    input_handle_key(t, 'A');
    TEST_ASSERT_EQUAL_STRING("A", t->buf->rows[0].line);

    input_handle_key(t, 'z' & 0x1f);   // Ctrl-Z
    TEST_ASSERT_EQUAL_STRING("", t->buf->rows[0].line);
    TEST_ASSERT_EQUAL_INT(0, t->cursor.pos.col);
    tab_free(t);
}

void test_ctrl_y_redoes_insert(void) {
    Tab *t = tab_new_empty();
    input_handle_key(t, 'B');
    input_handle_key(t, 'z' & 0x1f);   // undo
    input_handle_key(t, 'y' & 0x1f);   // redo
    TEST_ASSERT_EQUAL_STRING("B", t->buf->rows[0].line);
    tab_free(t);
}

void test_undo_redo_sequence(void) {
    Tab *t = tab_new_empty();
    input_handle_key(t, 'A');
    input_handle_key(t, 'B');
    input_handle_key(t, 'C');
    TEST_ASSERT_EQUAL_STRING("ABC", t->buf->rows[0].line);

    input_handle_key(t, 'z' & 0x1f);
    TEST_ASSERT_EQUAL_STRING("AB", t->buf->rows[0].line);

    input_handle_key(t, 'z' & 0x1f);
    TEST_ASSERT_EQUAL_STRING("A", t->buf->rows[0].line);

    input_handle_key(t, 'y' & 0x1f);
    TEST_ASSERT_EQUAL_STRING("AB", t->buf->rows[0].line);

    input_handle_key(t, 'y' & 0x1f);
    TEST_ASSERT_EQUAL_STRING("ABC", t->buf->rows[0].line);

    tab_free(t);
}

void test_new_edit_clears_redo_stack(void) {
    Tab *t = tab_new_empty();
    input_handle_key(t, 'A');
    input_handle_key(t, 'z' & 0x1f);   // undo -- 'A' on redo stack
    input_handle_key(t, 'B');           // new edit -- redo stack must clear
    input_handle_key(t, 'y' & 0x1f);   // redo should do nothing now
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
    Tab *t = make_tab_with_line("Hi");   // length 2
    input_handle_mouse(t, 0, 99);
    TEST_ASSERT_EQUAL_INT(2, t->cursor.pos.col);
    TEST_ASSERT_EQUAL_INT(2, t->cursor.desired_col);
    tab_free(t);
}

void test_mouse_clamps_row_to_buffer_bounds(void) {
    Tab *t = make_tab_with_line("Hi");   // 1 row
    input_handle_mouse(t, 99, 0);
    TEST_ASSERT_EQUAL_INT(0, t->cursor.pos.row);
    tab_free(t);
}

void test_mouse_then_arrow_uses_desired_col(void) {
    const char *lines[] = { "Hello", "Hi", "World" };
    Tab *t = make_tab_with_lines(lines, 3);

    // Click at row 0, col 4
    input_handle_mouse(t, 0, 4);
    TEST_ASSERT_EQUAL_INT(4, t->cursor.desired_col);

    // Move down to "Hi" (length 2) -- col clamps but desired_col preserved
    input_handle_key(t, KEY_DOWN);
    TEST_ASSERT_EQUAL_INT(2, t->cursor.pos.col);
    TEST_ASSERT_EQUAL_INT(4, t->cursor.desired_col);

    // Move down to "World" (length 5) -- col restores to 4
    input_handle_key(t, KEY_DOWN);
    TEST_ASSERT_EQUAL_INT(4, t->cursor.pos.col);

    tab_free(t);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(void) {
    UNITY_BEGIN();

    // Arrow keys
    RUN_TEST(test_key_left_moves_cursor_left);
    RUN_TEST(test_key_left_wraps_to_previous_row);
    RUN_TEST(test_key_left_at_origin_does_nothing);
    RUN_TEST(test_key_right_moves_cursor_right);
    RUN_TEST(test_key_right_wraps_to_next_row);
    RUN_TEST(test_key_right_at_end_of_buffer_does_nothing);
    RUN_TEST(test_key_up_moves_cursor_up);
    RUN_TEST(test_key_up_at_first_row_does_nothing);
    RUN_TEST(test_key_down_moves_cursor_down);
    RUN_TEST(test_key_down_at_last_row_does_nothing);

    // Printable characters
    RUN_TEST(test_printable_char_inserts_at_cursor);
    RUN_TEST(test_printable_chars_build_word);
    RUN_TEST(test_printable_char_inserts_mid_line);
    RUN_TEST(test_non_printable_below_32_ignored);
    RUN_TEST(test_del_127_ignored_as_printable);

    // Backspace
    RUN_TEST(test_backspace_KEY_BACKSPACE_deletes_left);
    RUN_TEST(test_backspace_ctrl_h_deletes_left);
    RUN_TEST(test_backspace_127_deletes_left);
    RUN_TEST(test_backspace_at_col0_merges_rows);
    RUN_TEST(test_backspace_at_origin_does_nothing);

    // Delete key
    RUN_TEST(test_delete_removes_char_at_cursor);
    RUN_TEST(test_delete_mid_line);
    RUN_TEST(test_delete_at_end_of_line_does_nothing);

    // Enter
    RUN_TEST(test_enter_newline_splits_line);
    RUN_TEST(test_enter_KEY_ENTER_splits_line);
    RUN_TEST(test_enter_cr_splits_line);
    RUN_TEST(test_enter_at_end_creates_empty_row);
    RUN_TEST(test_enter_at_start_creates_empty_first_row);

    // Undo / Redo
    RUN_TEST(test_ctrl_z_undoes_insert);
    RUN_TEST(test_ctrl_y_redoes_insert);
    RUN_TEST(test_undo_redo_sequence);
    RUN_TEST(test_new_edit_clears_redo_stack);

    // Mouse
    RUN_TEST(test_mouse_sets_cursor_position);
    RUN_TEST(test_mouse_clamps_col_to_line_length);
    RUN_TEST(test_mouse_clamps_row_to_buffer_bounds);
    RUN_TEST(test_mouse_then_arrow_uses_desired_col);

    return UNITY_END();
}