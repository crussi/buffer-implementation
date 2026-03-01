#include "unity.h"
#include "editor_history.h"
#include "buffer.h"
#include <stdlib.h>
#include <string.h>

// =============================================================================
// Fixtures
// =============================================================================

static EditorHistory *h;
static buffer        *buf;

void setUp(void) {
    h   = new_editor_history();
    buf = newBuf();
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

// Populate row 0 of buf with the given string.
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
    // Manually push something onto redo to simulate a prior undo.
    Action a = make_insert_char(0, 0, 'x');
    push_action(h->redo_stack, a);
    TEST_ASSERT_EQUAL_size_t(1, h->redo_stack->size);

    // Recording a new action must wipe redo.
    Action b = make_insert_char(0, 1, 'y');
    history_record(h, b);
    TEST_ASSERT_EQUAL_size_t(0, h->redo_stack->size);
}

void test_record_does_not_crash_on_null_history(void) {
    Action a = make_insert_char(0, 0, 'x');
    history_record(NULL, a);  // must not crash
}

// =============================================================================
// history_undo — return value
// =============================================================================

void test_undo_returns_false_on_empty_undo_stack(void) {
    TEST_ASSERT_FALSE(history_undo(h, buf));
}

void test_undo_returns_false_on_null_history(void) {
    TEST_ASSERT_FALSE(history_undo(NULL, buf));
}

void test_undo_returns_false_on_null_buffer(void) {
    Action a = make_insert_char(0, 0, 'x');
    history_record(h, a);
    TEST_ASSERT_FALSE(history_undo(h, NULL));
}

void test_undo_returns_true_when_action_present(void) {
    set_row(buf, 0, "a");
    Action a = make_insert_char(0, 0, 'a');
    history_record(h, a);
    TEST_ASSERT_TRUE(history_undo(h, buf));
}

// =============================================================================
// history_redo — return value
// =============================================================================

void test_redo_returns_false_on_empty_redo_stack(void) {
    TEST_ASSERT_FALSE(history_redo(h, buf));
}

void test_redo_returns_false_on_null_history(void) {
    TEST_ASSERT_FALSE(history_redo(NULL, buf));
}

void test_redo_returns_false_on_null_buffer(void) {
    TEST_ASSERT_FALSE(history_redo(h, NULL));
}

// =============================================================================
// INSERT_CHAR undo / redo
// =============================================================================

void test_undo_insert_char_removes_character(void) {
    // Simulate: user typed 'h' at col 0, buffer now contains "h"
    insertChar(&buf->rows[0], 0, 'h');
    history_record(h, make_insert_char(0, 0, 'h'));

    history_undo(h, buf);

    TEST_ASSERT_EQUAL_INT(0, buf->rows[0].length);
    TEST_ASSERT_EQUAL_CHAR('\0', buf->rows[0].line[0]);
}

void test_undo_insert_char_decrements_undo_stack(void) {
    insertChar(&buf->rows[0], 0, 'h');
    history_record(h, make_insert_char(0, 0, 'h'));

    history_undo(h, buf);

    TEST_ASSERT_EQUAL_size_t(0, h->undo_stack->size);
}

void test_undo_insert_char_pushes_onto_redo_stack(void) {
    insertChar(&buf->rows[0], 0, 'h');
    history_record(h, make_insert_char(0, 0, 'h'));

    history_undo(h, buf);

    TEST_ASSERT_EQUAL_size_t(1, h->redo_stack->size);
}

void test_redo_insert_char_restores_character(void) {
    insertChar(&buf->rows[0], 0, 'h');
    history_record(h, make_insert_char(0, 0, 'h'));
    history_undo(h, buf);

    history_redo(h, buf);

    TEST_ASSERT_EQUAL_INT(1, buf->rows[0].length);
    TEST_ASSERT_EQUAL_CHAR('h', buf->rows[0].line[0]);
}

void test_redo_insert_char_pushes_back_onto_undo_stack(void) {
    insertChar(&buf->rows[0], 0, 'h');
    history_record(h, make_insert_char(0, 0, 'h'));
    history_undo(h, buf);

    history_redo(h, buf);

    TEST_ASSERT_EQUAL_size_t(1, h->undo_stack->size);
    TEST_ASSERT_EQUAL_size_t(0, h->redo_stack->size);
}

