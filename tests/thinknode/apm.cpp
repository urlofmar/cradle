#include <cradle/thinknode/apm.h>

#include <cppcoro/sync_wait.hpp>

#include <cradle/core/monitoring.h>
#include <cradle/io/mock_http.h>
#include <cradle/service/core.h>
#include <cradle/utilities/testing.h>

using namespace cradle;

TEST_CASE("app version info", "[thinknode][apm]")
{
    service_core service;
    init_test_service(service);

    auto& mock_http = enable_http_mocking(service);
    mock_http.set_script(
        {{make_get_request(
              "https://mgh.thinknode.io/api/v1.0/apm/apps/acme/pets/"
              "versions/"
              "2.0.0?include_manifest=true",
              {{"Authorization", "Bearer xyz"},
               {"Accept", "application/json"}}),
          make_http_200_response(
              R"(
                        {
                            "name": "2.0.0",
                            "manifest": {
                                "dependencies": [],
                                "types": [
                                    {
                                        "name": "dog",
                                        "description": "Canis lupus familiaris",
                                        "schema": {
                                            "structure_type": {
                                                "fields": {
                                                    "name": {
                                                        "description": "The name of the dog",
                                                        "schema": {
                                                            "string_type": {}
                                                        }
                                                    },
                                                    "age": {
                                                        "description": "The age of the dog",
                                                        "schema": {
                                                            "integer_type": {}
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                ],
                                "functions": [
                                    {
                                        "name": "get_dog_years",
                                        "description": "Computes the dog's age in dog years.",
                                        "execution_class": "cpu.x1",
                                        "schema": {
                                            "function_type": {
                                                "parameters": [
                                                    {
                                                        "name": "dog",
                                                        "description": "A dog with an age in people years",
                                                        "schema": {
                                                            "named_type": {
                                                                "account": "acme",
                                                                "app": "pets",
                                                                "name": "dog"
                                                            }
                                                        }
                                                    }
                                                ],
                                                "returns": {
                                                    "description": "The computed age of the dog in dog years.",
                                                    "schema": {
                                                        "integer_type": {}
                                                    }
                                                }
                                            }
                                        }
                                    }
                                ],
                                "records": [],
                                "upgrades": []
                            },
                            "created_by": "tmadden",
                            "created_at": "2017-04-26T01:02:03.000Z"
                        }
                    )")}});

    thinknode_session session;
    session.api_url = "https://mgh.thinknode.io/api/v1.0";
    session.access_token = "xyz";

    auto expected_version_info = make_thinknode_app_version_info(
        "2.0.0",
        some(make_thinknode_app_manifest(
            {},
            none,
            {make_thinknode_named_type_info(
                "dog",
                "Canis lupus familiaris",
                make_thinknode_type_info_with_structure_type(
                    make_thinknode_structure_info(
                        {{"name",
                          make_thinknode_structure_field_info(
                              "The name of the dog",
                              none,
                              make_thinknode_type_info_with_string_type(
                                  thinknode_string_type()))},
                         {"age",
                          make_thinknode_structure_field_info(
                              "The age of the dog",
                              none,
                              make_thinknode_type_info_with_integer_type(
                                  thinknode_integer_type()))}})))},
            {make_thinknode_function_info(
                "get_dog_years",
                "Computes the dog's age in dog years.",
                "cpu.x1",
                make_thinknode_function_type_with_function_type(
                    make_thinknode_function_type_info(
                        {make_thinknode_function_parameter_info(
                            "dog",
                            "A dog with an age in people years",
                            make_thinknode_type_info_with_named_type(
                                make_thinknode_named_type_reference(
                                    some(string("acme")), "pets", "dog")))},
                        make_thinknode_function_result_info(
                            "The computed age of the dog in dog years.",
                            make_thinknode_type_info_with_integer_type(
                                thinknode_integer_type())))))},
            {},
            {})),
        "tmadden",
        ptime(
            date(2017, boost::gregorian::Apr, 26),
            boost::posix_time::time_duration(1, 2, 3)));

    auto version_info = cppcoro::sync_wait(
        get_app_version_info(service, session, "acme", "pets", "2.0.0"));
    REQUIRE(version_info == expected_version_info);

    REQUIRE(mock_http.is_complete());
    REQUIRE(mock_http.is_in_order());
}
