#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <set>
#include <string>

#include "imlib/hd.h"
#include "palette.h"
#include "specs.h"

// The loader decodes against the game palette; loader2.cpp owns this in the
// game and nothing in this suite draws, so a null one is the whole of it.
palette *pal = NULL;

// A pack with more than one picture for the same entry. The files are never
// decoded here: find() only looks at what is on disk, which is the part that
// decides which picture a run shows.
namespace {

std::string pack_root()
{
    static std::string root;
    if (!root.empty())
        return root;

    root = std::string(ABUSE_TEST_BUILD_DIR) + "/hdpack/";
    std::filesystem::create_directories(root + "hd/title");
    std::filesystem::create_directories(root + "hd/frame");

    auto touch = [](std::string const &path) {
        std::ofstream(path, std::ios::binary) << "not a png";
    };

    // Three of the title, one of the other, which is the shape of the pack
    // the repository ships.
    touch(root + "hd/title/title_screen.png");
    touch(root + "hd/title/title_screen.2.png");
    touch(root + "hd/title/title_screen.3.png");
    touch(root + "hd/frame/end_level_screen.png");

    set_filename_prefix(root.c_str());
    return root;
}

std::string found(char const *spe, char const *name)
{
    pack_root();
    char *p = abuse::hd::find(spe, name);
    std::string s(p ? p : "");
    free(p);
    return s;
}

}

TEST_CASE("without a seed the first picture is the one used") {
    abuse::hd::set_variant_seed(0);
    CHECK(found("art/title.spe", "title_screen") == "hd/title/title_screen.png");
}

TEST_CASE("an entry with one picture ignores the seed") {
    abuse::hd::set_variant_seed(12345);
    CHECK(found("art/frame.spe", "end_level_screen")
          == "hd/frame/end_level_screen.png");
}

TEST_CASE("a seeded run picks one of the three and keeps it") {
    abuse::hd::set_variant_seed(98765);

    std::string const first = found("art/title.spe", "title_screen");
    std::set<std::string> const allowed = {
        "hd/title/title_screen.png",
        "hd/title/title_screen.2.png",
        "hd/title/title_screen.3.png",
    };
    CHECK(allowed.count(first) == 1);

    // Asked again, as the cache does when it reloads the object. A title
    // screen that changed while the menu was open would read as a glitch.
    CHECK(found("art/title.spe", "title_screen") == first);
    CHECK(found("art/title.spe", "title_screen") == first);
}

TEST_CASE("a missing entry is still a miss") {
    abuse::hd::set_variant_seed(98765);
    CHECK(found("art/title.spe", "no_such_entry").empty());
}
