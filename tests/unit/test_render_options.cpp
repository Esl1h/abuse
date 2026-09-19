#include <doctest/doctest.h>

#include <initializer_list>

#include <string.h>

#include "render/options.h"

using namespace abuse::render;

TEST_CASE("scale mode names parse and round trip") {
    ScaleMode m = ScaleMode::Fit;

    REQUIRE(parse_scale_mode("integer", m));
    CHECK(m == ScaleMode::Integer);
    CHECK(strcmp(scale_mode_name(m), "integer") == 0);

    REQUIRE(parse_scale_mode("stretch", m));
    CHECK(m == ScaleMode::Stretch);
    CHECK(strcmp(scale_mode_name(m), "stretch") == 0);

    REQUIRE(parse_scale_mode("fit", m));
    CHECK(m == ScaleMode::Fit);
    CHECK(strcmp(scale_mode_name(m), "fit") == 0);
}

TEST_CASE("filter names parse and round trip") {
    Filter f = Filter::PixelArt;

    REQUIRE(parse_filter("nearest", f));
    CHECK(f == Filter::Nearest);
    CHECK(strcmp(filter_name(f), "nearest") == 0);

    REQUIRE(parse_filter("linear", f));
    CHECK(f == Filter::Linear);
    CHECK(strcmp(filter_name(f), "linear") == 0);

    REQUIRE(parse_filter("pixelart", f));
    CHECK(f == Filter::PixelArt);
    CHECK(strcmp(filter_name(f), "pixelart") == 0);
}

TEST_CASE("parsing ignores case, as the rest of abuserc does") {
    ScaleMode m = ScaleMode::Fit;
    CHECK(parse_scale_mode("INTEGER", m));
    CHECK(m == ScaleMode::Integer);

    Filter f = Filter::PixelArt;
    CHECK(parse_filter("Linear", f));
    CHECK(f == Filter::Linear);
}

// A typo in abuserc must not silently pick something arbitrary: the value is
// left alone and the caller keeps its default.
TEST_CASE("unknown names leave the value untouched") {
    ScaleMode m = ScaleMode::Integer;
    CHECK_FALSE(parse_scale_mode("bilinear", m));
    CHECK(m == ScaleMode::Integer);
    CHECK_FALSE(parse_scale_mode("", m));
    CHECK(m == ScaleMode::Integer);
    CHECK_FALSE(parse_scale_mode(nullptr, m));
    CHECK(m == ScaleMode::Integer);

    Filter f = Filter::Linear;
    CHECK_FALSE(parse_filter("smooth", f));
    CHECK(f == Filter::Linear);
    CHECK_FALSE(parse_filter(nullptr, f));
    CHECK(f == Filter::Linear);
}

TEST_CASE("letterbox colour accepts rrggbb with or without hash") {
    uint8_t c[3] = {9, 9, 9};

    REQUIRE(parse_letterbox("ff8000", c));
    CHECK((int)c[0] == 255);
    CHECK((int)c[1] == 128);
    CHECK((int)c[2] == 0);

    REQUIRE(parse_letterbox("#003366", c));
    CHECK((int)c[0] == 0);
    CHECK((int)c[1] == 51);
    CHECK((int)c[2] == 102);

    REQUIRE(parse_letterbox("ABCDEF", c));
    CHECK((int)c[0] == 171);
    CHECK((int)c[1] == 205);
    CHECK((int)c[2] == 239);
}

TEST_CASE("malformed letterbox colour leaves the value untouched") {
    uint8_t c[3] = {1, 2, 3};

    CHECK_FALSE(parse_letterbox("ff80", c));       // curto demais
    CHECK_FALSE(parse_letterbox("ff800000", c));   // longo demais
    CHECK_FALSE(parse_letterbox("gggggg", c));     // fora do hexadecimal
    CHECK_FALSE(parse_letterbox("", c));
    CHECK_FALSE(parse_letterbox(nullptr, c));

    CHECK((int)c[0] == 1);
    CHECK((int)c[1] == 2);
    CHECK((int)c[2] == 3);
}

// The defaults have to reproduce what the upstream renderer did before the
// options existed, or every snapshot taken so far becomes wrong.
TEST_CASE("defaults match the previous hard-coded behaviour") {
    Options fresh;
    CHECK(fresh.scale == ScaleMode::Fit);       // era LETTERBOX fixo
    CHECK(fresh.filter == Filter::PixelArt);    // era SCALEMODE_PIXELART fixo
    CHECK(fresh.vsync == true);
    CHECK(fresh.fps_limit == 0);
    CHECK((int)fresh.letterbox[0] == 0);
    CHECK((int)fresh.letterbox[1] == 0);
    CHECK((int)fresh.letterbox[2] == 0);
}

TEST_CASE("presets set scale and filter together") {
    Preset p = Preset::Classic;

    REQUIRE(parse_preset("sharp", p));
    CHECK(p == Preset::Sharp);
    CHECK(strcmp(preset_name(p), "sharp") == 0);

    Options opt;
    apply_preset(Preset::Sharp, opt);
    CHECK(opt.scale == ScaleMode::Integer);
    CHECK(opt.filter == Filter::Nearest);

    apply_preset(Preset::Classic, opt);
    CHECK(opt.scale == ScaleMode::Fit);
    CHECK(opt.filter == Filter::PixelArt);
}

// classic has to reproduce the defaults exactly: the Original mode forces it,
// and the reference frames were recorded with it.
TEST_CASE("classic preset equals the defaults") {
    Options fresh;
    Options applied;
    apply_preset(Preset::Classic, applied);

    CHECK(applied.scale == fresh.scale);
    CHECK(applied.filter == fresh.filter);
}

TEST_CASE("presets leave the display settings alone") {
    Options opt;
    opt.vsync = false;
    opt.fps_limit = 144;
    opt.letterbox[0] = 12;

    apply_preset(Preset::Sharp, opt);

    CHECK(opt.vsync == false);
    CHECK(opt.fps_limit == 144);
    CHECK((int)opt.letterbox[0] == 12);
}

TEST_CASE("unknown preset name is rejected") {
    Preset p = Preset::Classic;
    CHECK_FALSE(parse_preset("crt", p));
    CHECK_FALSE(parse_preset(nullptr, p));
    CHECK(p == Preset::Classic);
}

// Phase 6, block 6.1. Three keys in abuserc are now on/off, and a typo in any
// of them has to leave the setting where it was rather than guess.
TEST_CASE("on/off parses the spellings people type") {
    bool v = false;

    for (char const *yes : { "on", "ON", "true", "yes", "1" }) {
        v = false;
        CHECK(abuse::render::parse_switch(yes, v));
        CHECK(v);
    }

    for (char const *no : { "off", "OFF", "false", "no", "0" }) {
        v = true;
        CHECK(abuse::render::parse_switch(no, v));
        CHECK_FALSE(v);
    }

    v = true;
    CHECK_FALSE(abuse::render::parse_switch("maybe", v));
    CHECK(v);
    CHECK_FALSE(abuse::render::parse_switch(nullptr, v));
    CHECK(v);
}

TEST_CASE("smooth movement is on by default") {
    abuse::render::Options fresh;
    CHECK(fresh.interpolate);
}
