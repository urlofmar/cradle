#include <cradle/thinknode/messages.h>

namespace cradle {

message_header
deserialize_message_header(uint8_t const* input)
{
    message_header header;
    raw_input_buffer buffer(input, ipc_message_header_size);
    raw_memory_reader<raw_input_buffer> reader(buffer);
    header.ipc_version = read_int<uint8_t>(reader);
    header.reserved_a = read_int<uint8_t>(reader);
    header.code = read_int<uint8_t>(reader);
    header.reserved_b = read_int<uint8_t>(reader);
    header.body_length = read_int<uint64_t>(reader);
    return header;
}

byte_vector
serialize_message_header(message_header const& header)
{
    byte_vector bytes;
    byte_vector_buffer buffer(bytes);
    raw_memory_writer<byte_vector_buffer> writer(buffer);
    write_int(writer, header.ipc_version);
    write_int(writer, header.reserved_a);
    write_int(writer, header.code);
    write_int(writer, header.reserved_b);
    write_int(writer, header.body_length);
    assert(bytes.size() == ipc_message_header_size);
    return bytes;
}

} // namespace cradle
