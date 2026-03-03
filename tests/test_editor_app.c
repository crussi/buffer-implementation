// test_editor_app.c
//
// Unit tests for editor_app.c / editor_app.h.
//
// These tests exercise every public function in EditorApp, with particular
// depth on the two areas changed in the recent update:
//   1. Duplicate-file detection in app_open_tab
//   2. The new app_save_active_as function
//
// ncurses is NOT initialised here; none of these tests touch input.c.

#include "unity.h"
#include "editor_app.h"
#include "tab.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Write a small temp file and return its path (static buffer — use before
// the next call to make_temp_file).
static const char *make_temp_file(const char *name, const char *content) {
    FILE *f = fopen(name, "w");
    if (!f) return NULL;
    fputs(content, f);
    fclose(f);
    return name;
}

// Remove a temp file, ignoring errors.
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
// app_new / app_free
// ===========================================================================

void test_app_new_returns_non_null(void) {
    TEST_ASSERT_NOT_NULL(app);
}

void test_app_new_has_zero_tabs(void) {
    TEST_ASSERT_EQUAL_INT(0, app_tab_count(app));
}

void test_app_new_active_is_zero(void) {
    TEST_ASSERT_EQUAL_INT(0, app->active);
}

void test_app_free_null_does_not_crash(void) {
    app_free(NULL);
}

// ===========================================================================
// app_new_tab
// ===========================================================================

void test_app_new_tab_returns_non_null(void) {
    Tab *t = app_new_tab(app);
    TEST_ASSERT_NOT_NULL(t);
}

void test_app_new_tab_increments_count(void) {
    app_new_tab(app);
    TEST_ASSERT_EQUAL_INT(1, app_tab_count(app));
    app_new_tab(app);
    TEST_ASSERT_EQUAL_INT(2, app_tab_count(app));
}

void test_app_new_tab_becomes_active(void) {
    app_new_tab(app);
    app_new_tab(app);   // second tab
    TEST_ASSERT_EQUAL_INT(1, app->active);
}

void test_app_new_tab_is_empty(void) {
    Tab *t = app_new_tab(app);
    TEST_ASSERT_EQUAL_INT(1, t->buf->numrows);
    TEST_ASSERT_EQUAL_STRING("", t->buf->rows[0].line);
}

void test_app_new_tab_filepath_is_null(void) {
    Tab *t = app_new_tab(app);
    TEST_ASSERT_NULL(t->filepath);
}

void test_app_new_tab_not_dirty(void) {
    Tab *t = app_new_tab(app);
    TEST_ASSERT_FALSE(t->dirty);
}

void test_app_new_tab_null_app_returns_null(void) {
    TEST_ASSERT_NULL(app_new_tab(NULL));
}

// ===========================================================================
// app_open_tab — basic open
// ===========================================================================

void test_app_open_tab_returns_non_null_for_valid_file(void) {
    make_temp_file("tmp_open1.txt", "hello\n");
    Tab *t = app_open_tab(app, "tmp_open1.txt");
    TEST_ASSERT_NOT_NULL(t);
    rm("tmp_open1.txt");
}

void test_app_open_tab_increments_count(void) {
    make_temp_file("tmp_open2.txt", "hello\n");
    app_open_tab(app, "tmp_open2.txt");
    TEST_ASSERT_EQUAL_INT(1, app_tab_count(app));
    rm("tmp_open2.txt");
}

void test_app_open_tab_becomes_active(void) {
    make_temp_file("tmp_open3a.txt", "a\n");
    make_temp_file("tmp_open3b.txt", "b\n");
    app_open_tab(app, "tmp_open3a.txt");
    app_open_tab(app, "tmp_open3b.txt");
    TEST_ASSERT_EQUAL_INT(1, app->active);
    rm("tmp_open3a.txt");
    rm("tmp_open3b.txt");
}

void test_app_open_tab_loads_content(void) {
    make_temp_file("tmp_open4.txt", "world\n");
    Tab *t = app_open_tab(app, "tmp_open4.txt");
    TEST_ASSERT_EQUAL_STRING("world", t->buf->rows[0].line);
    rm("tmp_open4.txt");
}

