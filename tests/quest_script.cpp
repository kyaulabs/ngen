#include "script/lua_rule.hpp"
#include <catch2/catch_test_macros.hpp>
#include <string>

using namespace ngen::script;

TEST_CASE("a fresh Lua rule chooses a branch from its scalar facts", "[quest-script]") {
    constexpr auto rule =
        "local stage,event = ...; if stage == 2 and event == 7 then return 3 end; return 0";
    CHECK(evaluate(rule, {2, 7}) == Decision{std::uint8_t{3}});
    CHECK(evaluate(rule, {2, 8}) == Decision{std::uint8_t{0}});
    CHECK(evaluate("marker = 7; return marker", {}) == Decision{std::uint8_t{7}});
    CHECK(evaluate("return marker == nil and 0 or 1", {}) == Decision{std::uint8_t{0}});
}

TEST_CASE("small instruction grants stop the rule before it can return", "[quest-script]") {
    constexpr auto loop = "local n=0; for i=1,200 do n=n+1 end; return n";
    CHECK(evaluate(loop, {}) == Decision{std::uint8_t{200}});
    CHECK(evaluate(loop, {}, {.instructions = 32}) == Decision{Failure::instructions});
    // Pinned Lua 5.5.1 dispatches the load and return under two count hooks.
    CHECK(evaluate("return 7", {}, {.instructions = 1}) == Decision{Failure::instructions});
    CHECK(evaluate("return 7", {}, {.instructions = 2}) == Decision{std::uint8_t{7}});
    CHECK(evaluate("while true do end", {}) == Decision{Failure::instructions});
    CHECK(evaluate("return 7", {}) == Decision{std::uint8_t{7}});
}

TEST_CASE("heap limits contain both VM startup and rule allocations", "[quest-script]") {
    CHECK(evaluate("return 7", {}, {.heap_bytes = 1}) == Decision{Failure::heap});
    constexpr auto reduced = 12U * 1024;
    constexpr auto allocations = "local t={}; for i=1,256 do t[i]={} end; return #t-1";
    CHECK(evaluate("return 7", {}, {.heap_bytes = reduced}) == Decision{std::uint8_t{7}});
    CHECK(evaluate(allocations, {}, {.heap_bytes = reduced}) == Decision{Failure::heap});
    CHECK(evaluate(allocations, {}) == Decision{std::uint8_t{255}});
    const auto large_literal = "return '" + std::string(4000, 'x') + "'";
    CHECK(evaluate("return 7", {}, {.heap_bytes = 8192}) == Decision{std::uint8_t{7}});
    CHECK(evaluate(large_literal, {}, {.heap_bytes = 8192}) == Decision{Failure::heap});
}

TEST_CASE("malformed input and runtime errors cannot leak state or select a branch",
          "[quest-script]") {
    CHECK(evaluate(std::string(4097, ' '), {}) == Decision{Failure::input});
    CHECK(evaluate("return 7", {}, {.heap_bytes = 256 * 1024 + 1}) == Decision{Failure::input});
    CHECK(evaluate("return 7", {}, {.instructions = 0}) == Decision{Failure::input});
    CHECK(evaluate("return @", {}) == Decision{Failure::syntax});
    constexpr char binary[] = "\x1bLua\x55\x00invalid";
    CHECK(evaluate(std::string_view{binary, sizeof binary - 1}, {}) == Decision{Failure::syntax});
    CHECK(evaluate("return absent()", {}) == Decision{Failure::runtime});
    CHECK(evaluate("return '3'", {}) == Decision{Failure::result});
    CHECK(evaluate("return 1,2", {}) == Decision{Failure::result});
    CHECK(evaluate("return 256", {}) == Decision{Failure::result});
    CHECK(evaluate("return os==nil and io==nil and require==nil and debug==nil and 9 or -1", {}) ==
          Decision{std::uint8_t{9}});
}
