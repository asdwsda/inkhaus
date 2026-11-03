#include <unity.h>

#include "build_info_line.h"
#include "settings_ui.h"

using namespace settings_ui;

void setUp() {}
void tearDown() {}

static BuildInfo sample() {
    BuildInfo b;
    b.version = "1.3.1";
    b.variant = "debug";
    b.commit = "7727f0d";
    b.date = "2026-11-03";
    return b;
}

// 10 px per UTF-8 character (a lead byte counts, continuation bytes do not).
static int measure(const std::string& s) {
    int n = 0;
    for (unsigned char c : s)
        if ((c & 0xC0) != 0x80) ++n;
    return n * 10;
}

void test_line_full_dirty_and_missing() {
    BuildInfo b = sample();
    TEST_ASSERT_EQUAL_STRING("1.3.1 debug | 7727f0d | 2026-11-03", buildInfoLine(b).c_str());
    b.dirty = true;
    TEST_ASSERT_EQUAL_STRING("1.3.1 debug | 7727f0d+ | 2026-11-03", buildInfoLine(b).c_str());
    b.commit = "";
    TEST_ASSERT_EQUAL_STRING("1.3.1 debug | 2026-11-03", buildInfoLine(b).c_str());
}

void test_fit_drops_date_then_commit_then_ellipsis() {
    const BuildInfo b = sample();
    const std::string full = buildInfoLine(b);
    TEST_ASSERT_EQUAL_STRING(full.c_str(), buildInfoFit(b, 1000, measure).c_str());
    TEST_ASSERT_EQUAL_STRING(buildInfoLine(b, true, false).c_str(), buildInfoFit(b, 220, measure).c_str());
    TEST_ASSERT_EQUAL_STRING("1.3.1 debug", buildInfoFit(b, 120, measure).c_str());
    const std::string cut = buildInfoFit(b, 60, measure);
    TEST_ASSERT_TRUE(measure(cut) <= 60);
    TEST_ASSERT_TRUE(cut.size() > 3 && cut.substr(cut.size() - 3) == "\xE2\x80\xA6");
    TEST_ASSERT_EQUAL_STRING("\xE2\x80\xA6", buildInfoFit(b, 10, measure).c_str());
}

void test_tools_about_row_layout() {
    PageLayout p = computePageLayout(Page::Tools, settings::Orientation::Portrait);
    TEST_ASSERT_EQUAL_INT(368, p.about.y);
    TEST_ASSERT_EQUAL_INT(468, p.note.y);
    TEST_ASSERT_EQUAL_INT(4, p.stopCount);  // about is not a focus stop
    PageLayout l = computePageLayout(Page::Tools, settings::Orientation::Landscape);
    TEST_ASSERT_EQUAL_INT(264, l.about.y);
    TEST_ASSERT_TRUE(l.about.y >= l.rows[2].y + l.rows[2].h + 12);
    TEST_ASSERT_EQUAL_INT(0, computePageLayout(Page::Network, settings::Orientation::Portrait).about.w);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_line_full_dirty_and_missing);
    RUN_TEST(test_fit_drops_date_then_commit_then_ellipsis);
    RUN_TEST(test_tools_about_row_layout);
    return UNITY_END();
}