void test_app_open_tab_filepath_set(void) {
    make_temp_file("tmp_open5.txt", "x\n");
    Tab *t = app_open_tab(app, "tmp_open5.txt");
    TEST_ASSERT_EQUAL_STRING("tmp_open5.txt", t->filepath);
    rm("tmp_open5.txt");
}

void test_app_open_tab_not_dirty(void) {
    make_temp_file("tmp_open6.txt", "clean\n");
    Tab *t = app_open_tab(app, "tmp_open6.txt");
    TEST_ASSERT_FALSE(t->dirty);
    rm("tmp_open6.txt");
}

void test_app_open_tab_bad_path_returns_null(void) {
    Tab *t = app_open_tab(app, "/no/such/file/exists.txt");
    TEST_ASSERT_NULL(t);
}

void test_app_open_tab_bad_path_does_not_change_count(void) {
    app_open_tab(app, "/no/such/file/exists.txt");
    TEST_ASSERT_EQUAL_INT(0, app_tab_count(app));
}

void test_app_open_tab_null_path_returns_null(void) {
    TEST_ASSERT_NULL(app_open_tab(app, NULL));
}

void test_app_open_tab_null_app_returns_null(void) {
    TEST_ASSERT_NULL(app_open_tab(NULL, "anything.txt"));
}

// ===========================================================================
// app_open_tab — duplicate detection  (KEY NEW BEHAVIOUR)
// ===========================================================================

void test_open_same_file_twice_count_stays_one(void) {
    make_temp_file("tmp_dup1.txt", "dup\n");
    app_open_tab(app, "tmp_dup1.txt");
    app_open_tab(app, "tmp_dup1.txt");   // duplicate — must not add a second tab
    TEST_ASSERT_EQUAL_INT(1, app_tab_count(app));
    rm("tmp_dup1.txt");
}

void test_open_same_file_twice_returns_same_pointer(void) {
    make_temp_file("tmp_dup2.txt", "dup\n");
    Tab *first  = app_open_tab(app, "tmp_dup2.txt");
    Tab *second = app_open_tab(app, "tmp_dup2.txt");
    TEST_ASSERT_EQUAL_PTR(first, second);
    rm("tmp_dup2.txt");
}

void test_open_duplicate_switches_to_existing_tab(void) {
    make_temp_file("tmp_dup3a.txt", "a\n");
    make_temp_file("tmp_dup3b.txt", "b\n");
    app_open_tab(app, "tmp_dup3a.txt");   // index 0
    app_open_tab(app, "tmp_dup3b.txt");   // index 1, now active
    app_open_tab(app, "tmp_dup3a.txt");   // duplicate — should switch back to index 0
    TEST_ASSERT_EQUAL_INT(0, app->active);
    rm("tmp_dup3a.txt");
    rm("tmp_dup3b.txt");
}

void test_open_duplicate_does_not_reset_edits(void) {
    // Edits made to a tab must survive a duplicate open attempt.
    make_temp_file("tmp_dup4.txt", "hello\n");
    Tab *t = app_open_tab(app, "tmp_dup4.txt");
    tabInsertChar(t, 0, 5, '!');   // now "hello!"
    app_open_tab(app, "tmp_dup4.txt");   // duplicate
    TEST_ASSERT_EQUAL_STRING("hello!", t->buf->rows[0].line);
    rm("tmp_dup4.txt");
}

void test_open_duplicate_preserves_dirty_flag(void) {
    make_temp_file("tmp_dup5.txt", "abc\n");
    Tab *t = app_open_tab(app, "tmp_dup5.txt");
    tabInsertChar(t, 0, 0, 'X');    // dirty it
    TEST_ASSERT_TRUE(t->dirty);
    app_open_tab(app, "tmp_dup5.txt");   // duplicate — dirty must still be true
    TEST_ASSERT_TRUE(t->dirty);
    rm("tmp_dup5.txt");
}

void test_open_different_files_both_added(void) {
    make_temp_file("tmp_diff1.txt", "one\n");
    make_temp_file("tmp_diff2.txt", "two\n");
    app_open_tab(app, "tmp_diff1.txt");
    app_open_tab(app, "tmp_diff2.txt");
    TEST_ASSERT_EQUAL_INT(2, app_tab_count(app));
    rm("tmp_diff1.txt");
    rm("tmp_diff2.txt");
}

