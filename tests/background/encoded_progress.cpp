#include <cradle/background/encoded_progress.h>

#include <sstream>

#include <cradle/utilities/testing.h>

using namespace cradle;

TEST_CASE("encoded_progress", "[background]")
{
    encoded_optional_progress p;
    REQUIRE(decode_progress(p) == none);

    p = encode_progress(0.203f);
    REQUIRE(decode_progress(p) == some(0.203f));

    reset(p);
    REQUIRE(decode_progress(p) == none);
}
