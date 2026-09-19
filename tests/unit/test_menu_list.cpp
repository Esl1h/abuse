#include <doctest/doctest.h>

#include <initializer_list>

#include "common.h"
#include "image.h"
#include "ui/menu_list.h"
#include "ui/overlay.h"

using namespace abuse::ui;

// menu_list draws through the overlay, which asks the video layer how big the
// window is. The unit suite has no window, so it answers for it: every test
// here is about where things land, not about what is on screen.
static int g_test_w = 0;
static int g_test_h = 0;

bool window_pixel_size(int &w, int &h)
{
    if (g_test_w < 1 || g_test_h < 1)
        return false;
    w = g_test_w;
    h = g_test_h;
    return true;
}

TEST_CASE("the body is the widest label plus the widest value") {
    Row rows[2];
    rows[0].label = "ab";        // 2
    rows[0].value = "1234";      // 4
    rows[1].label = "abcdef";    // 6
    rows[1].value = "1";

    // 6 + 3 of air + 4
    CHECK(list_body_cells(rows, 2, nullptr, 0) == 13);
}

TEST_CASE("a marked row counts the asterisk it will draw") {
    Row plain, marked;
    plain.label = "abc";
    plain.value = "x";
    marked = plain;
    marked.marked = true;

    CHECK(list_body_cells(&marked, 1, nullptr, 0)
          == list_body_cells(&plain, 1, nullptr, 0) + 2);
}

// A footer is one long line with no value column, so it can be wider than
// every row and still has to fit.
TEST_CASE("a footer wider than the rows sets the width") {
    Row row;
    row.label = "a";
    row.value = "b";

    char const *footers[1] = { "a very long line of help text indeed" };
    CHECK(list_body_cells(&row, 1, footers, 1) == 36);
}

TEST_CASE("an empty list has a width of the air alone") {
    CHECK(list_body_cells(nullptr, 0, nullptr, 0) == 3);
}

TEST_CASE("a null label or value counts as nothing") {
    Row row;
    CHECK(list_body_cells(&row, 1, nullptr, 0) == 3);
}

TEST_CASE("the panel is centred in the window") {
    ListLayout l = list_layout(1280, 720, 20, 8, 2);
    CHECK(l.x == (1280 - l.width) / 2);
    CHECK(l.y == (720 - l.height) / 2);
    CHECK(l.width > 0);
    CHECK(l.height > 0);
}

// The reason the layout is computed and not fixed: the same screen has to fit
// a 640x480 window and a 4K one.
TEST_CASE("the text shrinks until the panel fits") {
    ListLayout big = list_layout(1920, 1080, 40, 11, 2);
    ListLayout small = list_layout(640, 480, 40, 11, 2);

    CHECK(big.scale > small.scale);
    CHECK(small.width <= 640);
    CHECK(small.height <= 480);
}

TEST_CASE("a window too narrow for any scale still gets the smallest") {
    ListLayout l = list_layout(160, 120, 60, 11, 2);
    CHECK(l.scale == 1);
    CHECK(l.width <= 160);
    CHECK(l.height <= 120);
}

TEST_CASE("rows are spaced by one row height, under the title") {
    ListLayout l = list_layout(1280, 720, 20, 5, 1);
    CHECK(l.first_row_y > l.y);
    CHECK(l.row > l.cell);
    // Five rows and half a gap below them.
    CHECK(l.footer_y == l.first_row_y + l.row * 5 + l.row / 2);
}

TEST_CASE("everything the panel draws stays inside the window") {
    for (int w : { 640, 800, 1024, 1280, 1920, 2560 })
        for (int h : { 360, 480, 600, 720, 1080, 1600 })
        {
            ListLayout l = list_layout(w, h, 44, 11, 2);
            REQUIRE(l.x >= 0);
            REQUIRE(l.y >= 0);
            REQUIRE(l.x + l.width <= w);
            REQUIRE(l.y + l.height <= h);
            // The last footer line has to land inside the panel too.
            REQUIRE(l.footer_y + l.row <= l.y + l.height);
        }
}

TEST_CASE("the selection wraps at both ends") {
    CHECK(list_wrap(0, -1, 5) == 4);
    CHECK(list_wrap(4, 1, 5) == 0);
    CHECK(list_wrap(2, 1, 5) == 3);
    CHECK(list_wrap(2, -1, 5) == 1);
}

TEST_CASE("wrapping an empty list stays at zero") {
    CHECK(list_wrap(0, 1, 0) == 0);
    CHECK(list_wrap(3, -1, -2) == 0);
}

// The screens draw before anything has sized the overlay, and before a window
// exists at all under the test harness.
TEST_CASE("drawing without a window is a no-op, not a crash") {
    g_test_w = 0;
    g_test_h = 0;

    Row row;
    row.label = "a";
    row.value = "b";
    draw_list("title", &row, 1, 0, nullptr, 0);

    CHECK_FALSE(overlay().Dirty());
}
