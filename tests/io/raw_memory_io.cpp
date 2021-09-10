#include <cradle/io/raw_memory_io.h>

#include <cradle/utilities/testing.h>

using namespace cradle;

TEST_CASE("raw memory I/O", "[io][raw_memory]")
{
    byte_vector bytes;
    byte_vector_buffer write_buffer(bytes);
    raw_memory_writer<byte_vector_buffer> writer(write_buffer);

    write_string<uint8_t>(writer, "hi");
    REQUIRE(bytes.size() == 3);

    write_int<uint8_t>(writer, 12);
    REQUIRE(bytes.size() == 4);

    write_int<uint32_t>(writer, 108);
    REQUIRE(bytes.size() == 8);

    write_float(writer, 1.5);
    REQUIRE(bytes.size() == 12);

    raw_input_buffer read_buffer(bytes.data(), bytes.size());
    raw_memory_reader<raw_input_buffer> reader(read_buffer);

    REQUIRE(read_string<uint8_t>(reader) == "hi");

    REQUIRE(read_int<uint8_t>(reader) == 12);

    REQUIRE(read_int<uint32_t>(reader) == 108);

    REQUIRE(read_float(reader) == 1.5);
}
