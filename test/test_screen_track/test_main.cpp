#include <unity.h>

#include <cstdio>

#include "crash_guard.h"
#include "screen_track.h"

using namespace screen_track;

void setUp() {}
void tearDown() {}

static Record rec(uint32_t id, uint32_t ch, uint32_t sig) {
    Record r;
    r.valid = true;
    r.screen_id = id;
    r.content_hash = ch;
    r.panel_sig = sig;
    return r;
}

void test_fnv_known_values() {
    TEST_ASSERT_EQUAL_UINT32(0x811C9DC5u, fnv1a("", kFnvSeed));
    TEST_ASSERT_EQUAL_UINT32(0xE40C292Cu, fnv1a("a"));
    TEST_ASSERT_EQUAL_UINT32(0xBF9CF968u, fnv1a("foobar"));
}

void test_fnv_chaining_differs() {
    TEST_ASSERT_NOT_EQUAL(fnv1a("a", fnv1a("b")), fnv1a("b", fnv1a("a")));
    TEST_ASSERT_NOT_EQUAL(fnv1a(uint32_t{1}), fnv1a(uint32_t{2}));
}

void test_decide_no_record() {
    auto d = decide(Record{}, 1, 2, 3, true);
    TEST_ASSERT_TRUE(d.full);
    TEST_ASSERT_EQUAL(int(Reason::NoRecord), int(d.reason));
    TEST_ASSERT_EQUAL_STRING("unknown_screen", reasonName(d.reason));
    TEST_ASSERT_EQUAL_STRING("no_record", reasonDetail(d.reason));
}

void test_decide_partial_when_everything_matches() {
    auto d = decide(rec(1, 2, 3), 1, 2, 3, true);
    TEST_ASSERT_FALSE(d.full);
    TEST_ASSERT_EQUAL(int(Reason::None), int(d.reason));
}

void test_decide_screen_or_content_changed() {
    TEST_ASSERT_EQUAL(int(Reason::ScreenChanged), int(decide(rec(1, 2, 3), 9, 2, 3, true).reason));
    TEST_ASSERT_EQUAL(int(Reason::ScreenChanged), int(decide(rec(1, 2, 3), 1, 9, 3, true).reason));
    TEST_ASSERT_EQUAL_STRING("unknown_screen", reasonName(Reason::ScreenChanged));
    TEST_ASSERT_EQUAL_STRING("screen_changed", reasonDetail(Reason::ScreenChanged));
    TEST_ASSERT_EQUAL_STRING("none", reasonName(Reason::None));
}

void test_decide_panel_mismatch() {
    auto d = decide(rec(1, 2, 3), 1, 2, 99, true);
    TEST_ASSERT_TRUE(d.full);
    TEST_ASSERT_EQUAL(int(Reason::PanelMismatch), int(d.reason));
    TEST_ASSERT_EQUAL_STRING("unknown_screen", reasonName(d.reason));
    TEST_ASSERT_EQUAL_STRING("panel_mismatch", reasonDetail(d.reason));
}

void test_decide_ignores_signature_when_memory_is_cold() {
    auto d = decide(rec(1, 2, 3), 1, 2, 99, false);
    TEST_ASSERT_FALSE(d.full);
    // the record still decides
    TEST_ASSERT_TRUE(decide(rec(1, 2, 3), 1, 7, 99, false).full);
    TEST_ASSERT_TRUE(decide(Record{}, 1, 2, 3, false).full);
}

void test_tracker_ignores_empty_and_keeps_disjoint() {
    DirtyTracker t;
    t.mark({0, 0, 0, 5});
    TEST_ASSERT_EQUAL(0, (int)t.size());
    t.mark({0, 0, 10, 10});
    t.mark({100, 100, 10, 10});
    TEST_ASSERT_EQUAL(2, (int)t.size());
    t.clear();
    TEST_ASSERT_EQUAL(0, (int)t.size());
}

void test_tracker_merges_overlap_and_chains() {
    DirtyTracker t;
    t.mark({0, 0, 10, 10});
    t.mark({50, 0, 10, 10});
    t.mark({5, 0, 50, 10});  // bridges both
    TEST_ASSERT_EQUAL(1, (int)t.size());
    TEST_ASSERT_EQUAL(0, t.at(0).x);
    TEST_ASSERT_EQUAL(60, t.at(0).w);
}

void test_tracker_overflow_never_loses_a_region() {
    DirtyTracker t;
    for (int i = 0; i < 20; ++i) t.mark({i * 100, 0, 10, 10});
    TEST_ASSERT_EQUAL((int)DirtyTracker::kCapacity, (int)t.size());
    bool covered = false;
    for (size_t i = 0; i < t.size(); ++i) {
        Rect r = t.at(i);
        if (r.x <= 1900 && r.x + r.w >= 1910) covered = true;
    }
    TEST_ASSERT_TRUE(covered);
}

