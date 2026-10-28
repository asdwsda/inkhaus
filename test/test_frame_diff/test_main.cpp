#include <unity.h>

#include <algorithm>
#include <cstring>
#include <vector>

#include "frame_diff.h"

using namespace screen_track;

void setUp() {}
void tearDown() {}

namespace {
constexpr int W = 540, H = 960;

std::vector<uint8_t> blank(int w = W, int h = H) { return std::vector<uint8_t>(size_t(w) * h, 255); }

void fill(std::vector<uint8_t>& f, int w, Rect r, uint8_t v) {
    for (int y = r.y; y < r.y + r.h; ++y)
        for (int x = r.x; x < r.x + r.w; ++x) f[size_t(y) * w + x] = v;
}

bool covers(const std::vector<Rect>& rs, int x, int y) {
    for (const Rect& r : rs)
        if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) return true;
    return false;
}

// Every pixel that differs must lie in a result rect.
void assertCovers(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b, int w, int h,
                  const std::vector<Rect>& rs) {
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            if (a[size_t(y) * w + x] != b[size_t(y) * w + x]) {
                if (!covers(rs, x, y)) TEST_FAIL_MESSAGE("changed pixel outside every rect");
            }
}
}  // namespace

void test_equal_frames_give_no_rects() {
    auto a = blank(), b = blank();
    TEST_ASSERT_EQUAL(0, (int)diffFrames(a.data(), b.data(), W, H, W).size());
}

void test_single_pixel_is_aligned_to_four() {
    auto a = blank(), b = blank();
    b[size_t(100) * W + 51] = 0;
    auto r = diffFrames(a.data(), b.data(), W, H, W);
    TEST_ASSERT_EQUAL(1, (int)r.size());
    TEST_ASSERT_EQUAL(48, r[0].x);
    TEST_ASSERT_EQUAL(4, r[0].w);
    TEST_ASSERT_EQUAL(100, r[0].y);
    TEST_ASSERT_EQUAL(1, r[0].h);
}

void test_one_widget_rect_is_exact() {
    auto a = blank(), b = blank();
    fill(b, W, {276, 300, 248, 104}, 0);
    auto r = diffFrames(a.data(), b.data(), W, H, W);
    TEST_ASSERT_EQUAL(1, (int)r.size());
    TEST_ASSERT_EQUAL(276, r[0].x);
    TEST_ASSERT_EQUAL(300, r[0].y);
    TEST_ASSERT_EQUAL(248, r[0].w);
    TEST_ASSERT_EQUAL(104, r[0].h);
}

void test_widgets_12px_apart_stay_separate() {
    auto a = blank(), b = blank();
    fill(b, W, {16, 300, 248, 104}, 0);
    fill(b, W, {276, 300, 248, 104}, 0);  // 12 px gap
    auto r = diffFrames(a.data(), b.data(), W, H, W);
    TEST_ASSERT_EQUAL(2, (int)r.size());
    assertCovers(a, b, W, H, r);
}

void test_side_by_side_with_different_heights_are_not_one_tall_rect() {
    auto a = blank(), b = blank();
    fill(b, W, {16, 300, 248, 104}, 0);
    fill(b, W, {276, 300, 248, 40}, 0);
    auto r = diffFrames(a.data(), b.data(), W, H, W);
    TEST_ASSERT_EQUAL(2, (int)r.size());
    for (const Rect& x : r)
        if (x.x >= 276) TEST_ASSERT_EQUAL(40, x.h);
    assertCovers(a, b, W, H, r);
}

void test_close_changes_merge() {
    auto a = blank(), b = blank();
    fill(b, W, {20, 100, 30, 10}, 0);
    fill(b, W, {56, 100, 30, 10}, 0);  // 6 px gap < merge gap
    auto r = diffFrames(a.data(), b.data(), W, H, W);
    TEST_ASSERT_EQUAL(1, (int)r.size());
    assertCovers(a, b, W, H, r);
}

void test_max_rects_is_respected_and_coverage_kept() {
    auto a = blank(), b = blank();
    for (int i = 0; i < 12; ++i) fill(b, W, {20 + (i % 3) * 150, 20 + i * 70, 40, 20}, 0);
    DiffOptions o;
    o.maxRects = 4;
    auto r = diffFrames(a.data(), b.data(), W, H, W, o);
    TEST_ASSERT_TRUE(r.size() <= 4);
    assertCovers(a, b, W, H, r);
}

