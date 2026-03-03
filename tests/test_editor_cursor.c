#include "unity.h"
#include "editor_cursor.h"
#include "buffer.h"
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static buffer *make_buf_from_lines(const char **lines, int count) {
    // Build a buffer manually so tests have no dependency on file I/O
    buffer *buf = malloc(sizeof(buffer));
    buf->numrows  = count;
    buf->capacity = count;
    buf->rows     = malloc(count * sizeof(row));
    for (int i = 0; i < count; i++) {
        int len = (int)strlen(lines[i]);
        buf->rows[i].length = len;
        buf->rows[i].line   = malloc(len + 1);
        memcpy(buf->rows[i].line, lines[i], len + 1);
    }
    return buf;
}

// ---------------------------------------------------------------------------
// setUp / tearDown  (required by Unity even if empty)
// ---------------------------------------------------------------------------

void setUp(void)    {}
void tearDown(void) {}

// ---------------------------------------------------------------------------
// cursor_init
// ---------------------------------------------------------------------------

void test_cursor_init_zeroes_all_fields(void) {
    EditorCursor c;
    // Dirty the struct first so we are sure init actually writes
    memset(&c, 0xFF, sizeof(c));
    cursor_init(&c);

    TEST_ASSERT_EQUAL_INT(0, c.pos.row);
    TEST_ASSERT_EQUAL_INT(0, c.pos.col);
    TEST_ASSERT_EQUAL_INT(0, c.anchor.row);
    TEST_ASSERT_EQUAL_INT(0, c.anchor.col);
    TEST_ASSERT_FALSE(c.selecting);
    TEST_ASSERT_EQUAL_INT(0, c.desired_col);
}

void test_cursor_init_null_safe(void) {
    // Should not crash
    cursor_init(NULL);
}

// ---------------------------------------------------------------------------
// cursor_clamp
// ---------------------------------------------------------------------------

void test_cursor_clamp_valid_position_unchanged(void) {
    const char *lines[] = { "Hello" };
    buffer *buf = make_buf_from_lines(lines, 1);

    EditorCursor c;
    cursor_init(&c);
    c.pos.row = 0;
    c.pos.col = 3;

    cursor_clamp(&c, buf);

    TEST_ASSERT_EQUAL_INT(0, c.pos.row);
    TEST_ASSERT_EQUAL_INT(3, c.pos.col);

    freeBuf(buf);
}

void test_cursor_clamp_col_past_end_of_line(void) {
    const char *lines[] = { "Hi" };   // length 2
    buffer *buf = make_buf_from_lines(lines, 1);

    EditorCursor c;
    cursor_init(&c);
    c.pos.col = 99;

    cursor_clamp(&c, buf);
    TEST_ASSERT_EQUAL_INT(2, c.pos.col);   // clamped to line length

    freeBuf(buf);
}

void test_cursor_clamp_row_past_end(void) {
    const char *lines[] = { "a", "b" };
    buffer *buf = make_buf_from_lines(lines, 2);

    EditorCursor c;
    cursor_init(&c);
    c.pos.row = 99;
    c.pos.col = 0;

    cursor_clamp(&c, buf);
    TEST_ASSERT_EQUAL_INT(1, c.pos.row);   // clamped to last row

    freeBuf(buf);
}

void test_cursor_clamp_negative_row(void) {
    const char *lines[] = { "abc" };
    buffer *buf = make_buf_from_lines(lines, 1);

    EditorCursor c;
    cursor_init(&c);
    c.pos.row = -5;

    cursor_clamp(&c, buf);
    TEST_ASSERT_EQUAL_INT(0, c.pos.row);

    freeBuf(buf);
}

void test_cursor_clamp_col_at_end_of_line_allowed(void) {
    // col == length is valid (cursor sits after last char)
    const char *lines[] = { "abc" };   // length 3
    buffer *buf = make_buf_from_lines(lines, 1);

    EditorCursor c;
    cursor_init(&c);
    c.pos.col = 3;

    cursor_clamp(&c, buf);
    TEST_ASSERT_EQUAL_INT(3, c.pos.col);   // should NOT be clamped

    freeBuf(buf);
}

// ---------------------------------------------------------------------------
// cursor_move_left
// ---------------------------------------------------------------------------

