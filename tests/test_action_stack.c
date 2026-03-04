// test_action_stack.c
//
// The legacy ActionStack shim API is fully preserved, so almost nothing
// changes here.  The only update is in the two Action-construction helpers:
// the Action struct now has cursor_before and cursor_after fields, so the
// helpers zero-initialise them to keep the struct well-defined.

#include "unity.h"
#include "action_stack.h"
#include <stdlib.h>
#include <string.h>

// ------------------------------------------------------------------ fixtures

static ActionStack *stack;

void setUp(void) {
    stack = new_action_stack(0, 0);
}

void tearDown(void) {
    free_action_stack(stack);
    stack = NULL;
}

// --------------------------------------------------------- helpers

static Action make_char_action(ActionType type, int row, int col, char c) {
    Action a;
    a.type            = type;
    a.position.row    = row;
    a.position.col    = col;
    a.character       = c;
    a.text            = NULL;
    // New fields added by the undo-tree refactor -- zero-initialise so the
    // struct is well-defined; the ActionStack shim does not inspect them.
    a.cursor_before.row = 0;  a.cursor_before.col = 0;
    a.cursor_after.row  = 0;  a.cursor_after.col  = 0;
    return a;
}

static Action make_text_action(ActionType type, int row, int col,
                               const char *text) {
    Action a;
    a.type            = type;
    a.position.row    = row;
    a.position.col    = col;
    a.character       = '\0';
    a.text            = text ? strdup(text) : NULL;
    a.cursor_before.row = 0;  a.cursor_before.col = 0;
    a.cursor_after.row  = 0;  a.cursor_after.col  = 0;
    return a;
}

// ---------------------------------------------------------------- new / free

void test_new_action_stack_returns_non_null(void) {
    TEST_ASSERT_NOT_NULL(stack);
}

void test_new_action_stack_is_empty(void) {
    TEST_ASSERT_TRUE(action_stack_is_empty(stack));
}

void test_new_action_stack_has_zero_size(void) {
    TEST_ASSERT_EQUAL_size_t(0, stack->size);
}

void test_new_action_stack_custom_initial_capacity(void) {
    ActionStack *s = new_action_stack(64, 0);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_size_t(64, s->capacity);
    free_action_stack(s);
}

void test_new_action_stack_max_capacity_clamps_initial(void) {
    ActionStack *s = new_action_stack(128, 8);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_size_t(8, s->capacity);
    free_action_stack(s);
}

// ------------------------------------------------------------------ push

void test_push_returns_true_on_success(void) {
    Action a = make_char_action(INSERT_CHAR, 0, 0, 'x');
    TEST_ASSERT_TRUE(push_action(stack, a));
}

void test_push_increases_size(void) {
    Action a = make_char_action(INSERT_CHAR, 0, 0, 'x');
    push_action(stack, a);
    TEST_ASSERT_EQUAL_size_t(1, stack->size);
}

void test_push_multiple_increases_size(void) {
    for (int i = 0; i < 10; i++) {
        Action a = make_char_action(INSERT_CHAR, 0, i, (char)('a' + i));
        push_action(stack, a);
    }
    TEST_ASSERT_EQUAL_size_t(10, stack->size);
}

void test_push_null_stack_returns_false(void) {
    Action a = make_char_action(INSERT_CHAR, 0, 0, 'x');
    TEST_ASSERT_FALSE(push_action(NULL, a));
}

void test_push_triggers_growth_beyond_default_capacity(void) {
    for (int i = 0; i < 32; i++) {
        Action a = make_char_action(INSERT_CHAR, 0, i, 'a');
        TEST_ASSERT_TRUE(push_action(stack, a));
    }
    TEST_ASSERT_EQUAL_size_t(32, stack->size);
    TEST_ASSERT_TRUE(stack->capacity >= 32);
}

// ------------------------------------------------------------------- pop

void test_pop_returns_false_on_empty_stack(void) {
    Action out;
    TEST_ASSERT_FALSE(pop_action(stack, &out));
}

void test_pop_returns_true_when_item_present(void) {
    Action a = make_char_action(INSERT_CHAR, 1, 2, 'z');
    push_action(stack, a);
    Action out;
    TEST_ASSERT_TRUE(pop_action(stack, &out));
}

