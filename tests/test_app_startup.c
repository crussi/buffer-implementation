// test_app_startup.c
//
// Unit tests for app_startup.c / app_init_from_args.
//
// app_init_from_args is the only logic extracted from main.c that is
// meaningfully unit-testable. The rest of main() — ncurses initialisation,
// the event loop, and teardown — requires a real TTY and cannot be exercised
// in a headless test harness.
//
// These tests verify every branch of the startup argument-handling logic:
//   - No arguments       → one empty tab
//   - One valid file     → one tab with file content
//   - Multiple files     → one tab per unique file
//   - Duplicate paths    → collapsed to one tab (via app_open_tab)
//   - Bad paths          → skipped; empty-tab fallback if all fail
//   - Mixed good/bad     → only good files opened, no fallback
//   - NULL / edge cases  → no crash

#include "unity.h"
#include "app_startup.h"
#include "editor_app.h"
#include "tab.h"
#include <stdio.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void make_file(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    if (f) { fputs(content, f); fclose(f); }
}

static void rm(const char *path) { remove(path); }

// ---------------------------------------------------------------------------
// setUp / tearDown
// ---------------------------------------------------------------------------

static EditorApp *app;

void setUp(void) {
    app = app_new();
}

void tearDown(void) {
    app_free(app);
    app = NULL;
}

// ===========================================================================
// No arguments  (argc == 1)
// ===========================================================================

void test_no_args_opens_one_tab(void) {
    char *argv[] = { (char *)"editor", NULL };
    app_init_from_args(app, 1, argv);
    TEST_ASSERT_EQUAL_INT(1, app_tab_count(app));
}

