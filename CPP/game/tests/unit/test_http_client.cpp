#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include "../../net/http_client.hpp"

TEST_CASE("disabled client short-circuits with status 0", "[net]") {
    net::set_enabled(false);
    auto f = net::get("https://example.invalid/x");
    REQUIRE(f.get().status == 0);
}

TEST_CASE("get fetches a local file url", "[net]") {
    net::set_enabled(true);
    std::ofstream("/tmp/rd_http_test.txt") << "hello-net";
    auto f = net::get("file:///tmp/rd_http_test.txt");
    auto r = f.get();
    REQUIRE(r.body == "hello-net");
    net::set_enabled(false);
}