void test_landscape_geometry_and_edges() {
    const int w = 960, h = 540;
    auto a = blank(w, h), b = blank(w, h);
    b[size_t(h - 1) * w + (w - 1)] = 0;
    b[0] = 0;
    auto r = diffFrames(a.data(), b.data(), w, h, w);
    TEST_ASSERT_EQUAL(2, (int)r.size());
    assertCovers(a, b, w, h, r);
    for (const Rect& x : r) {
        TEST_ASSERT_TRUE(x.x >= 0 && x.x + x.w <= w);
        TEST_ASSERT_TRUE(x.y >= 0 && x.y + x.h <= h);
    }
}

void test_stride_larger_than_width() {
    const int w = 20, h = 10, stride = 32;
    std::vector<uint8_t> a(size_t(stride) * h, 7), b = a;
    b[size_t(5) * stride + 3] = 9;
    b[size_t(5) * stride + 25] = 9;  // padding: must be ignored
    auto r = diffFrames(a.data(), b.data(), w, h, stride, [] {
        DiffOptions o;
        o.alignX = 1;
        return o;
    }());
    TEST_ASSERT_EQUAL(1, (int)r.size());
    TEST_ASSERT_EQUAL(3, r[0].x);
    TEST_ASSERT_EQUAL(1, r[0].w);
}

void test_merge_rects_basics() {
    std::vector<Rect> v = {{0, 0, 10, 10}, {12, 0, 10, 10}, {100, 100, 5, 5}, {0, 0, 0, 5}};
    mergeRects(v, 4, 8);
    TEST_ASSERT_EQUAL(2, (int)v.size());  // first two merge (gap 2), empty dropped
    mergeRects(v, 0, 1);
    TEST_ASSERT_EQUAL(1, (int)v.size());
    TEST_ASSERT_EQUAL(0, v[0].x);
    TEST_ASSERT_EQUAL(105, v[0].x + v[0].w);
}

void test_align_rect_clamps() {
    Rect r = alignRect({537, 5, 3, 2}, 4, 540);
    TEST_ASSERT_EQUAL(536, r.x);
    TEST_ASSERT_EQUAL(4, r.w);
    Rect n = alignRect({5, 5, 3, 2}, 1, 540);
    TEST_ASSERT_EQUAL(5, n.x);
    TEST_ASSERT_EQUAL(3, n.w);
}

void test_tile_grid_and_hash_stability() {
    auto a = blank();
    TileGrid g = tileGridFor(W, H);
    TEST_ASSERT_EQUAL(17, g.cols);
    TEST_ASSERT_EQUAL(30, g.rows);
    auto h1 = tileHashes(a.data(), W, H, W);
    auto h2 = tileHashes(a.data(), W, H, W);
    TEST_ASSERT_EQUAL(int(g.count()), (int)h1.size());
    TEST_ASSERT_TRUE(h1 == h2);
    TEST_ASSERT_EQUAL_UINT32(hashOfTiles(h1), hashOfTiles(h2));
}

void test_tile_diff_finds_changed_tile_only() {
    auto a = blank(), b = blank();
    b[size_t(70) * W + 40] = 0;  // tile col 1, row 2
    auto ha = tileHashes(a.data(), W, H, W), hb = tileHashes(b.data(), W, H, W);
    TEST_ASSERT_TRUE(hashOfTiles(ha) != hashOfTiles(hb));
    auto r = diffTiles(ha, hb, W, H);
    TEST_ASSERT_EQUAL(1, (int)r.size());
    TEST_ASSERT_EQUAL(32, r[0].x);
    TEST_ASSERT_EQUAL(64, r[0].y);
    TEST_ASSERT_EQUAL(32, r[0].w);
    TEST_ASSERT_EQUAL(32, r[0].h);
    TEST_ASSERT_EQUAL(0, (int)diffTiles(ha, ha, W, H).size());
}