void test_open_duplicate_after_save_as_new_path(void) {
    // After tab_save_as renames a tab's path, opening the old path should
    // open a fresh tab (the old path no longer matches any tab).
    make_temp_file("tmp_rename_src.txt", "data\n");
    Tab *t = app_open_tab(app, "tmp_rename_src.txt");
    tab_save_as(t, "tmp_rename_dst.txt");   // filepath now "tmp_rename_dst.txt"

    // Opening the original source path should create a NEW tab (count = 2),
    // because no tab has filepath == "tmp_rename_src.txt" anymore.
    make_temp_file("tmp_rename_src.txt", "data\n");   // re-create the file
    app_open_tab(app, "tmp_rename_src.txt");
    TEST_ASSERT_EQUAL_INT(2, app_tab_count(app));

    rm("tmp_rename_src.txt");
    rm("tmp_rename_dst.txt");
}

// ===========================================================================
// app_close_tab
// ===========================================================================

void test_app_close_tab_decrements_count(void) {
    app_new_tab(app);
    app_new_tab(app);
    app_close_tab(app, 0);
    TEST_ASSERT_EQUAL_INT(1, app_tab_count(app));
}

void test_app_close_tab_invalid_index_returns_false(void) {
    app_new_tab(app);
    TEST_ASSERT_FALSE(app_close_tab(app, 5));
    TEST_ASSERT_FALSE(app_close_tab(app, -1));
}

void test_app_close_tab_last_tab_count_zero(void) {
    app_new_tab(app);
    app_close_tab(app, 0);
    TEST_ASSERT_EQUAL_INT(0, app_tab_count(app));
}

void test_app_close_tab_active_clamps_when_last(void) {
    app_new_tab(app);
    app_new_tab(app);   // active = 1
    app_close_tab(app, 1);
    TEST_ASSERT_EQUAL_INT(0, app->active);
}

void test_app_close_tab_active_decrements_when_before_active(void) {
    app_new_tab(app);
    app_new_tab(app);
    app_new_tab(app);   // active = 2
    app_close_tab(app, 0);   // remove tab before active
    TEST_ASSERT_EQUAL_INT(1, app->active);
}

void test_app_close_tab_active_unchanged_when_after_active(void) {
    app_new_tab(app);
    app_new_tab(app);
    app_new_tab(app);
    app->active = 0;
    app_close_tab(app, 2);   // remove tab after active
    TEST_ASSERT_EQUAL_INT(0, app->active);
}

void test_app_close_tab_active_zero_when_all_closed(void) {
    app_new_tab(app);
    app_close_tab(app, 0);
    TEST_ASSERT_EQUAL_INT(0, app->active);
}

void test_app_close_tab_remaining_tab_has_correct_content(void) {
    make_temp_file("tmp_close1.txt", "keep\n");
    make_temp_file("tmp_close2.txt", "gone\n");
    app_open_tab(app, "tmp_close1.txt");
    app_open_tab(app, "tmp_close2.txt");
    app_close_tab(app, 1);   // remove "gone"
    TEST_ASSERT_EQUAL_INT(1, app_tab_count(app));
    TEST_ASSERT_EQUAL_STRING("keep", app->tabs[0]->buf->rows[0].line);
    rm("tmp_close1.txt");
    rm("tmp_close2.txt");
}

// ===========================================================================
// app_switch_tab
// ===========================================================================

void test_app_switch_tab_changes_active(void) {
    app_new_tab(app);
    app_new_tab(app);
    app_switch_tab(app, 0);
    TEST_ASSERT_EQUAL_INT(0, app->active);
}

void test_app_switch_tab_wraps_forward(void) {
    app_new_tab(app);
    app_new_tab(app);   // 2 tabs, indices 0-1
    app->active = 1;
    app_switch_tab(app, 2);   // past end — wraps to 0
    TEST_ASSERT_EQUAL_INT(0, app->active);
}