void test_move_left_basic(void) {
    const char *lines[] = { "Hello" };
    buffer *buf = make_buf_from_lines(lines, 1);

    EditorCursor c;
    cursor_init(&c);
    c.pos.col = 3;

    cursor_move_left(&c, buf);
    TEST_ASSERT_EQUAL_INT(0, c.pos.row);
    TEST_ASSERT_EQUAL_INT(2, c.pos.col);

    freeBuf(buf);
}

void test_move_left_wraps_to_previous_row(void) {
    const char *lines[] = { "Hello", "World" };
    buffer *buf = make_buf_from_lines(lines, 2);

    EditorCursor c;
    cursor_init(&c);
    c.pos.row = 1;
    c.pos.col = 0;

    cursor_move_left(&c, buf);
    TEST_ASSERT_EQUAL_INT(0, c.pos.row);
    TEST_ASSERT_EQUAL_INT(5, c.pos.col);   // end of "Hello"

    freeBuf(buf);
}

void test_move_left_at_origin_does_nothing(void) {
    const char *lines[] = { "Hello" };
    buffer *buf = make_buf_from_lines(lines, 1);

    EditorCursor c;
    cursor_init(&c);   // row=0, col=0

    cursor_move_left(&c, buf);
    TEST_ASSERT_EQUAL_INT(0, c.pos.row);
    TEST_ASSERT_EQUAL_INT(0, c.pos.col);

    freeBuf(buf);
}

void test_move_left_updates_desired_col(void) {
    const char *lines[] = { "Hello" };
    buffer *buf = make_buf_from_lines(lines, 1);

    EditorCursor c;
    cursor_init(&c);
    c.pos.col     = 4;
    c.desired_col = 4;

    cursor_move_left(&c, buf);
    TEST_ASSERT_EQUAL_INT(3, c.desired_col);

    freeBuf(buf);
}

// ---------------------------------------------------------------------------
// cursor_move_right
// ---------------------------------------------------------------------------

void test_move_right_basic(void) {
    const char *lines[] = { "Hello" };
    buffer *buf = make_buf_from_lines(lines, 1);

    EditorCursor c;
    cursor_init(&c);
    c.pos.col = 2;

    cursor_move_right(&c, buf);
    TEST_ASSERT_EQUAL_INT(3, c.pos.col);

    freeBuf(buf);
}

void test_move_right_wraps_to_next_row(void) {
    const char *lines[] = { "Hi", "There" };
    buffer *buf = make_buf_from_lines(lines, 2);

    EditorCursor c;
    cursor_init(&c);
    c.pos.row = 0;
    c.pos.col = 2;   // end of "Hi"

    cursor_move_right(&c, buf);
    TEST_ASSERT_EQUAL_INT(1, c.pos.row);
    TEST_ASSERT_EQUAL_INT(0, c.pos.col);

    freeBuf(buf);
}

void test_move_right_at_end_of_buffer_does_nothing(void) {
    const char *lines[] = { "Hi" };
    buffer *buf = make_buf_from_lines(lines, 1);

    EditorCursor c;
    cursor_init(&c);
    c.pos.row = 0;
    c.pos.col = 2;   // end of last line

    cursor_move_right(&c, buf);
    TEST_ASSERT_EQUAL_INT(0, c.pos.row);
    TEST_ASSERT_EQUAL_INT(2, c.pos.col);

    freeBuf(buf);
}

void test_move_right_updates_desired_col(void) {
    const char *lines[] = { "Hello" };
    buffer *buf = make_buf_from_lines(lines, 1);

    EditorCursor c;
    cursor_init(&c);
    c.pos.col = 2;

    cursor_move_right(&c, buf);
    TEST_ASSERT_EQUAL_INT(3, c.desired_col);

    freeBuf(buf);
}

// ---------------------------------------------------------------------------
// cursor_move_up
// ---------------------------------------------------------------------------

void test_move_up_basic(void) {
    const char *lines[] = { "Hello", "World" };
    buffer *buf = make_buf_from_lines(lines, 2);

    EditorCursor c;
    cursor_init(&c);
    c.pos.row     = 1;
    c.pos.col     = 3;
    c.desired_col = 3;

    cursor_move_up(&c, buf);
    TEST_ASSERT_EQUAL_INT(0, c.pos.row);
    TEST_ASSERT_EQUAL_INT(3, c.pos.col);

    freeBuf(buf);
}