void test_tile_diff_partial_edge_tiles_are_clamped() {
    auto a = blank(), b = blank();
    b[size_t(H - 1) * W + (W - 1)] = 0;  // last (partial) tile: 28 px wide, 32 high
    auto r = diffTiles(tileHashes(a.data(), W, H, W), tileHashes(b.data(), W, H, W), W, H);
    TEST_ASSERT_EQUAL(1, (int)r.size());
    TEST_ASSERT_TRUE(r[0].x + r[0].w <= W);
    TEST_ASSERT_TRUE(r[0].y + r[0].h <= H);
    TEST_ASSERT_TRUE(covers(r, W - 1, H - 1));
}

void test_tile_diff_geometry_mismatch_means_everything() {
    auto a = blank(), b = blank(960, 540);
    auto r = diffTiles(tileHashes(a.data(), W, H, W), tileHashes(b.data(), 960, 540, 960), W, H);
    TEST_ASSERT_EQUAL(1, (int)r.size());
    TEST_ASSERT_EQUAL(W, r[0].w);
    TEST_ASSERT_EQUAL(H, r[0].h);
    auto r2 = diffTiles({}, {}, W, H);
    TEST_ASSERT_EQUAL(1, (int)r2.size());
}

void test_tile_diff_agrees_with_pixel_diff_coverage() {
    auto a = blank(), b = blank();
    fill(b, W, {276, 300, 248, 104}, 0);
    fill(b, W, {16, 68, 508, 104}, 40);
    auto rt = diffTiles(tileHashes(a.data(), W, H, W), tileHashes(b.data(), W, H, W), W, H);
    assertCovers(a, b, W, H, rt);
}

void test_rle_roundtrip_and_overflow() {
    std::vector<uint8_t> in(1000, 255);
    in[10] = 0;
    in[500] = 0x44;
    std::vector<uint8_t> out(4000);
    size_t n = rleEncode(in.data(), in.size(), out.data(), out.size());
    TEST_ASSERT_TRUE(n > 0 && n % 2 == 0);
    std::vector<uint8_t> back;
    for (size_t i = 0; i < n; i += 2) back.insert(back.end(), size_t(out[i]) + 1, out[i + 1]);
    TEST_ASSERT_TRUE(back == in);
    TEST_ASSERT_EQUAL(0, (int)rleEncode(in.data(), in.size(), out.data(), 4));  // too small: 0
}


// ---- tester additions (task 017) ----

static uint32_t rngState = 12345;
static uint32_t rnd() { rngState = rngState * 1664525u + 1013904223u; return rngState >> 8; }

void test_t_random_changes_are_covered_in_bounds_and_aligned() {
    for (int iter = 0; iter < 40; ++iter) {
        const int w = iter % 2 ? 540 : 960, h = iter % 2 ? 960 : 540;
        auto a = blank(w, h);
        auto b = a;
        const int n = 1 + int(rnd() % 12);
        for (int i = 0; i < n; ++i) {
            Rect r{int(rnd() % w), int(rnd() % h), 1 + int(rnd() % 60), 1 + int(rnd() % 40)};
            r.w = std::min(r.w, w - r.x);
            r.h = std::min(r.h, h - r.y);
            fill(b, w, r, uint8_t(rnd() % 255));
        }
        DiffOptions opts;
        auto rs = diffFrames(a.data(), b.data(), w, h, w, opts);
        TEST_ASSERT_TRUE(rs.size() <= opts.maxRects);
        for (const Rect& r : rs) {
            TEST_ASSERT_TRUE(r.x >= 0 && r.y >= 0 && r.w > 0 && r.h > 0);
            TEST_ASSERT_TRUE(r.x + r.w <= w && r.y + r.h <= h);
            TEST_ASSERT_EQUAL(0, r.x % 4);
        }
        assertCovers(a, b, w, h, rs);
    }
}

void test_t_full_frame_change_gives_whole_screen() {
    auto a = blank();
    auto b = std::vector<uint8_t>(a.size(), 0);
    auto rs = diffFrames(a.data(), b.data(), W, H, W);
    TEST_ASSERT_EQUAL(1, (int)rs.size());
    TEST_ASSERT_EQUAL(0, rs[0].x);
    TEST_ASSERT_EQUAL(0, rs[0].y);
    TEST_ASSERT_EQUAL(W, rs[0].w);
    TEST_ASSERT_EQUAL(H, rs[0].h);
}