void test_pop_decreases_size(void) {
    Action a = make_char_action(INSERT_CHAR, 0, 0, 'a');
    push_action(stack, a);
    Action out;
    pop_action(stack, &out);
    TEST_ASSERT_EQUAL_size_t(0, stack->size);
}

void test_pop_returns_correct_action(void) {
    Action a = make_char_action(DELETE_CHAR, 3, 7, 'q');
    push_action(stack, a);
    Action out;
    pop_action(stack, &out);
    TEST_ASSERT_EQUAL_INT(DELETE_CHAR,  out.type);
    TEST_ASSERT_EQUAL_INT(3,            out.position.row);
    TEST_ASSERT_EQUAL_INT(7,            out.position.col);
    TEST_ASSERT_EQUAL_CHAR('q',         out.character);
}

void test_pop_is_lifo_order(void) {
    for (int i = 0; i < 5; i++) {
        Action a = make_char_action(INSERT_CHAR, 0, i, (char)('a' + i));
        push_action(stack, a);
    }
    for (int i = 4; i >= 0; i--) {
        Action out;
        pop_action(stack, &out);
        TEST_ASSERT_EQUAL_CHAR((char)('a' + i), out.character);
    }
}

void test_pop_null_stack_returns_false(void) {
    Action out;
    TEST_ASSERT_FALSE(pop_action(NULL, &out));
}

// ------------------------------------------------------------------ peek

void test_peek_returns_false_on_empty_stack(void) {
    Action out;
    TEST_ASSERT_FALSE(peek_action(stack, &out));
}

void test_peek_returns_true_when_item_present(void) {
    Action a = make_char_action(INSERT_CHAR, 0, 0, 'p');
    push_action(stack, a);
    Action out;
    TEST_ASSERT_TRUE(peek_action(stack, &out));
}

void test_peek_does_not_change_size(void) {
    Action a = make_char_action(INSERT_CHAR, 0, 0, 'p');
    push_action(stack, a);
    Action out;
    peek_action(stack, &out);
    TEST_ASSERT_EQUAL_size_t(1, stack->size);
}

void test_peek_returns_top_item(void) {
    Action a = make_char_action(INSERT_CR,  2, 0, '\0');
    Action b = make_char_action(DELETE_CR,  5, 0, '\0');
    push_action(stack, a);
    push_action(stack, b);
    Action out;
    peek_action(stack, &out);
    TEST_ASSERT_EQUAL_INT(DELETE_CR, out.type);
    TEST_ASSERT_EQUAL_INT(5,         out.position.row);
}

void test_peek_null_stack_returns_false(void) {
    Action out;
    TEST_ASSERT_FALSE(peek_action(NULL, &out));
}

// ------------------------------------------------------------- is_empty

void test_is_empty_true_on_new_stack(void) {
    TEST_ASSERT_TRUE(action_stack_is_empty(stack));
}

void test_is_empty_false_after_push(void) {
    Action a = make_char_action(INSERT_CHAR, 0, 0, 'x');
    push_action(stack, a);
    TEST_ASSERT_FALSE(action_stack_is_empty(stack));
}

void test_is_empty_true_after_push_and_pop(void) {
    Action a = make_char_action(INSERT_CHAR, 0, 0, 'x');
    push_action(stack, a);
    Action out;
    pop_action(stack, &out);
    TEST_ASSERT_TRUE(action_stack_is_empty(stack));
}

void test_is_empty_true_for_null_stack(void) {
    TEST_ASSERT_TRUE(action_stack_is_empty(NULL));
}

// --------------------------------------------------------------- reset

void test_reset_clears_size(void) {
    for (int i = 0; i < 5; i++) {
        Action a = make_char_action(INSERT_CHAR, 0, i, 'a');
        push_action(stack, a);
    }
    reset_action_stack(stack);
    TEST_ASSERT_EQUAL_size_t(0, stack->size);
}

void test_reset_preserves_capacity(void) {
    for (int i = 0; i < 5; i++) {
        Action a = make_char_action(INSERT_CHAR, 0, i, 'a');
        push_action(stack, a);
    }
    size_t cap = stack->capacity;
    reset_action_stack(stack);
    TEST_ASSERT_EQUAL_size_t(cap, stack->capacity);
}

void test_reset_then_push_works(void) {
    Action a = make_char_action(INSERT_CHAR, 0, 0, 'r');
    push_action(stack, a);
    reset_action_stack(stack);
    Action b = make_char_action(INSERT_CHAR, 0, 0, 's');
    TEST_ASSERT_TRUE(push_action(stack, b));
    TEST_ASSERT_EQUAL_size_t(1, stack->size);
}

