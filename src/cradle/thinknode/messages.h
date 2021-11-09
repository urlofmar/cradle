#ifndef CRADLE_THINKNODE_MESSAGES_H
#define CRADLE_THINKNODE_MESSAGES_H

// This file implements the general pattern used within Astroid to transmit
// messages over TCP.

#include <cradle/io/asio.h>

#include <cstring>
#include <queue>

#include <boost/shared_array.hpp>
#include <boost/static_assert.hpp>

#include <cradle/io/endian.h>
#include <cradle/io/raw_memory_io.h>

namespace cradle {

CRADLE_DEFINE_EXCEPTION(ipc_version_mismatch)
CRADLE_DEFINE_ERROR_INFO(int, local_ipc_version)
CRADLE_DEFINE_ERROR_INFO(int, remote_ipc_version)

using boost::asio::ip::tcp;

struct message_header
{
    uint8_t ipc_version;
    uint8_t reserved_a;
    uint8_t code;
    uint8_t reserved_b;
    uint64_t body_length;
};

size_t const ipc_message_header_size = 12;

// Deserialize the message header from the given buffer.
// Buffer must contain (at least) ipc_message_header_size bytes.
message_header
deserialize_message_header(uint8_t const* input);

// Serialize the given message header into a buffer for transmission.
byte_vector
serialize_message_header(message_header const& header);

// Read a message from the given socket.
template<class IncomingMessage>
IncomingMessage
read_message(tcp::socket& socket, uint8_t ipc_version)
{
    // Read the header.
    boost::shared_array<uint8_t> header_buffer(
        new uint8_t[ipc_message_header_size]);
    boost::asio::read(
        socket,
        boost::asio::buffer(header_buffer.get(), ipc_message_header_size));
    auto header = deserialize_message_header(header_buffer.get());
    if (header.ipc_version != ipc_version)
    {
        CRADLE_THROW(
            ipc_version_mismatch() << local_ipc_version_info(
                ipc_version) << remote_ipc_version_info(header.ipc_version));
    }

    // Read the body.
    auto body_length = boost::numeric_cast<size_t>(header.body_length);
    boost::shared_array<uint8_t> body_buffer(new uint8_t[body_length]);
    boost::asio::read(
        socket, boost::asio::buffer(body_buffer.get(), body_length));
    IncomingMessage message;
    read_message_body(&message, header.code, body_buffer, body_length);
    return message;
}

// Write a message to the given socket.
template<class OutgoingMessage>
void
write_message(
    tcp::socket& socket, uint8_t ipc_version, OutgoingMessage const& message)
{
    // Write the header.
    message_header header;
    header.ipc_version = ipc_version;
    header.reserved_a = 0;
    header.code = uint8_t(get_message_code(message));
    header.reserved_b = 0;
    header.body_length = uint64_t(get_message_body_size(message));
    auto buffer = serialize_message_header(header);
    boost::asio::write(socket, boost::asio::buffer(&buffer[0], buffer.size()));

    // Write the body.
    write_message_body(socket, message);
}

} // namespace cradle

#endif
