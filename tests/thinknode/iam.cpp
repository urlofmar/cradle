#include <cradle/thinknode/iam.h>

#include <cradle/core/monitoring.h>
#include <cradle/io/mock_http.h>
#include <cradle/utilities/testing.h>

using namespace cradle;

TEST_CASE("context contents retrieval", "[thinknode][iam]")
{
    mock_http_session mock_http;
    mock_http.set_script(
        {{make_get_request(
              "https://mgh.thinknode.io/api/v1.0/iam/contexts/123",
              {{"Authorization", "Bearer xyz"},
               {"Accept", "application/json"}}),
          make_http_200_response(
              R"(
                        {
                            "bucket": "hacks",
                            "contents": [
                                {
                                    "account": "outatime",
                                    "app": "grays",
                                    "source": {
                                        "version": "1.0.0"
                                    }
                                },
                                {
                                    "account": "chaom",
                                    "app": "landsraad",
                                    "source": {
                                        "branch": "main"
                                }
                                },
                                {
                                    "account": "wayne_enterprises",
                                    "app": "cellsonar",
                                    "source": {
                                        "commit": "a7e1d608d6ce0c25dc6aa597492a6f09"
                                    }
                                }
                            ]
                        }
                    )")}});

    thinknode_session session;
    session.api_url = "https://mgh.thinknode.io/api/v1.0";
    session.access_token = "xyz";

    mock_http_connection connection(mock_http);
    auto contents = get_context_contents(connection, session, "123");
    REQUIRE(
        contents
        == make_thinknode_context_contents(
            "hacks",
            {make_thinknode_context_app_info(
                 "outatime",
                 "grays",
                 make_thinknode_app_source_info_with_version("1.0.0")),
             make_thinknode_context_app_info(
                 "chaom",
                 "landsraad",
                 make_thinknode_app_source_info_with_branch("main")),
             make_thinknode_context_app_info(
                 "wayne_enterprises",
                 "cellsonar",
                 make_thinknode_app_source_info_with_commit(
                     "a7e1d608d6ce0c25dc6aa597492a6f09"))}));

    REQUIRE(mock_http.is_complete());
    REQUIRE(mock_http.is_in_order());
}
