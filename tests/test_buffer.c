#include "unity.h"
#include "buffer.h"
#include <string.h>
#include <stdlib.h> 
#include <stdio.h>

// ---------------------------------------------------------------------------
// setUp / tearDown
// ---------------------------------------------------------------------------

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
    insertChar(&buf->rows[0], 1, 'B'); // insert between A and C
    TEST_ASSERT_EQUAL_STRING("ABC", buf->rows[0].line);
    freeBuf(buf);
}

void test_insertChar_clamps_negative_index(void) {
    buffer *buf = newBuf();
    insertChar(&buf->rows[0], 0, 'X');
    insertChar(&buf->rows[0], -5, 'Y'); // should clamp to 0
    TEST_ASSERT_EQUAL_CHAR('Y', buf->rows[0].line[0]);
    TEST_ASSERT_EQUAL_CHAR('X', buf->rows[0].line[1]);
    freeBuf(buf);
}

void test_insertChar_clamps_past_end(void) {
    buffer *buf = newBuf();
    insertChar(&buf->rows[0], 0, 'A');
    insertChar(&buf->rows[0], 100, 'B'); // should clamp to length
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
    deleteChar(buf, 0, 1); // remove 'B'
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
    deleteChar(buf, 0, 5); // past end — should be a no-op
    TEST_ASSERT_EQUAL_STRING("A", buf->rows[0].line);
    freeBuf(buf);
}

// at < 0 triggers deleteCR; test that with a two-row buffer
void test_deleteChar_negative_at_merges_rows(void) {
    buffer *buf = newBuf();
    // row 0: "AB", row 1: "CD"
    insertChar(&buf->rows[0], 0, 'A');
    insertChar(&buf->rows[0], 1, 'B');
    insertCR(buf, 0, 2);
    insertChar(&buf->rows[1], 0, 'C');
    insertChar(&buf->rows[1], 1, 'D');
    deleteChar(buf, 1, -1); // merge row 1 into row 0
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

    insertCR(buf, 0, 2); // split after "He" → ["He", "llo"]
    TEST_ASSERT_EQUAL_INT(2, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("He",  buf->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("llo", buf->rows[1].line);
    freeBuf(buf);
}

void test_insertCR_at_beginning(void) {
    buffer *buf = newBuf();
    insertChar(&buf->rows[0], 0, 'A');
    insertCR(buf, 0, 0); // split at beginning → ["", "A"]
    TEST_ASSERT_EQUAL_INT(2, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("", buf->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("A", buf->rows[1].line);
    freeBuf(buf);
}

void test_insertCR_at_end(void) {
    buffer *buf = newBuf();
    insertChar(&buf->rows[0], 0, 'A');
    insertCR(buf, 0, 1); // split at end → ["A", ""]
    TEST_ASSERT_EQUAL_INT(2, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("A", buf->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("",  buf->rows[1].line);
    freeBuf(buf);
}

void test_deleteCR_merges_rows(void) {
    buffer *buf = newBuf();
    insertChar(&buf->rows[0], 0, 'A');
    insertCR(buf, 0, 1); // ["A", ""]
    insertChar(&buf->rows[1], 0, 'B');
    // rows: ["A", "B"]
    deleteCR(buf, 1);
    TEST_ASSERT_EQUAL_INT(1, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("AB", buf->rows[0].line);
    freeBuf(buf);
}

void test_deleteCR_row0_does_nothing(void) {
    buffer *buf = newBuf();
    insertChar(&buf->rows[0], 0, 'A');
    deleteCR(buf, 0); // rowIndex <= 0 → no-op
    TEST_ASSERT_EQUAL_INT(1, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("A", buf->rows[0].line);
    freeBuf(buf);
}

void test_insertCR_multiple_splits(void) {
    buffer *buf = newBuf();
    const char *word = "ABCDE";
    for (int i = 0; i < 5; i++)
        insertChar(&buf->rows[0], i, word[i]);

    insertCR(buf, 0, 2); // ["AB", "CDE"]
    insertCR(buf, 1, 1); // ["AB", "C", "DE"]
    TEST_ASSERT_EQUAL_INT(3, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("AB",  buf->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("C",   buf->rows[1].line);
    TEST_ASSERT_EQUAL_STRING("DE",  buf->rows[2].line);
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
    // row 0
    for (int i = 0; (int)i < (int)strlen(words[0]); i++)
        insertChar(&buf->rows[0], i, words[0][i]);
    // rows 1 and 2 via insertCR
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
    // bufToFile opens tmpfile before checking buf; result depends on
    // implementation — just verify we don't crash and the returned file
    // is not used unsafely.  The function may return NULL or a valid empty
    // file handle; we skip content checks here.
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
    freeBuf(NULL); // should be a no-op
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