void test_reset_null_does_not_crash(void) {
    reset_action_stack(NULL);
}

// ----------------------------------------------------------- text payload

void test_push_pop_text_payload_round_trips(void) {
    Action a = make_text_action(INSERT_CHAR, 0, 0, "hello");
    push_action(stack, a);
    Action out;
    pop_action(stack, &out);
    TEST_ASSERT_NOT_NULL(out.text);
    TEST_ASSERT_EQUAL_STRING("hello", out.text);
    free(out.text);  // caller owns text after pop
}

void test_reset_frees_text_payloads(void) {
    Action a = make_text_action(INSERT_CHAR, 0, 0, "undo this");
    push_action(stack, a);
    reset_action_stack(stack);
    TEST_ASSERT_EQUAL_size_t(0, stack->size);
}

// --------------------------------------------------------- max_capacity

void test_max_capacity_size_never_exceeds_limit(void) {
    ActionStack *s = new_action_stack(4, 4);
    for (int i = 0; i < 6; i++) {
        Action a = make_char_action(INSERT_CHAR, 0, i, (char)('a' + i));
        push_action(s, a);
    }
    TEST_ASSERT_EQUAL_size_t(4, s->size);
    free_action_stack(s);
}

void test_max_capacity_sliding_window_keeps_newest(void) {
    ActionStack *s = new_action_stack(2, 2);
    // Push a, b, c, d — with cap 2 the stack should evict oldest entries,
    // leaving c then d on top.
    for (int i = 0; i < 4; i++) {
        Action a = make_char_action(INSERT_CHAR, 0, i, (char)('a' + i));
        push_action(s, a);
    }
    Action out;
    pop_action(s, &out);
    TEST_ASSERT_EQUAL_CHAR('d', out.character);
    pop_action(s, &out);
    TEST_ASSERT_EQUAL_CHAR('c', out.character);
    free_action_stack(s);
}

// --------------------------------------------------------- free_action_stack

void test_free_null_stack_does_not_crash(void) {
    free_action_stack(NULL);
}

// ------------------------------------------------------------------- main

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_new_action_stack_returns_non_null);
    RUN_TEST(test_new_action_stack_is_empty);
    RUN_TEST(test_new_action_stack_has_zero_size);
    RUN_TEST(test_new_action_stack_custom_initial_capacity);
    RUN_TEST(test_new_action_stack_max_capacity_clamps_initial);

    RUN_TEST(test_push_returns_true_on_success);
    RUN_TEST(test_push_increases_size);
    RUN_TEST(test_push_multiple_increases_size);
    RUN_TEST(test_push_null_stack_returns_false);
    RUN_TEST(test_push_triggers_growth_beyond_default_capacity);

    RUN_TEST(test_pop_returns_false_on_empty_stack);
    RUN_TEST(test_pop_returns_true_when_item_present);
    RUN_TEST(test_pop_decreases_size);
    RUN_TEST(test_pop_returns_correct_action);
    RUN_TEST(test_pop_is_lifo_order);
    RUN_TEST(test_pop_null_stack_returns_false);

    RUN_TEST(test_peek_returns_false_on_empty_stack);
    RUN_TEST(test_peek_returns_true_when_item_present);
    RUN_TEST(test_peek_does_not_change_size);
    RUN_TEST(test_peek_returns_top_item);
    RUN_TEST(test_peek_null_stack_returns_false);

    RUN_TEST(test_is_empty_true_on_new_stack);
    RUN_TEST(test_is_empty_false_after_push);
    RUN_TEST(test_is_empty_true_after_push_and_pop);
    RUN_TEST(test_is_empty_true_for_null_stack);

    RUN_TEST(test_reset_clears_size);
    RUN_TEST(test_reset_preserves_capacity);
    RUN_TEST(test_reset_then_push_works);
    RUN_TEST(test_reset_null_does_not_crash);

    RUN_TEST(test_push_pop_text_payload_round_trips);
    RUN_TEST(test_reset_frees_text_payloads);

    RUN_TEST(test_max_capacity_size_never_exceeds_limit);
    RUN_TEST(test_max_capacity_sliding_window_keeps_newest);

    RUN_TEST(test_free_null_stack_does_not_crash);

    return UNITY_END();
}