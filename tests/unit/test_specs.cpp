#include <doctest/doctest.h>

#include <string.h>

#include "specs.h"

#ifndef ABUSE_TEST_DATA_DIR
#   define ABUSE_TEST_DATA_DIR "data"
#endif

namespace {

// The engine resolves relative names against a prefix; tests address the tree
// directly instead, so they do not depend on process-wide state.
char const *level_path()
{
    static char path[512];
    snprintf(path, sizeof(path), "%s/levels/level00.spe", ABUSE_TEST_DATA_DIR);
    return path;
}

}

TEST_CASE("a level file carries the SPEC1.0 signature") {
    jFILE fp(level_path(), "rb");
    REQUIRE_FALSE(fp.open_failure());

    char sig[SPEC_SIG_SIZE];
    CHECK(fp.read(sig, SPEC_SIG_SIZE) == SPEC_SIG_SIZE);
    CHECK(memcmp(sig, SPEC_SIGNATURE, strlen(SPEC_SIGNATURE)) == 0);
}

TEST_CASE("a level directory lists the entries the loader expects") {
    jFILE fp(level_path(), "rb");
    REQUIRE_FALSE(fp.open_failure());

    spec_directory sd(&fp);
    CHECK(sd.total > 0);

    // Written by level::write; read back by level::load.
    CHECK(sd.find("tick_counter") != NULL);

    // Every entry is named and typed, and its extent stays inside the file.
    long end = fp.file_size();
    for (int i = 0; i < sd.total; i++)
    {
        spec_entry *e = sd.entries[i];
        REQUIRE(e != NULL);
        CHECK(e->name != NULL);
        CHECK(e->type != SPEC_INVALID_TYPE);
        CHECK(e->offset >= 0);
        CHECK(e->offset + (long)e->size <= end);
    }
}

TEST_CASE("entries are listed in ascending offset order") {
    jFILE fp(level_path(), "rb");
    REQUIRE_FALSE(fp.open_failure());

    spec_directory sd(&fp);
    REQUIRE(sd.total > 1);

    // add_actives and the loaders walk this list in order; the replay hash
    // depends on that order being the file's own.
    for (int i = 1; i < sd.total; i++)
        CHECK(sd.entries[i]->offset >= sd.entries[i - 1]->offset);
}

TEST_CASE("find is exact and type aware") {
    jFILE fp(level_path(), "rb");
    REQUIRE_FALSE(fp.open_failure());

    spec_directory sd(&fp);

    CHECK(sd.find("no_such_entry_at_all") == NULL);

    spec_entry *e = sd.find("tick_counter");
    REQUIRE(e != NULL);
    CHECK(sd.find("tick_counter", e->type) == e);
    // Same name, wrong type: no match.
    CHECK(sd.find("tick_counter", SPEC_PALETTE) == NULL);
}

TEST_CASE("data_start_offset lands on the first entry") {
    jFILE fp(level_path(), "rb");
    REQUIRE_FALSE(fp.open_failure());

    spec_directory sd(&fp);
    REQUIRE(sd.total > 0);
    CHECK(sd.data_start_offset() == sd.entries[0]->offset);
    CHECK(sd.data_start_offset() > SPEC_SIG_SIZE + 2);
}

TEST_CASE("data_end_offset returns the end of the last entry") {
    jFILE fp(level_path(), "rb");
    REQUIRE_FALSE(fp.open_failure());

    spec_directory sd(&fp);
    REQUIRE(sd.total > 1);

    spec_entry *last = sd.entries[sd.total - 1];

    CHECK(sd.data_end_offset() == last->offset + (long)last->size);
    CHECK(sd.data_end_offset() == fp.file_size());
}
