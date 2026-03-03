#include "unity.h"
#include "buffer.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void setUp(void) {}
void tearDown(void) {}

// ---------------------------------------------------------------------------
// newBuf
// ---------------------------------------------------------------------------

void test_newBuf_returns_non_null(void) {
    buffer *buf = newBuf();
    TEST_ASSERT_NOT_NULL(buf);
    freeBuf(buf);
}

void test_newBuf_has_one_empty_row(void) {
    buffer *buf = newBuf();
    TEST_ASSERT_EQUAL_INT(1, buf->numrows);
    TEST_ASSERT_EQUAL_INT(0, buf->rows[0].length);
    TEST_ASSERT_EQUAL_STRING("", buf->rows[0].line);
    freeBuf(buf);
}

// ---------------------------------------------------------------------------
// insertChar
// ---------------------------------------------------------------------------

void test_insertChar_appends_to_empty_row(void) {
    buffer *buf = newBuf();
    insertChar(&buf->rows[0], 0, 'H');
    TEST_ASSERT_EQUAL_INT(1, buf->rows[0].length);
    TEST_ASSERT_EQUAL_CHAR('H', buf->rows[0].line[0]);
    freeBuf(buf);
}

void test_insertChar_builds_string(void) {
    buffer *buf = newBuf();
    const char *word = "Hello";
    for (int i = 0; i < 5; i++)
        insertChar(&buf->rows[0], i, word[i]);
    TEST_ASSERT_EQUAL_STRING("Hello", buf->rows[0].line);
    TEST_ASSERT_EQUAL_INT(5, buf->rows[0].length);
    freeBuf(buf);
}

void test_insertChar_inserts_in_middle(void) {
    buffer *buf = newBuf();
    insertChar(&buf->rows[0], 0, 'A');
    insertChar(&buf->rows[0], 1, 'C');
    insertChar(&buf->rows[0], 1, 'B');
    TEST_ASSERT_EQUAL_STRING("ABC", buf->rows[0].line);
    freeBuf(buf);
}

void test_insertChar_clamps_negative_index(void) {
    buffer *buf = newBuf();
    insertChar(&buf->rows[0], 0, 'X');
    insertChar(&buf->rows[0], -5, 'Y');
    TEST_ASSERT_EQUAL_CHAR('Y', buf->rows[0].line[0]);
    TEST_ASSERT_EQUAL_CHAR('X', buf->rows[0].line[1]);
    freeBuf(buf);
}

void test_insertChar_clamps_past_end(void) {
    buffer *buf = newBuf();
    insertChar(&buf->rows[0], 0, 'A');
    insertChar(&buf->rows[0], 100, 'B');
    TEST_ASSERT_EQUAL_STRING("AB", buf->rows[0].line);
    freeBuf(buf);
}

// ---------------------------------------------------------------------------
// deleteChar
// ---------------------------------------------------------------------------

void test_deleteChar_removes_character(void) {
    buffer *buf = newBuf();
    insertChar(&buf->rows[0], 0, 'A');
    insertChar(&buf->rows[0], 1, 'B');
    insertChar(&buf->rows[0], 2, 'C');
    deleteChar(buf, 0, 1);
    TEST_ASSERT_EQUAL_STRING("AC", buf->rows[0].line);
    TEST_ASSERT_EQUAL_INT(2, buf->rows[0].length);
    freeBuf(buf);
}

void test_deleteChar_removes_first(void) {
    buffer *buf = newBuf();
    insertChar(&buf->rows[0], 0, 'A');
    insertChar(&buf->rows[0], 1, 'B');
    deleteChar(buf, 0, 0);
    TEST_ASSERT_EQUAL_STRING("B", buf->rows[0].line);
    freeBuf(buf);
}

void test_deleteChar_removes_last(void) {
    buffer *buf = newBuf();
    insertChar(&buf->rows[0], 0, 'A');
    insertChar(&buf->rows[0], 1, 'B');
    deleteChar(buf, 0, 1);
    TEST_ASSERT_EQUAL_STRING("A", buf->rows[0].line);
    freeBuf(buf);
}