void test_app_switch_tab_wraps_backward(void) {
    app_new_tab(app);
    app_new_tab(app);
    app->active = 0;
    app_switch_tab(app, -1);  // before start — wraps to last
    TEST_ASSERT_EQUAL_INT(1, app->active);
}

void test_app_switch_tab_no_tabs_returns_false(void) {
    TEST_ASSERT_FALSE(app_switch_tab(app, 0));
}

void test_app_switch_tab_null_app_returns_false(void) {
    TEST_ASSERT_FALSE(app_switch_tab(NULL, 0));
}

// ===========================================================================
// app_active_tab
// ===========================================================================

void test_app_active_tab_returns_null_when_no_tabs(void) {
    TEST_ASSERT_NULL(app_active_tab(app));
}

void test_app_active_tab_returns_correct_tab(void) {
    Tab *t0 = app_new_tab(app);
    Tab *t1 = app_new_tab(app);
    app->active = 0;
    TEST_ASSERT_EQUAL_PTR(t0, app_active_tab(app));
    app->active = 1;
    TEST_ASSERT_EQUAL_PTR(t1, app_active_tab(app));
}

void test_app_active_tab_null_app_returns_null(void) {
    TEST_ASSERT_NULL(app_active_tab(NULL));
}

// ===========================================================================
// app_tab_count
// ===========================================================================

void test_app_tab_count_zero_initially(void) {
    TEST_ASSERT_EQUAL_INT(0, app_tab_count(app));
}

void test_app_tab_count_grows_with_tabs(void) {
    app_new_tab(app);
    TEST_ASSERT_EQUAL_INT(1, app_tab_count(app));
    app_new_tab(app);
    TEST_ASSERT_EQUAL_INT(2, app_tab_count(app));
}

void test_app_tab_count_null_returns_zero(void) {
    TEST_ASSERT_EQUAL_INT(0, app_tab_count(NULL));
}

// ===========================================================================
// app_any_dirty
// ===========================================================================

void test_app_any_dirty_false_when_no_tabs(void) {
    TEST_ASSERT_FALSE(app_any_dirty(app));
}

void test_app_any_dirty_false_when_all_clean(void) {
    app_new_tab(app);
    app_new_tab(app);
    TEST_ASSERT_FALSE(app_any_dirty(app));
}

void test_app_any_dirty_true_when_one_dirty(void) {
    Tab *t = app_new_tab(app);
    app_new_tab(app);
    tabInsertChar(t, 0, 0, 'X');
    TEST_ASSERT_TRUE(app_any_dirty(app));
}

void test_app_any_dirty_true_only_last_tab_dirty(void) {
    app_new_tab(app);
    Tab *t = app_new_tab(app);
    tabInsertChar(t, 0, 0, 'Y');
    TEST_ASSERT_TRUE(app_any_dirty(app));
}

void test_app_any_dirty_false_after_save(void) {
    Tab *t = app_new_tab(app);
    tabInsertChar(t, 0, 0, 'Z');
    TEST_ASSERT_TRUE(app_any_dirty(app));
    tab_save_as(t, "tmp_dirty_save.txt");
    TEST_ASSERT_FALSE(app_any_dirty(app));
    rm("tmp_dirty_save.txt");
}

void test_app_any_dirty_null_returns_false(void) {
    TEST_ASSERT_FALSE(app_any_dirty(NULL));
}

// ===========================================================================
// app_save_active
// ===========================================================================

void test_app_save_active_no_tabs_returns_false(void) {
    TEST_ASSERT_FALSE(app_save_active(app));
}

void test_app_save_active_no_filepath_returns_false(void) {
    // A new empty tab has no filepath — save must fail gracefully.
    app_new_tab(app);
    TEST_ASSERT_FALSE(app_save_active(app));
}

void test_app_save_active_writes_file(void) {
    Tab *t = app_new_tab(app);
    tabInsertChar(t, 0, 0, 'H');
    tab_save_as(t, "tmp_sva.txt");   // assign a path first
    tabInsertChar(t, 0, 1, 'i');    // now dirty again
    bool ok = app_save_active(app);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_FALSE(t->dirty);

    FILE *f = fopen("tmp_sva.txt", "r");
    char buf[32] = {0};
    fgets(buf, sizeof(buf), f);
    fclose(f);
    TEST_ASSERT_EQUAL_STRING("Hi\n", buf);
    rm("tmp_sva.txt");
}

