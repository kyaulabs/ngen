#include "render/capabilities.hpp"
#include <catch2/catch_test_macros.hpp>
using namespace ngen::render;
TEST_CASE("renderer requires the baseline independently of optional modern features") {
    const Capabilities valid{1, 3, true, true, true, true, true, true, true};
    REQUIRE_FALSE(check(valid));
    auto candidate = valid;
    candidate.api_minor = 2;
    REQUIRE(check(candidate) == Missing::api);
    candidate = valid;
    candidate.dynamic_rendering = false;
    REQUIRE(check(candidate) == Missing::dynamic_rendering);
    candidate = valid;
    candidate.presentation = false;
    REQUIRE(check(candidate) == Missing::presentation);
    candidate = valid;
    candidate.graphics = false;
    REQUIRE(check(candidate) == Missing::graphics);
    candidate = valid;
    candidate.synchronization2 = false;
    REQUIRE(check(candidate) == Missing::synchronization2);
    candidate = valid;
    candidate.timeline_semaphore = false;
    REQUIRE(check(candidate) == Missing::timeline_semaphore);
    candidate = valid;
    candidate.color_attachment = false;
    REQUIRE(check(candidate) == Missing::color_attachment);
    candidate = valid;
    candidate.depth_attachment = false;
    REQUIRE(check(candidate) == Missing::depth_attachment);
}