void test_deleteChar_out_of_bounds_does_nothing(void) {
    buffer *buf = newBuf();
    insertChar(&buf->rows[0], 0, 'A');
    deleteChar(buf, 0, 5);
    TEST_ASSERT_EQUAL_STRING("A", buf->rows[0].line);
    freeBuf(buf);
}

void test_deleteChar_negative_at_merges_rows(void) {
    buffer *buf = newBuf();
    insertChar(&buf->rows[0], 0, 'A');
    insertChar(&buf->rows[0], 1, 'B');
    insertCR(buf, 0, 2);
    insertChar(&buf->rows[1], 0, 'C');
    insertChar(&buf->rows[1], 1, 'D');
    deleteChar(buf, 1, -1);
    TEST_ASSERT_EQUAL_INT(1, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("ABCD", buf->rows[0].line);
    freeBuf(buf);
}

// ---------------------------------------------------------------------------
// insertCR / deleteCR
// ---------------------------------------------------------------------------

void test_insertCR_splits_row(void) {
    buffer *buf = newBuf();
    const char *word = "Hello";
    for (int i = 0; i < 5; i++)
        insertChar(&buf->rows[0], i, word[i]);
    insertCR(buf, 0, 2);
    TEST_ASSERT_EQUAL_INT(2, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("He",  buf->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("llo", buf->rows[1].line);
    freeBuf(buf);
}

void test_insertCR_at_beginning(void) {
    buffer *buf = newBuf();
    insertChar(&buf->rows[0], 0, 'A');
    insertCR(buf, 0, 0);
    TEST_ASSERT_EQUAL_INT(2, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("", buf->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("A", buf->rows[1].line);
    freeBuf(buf);
}

void test_insertCR_at_end(void) {
    buffer *buf = newBuf();
    insertChar(&buf->rows[0], 0, 'A');
    insertCR(buf, 0, 1);
    TEST_ASSERT_EQUAL_INT(2, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("A", buf->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("",  buf->rows[1].line);
    freeBuf(buf);
}

void test_deleteCR_merges_rows(void) {
    buffer *buf = newBuf();
    insertChar(&buf->rows[0], 0, 'A');
    insertCR(buf, 0, 1);
    insertChar(&buf->rows[1], 0, 'B');
    deleteCR(buf, 1);
    TEST_ASSERT_EQUAL_INT(1, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("AB", buf->rows[0].line);
    freeBuf(buf);
}

void test_deleteCR_row0_does_nothing(void) {
    buffer *buf = newBuf();
    insertChar(&buf->rows[0], 0, 'A');
    deleteCR(buf, 0);
    TEST_ASSERT_EQUAL_INT(1, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("A", buf->rows[0].line);
    freeBuf(buf);
}

void test_insertCR_multiple_splits(void) {
    buffer *buf = newBuf();
    const char *word = "ABCDE";
    for (int i = 0; i < 5; i++)
        insertChar(&buf->rows[0], i, word[i]);
    insertCR(buf, 0, 2);
    insertCR(buf, 1, 1);
    TEST_ASSERT_EQUAL_INT(3, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("AB",  buf->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("C",   buf->rows[1].line);
    TEST_ASSERT_EQUAL_STRING("DE",  buf->rows[2].line);
    freeBuf(buf);
}

// ---------------------------------------------------------------------------
// insertText
// ---------------------------------------------------------------------------

void test_insertText_single_line_no_newline(void) {
    buffer *buf = newBuf();
    Position end = insertText(buf, 0, 0, "hello");
    TEST_ASSERT_EQUAL_INT(1, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("hello", buf->rows[0].line);
    TEST_ASSERT_EQUAL_INT(0, end.row);
    TEST_ASSERT_EQUAL_INT(5, end.col);
    freeBuf(buf);
}

void test_insertText_with_embedded_newline(void) {
    buffer *buf = newBuf();
    Position end = insertText(buf, 0, 0, "hello\nworld");
    TEST_ASSERT_EQUAL_INT(2, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("hello", buf->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("world", buf->rows[1].line);
    TEST_ASSERT_EQUAL_INT(1, end.row);
    TEST_ASSERT_EQUAL_INT(5, end.col);
    freeBuf(buf);
}

void test_insertText_multiple_newlines(void) {
    buffer *buf = newBuf();
    insertText(buf, 0, 0, "a\nb\nc");
    TEST_ASSERT_EQUAL_INT(3, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("a", buf->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("b", buf->rows[1].line);
    TEST_ASSERT_EQUAL_STRING("c", buf->rows[2].line);
    freeBuf(buf);
}

void test_insertText_into_existing_content_mid_line(void) {
    // Row 0: "AC" -> insert "B" at col 1 -> "ABC"
    buffer *buf = newBuf();
    insertChar(&buf->rows[0], 0, 'A');
    insertChar(&buf->rows[0], 1, 'C');
    insertText(buf, 0, 1, "B");
    TEST_ASSERT_EQUAL_INT(1, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("ABC", buf->rows[0].line);
    freeBuf(buf);
}

void test_insertText_newline_at_end_creates_empty_row(void) {
    buffer *buf = newBuf();
    Position end = insertText(buf, 0, 0, "hi\n");
    TEST_ASSERT_EQUAL_INT(2, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("hi", buf->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("",   buf->rows[1].line);
    TEST_ASSERT_EQUAL_INT(1, end.row);
    TEST_ASSERT_EQUAL_INT(0, end.col);
    freeBuf(buf);
}

void test_insertText_empty_string_is_noop(void) {
    buffer *buf = newBuf();
    insertChar(&buf->rows[0], 0, 'X');
    Position end = insertText(buf, 0, 1, "");
    TEST_ASSERT_EQUAL_INT(1, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("X", buf->rows[0].line);
    TEST_ASSERT_EQUAL_INT(0, end.row);
    TEST_ASSERT_EQUAL_INT(1, end.col);
    freeBuf(buf);
}

void test_insertText_null_text_returns_start(void) {
    buffer *buf = newBuf();
    Position end = insertText(buf, 0, 0, NULL);
    TEST_ASSERT_EQUAL_INT(0, end.row);
    TEST_ASSERT_EQUAL_INT(0, end.col);
    TEST_ASSERT_EQUAL_INT(1, buf->numrows);
    freeBuf(buf);
}

void test_insertText_returns_correct_end_position_multiline(void) {
    buffer *buf = newBuf();
    // "ab\ncd\nef" — end should be row 2, col 2
    Position end = insertText(buf, 0, 0, "ab\ncd\nef");
    TEST_ASSERT_EQUAL_INT(2, end.row);
    TEST_ASSERT_EQUAL_INT(2, end.col);
    freeBuf(buf);
}

// ---------------------------------------------------------------------------
// deleteTextRange
// ---------------------------------------------------------------------------

void test_deleteTextRange_single_line_chars(void) {
    buffer *buf = newBuf();
    insertText(buf, 0, 0, "hello");
    Position start;
    start.row = 0;
    start.col = 1;
    deleteTextRange(buf, start, 3);   // delete "ell"
    TEST_ASSERT_EQUAL_INT(1, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("ho", buf->rows[0].line);
    freeBuf(buf);
}

void test_deleteTextRange_across_newline(void) {
    buffer *buf = newBuf();
    insertText(buf, 0, 0, "hello\nworld");
    // Delete from col 3 of row 0 through the newline and "wo" of row 1
    // That is: "lo\nwo" = 5 logical chars
    Position start;
    start.row = 0;
    start.col = 3;
    deleteTextRange(buf, start, 5);
    TEST_ASSERT_EQUAL_INT(1, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("helrld", buf->rows[0].line);
    freeBuf(buf);
}

void test_deleteTextRange_len_zero_is_noop(void) {
    buffer *buf = newBuf();
    insertText(buf, 0, 0, "hello");
    Position start;
    start.row = 0;
    start.col = 0;
    deleteTextRange(buf, start, 0);
    TEST_ASSERT_EQUAL_STRING("hello", buf->rows[0].line);
    freeBuf(buf);
}

void test_deleteTextRange_entire_single_line(void) {
    buffer *buf = newBuf();
    insertText(buf, 0, 0, "hello");
    Position start;
    start.row = 0;
    start.col = 0;
    deleteTextRange(buf, start, 5);
    TEST_ASSERT_EQUAL_INT(1, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("", buf->rows[0].line);
    freeBuf(buf);
}

void test_deleteTextRange_newline_only(void) {
    // "hello\nworld" — delete just the newline at col 5
    buffer *buf = newBuf();
    insertText(buf, 0, 0, "hello\nworld");
    Position start;
    start.row = 0;
    start.col = 5;
    deleteTextRange(buf, start, 1);
    TEST_ASSERT_EQUAL_INT(1, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("helloworld", buf->rows[0].line);
    freeBuf(buf);
}

void test_insertText_then_deleteTextRange_round_trips(void) {
    // Insert multiline text, then delete exactly the same span — should
    // leave the buffer in its original state.
    buffer *buf = newBuf();
    insertChar(&buf->rows[0], 0, 'X');

    // Insert "foo\nbar" at col 1
    Position start;
    start.row = 0;
    start.col = 1;
    insertText(buf, 0, 1, "foo\nbar");

    // 7 logical chars: 'f','o','o','\n','b','a','r'
    deleteTextRange(buf, start, 7);

    TEST_ASSERT_EQUAL_INT(1, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("X", buf->rows[0].line);
    freeBuf(buf);
}

// ---------------------------------------------------------------------------
// fileToBuf / bufToFile round-trip
// ---------------------------------------------------------------------------

void test_fileToBuf_reads_lines(void) {
    FILE *f = tmpfile();
    fputs("line1\nline2\nline3\n", f);
    rewind(f);
    buffer *buf = fileToBuf(f);
    fclose(f);
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_EQUAL_INT(3, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("line1", buf->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("line2", buf->rows[1].line);
    TEST_ASSERT_EQUAL_STRING("line3", buf->rows[2].line);
    freeBuf(buf);
}

void test_fileToBuf_empty_file_gives_one_empty_row(void) {
    FILE *f = tmpfile();
    rewind(f);
    buffer *buf = fileToBuf(f);
    fclose(f);
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_EQUAL_INT(1, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("", buf->rows[0].line);
    freeBuf(buf);
}

void test_bufToFile_roundtrip(void) {
    buffer *buf = newBuf();
    const char *words[] = { "alpha", "beta", "gamma" };
    for (int i = 0; i < (int)strlen(words[0]); i++)
        insertChar(&buf->rows[0], i, words[0][i]);
    insertCR(buf, 0, buf->rows[0].length);
    for (int i = 0; i < (int)strlen(words[1]); i++)
        insertChar(&buf->rows[1], i, words[1][i]);
    insertCR(buf, 1, buf->rows[1].length);
    for (int i = 0; i < (int)strlen(words[2]); i++)
        insertChar(&buf->rows[2], i, words[2][i]);
    FILE *f = bufToFile(buf);
    TEST_ASSERT_NOT_NULL(f);
    buffer *buf2 = fileToBuf(f);
    fclose(f);
    TEST_ASSERT_EQUAL_INT(3, buf2->numrows);
    TEST_ASSERT_EQUAL_STRING("alpha", buf2->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("beta",  buf2->rows[1].line);
    TEST_ASSERT_EQUAL_STRING("gamma", buf2->rows[2].line);
    freeBuf(buf);
    freeBuf(buf2);
}

void test_bufToFile_null_buf_returns_null(void) {
    FILE *f = bufToFile(NULL);
    if (f) fclose(f);
}

// ---------------------------------------------------------------------------
// fileGetline
// ---------------------------------------------------------------------------

void test_fileGetline_reads_line_with_newline(void) {
    FILE *f = tmpfile();
    fputs("hello\n", f);
    rewind(f);
    char *line = NULL;
    size_t n = 0;
    long int r = fileGetline(&line, &n, f);
    TEST_ASSERT_TRUE(r > 0);
    TEST_ASSERT_EQUAL_STRING("hello\n", line);
    free(line);
    fclose(f);
}

void test_fileGetline_reads_line_without_trailing_newline(void) {
    FILE *f = tmpfile();
    fputs("world", f);
    rewind(f);
    char *line = NULL;
    size_t n = 0;
    long int r = fileGetline(&line, &n, f);
    TEST_ASSERT_EQUAL_INT(5, r);
    TEST_ASSERT_EQUAL_STRING("world", line);
    free(line);
    fclose(f);
}

void test_fileGetline_empty_stream_returns_minus1(void) {
    FILE *f = tmpfile();
    rewind(f);
    char *line = NULL;
    size_t n = 0;
    long int r = fileGetline(&line, &n, f);
    TEST_ASSERT_EQUAL_INT(-1, r);
    free(line);
    fclose(f);
}

void test_fileGetline_null_args_return_minus1(void) {
    TEST_ASSERT_EQUAL_INT(-1, fileGetline(NULL, NULL, NULL));
}

// ---------------------------------------------------------------------------
// freeBuf
// ---------------------------------------------------------------------------

void test_freeBuf_null_does_not_crash(void) {
    freeBuf(NULL);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_newBuf_returns_non_null);
    RUN_TEST(test_newBuf_has_one_empty_row);

    RUN_TEST(test_insertChar_appends_to_empty_row);
    RUN_TEST(test_insertChar_builds_string);
    RUN_TEST(test_insertChar_inserts_in_middle);
    RUN_TEST(test_insertChar_clamps_negative_index);
    RUN_TEST(test_insertChar_clamps_past_end);

    RUN_TEST(test_deleteChar_removes_character);
    RUN_TEST(test_deleteChar_removes_first);
    RUN_TEST(test_deleteChar_removes_last);
    RUN_TEST(test_deleteChar_out_of_bounds_does_nothing);
    RUN_TEST(test_deleteChar_negative_at_merges_rows);

    RUN_TEST(test_insertCR_splits_row);
    RUN_TEST(test_insertCR_at_beginning);
    RUN_TEST(test_insertCR_at_end);
    RUN_TEST(test_deleteCR_merges_rows);
    RUN_TEST(test_deleteCR_row0_does_nothing);
    RUN_TEST(test_insertCR_multiple_splits);

    // insertText
    RUN_TEST(test_insertText_single_line_no_newline);
    RUN_TEST(test_insertText_with_embedded_newline);
    RUN_TEST(test_insertText_multiple_newlines);
    RUN_TEST(test_insertText_into_existing_content_mid_line);
    RUN_TEST(test_insertText_newline_at_end_creates_empty_row);
    RUN_TEST(test_insertText_empty_string_is_noop);
    RUN_TEST(test_insertText_null_text_returns_start);
    RUN_TEST(test_insertText_returns_correct_end_position_multiline);

    // deleteTextRange
    RUN_TEST(test_deleteTextRange_single_line_chars);
    RUN_TEST(test_deleteTextRange_across_newline);
    RUN_TEST(test_deleteTextRange_len_zero_is_noop);
    RUN_TEST(test_deleteTextRange_entire_single_line);
    RUN_TEST(test_deleteTextRange_newline_only);
    RUN_TEST(test_insertText_then_deleteTextRange_round_trips);

    RUN_TEST(test_fileToBuf_reads_lines);
    RUN_TEST(test_fileToBuf_empty_file_gives_one_empty_row);
    RUN_TEST(test_bufToFile_roundtrip);
    RUN_TEST(test_bufToFile_null_buf_returns_null);

    RUN_TEST(test_fileGetline_reads_line_with_newline);
    RUN_TEST(test_fileGetline_reads_line_without_trailing_newline);
    RUN_TEST(test_fileGetline_empty_stream_returns_minus1);
    RUN_TEST(test_fileGetline_null_args_return_minus1);

    RUN_TEST(test_freeBuf_null_does_not_crash);

    return UNITY_END();
}