// ---- added by tester (task 014 review) --------------------------------------------------------

// Exhaustive truth table: record valid/missing x id match x hash match x sig match x sig usable.
void test_decide_full_truth_table() {
    for (int mask = 0; mask < 32; ++mask) {
        const bool valid = mask & 1, idOk = mask & 2, hashOk = mask & 4, sigOk = mask & 8,
                   usable = mask & 16;
        Record st = valid ? rec(10, 20, 30) : Record{};
        if (!valid) {  // a missing record must never be rescued by matching fields
            st.screen_id = 10; st.content_hash = 20; st.panel_sig = 30;
        }
        Decision d = decide(st, idOk ? 10 : 11, hashOk ? 20 : 21, sigOk ? 30 : 31, usable);
        bool expectFull;
        Reason expectReason;
        if (!valid) { expectFull = true; expectReason = Reason::NoRecord; }
        else if (!idOk || !hashOk) { expectFull = true; expectReason = Reason::ScreenChanged; }
        else if (usable && !sigOk) { expectFull = true; expectReason = Reason::PanelMismatch; }
        else { expectFull = false; expectReason = Reason::None; }
        char msg[48];
        snprintf(msg, sizeof msg, "mask=%d", mask);
        TEST_ASSERT_EQUAL_MESSAGE(expectFull, d.full, msg);
        TEST_ASSERT_EQUAL_MESSAGE((int)expectReason, (int)d.reason, msg);
        // full <=> reason != None
        TEST_ASSERT_EQUAL_MESSAGE(d.full, d.reason != Reason::None, msg);
    }
}

void test_decide_zero_valued_record_is_not_missing() {
    // all-zero fields with valid=true is a real record, not "no record"
    Record z;
    z.valid = true;
    TEST_ASSERT_FALSE(decide(z, 0, 0, 0, true).full);
    // default Record (valid=false) with matching zeros is still a full draw
    TEST_ASSERT_TRUE(decide(Record{}, 0, 0, 0, true).full);
}

void test_tracker_touching_edges_merge_but_gap_does_not() {
    DirtyTracker t;
    t.mark({0, 0, 10, 10});
    t.mark({10, 0, 10, 10});  // shares an edge
    TEST_ASSERT_EQUAL(1, (int)t.size());
    TEST_ASSERT_EQUAL(20, t.at(0).w);
    t.mark({21, 0, 5, 10});  // 1 px gap
    TEST_ASSERT_EQUAL(2, (int)t.size());
}

void test_tracker_contained_and_duplicate() {
    DirtyTracker t;
    t.mark({0, 0, 100, 100});
    t.mark({10, 10, 5, 5});
    t.mark({0, 0, 100, 100});
    TEST_ASSERT_EQUAL(1, (int)t.size());
    TEST_ASSERT_EQUAL(100, t.at(0).w);
    TEST_ASSERT_EQUAL(100, t.at(0).h);
}

void test_tracker_negative_size_ignored() {
    DirtyTracker t;
    t.mark({0, 0, -5, 10});
    t.mark({0, 0, 5, -10});
    TEST_ASSERT_EQUAL(0, (int)t.size());
}

void test_tracker_big_rect_swallows_all() {
    DirtyTracker t;
    for (int i = 0; i < 8; ++i) t.mark({i * 100, 0, 10, 10});
    TEST_ASSERT_EQUAL(8, (int)t.size());
    t.mark({0, 0, 1000, 10});
    TEST_ASSERT_EQUAL(1, (int)t.size());
    TEST_ASSERT_EQUAL(1000, t.at(0).w);
}

// Property: after any sequence of marks (including overflow) every marked pixel is covered.
void test_tracker_coverage_property_with_overflow() {
    DirtyTracker t;
    Rect marks[40];
    uint32_t seed = 12345;
    auto rnd = [&](int m) { seed = seed * 1664525u + 1013904223u; return (int)((seed >> 8) % m); };
    for (int i = 0; i < 40; ++i) {
        marks[i] = {rnd(900), rnd(500), 1 + rnd(30), 1 + rnd(30)};
        t.mark(marks[i]);
        TEST_ASSERT_TRUE(t.size() <= DirtyTracker::kCapacity);
        for (int j = 0; j <= i; ++j) {
            bool covered = false;
            for (size_t k = 0; k < t.size(); ++k) {
                Rect r = t.at(k);
                if (r.x <= marks[j].x && r.y <= marks[j].y && r.x + r.w >= marks[j].x + marks[j].w &&
                    r.y + r.h >= marks[j].y + marks[j].h)
                    covered = true;
            }
            TEST_ASSERT_TRUE_MESSAGE(covered, "region lost");
        }
    }
}