void test_t_null_or_empty_input_is_safe() {
    auto a = blank();
    TEST_ASSERT_TRUE(diffFrames(nullptr, a.data(), W, H, W).empty());
    TEST_ASSERT_TRUE(diffFrames(a.data(), nullptr, W, H, W).empty());
    TEST_ASSERT_TRUE(diffFrames(a.data(), a.data(), 0, H, W).empty());
    TEST_ASSERT_TRUE(diffFrames(a.data(), a.data(), W, 0, W).empty());
    std::vector<Rect> none;
    mergeRects(none, 8, 6);
    TEST_ASSERT_TRUE(none.empty());
}

void test_t_changes_at_last_pixel_and_odd_width() {
    const int w = 541, h = 3;  // width not a multiple of 4
    auto a = blank(w, h);
    auto b = a;
    b[size_t(h - 1) * w + (w - 1)] = 0;
    auto rs = diffFrames(a.data(), b.data(), w, h, w);
    TEST_ASSERT_EQUAL(1, (int)rs.size());
    TEST_ASSERT_TRUE(rs[0].x + rs[0].w == w);  // clamped, not past the edge
    assertCovers(a, b, w, h, rs);
}

void test_t_tile_hash_detects_single_pixel_and_diff_rect_contains_it() {
    auto a = blank();
    auto b = a;
    b[size_t(500) * W + 333] = 0;
    auto ra = diffTiles(tileHashes(a.data(), W, H, W), tileHashes(b.data(), W, H, W), W, H);
    TEST_ASSERT_EQUAL(1, (int)ra.size());
    TEST_ASSERT_TRUE(covers(ra, 333, 500));
    TEST_ASSERT_TRUE(diffTiles(tileHashes(a.data(), W, H, W), tileHashes(a.data(), W, H, W), W, H).empty());
}

void test_t_rle_empty_and_max_run() {
    uint8_t out[8];
    TEST_ASSERT_EQUAL(0, (int)rleEncode(nullptr, 0, out, sizeof(out)));
    std::vector<uint8_t> in(257, 7);  // run of 256 + 1
    std::vector<uint8_t> o(8);
    TEST_ASSERT_EQUAL(4, (int)rleEncode(in.data(), in.size(), o.data(), o.size()));
    TEST_ASSERT_EQUAL(255, o[0]);
    TEST_ASSERT_EQUAL(0, o[2]);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_equal_frames_give_no_rects);
    RUN_TEST(test_single_pixel_is_aligned_to_four);
    RUN_TEST(test_one_widget_rect_is_exact);
    RUN_TEST(test_widgets_12px_apart_stay_separate);
    RUN_TEST(test_side_by_side_with_different_heights_are_not_one_tall_rect);
    RUN_TEST(test_close_changes_merge);
    RUN_TEST(test_max_rects_is_respected_and_coverage_kept);
    RUN_TEST(test_landscape_geometry_and_edges);
    RUN_TEST(test_stride_larger_than_width);
    RUN_TEST(test_merge_rects_basics);
    RUN_TEST(test_align_rect_clamps);
    RUN_TEST(test_tile_grid_and_hash_stability);
    RUN_TEST(test_tile_diff_finds_changed_tile_only);
    RUN_TEST(test_tile_diff_partial_edge_tiles_are_clamped);
    RUN_TEST(test_tile_diff_geometry_mismatch_means_everything);
    RUN_TEST(test_tile_diff_agrees_with_pixel_diff_coverage);
    RUN_TEST(test_rle_roundtrip_and_overflow);
    RUN_TEST(test_t_random_changes_are_covered_in_bounds_and_aligned);
    RUN_TEST(test_t_full_frame_change_gives_whole_screen);
    RUN_TEST(test_t_null_or_empty_input_is_safe);
    RUN_TEST(test_t_changes_at_last_pixel_and_odd_width);
    RUN_TEST(test_t_tile_hash_detects_single_pixel_and_diff_rect_contains_it);
    RUN_TEST(test_t_rle_empty_and_max_run);
    return UNITY_END();
}