void test_move_up_clamps_col_to_shorter_line(void) {
    const char *lines[] = { "Hi", "World" };   // row 0 is shorter
    buffer *buf = make_buf_from_lines(lines, 2);

    EditorCursor c;
    cursor_init(&c);
    c.pos.row     = 1;
    c.pos.col     = 5;
    c.desired_col = 5;

    cursor_move_up(&c, buf);
    TEST_ASSERT_EQUAL_INT(0, c.pos.row);
    TEST_ASSERT_EQUAL_INT(2, c.pos.col);   // clamped to length of "Hi"

    freeBuf(buf);
}

void test_move_up_preserves_desired_col(void) {
    // desired_col must NOT be changed by move_up so a later move_down
    // can restore the original column
    const char *lines[] = { "Hi", "Hello", "World" };
    buffer *buf = make_buf_from_lines(lines, 3);

    EditorCursor c;
    cursor_init(&c);
    c.pos.row     = 2;
    c.pos.col     = 4;
    c.desired_col = 4;

    cursor_move_up(&c, buf);   // row 1 "Hello" length 5 -- col stays 4
    TEST_ASSERT_EQUAL_INT(4, c.desired_col);

    cursor_move_up(&c, buf);   // row 0 "Hi" length 2 -- col clamped to 2
    TEST_ASSERT_EQUAL_INT(4, c.desired_col);   // desired_col still 4

    freeBuf(buf);
}

void test_move_up_at_first_row_does_nothing(void) {
    const char *lines[] = { "Hello" };
    buffer *buf = make_buf_from_lines(lines, 1);

    EditorCursor c;
    cursor_init(&c);
    c.pos.row = 0;
    c.pos.col = 2;

    cursor_move_up(&c, buf);
    TEST_ASSERT_EQUAL_INT(0, c.pos.row);
    TEST_ASSERT_EQUAL_INT(2, c.pos.col);

    freeBuf(buf);
}

// ---------------------------------------------------------------------------
// cursor_move_down
// ---------------------------------------------------------------------------

void test_move_down_basic(void) {
    const char *lines[] = { "Hello", "World" };
    buffer *buf = make_buf_from_lines(lines, 2);

    EditorCursor c;
    cursor_init(&c);
    c.pos.row     = 0;
    c.pos.col     = 3;
    c.desired_col = 3;

    cursor_move_down(&c, buf);
    TEST_ASSERT_EQUAL_INT(1, c.pos.row);
    TEST_ASSERT_EQUAL_INT(3, c.pos.col);

    freeBuf(buf);
}

void test_move_down_clamps_col_to_shorter_line(void) {
    const char *lines[] = { "Hello", "Hi" };   // row 1 is shorter
    buffer *buf = make_buf_from_lines(lines, 2);

    EditorCursor c;
    cursor_init(&c);
    c.pos.row     = 0;
    c.pos.col     = 5;
    c.desired_col = 5;

    cursor_move_down(&c, buf);
    TEST_ASSERT_EQUAL_INT(1, c.pos.row);
    TEST_ASSERT_EQUAL_INT(2, c.pos.col);   // clamped to length of "Hi"

    freeBuf(buf);
}

void test_move_down_restores_desired_col_on_longer_line(void) {
    const char *lines[] = { "Hello", "Hi", "World!" };
    buffer *buf = make_buf_from_lines(lines, 3);

    EditorCursor c;
    cursor_init(&c);
    c.pos.row     = 0;
    c.pos.col     = 4;
    c.desired_col = 4;

    cursor_move_down(&c, buf);   // row 1 "Hi" -- clamped to 2
    TEST_ASSERT_EQUAL_INT(2, c.pos.col);
    TEST_ASSERT_EQUAL_INT(4, c.desired_col);

    cursor_move_down(&c, buf);   // row 2 "World!" length 6 -- restored to 4
    TEST_ASSERT_EQUAL_INT(4, c.pos.col);

    freeBuf(buf);
}