void test_undo_redo_insert_char_multiple_times(void) {
    // Type "hi", undo both, redo both, check buffer is "hi" again.
    insertChar(&buf->rows[0], 0, 'h');
    history_record(h, make_insert_char(0, 0, 'h'));
    insertChar(&buf->rows[0], 1, 'i');
    history_record(h, make_insert_char(0, 1, 'i'));

    history_undo(h, buf);
    history_undo(h, buf);
    TEST_ASSERT_EQUAL_INT(0, buf->rows[0].length);

    history_redo(h, buf);
    history_redo(h, buf);
    TEST_ASSERT_EQUAL_INT(2, buf->rows[0].length);
    TEST_ASSERT_EQUAL_CHAR('h', buf->rows[0].line[0]);
    TEST_ASSERT_EQUAL_CHAR('i', buf->rows[0].line[1]);
}

// =============================================================================
// DELETE_CHAR undo / redo
// =============================================================================

void test_undo_delete_char_restores_character(void) {
    // Buffer contains "ab". User deletes 'a' at col 0. Buffer is now "b".
    set_row(buf, 0, "ab");
    history_record(h, make_delete_char(0, 0, 'a'));
    deleteChar(buf, 0, 0);

    history_undo(h, buf);

    TEST_ASSERT_EQUAL_INT(2, buf->rows[0].length);
    TEST_ASSERT_EQUAL_CHAR('a', buf->rows[0].line[0]);
    TEST_ASSERT_EQUAL_CHAR('b', buf->rows[0].line[1]);
}

void test_undo_delete_char_pushes_onto_redo_stack(void) {
    set_row(buf, 0, "ab");
    history_record(h, make_delete_char(0, 0, 'a'));
    deleteChar(buf, 0, 0);

    history_undo(h, buf);

    TEST_ASSERT_EQUAL_size_t(1, h->redo_stack->size);
}

void test_redo_delete_char_removes_character_again(void) {
    set_row(buf, 0, "ab");
    history_record(h, make_delete_char(0, 0, 'a'));
    deleteChar(buf, 0, 0);
    history_undo(h, buf);  // buffer is "ab" again

    history_redo(h, buf);  // should delete 'a' again

    TEST_ASSERT_EQUAL_INT(1, buf->rows[0].length);
    TEST_ASSERT_EQUAL_CHAR('b', buf->rows[0].line[0]);
}

void test_undo_redo_delete_char_cycle(void) {
    set_row(buf, 0, "abc");
    history_record(h, make_delete_char(0, 1, 'b'));
    deleteChar(buf, 0, 1);
    TEST_ASSERT_EQUAL_STRING("ac", buf->rows[0].line);

    history_undo(h, buf);
    TEST_ASSERT_EQUAL_STRING("abc", buf->rows[0].line);

    history_redo(h, buf);
    TEST_ASSERT_EQUAL_STRING("ac", buf->rows[0].line);
}

// =============================================================================
// INSERT_CR undo / redo
// =============================================================================