void test_app_save_active_clears_dirty(void) {
    Tab *t = app_new_tab(app);
    tabInsertChar(t, 0, 0, 'A');
    tab_save_as(t, "tmp_svd.txt");
    tabInsertChar(t, 0, 1, 'B');
    app_save_active(app);
    TEST_ASSERT_FALSE(t->dirty);
    rm("tmp_svd.txt");
}

void test_app_save_active_null_app_returns_false(void) {
    TEST_ASSERT_FALSE(app_save_active(NULL));
}

// ===========================================================================
// app_save_active_as  (NEW FUNCTION)
// ===========================================================================

void test_app_save_active_as_no_tabs_returns_false(void) {
    TEST_ASSERT_FALSE(app_save_active_as(app, "anything.txt"));
}

void test_app_save_active_as_null_path_returns_false(void) {
    app_new_tab(app);
    TEST_ASSERT_FALSE(app_save_active_as(app, NULL));
}

void test_app_save_active_as_writes_file(void) {
    Tab *t = app_new_tab(app);
    tabInsertChar(t, 0, 0, 'X');
    bool ok = app_save_active_as(app, "tmp_svaa.txt");
    TEST_ASSERT_TRUE(ok);

    FILE *f = fopen("tmp_svaa.txt", "r");
    TEST_ASSERT_NOT_NULL(f);
    char buf[32] = {0};
    fgets(buf, sizeof(buf), f);
    fclose(f);
    TEST_ASSERT_EQUAL_STRING("X\n", buf);
    rm("tmp_svaa.txt");
}

void test_app_save_active_as_sets_filepath(void) {
    Tab *t = app_new_tab(app);
    TEST_ASSERT_NULL(t->filepath);
    app_save_active_as(app, "tmp_svaa_path.txt");
    TEST_ASSERT_EQUAL_STRING("tmp_svaa_path.txt", t->filepath);
    rm("tmp_svaa_path.txt");
}

void test_app_save_active_as_clears_dirty(void) {
    Tab *t = app_new_tab(app);
    tabInsertChar(t, 0, 0, 'A');
    TEST_ASSERT_TRUE(t->dirty);
    app_save_active_as(app, "tmp_svaa_dirty.txt");
    TEST_ASSERT_FALSE(t->dirty);
    rm("tmp_svaa_dirty.txt");
}

void test_app_save_active_as_renames_existing_path(void) {
    // A tab saved at path A can be re-saved to path B.
    // After that, filepath == B and B exists on disk.
    Tab *t = app_new_tab(app);
    tabInsertChar(t, 0, 0, 'R');
    app_save_active_as(app, "tmp_rename_a.txt");
    app_save_active_as(app, "tmp_rename_b.txt");
    TEST_ASSERT_EQUAL_STRING("tmp_rename_b.txt", t->filepath);

    FILE *f = fopen("tmp_rename_b.txt", "r");
    TEST_ASSERT_NOT_NULL(f);
    fclose(f);
    rm("tmp_rename_a.txt");
    rm("tmp_rename_b.txt");
}

void test_app_save_active_as_saves_correct_tab(void) {
    // With multiple tabs open, save-as must write the ACTIVE tab, not others.
    Tab *t0 = app_new_tab(app);
    tabInsertChar(t0, 0, 0, 'A');
    Tab *t1 = app_new_tab(app);   // active
    tabInsertChar(t1, 0, 0, 'B');

    app_save_active_as(app, "tmp_svaa_correct.txt");

    FILE *f = fopen("tmp_svaa_correct.txt", "r");
    char buf[32] = {0};
    fgets(buf, sizeof(buf), f);
    fclose(f);
    TEST_ASSERT_EQUAL_STRING("B\n", buf);   // must be t1's content
    rm("tmp_svaa_correct.txt");
}