void test_no_args_tab_is_empty(void) {
    char *argv[] = { (char *)"editor", NULL };
    app_init_from_args(app, 1, argv);
    Tab *t = app_active_tab(app);
    TEST_ASSERT_NOT_NULL(t);
    TEST_ASSERT_EQUAL_INT(1, t->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("", t->buf->rows[0].line);
}

void test_no_args_tab_filepath_is_null(void) {
    char *argv[] = { (char *)"editor", NULL };
    app_init_from_args(app, 1, argv);
    TEST_ASSERT_NULL(app_active_tab(app)->filepath);
}

void test_no_args_tab_not_dirty(void) {
    char *argv[] = { (char *)"editor", NULL };
    app_init_from_args(app, 1, argv);
    TEST_ASSERT_FALSE(app_active_tab(app)->dirty);
}

// ===========================================================================
// One valid file argument
// ===========================================================================

void test_one_valid_file_opens_one_tab(void) {
    make_file("tmp_s_one.txt", "hello\n");
    char *argv[] = { (char *)"editor", (char *)"tmp_s_one.txt", NULL };
    app_init_from_args(app, 2, argv);
    TEST_ASSERT_EQUAL_INT(1, app_tab_count(app));
    rm("tmp_s_one.txt");
}

void test_one_valid_file_loads_content(void) {
    make_file("tmp_s_content.txt", "world\n");
    char *argv[] = { (char *)"editor", (char *)"tmp_s_content.txt", NULL };
    app_init_from_args(app, 2, argv);
    Tab *t = app_active_tab(app);
    TEST_ASSERT_EQUAL_STRING("world", t->buf->rows[0].line);
    rm("tmp_s_content.txt");
}

void test_one_valid_file_filepath_set(void) {
    make_file("tmp_s_path.txt", "x\n");
    char *argv[] = { (char *)"editor", (char *)"tmp_s_path.txt", NULL };
    app_init_from_args(app, 2, argv);
    TEST_ASSERT_EQUAL_STRING("tmp_s_path.txt", app_active_tab(app)->filepath);
    rm("tmp_s_path.txt");
}

void test_one_valid_file_tab_not_dirty(void) {
    make_file("tmp_s_dirty.txt", "clean\n");
    char *argv[] = { (char *)"editor", (char *)"tmp_s_dirty.txt", NULL };
    app_init_from_args(app, 2, argv);
    TEST_ASSERT_FALSE(app_active_tab(app)->dirty);
    rm("tmp_s_dirty.txt");
}

void test_one_valid_file_active_tab_is_zero(void) {
    make_file("tmp_s_active.txt", "a\n");
    char *argv[] = { (char *)"editor", (char *)"tmp_s_active.txt", NULL };
    app_init_from_args(app, 2, argv);
    TEST_ASSERT_EQUAL_INT(0, app->active);
    rm("tmp_s_active.txt");
}

// ===========================================================================
// Multiple valid file arguments
// ===========================================================================

void test_two_valid_files_opens_two_tabs(void) {
    make_file("tmp_m_a.txt", "aaa\n");
    make_file("tmp_m_b.txt", "bbb\n");
    char *argv[] = { (char *)"editor",
                     (char *)"tmp_m_a.txt",
                     (char *)"tmp_m_b.txt", NULL };
    app_init_from_args(app, 3, argv);
    TEST_ASSERT_EQUAL_INT(2, app_tab_count(app));
    rm("tmp_m_a.txt");
    rm("tmp_m_b.txt");
}

void test_two_valid_files_last_is_active(void) {
    make_file("tmp_m_act_a.txt", "a\n");
    make_file("tmp_m_act_b.txt", "b\n");
    char *argv[] = { (char *)"editor",
                     (char *)"tmp_m_act_a.txt",
                     (char *)"tmp_m_act_b.txt", NULL };
    app_init_from_args(app, 3, argv);
    TEST_ASSERT_EQUAL_INT(1, app->active);
    rm("tmp_m_act_a.txt");
    rm("tmp_m_act_b.txt");
}

void test_two_valid_files_content_correct(void) {
    make_file("tmp_m_c1.txt", "first\n");
    make_file("tmp_m_c2.txt", "second\n");
    char *argv[] = { (char *)"editor",
                     (char *)"tmp_m_c1.txt",
                     (char *)"tmp_m_c2.txt", NULL };
    app_init_from_args(app, 3, argv);
    TEST_ASSERT_EQUAL_STRING("first",  app->tabs[0]->buf->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("second", app->tabs[1]->buf->rows[0].line);
    rm("tmp_m_c1.txt");
    rm("tmp_m_c2.txt");
}

void test_three_valid_files_opens_three_tabs(void) {
    make_file("tmp_3_a.txt", "a\n");
    make_file("tmp_3_b.txt", "b\n");
    make_file("tmp_3_c.txt", "c\n");
    char *argv[] = { (char *)"editor",
                     (char *)"tmp_3_a.txt",
                     (char *)"tmp_3_b.txt",
                     (char *)"tmp_3_c.txt", NULL };
    app_init_from_args(app, 4, argv);
    TEST_ASSERT_EQUAL_INT(3, app_tab_count(app));
    rm("tmp_3_a.txt"); rm("tmp_3_b.txt"); rm("tmp_3_c.txt");
}

// ===========================================================================
// Duplicate file arguments
// ===========================================================================

void test_duplicate_path_opens_only_one_tab(void) {
    make_file("tmp_dup_s.txt", "dup\n");
    char *argv[] = { (char *)"editor",
                     (char *)"tmp_dup_s.txt",
                     (char *)"tmp_dup_s.txt", NULL };
    app_init_from_args(app, 3, argv);
    TEST_ASSERT_EQUAL_INT(1, app_tab_count(app));
    rm("tmp_dup_s.txt");
}

void test_duplicate_path_switches_to_existing_tab(void) {
    // With two valid files plus a duplicate of the first, the active tab
    // should end up at index 0 (the duplicate switches back to it).
    make_file("tmp_dup_sw_a.txt", "a\n");
    make_file("tmp_dup_sw_b.txt", "b\n");
    char *argv[] = { (char *)"editor",
                     (char *)"tmp_dup_sw_a.txt",   // opens at 0
                     (char *)"tmp_dup_sw_b.txt",   // opens at 1
                     (char *)"tmp_dup_sw_a.txt",   // duplicate → switches to 0
                     NULL };
    app_init_from_args(app, 4, argv);
    TEST_ASSERT_EQUAL_INT(2, app_tab_count(app));
    TEST_ASSERT_EQUAL_INT(0, app->active);
    rm("tmp_dup_sw_a.txt");
    rm("tmp_dup_sw_b.txt");
}

void test_all_duplicate_args_results_in_one_tab(void) {
    make_file("tmp_alldups.txt", "dup\n");
    char *argv[] = { (char *)"editor",
                     (char *)"tmp_alldups.txt",
                     (char *)"tmp_alldups.txt",
                     (char *)"tmp_alldups.txt", NULL };
    app_init_from_args(app, 4, argv);
    TEST_ASSERT_EQUAL_INT(1, app_tab_count(app));
    rm("tmp_alldups.txt");
}

// ===========================================================================
// Bad / nonexistent file arguments
// ===========================================================================

void test_one_bad_path_falls_back_to_empty_tab(void) {
    char *argv[] = { (char *)"editor",
                     (char *)"/no/such/file.txt", NULL };
    app_init_from_args(app, 2, argv);
    TEST_ASSERT_EQUAL_INT(1, app_tab_count(app));
    // The fallback tab must be empty with no filepath
    TEST_ASSERT_NULL(app_active_tab(app)->filepath);
    TEST_ASSERT_EQUAL_STRING("", app_active_tab(app)->buf->rows[0].line);
}

void test_all_bad_paths_falls_back_to_one_empty_tab(void) {
    char *argv[] = { (char *)"editor",
                     (char *)"/no/a.txt",
                     (char *)"/no/b.txt",
                     (char *)"/no/c.txt", NULL };
    app_init_from_args(app, 4, argv);
    TEST_ASSERT_EQUAL_INT(1, app_tab_count(app));
    TEST_ASSERT_NULL(app_active_tab(app)->filepath);
}

void test_all_bad_paths_no_dirty_tabs(void) {
    char *argv[] = { (char *)"editor",
                     (char *)"/no/such/file.txt", NULL };
    app_init_from_args(app, 2, argv);
    TEST_ASSERT_FALSE(app_any_dirty(app));
}

// ===========================================================================
// Mixed good and bad file arguments
// ===========================================================================

void test_mixed_good_and_bad_opens_only_good(void) {
    make_file("tmp_mix_good.txt", "good\n");
    char *argv[] = { (char *)"editor",
                     (char *)"/no/bad.txt",
                     (char *)"tmp_mix_good.txt", NULL };
    app_init_from_args(app, 3, argv);
    TEST_ASSERT_EQUAL_INT(1, app_tab_count(app));
    TEST_ASSERT_EQUAL_STRING("good", app_active_tab(app)->buf->rows[0].line);
    rm("tmp_mix_good.txt");
}

void test_mixed_no_fallback_when_at_least_one_good(void) {
    // If at least one file opened successfully there should be no fallback
    // empty tab — count must equal the number of successful opens only.
    make_file("tmp_mix_nofb.txt", "ok\n");
    char *argv[] = { (char *)"editor",
                     (char *)"tmp_mix_nofb.txt",
                     (char *)"/bad/path.txt", NULL };
    app_init_from_args(app, 3, argv);
    TEST_ASSERT_EQUAL_INT(1, app_tab_count(app));
    rm("tmp_mix_nofb.txt");
}

void test_mixed_bad_first_good_last_active_is_good(void) {
    make_file("tmp_mix_order.txt", "last\n");
    char *argv[] = { (char *)"editor",
                     (char *)"/bad/one.txt",
                     (char *)"/bad/two.txt",
                     (char *)"tmp_mix_order.txt", NULL };
    app_init_from_args(app, 4, argv);
    TEST_ASSERT_EQUAL_STRING("last", app_active_tab(app)->buf->rows[0].line);
    rm("tmp_mix_order.txt");
}

void test_two_good_one_bad_middle_opens_two_tabs(void) {
    make_file("tmp_mix2_a.txt", "alpha\n");
    make_file("tmp_mix2_b.txt", "beta\n");
    char *argv[] = { (char *)"editor",
                     (char *)"tmp_mix2_a.txt",
                     (char *)"/bad/middle.txt",
                     (char *)"tmp_mix2_b.txt", NULL };
    app_init_from_args(app, 4, argv);
    TEST_ASSERT_EQUAL_INT(2, app_tab_count(app));
    TEST_ASSERT_EQUAL_STRING("alpha", app->tabs[0]->buf->rows[0].line);
    TEST_ASSERT_EQUAL_STRING("beta",  app->tabs[1]->buf->rows[0].line);
    rm("tmp_mix2_a.txt");
    rm("tmp_mix2_b.txt");
}

// ===========================================================================
// Null / edge case safety
// ===========================================================================

void test_null_app_does_not_crash(void) {
    char *argv[] = { (char *)"editor", NULL };
    app_init_from_args(NULL, 1, argv);   // must not crash
}

void test_argc_zero_does_not_crash(void) {
    // Degenerate: argc == 0 means no argv[0] either.
    // app_init_from_args treats any argc <= 1 as "no file arguments".
    char *argv[] = { NULL };
    app_init_from_args(app, 0, argv);
    // Should either produce one empty tab or do nothing — must not crash.
}

// ===========================================================================
// Idempotency — calling twice on the same app
// ===========================================================================

void test_called_twice_adds_tabs_both_times(void) {
    // app_init_from_args has no "already initialised" guard — calling it
    // twice on the same app should simply add more tabs (or collapse
    // duplicates).  This documents the actual behaviour so a future change
    // that breaks it is visible.
    make_file("tmp_idem_a.txt", "a\n");
    make_file("tmp_idem_b.txt", "b\n");

    char *argv1[] = { (char *)"editor", (char *)"tmp_idem_a.txt", NULL };
    char *argv2[] = { (char *)"editor", (char *)"tmp_idem_b.txt", NULL };

    app_init_from_args(app, 2, argv1);
    int after_first = app_tab_count(app);
    app_init_from_args(app, 2, argv2);
    int after_second = app_tab_count(app);

    TEST_ASSERT_EQUAL_INT(1, after_first);
    TEST_ASSERT_EQUAL_INT(2, after_second);

    rm("tmp_idem_a.txt");
    rm("tmp_idem_b.txt");
}

void test_called_twice_with_same_file_stays_at_one_tab(void) {
    make_file("tmp_idem_dup.txt", "dup\n");
    char *argv[] = { (char *)"editor", (char *)"tmp_idem_dup.txt", NULL };
    app_init_from_args(app, 2, argv);
    app_init_from_args(app, 2, argv);   // duplicate — still one tab
    TEST_ASSERT_EQUAL_INT(1, app_tab_count(app));
    rm("tmp_idem_dup.txt");
}

// ===========================================================================
// main
// ===========================================================================

int main(void) {
    UNITY_BEGIN();

    // No arguments
    RUN_TEST(test_no_args_opens_one_tab);
    RUN_TEST(test_no_args_tab_is_empty);
    RUN_TEST(test_no_args_tab_filepath_is_null);
    RUN_TEST(test_no_args_tab_not_dirty);

    // One valid file
    RUN_TEST(test_one_valid_file_opens_one_tab);
    RUN_TEST(test_one_valid_file_loads_content);
    RUN_TEST(test_one_valid_file_filepath_set);
    RUN_TEST(test_one_valid_file_tab_not_dirty);
    RUN_TEST(test_one_valid_file_active_tab_is_zero);

    // Multiple valid files
    RUN_TEST(test_two_valid_files_opens_two_tabs);
    RUN_TEST(test_two_valid_files_last_is_active);
    RUN_TEST(test_two_valid_files_content_correct);
    RUN_TEST(test_three_valid_files_opens_three_tabs);

    // Duplicates
    RUN_TEST(test_duplicate_path_opens_only_one_tab);
    RUN_TEST(test_duplicate_path_switches_to_existing_tab);
    RUN_TEST(test_all_duplicate_args_results_in_one_tab);

    // Bad paths
    RUN_TEST(test_one_bad_path_falls_back_to_empty_tab);
    RUN_TEST(test_all_bad_paths_falls_back_to_one_empty_tab);
    RUN_TEST(test_all_bad_paths_no_dirty_tabs);

    // Mixed good and bad
    RUN_TEST(test_mixed_good_and_bad_opens_only_good);
    RUN_TEST(test_mixed_no_fallback_when_at_least_one_good);
    RUN_TEST(test_mixed_bad_first_good_last_active_is_good);
    RUN_TEST(test_two_good_one_bad_middle_opens_two_tabs);

    // Null / edge cases
    RUN_TEST(test_null_app_does_not_crash);
    RUN_TEST(test_argc_zero_does_not_crash);

    // Idempotency
    RUN_TEST(test_called_twice_adds_tabs_both_times);
    RUN_TEST(test_called_twice_with_same_file_stays_at_one_tab);

    return UNITY_END();
}