void test_undo_insert_cr_merges_rows(void) {
    // Buffer row 0 is "hello". insertCR at col 3 splits to "hel" / "lo".
    set_row(buf, 0, "hello");
    insertCR(buf, 0, 3);
    history_record(h, make_insert_cr(0, 3));

    TEST_ASSERT_EQUAL_INT(2, buf->numrows);

    history_undo(h, buf);

    TEST_ASSERT_EQUAL_INT(1, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("hello", buf->rows[0].line);
}

void test_undo_insert_cr_pushes_onto_redo_stack(void) {
    set_row(buf, 0, "hello");
    insertCR(buf, 0, 3);
    history_record(h, make_insert_cr(0, 3));

    history_undo(h, buf);

    TEST_ASSERT_EQUAL_size_t(1, h->redo_stack->size);
}

void test_redo_insert_cr_splits_row_again(void) {
    set_row(buf, 0, "hello");
    insertCR(buf, 0, 3);
    history_record(h, make_insert_cr(0, 3));
    history_undo(h, buf);
    TEST_ASSERT_EQUAL_INT(1, buf->numrows);

    history_redo(h, buf);

    TEST_ASSERT_EQUAL_INT(2, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("hel", buf->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("lo",  buf->rows[1].line);
}

void test_undo_redo_insert_cr_cycle(void) {
    set_row(buf, 0, "abcd");
    insertCR(buf, 0, 2);
    history_record(h, make_insert_cr(0, 2));

    history_undo(h, buf);
    TEST_ASSERT_EQUAL_INT(1,      buf->numrows);
    TEST_ASSERT_EQUAL_STRING("abcd", buf->rows[0].line);

    history_redo(h, buf);
    TEST_ASSERT_EQUAL_INT(2,    buf->numrows);
    TEST_ASSERT_EQUAL_STRING("ab", buf->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("cd", buf->rows[1].line);

    history_undo(h, buf);
    TEST_ASSERT_EQUAL_INT(1,      buf->numrows);
    TEST_ASSERT_EQUAL_STRING("abcd", buf->rows[0].line);
}

// =============================================================================
// DELETE_CR undo / redo
// =============================================================================

void test_undo_delete_cr_splits_rows(void) {
    // Start with two rows "hel" and "lo". deleteCR merges them to "hello".
    // The action records row=0, col=3 (length of row 0 before merge).
    set_row(buf, 0, "hel");
    insertCR(buf, 0, 3);  // creates row 1 = ""
    // Manually set row 1 content
    set_row(buf, 1, "lo");

    history_record(h, make_delete_cr(0, 3));
    deleteCR(buf, 1);
    TEST_ASSERT_EQUAL_INT(1, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("hello", buf->rows[0].line);

    history_undo(h, buf);

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
    history_undo(h, buf);

    TEST_ASSERT_EQUAL_size_t(1, h->redo_stack->size);
}

void test_redo_delete_cr_merges_rows_again(void) {
    set_row(buf, 0, "hel");
    insertCR(buf, 0, 3);
    set_row(buf, 1, "lo");

    history_record(h, make_delete_cr(0, 3));
    deleteCR(buf, 1);
    history_undo(h, buf);
    TEST_ASSERT_EQUAL_INT(2, buf->numrows);

    history_redo(h, buf);

    TEST_ASSERT_EQUAL_INT(1, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("hello", buf->rows[0].line);
}

// =============================================================================
// Mixed action sequences
// =============================================================================

void test_undo_sequence_restores_in_reverse_order(void) {
    // Type "hi" character by character, then undo both.
    insertChar(&buf->rows[0], 0, 'h');
    history_record(h, make_insert_char(0, 0, 'h'));
    insertChar(&buf->rows[0], 1, 'i');
    history_record(h, make_insert_char(0, 1, 'i'));

    history_undo(h, buf);
    TEST_ASSERT_EQUAL_INT(1, buf->rows[0].length);
    TEST_ASSERT_EQUAL_CHAR('h', buf->rows[0].line[0]);

    history_undo(h, buf);
    TEST_ASSERT_EQUAL_INT(0, buf->rows[0].length);
}

void test_new_edit_after_undo_clears_redo(void) {
    insertChar(&buf->rows[0], 0, 'a');
    history_record(h, make_insert_char(0, 0, 'a'));
    history_undo(h, buf);
    TEST_ASSERT_EQUAL_size_t(1, h->redo_stack->size);

    // New edit — redo must be wiped.
    insertChar(&buf->rows[0], 0, 'b');
    history_record(h, make_insert_char(0, 0, 'b'));
    TEST_ASSERT_EQUAL_size_t(0, h->redo_stack->size);
}

void test_redo_is_unavailable_after_new_edit(void) {
    insertChar(&buf->rows[0], 0, 'a');
    history_record(h, make_insert_char(0, 0, 'a'));
    history_undo(h, buf);

    // New edit invalidates redo history.
    insertChar(&buf->rows[0], 0, 'b');
    history_record(h, make_insert_char(0, 0, 'b'));

    TEST_ASSERT_FALSE(history_redo(h, buf));
}

void test_full_undo_redo_cycle_mixed_actions(void) {
    // Type "ab", press Enter, type "c".
    // Buffer should be:  row0="ab"  row1="c"
    // Undo all four actions and verify we return to empty buffer.
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

    history_undo(h, buf);  // undo insert 'c'
    TEST_ASSERT_EQUAL_STRING("", buf->rows[1].line);

    history_undo(h, buf);  // undo insert CR
    TEST_ASSERT_EQUAL_INT(1, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("ab", buf->rows[0].line);

    history_undo(h, buf);  // undo insert 'b'
    TEST_ASSERT_EQUAL_STRING("a", buf->rows[0].line);

    history_undo(h, buf);  // undo insert 'a'
    TEST_ASSERT_EQUAL_INT(0, buf->rows[0].length);

    // Now redo everything and verify we're back to the final state.
    history_redo(h, buf);
    history_redo(h, buf);
    history_redo(h, buf);
    history_redo(h, buf);

    TEST_ASSERT_EQUAL_INT(2, buf->numrows);
    TEST_ASSERT_EQUAL_STRING("ab", buf->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("c",  buf->rows[1].line);
}

void test_undo_beyond_history_returns_false(void) {
    insertChar(&buf->rows[0], 0, 'x');
    history_record(h, make_insert_char(0, 0, 'x'));
    history_undo(h, buf);

    // Stack is now empty — further undos must return false.
    TEST_ASSERT_FALSE(history_undo(h, buf));
    TEST_ASSERT_FALSE(history_undo(h, buf));
}

void test_redo_beyond_history_returns_false(void) {
    insertChar(&buf->rows[0], 0, 'x');
    history_record(h, make_insert_char(0, 0, 'x'));
    history_undo(h, buf);
    history_redo(h, buf);

    // Redo stack is now empty — further redos must return false.
    TEST_ASSERT_FALSE(history_redo(h, buf));
    TEST_ASSERT_FALSE(history_redo(h, buf));
}

void test_alternating_undo_redo_is_stable(void) {
    insertChar(&buf->rows[0], 0, 'z');
    history_record(h, make_insert_char(0, 0, 'z'));

    for (int i = 0; i < 10; i++) {
        history_undo(h, buf);
        TEST_ASSERT_EQUAL_INT(0, buf->rows[0].length);
        history_redo(h, buf);
        TEST_ASSERT_EQUAL_INT(1, buf->rows[0].length);
        TEST_ASSERT_EQUAL_CHAR('z', buf->rows[0].line[0]);
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

    history_undo(h, buf);
    TEST_ASSERT_EQUAL_size_t(2, h->undo_stack->size);

    history_undo(h, buf);
    TEST_ASSERT_EQUAL_size_t(1, h->undo_stack->size);
}

void test_redo_reduces_redo_stack_by_one(void) {
    for (int i = 0; i < 3; i++) {
        Action a = make_insert_char(0, i, (char)('a' + i));
        insertChar(&buf->rows[0], i, (char)('a' + i));
        history_record(h, a);
    }
    history_undo(h, buf);
    history_undo(h, buf);
    history_undo(h, buf);
    TEST_ASSERT_EQUAL_size_t(3, h->redo_stack->size);

    history_redo(h, buf);
    TEST_ASSERT_EQUAL_size_t(2, h->redo_stack->size);

    history_redo(h, buf);
    TEST_ASSERT_EQUAL_size_t(1, h->redo_stack->size);
}

void test_undo_all_then_redo_all_stack_sizes(void) {
    for (int i = 0; i < 4; i++) {
        insertChar(&buf->rows[0], i, (char)('a' + i));
        history_record(h, make_insert_char(0, i, (char)('a' + i)));
    }

    for (int i = 0; i < 4; i++)
        history_undo(h, buf);

    TEST_ASSERT_EQUAL_size_t(0, h->undo_stack->size);
    TEST_ASSERT_EQUAL_size_t(4, h->redo_stack->size);

    for (int i = 0; i < 4; i++)
        history_redo(h, buf);

    TEST_ASSERT_EQUAL_size_t(4, h->undo_stack->size);
    TEST_ASSERT_EQUAL_size_t(0, h->redo_stack->size);
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

    // Mixed sequences
    RUN_TEST(test_undo_sequence_restores_in_reverse_order);
    RUN_TEST(test_new_edit_after_undo_clears_redo);
    RUN_TEST(test_redo_is_unavailable_after_new_edit);
    RUN_TEST(test_full_undo_redo_cycle_mixed_actions);
    RUN_TEST(test_undo_beyond_history_returns_false);
    RUN_TEST(test_redo_beyond_history_returns_false);
    RUN_TEST(test_alternating_undo_redo_is_stable);

    // Stack state invariants
    RUN_TEST(test_undo_reduces_undo_stack_by_one);
    RUN_TEST(test_redo_reduces_redo_stack_by_one);
    RUN_TEST(test_undo_all_then_redo_all_stack_sizes);

    return UNITY_END();
}