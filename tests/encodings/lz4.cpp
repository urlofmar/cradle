#include <cradle/encodings/lz4.h>

#include <cradle/utilities/testing.h>

using namespace cradle;

TEST_CASE("simple lz4 compression", "[encodings][lz4]")
{
    size_t const original_data_size = 0x3020401;
    std::unique_ptr<uint8_t[]> original_data(new uint8_t[original_data_size]);
    for (size_t i = 0; i != original_data_size; ++i)
        original_data[i] = (std::rand() & 0x7f) + 0x70;

    size_t max_compressed_size = lz4::max_compressed_size(original_data_size);
    std::unique_ptr<uint8_t[]> compressed_data(
        new uint8_t[max_compressed_size]);
    size_t actual_compressed_size = lz4::compress(
        compressed_data.get(),
        max_compressed_size,
        original_data.get(),
        original_data_size);

    std::unique_ptr<uint8_t[]> decompressed_data(
        new uint8_t[original_data_size]);
    lz4::decompress(
        decompressed_data.get(),
        original_data_size,
        compressed_data.get(),
        actual_compressed_size);

    REQUIRE(
        std::memcmp(
            original_data.get(), decompressed_data.get(), original_data_size)
        == 0);
}

TEST_CASE("lz4 compression error", "[encodings][lz4]")
{
    char const* text = "text";
    REQUIRE_THROWS(lz4::compress(nullptr, 0, text, 4));
}

TEST_CASE("lz4 decompression error", "[encodings][lz4]")
{
    char const* bad_lz4_data = "whatever";
    REQUIRE_THROWS(lz4::decompress(nullptr, 0, bad_lz4_data, 8));
}