// Entries stay pairwise disjoint when there is no overflow.
void test_tracker_entries_disjoint_without_overflow() {
    DirtyTracker t;
    t.mark({0, 0, 10, 10});
    t.mark({100, 0, 10, 10});
    t.mark({200, 0, 10, 10});
    t.mark({5, 5, 100, 2});  // bridges first two
    TEST_ASSERT_EQUAL(2, (int)t.size());
    for (size_t i = 0; i < t.size(); ++i)
        for (size_t j = i + 1; j < t.size(); ++j) {
            Rect a = t.at(i), b = t.at(j);
            bool overlap = a.x <= b.x + b.w && b.x <= a.x + a.w && a.y <= b.y + b.h && b.y <= a.y + a.h;
            TEST_ASSERT_FALSE(overlap);
        }
}

void test_tracker_clear_then_reuse() {
    DirtyTracker t;
    for (int i = 0; i < 12; ++i) t.mark({i * 100, 0, 10, 10});
    t.clear();
    t.mark({1, 1, 2, 2});
    TEST_ASSERT_EQUAL(1, (int)t.size());
    TEST_ASSERT_EQUAL(2, t.at(0).w);
}

void test_fnv_uint32_is_little_endian_bytes() {
    const uint8_t b[4] = {0x78, 0x56, 0x34, 0x12};
    TEST_ASSERT_EQUAL_UINT32(fnv1a(b, 4), fnv1a(uint32_t{0x12345678}));
    TEST_ASSERT_EQUAL_UINT32(fnv1a("ab"), fnv1a("b", fnv1a("a")));
}

static CrashState st(uint32_t magic, uint32_t count) {
    CrashState s;
    s.magic = magic;
    s.count = count;
    return s;
}

void test_crash_guard_normal_reset_clears() {
    auto d = crashGuard(st(kCrashMagic, 3), false);
    TEST_ASSERT_EQUAL_UINT32(0, d.next.count);
    TEST_ASSERT_EQUAL_UINT32(kCrashMagic, d.next.magic);
    TEST_ASSERT_TRUE(d.mode == BootMode::Normal);
}

void test_crash_guard_garbage_rtc_counts_as_first() {
    auto d = crashGuard(st(0x12345678u, 99), true);
    TEST_ASSERT_EQUAL_UINT32(1, d.next.count);
    TEST_ASSERT_TRUE(d.mode == BootMode::Normal);
}

void test_crash_guard_second_crash_falls_back() {
    auto d = crashGuard(crashGuard(st(0, 0), true).next, true);
    TEST_ASSERT_EQUAL_UINT32(2, d.next.count);
    TEST_ASSERT_TRUE(d.mode == BootMode::StockFallback);
}

void test_crash_guard_never_loops_forever() {
    CrashState s;
    BootMode m = BootMode::Normal;
    for (int i = 0; i < 100; ++i) {
        auto d = crashGuard(s, true);
        s = d.next;
        m = d.mode;
    }
    TEST_ASSERT_TRUE(m == BootMode::GiveUp);
    TEST_ASSERT_EQUAL_UINT32(kGiveUpAfter, s.count);  // saturated
}

void test_crash_guard_sequence_modes() {
    CrashState s;
    const BootMode want[] = {BootMode::Normal, BootMode::StockFallback, BootMode::StockFallback,
                             BootMode::GiveUp};
    for (BootMode w : want) {
        auto d = crashGuard(s, true);
        s = d.next;
        TEST_ASSERT_TRUE(d.mode == w);
    }
}

// boot.cpp's retry after the give-up sleep: state {magic, kFallbackAfter-1} plus one crash.
void test_crash_guard_retry_after_give_up_starts_with_fallback() {
    auto d = crashGuard(st(kCrashMagic, kFallbackAfter - 1), true);
    TEST_ASSERT_TRUE(d.mode == BootMode::StockFallback);
    // A second crash in the fallback leads to give-up again, not to a loop.
    TEST_ASSERT_TRUE(crashGuard(crashGuard(d.next, true).next, true).mode == BootMode::GiveUp);
    // A clean start-up clears the counter.
    TEST_ASSERT_EQUAL_UINT32(0, crashGuard(d.next, false).next.count);
}

// ---- crash_guard gaps (task 016 verification) ----
void test_crash_guard_third_crash_still_fallback() {
    auto d = crashGuard(st(kCrashMagic, 2), true);
    TEST_ASSERT_EQUAL_UINT32(3, d.next.count);
    TEST_ASSERT_TRUE(d.mode == BootMode::StockFallback);
}