void test_move_down_at_last_row_does_nothing(void) {
    const char *lines[] = { "Hello" };
    buffer *buf = make_buf_from_lines(lines, 1);

    EditorCursor c;
    cursor_init(&c);
    c.pos.row = 0;
    c.pos.col = 2;

    cursor_move_down(&c, buf);
    TEST_ASSERT_EQUAL_INT(0, c.pos.row);
    TEST_ASSERT_EQUAL_INT(2, c.pos.col);

    freeBuf(buf);
}

// ---------------------------------------------------------------------------
// Selection
// ---------------------------------------------------------------------------

void test_cursor_start_selection_sets_anchor(void) {
    EditorCursor c;
    cursor_init(&c);
    c.pos.row = 2;
    c.pos.col = 5;

    cursor_start_selection(&c);

    TEST_ASSERT_TRUE(c.selecting);
    TEST_ASSERT_EQUAL_INT(2, c.anchor.row);
    TEST_ASSERT_EQUAL_INT(5, c.anchor.col);
}

void test_cursor_clear_selection(void) {
    EditorCursor c;
    cursor_init(&c);
    cursor_start_selection(&c);
    cursor_clear_selection(&c);

    TEST_ASSERT_FALSE(c.selecting);
}

void test_cursor_has_selection_false_when_not_selecting(void) {
    EditorCursor c;
    cursor_init(&c);

    TEST_ASSERT_FALSE(cursor_has_selection(&c));
}

void test_cursor_has_selection_false_when_zero_length(void) {
    // selecting==true but anchor==pos means zero-length selection
    EditorCursor c;
    cursor_init(&c);
    c.pos.row = 1; c.pos.col = 3;
    cursor_start_selection(&c);   // anchor = pos

    TEST_ASSERT_FALSE(cursor_has_selection(&c));
}

void test_cursor_has_selection_true_when_moved_after_anchor(void) {
    const char *lines[] = { "Hello World" };
    buffer *buf = make_buf_from_lines(lines, 1);

    EditorCursor c;
    cursor_init(&c);
    c.pos.col = 2;
    cursor_start_selection(&c);   // anchor at col 2

    cursor_move_right(&c, buf);   // pos now at col 3
    TEST_ASSERT_TRUE(cursor_has_selection(&c));

    freeBuf(buf);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(void) {
    UNITY_BEGIN();

    // cursor_init
    RUN_TEST(test_cursor_init_zeroes_all_fields);
    RUN_TEST(test_cursor_init_null_safe);

    // cursor_clamp
    RUN_TEST(test_cursor_clamp_valid_position_unchanged);
    RUN_TEST(test_cursor_clamp_col_past_end_of_line);
    RUN_TEST(test_cursor_clamp_row_past_end);
    RUN_TEST(test_cursor_clamp_negative_row);
    RUN_TEST(test_cursor_clamp_col_at_end_of_line_allowed);

    // cursor_move_left
    RUN_TEST(test_move_left_basic);
    RUN_TEST(test_move_left_wraps_to_previous_row);
    RUN_TEST(test_move_left_at_origin_does_nothing);
    RUN_TEST(test_move_left_updates_desired_col);

    // cursor_move_right
    RUN_TEST(test_move_right_basic);
    RUN_TEST(test_move_right_wraps_to_next_row);
    RUN_TEST(test_move_right_at_end_of_buffer_does_nothing);
    RUN_TEST(test_move_right_updates_desired_col);

    // cursor_move_up
    RUN_TEST(test_move_up_basic);
    RUN_TEST(test_move_up_clamps_col_to_shorter_line);
    RUN_TEST(test_move_up_preserves_desired_col);
    RUN_TEST(test_move_up_at_first_row_does_nothing);

    // cursor_move_down
    RUN_TEST(test_move_down_basic);
    RUN_TEST(test_move_down_clamps_col_to_shorter_line);
    RUN_TEST(test_move_down_restores_desired_col_on_longer_line);
    RUN_TEST(test_move_down_at_last_row_does_nothing);

    // selection
    RUN_TEST(test_cursor_start_selection_sets_anchor);
    RUN_TEST(test_cursor_clear_selection);
    RUN_TEST(test_cursor_has_selection_false_when_not_selecting);
    RUN_TEST(test_cursor_has_selection_false_when_zero_length);
    RUN_TEST(test_cursor_has_selection_true_when_moved_after_anchor);

    return UNITY_END();
}