void test_app_save_active_as_does_not_affect_other_tabs(void) {
    Tab *t0 = app_new_tab(app);
    tabInsertChar(t0, 0, 0, 'P');
    Tab *t1 = app_new_tab(app);
    tabInsertChar(t1, 0, 0, 'Q');

    app_save_active_as(app, "tmp_svaa_other.txt");

    // t0 was not saved — must still be dirty
    TEST_ASSERT_TRUE(t0->dirty);
    rm("tmp_svaa_other.txt");
}

void test_app_save_active_as_null_app_returns_false(void) {
    TEST_ASSERT_FALSE(app_save_active_as(NULL, "anything.txt"));
}

// ===========================================================================
// app_save_all
// ===========================================================================

void test_app_save_all_no_tabs_returns_true(void) {
    // Nothing to save — vacuously true
    TEST_ASSERT_TRUE(app_save_all(app));
}

void test_app_save_all_saves_all_dirty_tabs(void) {
    Tab *t0 = app_new_tab(app);
    tabInsertChar(t0, 0, 0, 'A');
    tab_save_as(t0, "tmp_all0.txt");
    tabInsertChar(t0, 0, 1, 'B');   // dirty again

    Tab *t1 = app_new_tab(app);
    tabInsertChar(t1, 0, 0, 'C');
    tab_save_as(t1, "tmp_all1.txt");
    tabInsertChar(t1, 0, 1, 'D');   // dirty again

    app_save_all(app);
    TEST_ASSERT_FALSE(t0->dirty);
    TEST_ASSERT_FALSE(t1->dirty);
    rm("tmp_all0.txt");
    rm("tmp_all1.txt");
}

void test_app_save_all_skips_tabs_without_filepath(void) {
    // A new tab has no path — save_all should return false (partial failure)
    // but must not crash, and must not dirty other tabs.
    Tab *t0 = app_new_tab(app);
    tabInsertChar(t0, 0, 0, 'X');
    tab_save_as(t0, "tmp_all_skip.txt");
    tabInsertChar(t0, 0, 1, 'Y');   // t0 dirty, has path

    Tab *t1 = app_new_tab(app);
    tabInsertChar(t1, 0, 0, 'Z');   // t1 dirty, NO path

    bool ok = app_save_all(app);
    // t0 should be saved, t1 skipped → not all saves succeeded
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_FALSE(t0->dirty);    // t0 was saved
    TEST_ASSERT_TRUE(t1->dirty);     // t1 was not saved
    rm("tmp_all_skip.txt");
}

void test_app_save_all_clean_tabs_not_written(void) {
    // A clean tab must not be touched by save_all.
    Tab *t = app_new_tab(app);
    tabInsertChar(t, 0, 0, 'A');
    tab_save_as(t, "tmp_all_clean.txt");   // clears dirty
    TEST_ASSERT_FALSE(t->dirty);

    // Overwrite the file so we can detect if save_all rewrites it
    FILE *f = fopen("tmp_all_clean.txt", "w");
    fputs("sentinel\n", f);
    fclose(f);

    app_save_all(app);

    // File should still contain our sentinel, not the tab content,
    // because the tab was clean and save_all skips clean tabs.
    f = fopen("tmp_all_clean.txt", "r");
    char buf[32] = {0};
    fgets(buf, sizeof(buf), f);
    fclose(f);
    TEST_ASSERT_EQUAL_STRING("sentinel\n", buf);
    rm("tmp_all_clean.txt");
}

void test_app_save_all_null_returns_false(void) {
    TEST_ASSERT_FALSE(app_save_all(NULL));
}

// ===========================================================================
// Integration — multi-tab workflow
// ===========================================================================

void test_workflow_open_edit_save_as_reopen(void) {
    // Open a file, edit it, save-as to a new name, then re-open original.
    make_temp_file("tmp_wf_src.txt", "original\n");

    Tab *t = app_open_tab(app, "tmp_wf_src.txt");
    tabInsertChar(t, 0, 8, '!');  // "original!"
    app_save_active_as(app, "tmp_wf_dst.txt");

    // Re-opening original should give a new tab (different path)
    app_open_tab(app, "tmp_wf_src.txt");
    TEST_ASSERT_EQUAL_INT(2, app_tab_count(app));

    // Active tab should be the original (index 1)
    TEST_ASSERT_EQUAL_STRING("original", app_active_tab(app)->buf->rows[0].line);

    rm("tmp_wf_src.txt");
    rm("tmp_wf_dst.txt");
}

