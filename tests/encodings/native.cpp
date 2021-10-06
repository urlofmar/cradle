#include <cradle/encodings/native.h>

#include <cstring>

#include <cradle/encodings/json.h>
#include <cradle/utilities/testing.h>
#include <cradle/utilities/text.h>

using namespace cradle;

TEST_CASE("basic native encoding", "[encodings][native]")
{
    string test_json =
        R"(
            {
                "alpha": null,
                "beta": true,
                "gamma": false,
                "delta": [ -60, 4096 ],
                "epsilon": [ -1.5, 12.5 ],
                "zeta": "foo",
                "eta": {
                    "type": "base64-encoded-blob",
                    "blob": "V2lsbCBhbnlvbmUgZXZlciBzZWUgdGhpcz8="
                },
                "theta": "1970-01-01T00:00:00.100Z",
                "iota": "1970-01-01T00:00:30.000Z",
                "kappa": "1970-01-03T00:00:00.000Z",
                "lambda": "2000-01-01T00:00:00.000Z",
                "mu": {
                    "a": null,
                    "b": true,
                    "c": false
                },
                "nu": [
                    {
                        "key": null,
                        "value": "a"
                    },
                    {
                        "key": true,
                        "value": "b"
                    },
                    {
                        "key": false,
                        "value": "c"
                    }
                ],
                "xi": [ null, true, false ]
            }
        )";

    auto original_data = parse_json_value(test_json);

    auto native_data = write_natively_encoded_value(original_data);

    auto decoded_data
        = read_natively_encoded_value(native_data.data(), native_data.size());

    REQUIRE(decoded_data == original_data);
}

TEST_CASE("malformed natively encoded data", "[encodings][native]")
{
    {
        uint8_t encoded_data[] = {0xd4, 0x01, 0x00};
        REQUIRE_THROWS(read_natively_encoded_value(encoded_data, 3));
    }
    {
        uint8_t encoded_data[] = {0xc1};
        REQUIRE_THROWS(read_natively_encoded_value(encoded_data, 1));
    }
}