void test_crash_guard_fourth_crash_gives_up_and_saturates() {
    auto d = crashGuard(st(kCrashMagic, kGiveUpAfter - 1), true);
    TEST_ASSERT_TRUE(d.mode == BootMode::GiveUp);
    d = crashGuard(st(kCrashMagic, kGiveUpAfter), true);
    TEST_ASSERT_TRUE(d.mode == BootMode::GiveUp);
    TEST_ASSERT_EQUAL_UINT32(kGiveUpAfter, d.next.count);
}

void test_crash_guard_huge_valid_count_does_not_overflow() {
    auto d = crashGuard(st(kCrashMagic, 0xFFFFFFFFu), true);
    TEST_ASSERT_TRUE(d.mode == BootMode::GiveUp);
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFFu, d.next.count);  // not incremented; documents behaviour
}

void test_crash_guard_garbage_non_crash_resets_and_arms_magic() {
    auto d = crashGuard(st(0xDEADBEEFu, 77), false);
    TEST_ASSERT_EQUAL_UINT32(0, d.next.count);
    TEST_ASSERT_EQUAL_UINT32(kCrashMagic, d.next.magic);
    TEST_ASSERT_TRUE(d.mode == BootMode::Normal);
}

void test_crash_guard_zeroed_rtc_counts_first_crash_as_one() {
    auto d = crashGuard(CrashState(), true);
    TEST_ASSERT_EQUAL_UINT32(1, d.next.count);
    TEST_ASSERT_TRUE(d.mode == BootMode::Normal);
}

void test_crash_guard_clean_boot_between_crashes_restarts_sequence() {
    CrashState s = crashGuard(CrashState(), true).next;     // 1
    s = crashGuard(s, false).next;                          // setup finished
    auto d = crashGuard(s, true);                           // 1 again, not 2
    TEST_ASSERT_EQUAL_UINT32(1, d.next.count);
    TEST_ASSERT_TRUE(d.mode == BootMode::Normal);
}

void test_crash_guard_thresholds_sane() {
    TEST_ASSERT_TRUE(kFallbackAfter >= 1);
    TEST_ASSERT_TRUE(kGiveUpAfter > kFallbackAfter);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_fnv_known_values);
    RUN_TEST(test_fnv_chaining_differs);
    RUN_TEST(test_decide_no_record);
    RUN_TEST(test_decide_partial_when_everything_matches);
    RUN_TEST(test_decide_screen_or_content_changed);
    RUN_TEST(test_decide_panel_mismatch);
    RUN_TEST(test_decide_ignores_signature_when_memory_is_cold);
    RUN_TEST(test_tracker_ignores_empty_and_keeps_disjoint);
    RUN_TEST(test_tracker_merges_overlap_and_chains);
    RUN_TEST(test_tracker_overflow_never_loses_a_region);
    RUN_TEST(test_decide_full_truth_table);
    RUN_TEST(test_decide_zero_valued_record_is_not_missing);
    RUN_TEST(test_tracker_touching_edges_merge_but_gap_does_not);
    RUN_TEST(test_tracker_contained_and_duplicate);
    RUN_TEST(test_tracker_negative_size_ignored);
    RUN_TEST(test_tracker_big_rect_swallows_all);
    RUN_TEST(test_tracker_coverage_property_with_overflow);
    RUN_TEST(test_tracker_entries_disjoint_without_overflow);
    RUN_TEST(test_tracker_clear_then_reuse);
    RUN_TEST(test_fnv_uint32_is_little_endian_bytes);
    RUN_TEST(test_crash_guard_normal_reset_clears);
    RUN_TEST(test_crash_guard_garbage_rtc_counts_as_first);
    RUN_TEST(test_crash_guard_second_crash_falls_back);
    RUN_TEST(test_crash_guard_never_loops_forever);
    RUN_TEST(test_crash_guard_sequence_modes);
    RUN_TEST(test_crash_guard_retry_after_give_up_starts_with_fallback);
    RUN_TEST(test_crash_guard_third_crash_still_fallback);
    RUN_TEST(test_crash_guard_fourth_crash_gives_up_and_saturates);
    RUN_TEST(test_crash_guard_huge_valid_count_does_not_overflow);
    RUN_TEST(test_crash_guard_garbage_non_crash_resets_and_arms_magic);
    RUN_TEST(test_crash_guard_zeroed_rtc_counts_first_crash_as_one);
    RUN_TEST(test_crash_guard_clean_boot_between_crashes_restarts_sequence);
    RUN_TEST(test_crash_guard_thresholds_sane);
    return UNITY_END();
}