void test_workflow_duplicate_open_across_multiple_tabs(void) {
    make_temp_file("tmp_multi_a.txt", "aaa\n");
    make_temp_file("tmp_multi_b.txt", "bbb\n");
    make_temp_file("tmp_multi_c.txt", "ccc\n");

    app_open_tab(app, "tmp_multi_a.txt");   // index 0
    app_open_tab(app, "tmp_multi_b.txt");   // index 1
    app_open_tab(app, "tmp_multi_c.txt");   // index 2, active

    // Duplicate open of b — should switch to index 1 without adding a tab
    Tab *b = app_open_tab(app, "tmp_multi_b.txt");
    TEST_ASSERT_EQUAL_INT(3, app_tab_count(app));
    TEST_ASSERT_EQUAL_INT(1, app->active);
    TEST_ASSERT_EQUAL_STRING("bbb", b->buf->rows[0].line);

    rm("tmp_multi_a.txt");
    rm("tmp_multi_b.txt");
    rm("tmp_multi_c.txt");
}

void test_workflow_close_dirty_tab_then_check_any_dirty(void) {
    Tab *t0 = app_new_tab(app);
    tabInsertChar(t0, 0, 0, 'D');   // dirty
    Tab *t1 = app_new_tab(app);     // clean
    (void)t1;

    TEST_ASSERT_TRUE(app_any_dirty(app));
    app_close_tab(app, 0);   // close the dirty one
    TEST_ASSERT_FALSE(app_any_dirty(app));
}

void test_workflow_switch_then_save_as(void) {
    Tab *t0 = app_new_tab(app);
    tabInsertChar(t0, 0, 0, 'F');
    Tab *t1 = app_new_tab(app);
    tabInsertChar(t1, 0, 0, 'S');

    app_switch_tab(app, 0);
    app_save_active_as(app, "tmp_switch_save.txt");

    // Saved file should contain t0's content, not t1's
    FILE *f = fopen("tmp_switch_save.txt", "r");
    char buf[32] = {0};
    fgets(buf, sizeof(buf), f);
    fclose(f);
    TEST_ASSERT_EQUAL_STRING("F\n", buf);
    rm("tmp_switch_save.txt");
}

// ===========================================================================
// main
// ===========================================================================

