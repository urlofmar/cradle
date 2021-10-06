#include <cradle/io/raw_memory_io.h>

namespace cradle {

// This is used for encoding datetimes.
static boost::posix_time::ptime const
    the_epoch(boost::gregorian::date(1970, 1, 1));

void
read_natively_encoded_value(raw_memory_reader<raw_input_buffer>& r, dynamic& v)
{
    value_type type;
    {
        uint32_t t;
        raw_read(r, &t, 4);
        type = value_type(t);
    }
    switch (type)
    {
        case value_type::NIL:
            v = nil;
            break;
        case value_type::BOOLEAN: {
            uint8_t x;
            raw_read(r, &x, 1);
            v = bool(x != 0);
            break;
        }
        case value_type::INTEGER: {
            integer x;
            raw_read(r, &x, 8);
            v = x;
            break;
        }
        case value_type::FLOAT: {
            double x;
            raw_read(r, &x, 8);
            v = x;
            break;
        }
        case value_type::STRING: {
            v = read_string<uint32_t>(r);
            break;
        }
        case value_type::BLOB: {
            uint64_t length;
            raw_read(r, &length, 8);
            blob x;
            x.size = boost::numeric_cast<size_t>(length);
            std::shared_ptr<uint8_t> ptr(
                new uint8_t[x.size], array_deleter<uint8_t>());
            x.ownership = ptr;
            x.data = reinterpret_cast<char const*>(ptr.get());
            raw_read(r, const_cast<char*>(x.data), x.size);
            v = x;
            break;
        }
        case value_type::DATETIME: {
            int64_t t;
            raw_read(r, &t, 8);
            v = the_epoch + boost::posix_time::milliseconds(t);
            break;
        }
        case value_type::ARRAY: {
            uint64_t length;
            raw_read(r, &length, 8);
            dynamic_array value(boost::numeric_cast<size_t>(length));
            for (auto& item : value)
                read_natively_encoded_value(r, item);
            v = value;
            break;
        }
        case value_type::MAP: {
            uint64_t length;
            raw_read(r, &length, 8);
            dynamic_map map;
            for (uint64_t i = 0; i != length; ++i)
            {
                dynamic key;
                read_natively_encoded_value(r, key);
                dynamic value;
                read_natively_encoded_value(r, value);
                map[key] = value;
            }
            v = map;
            break;
        }
    }
}

dynamic
read_natively_encoded_value(uint8_t const* data, size_t size)
{
    dynamic value;
    raw_input_buffer buffer(data, size);
    raw_memory_reader r(buffer);
    read_natively_encoded_value(r, value);
    return value;
}

void
write_natively_encoded_value(
    raw_memory_writer<byte_vector_buffer>& w, dynamic const& v)
{
    {
        uint32_t t = uint32_t(v.type());
        raw_write(w, &t, 4);
    }
    switch (v.type())
    {
        case value_type::NIL:
            break;
        case value_type::BOOLEAN: {
            uint8_t t = cast<bool>(v) ? 1 : 0;
            raw_write(w, &t, 1);
            break;
        }
        case value_type::INTEGER: {
            integer t = cast<integer>(v);
            raw_write(w, &t, 8);
            break;
        }
        case value_type::FLOAT: {
            double t = cast<double>(v);
            raw_write(w, &t, 8);
            break;
        }
        case value_type::STRING:
            write_string<uint32_t>(w, cast<string>(v));
            break;
        case value_type::BLOB: {
            blob const& x = cast<blob>(v);
            uint64_t length = x.size;
            raw_write(w, &length, 8);
            raw_write(w, x.data, x.size);
            break;
        }
        case value_type::DATETIME: {
            int64_t t = (cast<boost::posix_time::ptime>(v) - the_epoch)
                            .total_milliseconds();
            raw_write(w, &t, 8);
            break;
        }
        case value_type::ARRAY: {
            dynamic_array const& x = cast<dynamic_array>(v);
            uint64_t size = x.size();
            raw_write(w, &size, 8);
            for (auto const& item : x)
                write_natively_encoded_value(w, item);
            break;
        }
        case value_type::MAP: {
            dynamic_map const& x = cast<dynamic_map>(v);
            uint64_t size = x.size();
            raw_write(w, &size, 8);
            for (auto const& entry : x)
            {
                write_natively_encoded_value(w, entry.first);
                write_natively_encoded_value(w, entry.second);
            }
            break;
        }
    }
}

byte_vector
write_natively_encoded_value(dynamic const& v)
{
    byte_vector data;
    byte_vector_buffer buffer(data);
    raw_memory_writer<byte_vector_buffer> w(buffer);
    write_natively_encoded_value(w, v);
    return data;
}

} // namespace cradle
