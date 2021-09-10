#include <cradle/io/endian.h>

#include <cradle/utilities/testing.h>

using namespace cradle;

#if !defined(CRADLE_LITTLE_ENDIAN) && !defined(CRADLE_BIG_ENDIAN)
#error Neither CRADLE_LITTLE_ENDIAN nor CRADLE_BIG_ENDIAN is defined!
#elif defined(CRADLE_LITTLE_ENDIAN) && defined(CRADLE_BIG_ENDIAN)
#error Both CRADLE_LITTLE_ENDIAN and CRADLE_BIG_ENDIAN are defined!
#endif

TEST_CASE("scalar endian swapping", "[io][endian]")
{
    REQUIRE(swap_uint16_endian(0xf1f0) == 0xf0f1);
    {
        uint16_t x = 0xf1f0;
        swap_endian(&x);
        REQUIRE(x == 0xf0f1);
    }

    REQUIRE(swap_uint32_endian(0xbaadf1f0) == 0xf0f1adba);
    {
        uint32_t x = 0xbaadf1f0;
        swap_endian(&x);
        REQUIRE(x == 0xf0f1adba);
    }

    REQUIRE(swap_uint64_endian(0xdeadc0debaadf1f0) == 0xf0f1adbadec0adde);
    {
        uint64_t x = 0xdeadc0debaadf1f0;
        swap_endian(&x);
        REQUIRE(x == 0xf0f1adbadec0adde);
    }
}

TEST_CASE("architecture-specific scalar endian swapping", "[io][endian]")
{
#ifdef CRADLE_LITTLE_ENDIAN
    REQUIRE(swap_uint16_on_little_endian(0xf1f0) == 0xf0f1);
#else
    REQUIRE(swap_uint16_on_big_endian(0xf1f0) == 0xf0f1);
#endif
    {
        uint16_t x = 0xf1f0;
#ifdef CRADLE_LITTLE_ENDIAN
        swap_on_little_endian(&x);
#else
        swap_on_big_endian(&x);
#endif
        REQUIRE(x == 0xf0f1);
    }

#ifdef CRADLE_LITTLE_ENDIAN
    REQUIRE(swap_uint32_on_little_endian(0xbaadf1f0) == 0xf0f1adba);
#else
    REQUIRE(swap_uint32_on_big_endian(0xbaadf1f0) == 0xf0f1adba);
#endif
    {
        uint32_t x = 0xbaadf1f0;
#ifdef CRADLE_LITTLE_ENDIAN
        swap_on_little_endian(&x);
#else
        swap_on_big_endian(&x);
#endif
        REQUIRE(x == 0xf0f1adba);
    }

#ifdef CRADLE_LITTLE_ENDIAN
    REQUIRE(
        swap_uint64_on_little_endian(0xdeadc0debaadf1f0)
        == 0xf0f1adbadec0adde);
#else
    REQUIRE(
        swap_uint64_on_big_endian(0xdeadc0debaadf1f0) == 0xf0f1adbadec0adde);
#endif
    {
        uint64_t x = 0xdeadc0debaadf1f0;
#ifdef CRADLE_LITTLE_ENDIAN
        swap_on_little_endian(&x);
#else
        swap_on_big_endian(&x);
#endif
        REQUIRE(x == 0xf0f1adbadec0adde);
    }
}

TEST_CASE("16-bit array endian swapping", "[io][endian]")
{
    uint16_t array[] = {0xf0f1, 0xbada, 0xcab1},
             swapped[] = {0xf1f0, 0xdaba, 0xb1ca};
    swap_array_endian(array, 3);
    for (int i = 0; i < 3; ++i)
        REQUIRE(array[i] == swapped[i]);
}

TEST_CASE("architecture-specifc 16-bit array endian swapping", "[io][endian]")
{
    uint16_t array[] = {0xf0f1, 0xbada, 0xcab1},
             swapped[] = {0xf1f0, 0xdaba, 0xb1ca};
#ifdef CRADLE_LITTLE_ENDIAN
    swap_array_on_little_endian(array, 3);
#else
    swap_array_on_big_endian(array, 3);
#endif
    for (int i = 0; i < 3; ++i)
        REQUIRE(array[i] == swapped[i]);
}

TEST_CASE("32-bit array endian swapping", "[io][endian]")
{
    uint32_t array[] = {0xf0f113f3, 0xbadacab0, 0xcab14d3d},
             swapped[] = {0xf313f1f0, 0xb0cadaba, 0x3d4db1ca};
    swap_array_endian(array, 3);
    for (int i = 0; i < 3; ++i)
        REQUIRE(array[i] == swapped[i]);
}

TEST_CASE("architecture-specific 32-bit array endian swapping", "[io][endian]")
{
    uint32_t array[] = {0xf0f113f3, 0xbadacab0, 0xcab14d3d},
             swapped[] = {0xf313f1f0, 0xb0cadaba, 0x3d4db1ca};
#ifdef CRADLE_LITTLE_ENDIAN
    swap_array_on_little_endian(array, 3);
#else
    swap_array_on_big_endian(array, 3);
#endif
    for (int i = 0; i < 3; ++i)
        REQUIRE(array[i] == swapped[i]);
}

TEST_CASE("64-bit array endian swapping", "[io][endian]")
{
    uint64_t array[]
        = {0xf0f113f3badacab0, 0xbadacab021435465, 0xcab14d3df0f113f3},
        swapped[]
        = {0xb0cadabaf313f1f0, 0x65544321b0cadaba, 0xf313f1f03d4db1ca};
    swap_array_endian(array, 3);
    for (int i = 0; i < 3; ++i)
        REQUIRE(array[i] == swapped[i]);
}

TEST_CASE("architecture-specific 64-bit array endian swapping", "[io][endian]")
{
    uint64_t array[]
        = {0xf0f113f3badacab0, 0xbadacab021435465, 0xcab14d3df0f113f3},
        swapped[]
        = {0xb0cadabaf313f1f0, 0x65544321b0cadaba, 0xf313f1f03d4db1ca};
#ifdef CRADLE_LITTLE_ENDIAN
    swap_array_on_little_endian(array, 3);
#else
    swap_array_on_big_endian(array, 3);
#endif
    for (int i = 0; i < 3; ++i)
        REQUIRE(array[i] == swapped[i]);
}