int main(void) {
    UNITY_BEGIN();

    // app_new / app_free
    RUN_TEST(test_app_new_returns_non_null);
    RUN_TEST(test_app_new_has_zero_tabs);
    RUN_TEST(test_app_new_active_is_zero);
    RUN_TEST(test_app_free_null_does_not_crash);

    // app_new_tab
    RUN_TEST(test_app_new_tab_returns_non_null);
    RUN_TEST(test_app_new_tab_increments_count);
    RUN_TEST(test_app_new_tab_becomes_active);
    RUN_TEST(test_app_new_tab_is_empty);
    RUN_TEST(test_app_new_tab_filepath_is_null);
    RUN_TEST(test_app_new_tab_not_dirty);
    RUN_TEST(test_app_new_tab_null_app_returns_null);

    // app_open_tab — basic
    RUN_TEST(test_app_open_tab_returns_non_null_for_valid_file);
    RUN_TEST(test_app_open_tab_increments_count);
    RUN_TEST(test_app_open_tab_becomes_active);
    RUN_TEST(test_app_open_tab_loads_content);
    RUN_TEST(test_app_open_tab_filepath_set);
    RUN_TEST(test_app_open_tab_not_dirty);
    RUN_TEST(test_app_open_tab_bad_path_returns_null);
    RUN_TEST(test_app_open_tab_bad_path_does_not_change_count);
    RUN_TEST(test_app_open_tab_null_path_returns_null);
    RUN_TEST(test_app_open_tab_null_app_returns_null);

    // app_open_tab — duplicate detection
    RUN_TEST(test_open_same_file_twice_count_stays_one);
    RUN_TEST(test_open_same_file_twice_returns_same_pointer);
    RUN_TEST(test_open_duplicate_switches_to_existing_tab);
    RUN_TEST(test_open_duplicate_does_not_reset_edits);
    RUN_TEST(test_open_duplicate_preserves_dirty_flag);
    RUN_TEST(test_open_different_files_both_added);
    RUN_TEST(test_open_duplicate_after_save_as_new_path);

    // app_close_tab
    RUN_TEST(test_app_close_tab_decrements_count);
    RUN_TEST(test_app_close_tab_invalid_index_returns_false);
    RUN_TEST(test_app_close_tab_last_tab_count_zero);
    RUN_TEST(test_app_close_tab_active_clamps_when_last);
    RUN_TEST(test_app_close_tab_active_decrements_when_before_active);
    RUN_TEST(test_app_close_tab_active_unchanged_when_after_active);
    RUN_TEST(test_app_close_tab_active_zero_when_all_closed);
    RUN_TEST(test_app_close_tab_remaining_tab_has_correct_content);

    // app_switch_tab
    RUN_TEST(test_app_switch_tab_changes_active);
    RUN_TEST(test_app_switch_tab_wraps_forward);
    RUN_TEST(test_app_switch_tab_wraps_backward);
    RUN_TEST(test_app_switch_tab_no_tabs_returns_false);
    RUN_TEST(test_app_switch_tab_null_app_returns_false);

    // app_active_tab
    RUN_TEST(test_app_active_tab_returns_null_when_no_tabs);
    RUN_TEST(test_app_active_tab_returns_correct_tab);
    RUN_TEST(test_app_active_tab_null_app_returns_null);

    // app_tab_count
    RUN_TEST(test_app_tab_count_zero_initially);
    RUN_TEST(test_app_tab_count_grows_with_tabs);
    RUN_TEST(test_app_tab_count_null_returns_zero);

    // app_any_dirty
    RUN_TEST(test_app_any_dirty_false_when_no_tabs);
    RUN_TEST(test_app_any_dirty_false_when_all_clean);
    RUN_TEST(test_app_any_dirty_true_when_one_dirty);
    RUN_TEST(test_app_any_dirty_true_only_last_tab_dirty);
    RUN_TEST(test_app_any_dirty_false_after_save);
    RUN_TEST(test_app_any_dirty_null_returns_false);

    // app_save_active
    RUN_TEST(test_app_save_active_no_tabs_returns_false);
    RUN_TEST(test_app_save_active_no_filepath_returns_false);
    RUN_TEST(test_app_save_active_writes_file);
    RUN_TEST(test_app_save_active_clears_dirty);
    RUN_TEST(test_app_save_active_null_app_returns_false);

    // app_save_active_as  (new function)
    RUN_TEST(test_app_save_active_as_no_tabs_returns_false);
    RUN_TEST(test_app_save_active_as_null_path_returns_false);
    RUN_TEST(test_app_save_active_as_writes_file);
    RUN_TEST(test_app_save_active_as_sets_filepath);
    RUN_TEST(test_app_save_active_as_clears_dirty);
    RUN_TEST(test_app_save_active_as_renames_existing_path);
    RUN_TEST(test_app_save_active_as_saves_correct_tab);
    RUN_TEST(test_app_save_active_as_does_not_affect_other_tabs);
    RUN_TEST(test_app_save_active_as_null_app_returns_false);

    // app_save_all
    RUN_TEST(test_app_save_all_no_tabs_returns_true);
    RUN_TEST(test_app_save_all_saves_all_dirty_tabs);
    RUN_TEST(test_app_save_all_skips_tabs_without_filepath);
    RUN_TEST(test_app_save_all_clean_tabs_not_written);
    RUN_TEST(test_app_save_all_null_returns_false);

    // Integration
    RUN_TEST(test_workflow_open_edit_save_as_reopen);
    RUN_TEST(test_workflow_duplicate_open_across_multiple_tabs);
    RUN_TEST(test_workflow_close_dirty_tab_then_check_any_dirty);
    RUN_TEST(test_workflow_switch_then_save_as);

    return UNITY_END();
}