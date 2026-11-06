#include "jobs/jobs.hpp"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <latch>
#include <stdexcept>
using namespace ngen;
namespace {
void increment(void *ptr, const std::atomic<bool> &) noexcept { ++*static_cast<int *>(ptr); }
} // namespace
TEST_CASE("cancellation joins running work and suppresses dependents") {
    struct Context {
        std::latch entered{1};
        std::atomic<int> calls{};
    } context;
    Jobs jobs(2);
    auto work = [](void *ptr, const std::atomic<bool> &stop) noexcept {
        auto &c = *static_cast<Context *>(ptr);
        c.entered.count_down();
        while (!stop.load())
            std::this_thread::yield();
        ++c.calls;
    };
    REQUIRE(std::get<std::size_t>(jobs.submit(work, &context)) == 0);
    REQUIRE(std::get<std::size_t>(jobs.submit(work, &context, 0)) == 1);
    REQUIRE(std::holds_alternative<std::monostate>(jobs.start()));
    context.entered.wait();
    jobs.shutdown();
    REQUIRE(context.calls.load() == 1);
}
TEST_CASE("jobs reject invalid configuration and forward dependencies") {
    REQUIRE_THROWS_AS(Jobs(8), std::invalid_argument);
    Jobs jobs(0);
    int count = 0;
    REQUIRE(std::get<Error>(jobs.submit(nullptr, nullptr)) == Error::invalid);
    REQUIRE(std::get<Error>(jobs.submit(increment, &count, 0)) == Error::invalid);
    REQUIRE(std::get<std::size_t>(jobs.submit(increment, &count)) == 0);
    jobs.request_cancel();
    REQUIRE(std::holds_alternative<std::monostate>(jobs.start()));
    jobs.drain();
    REQUIRE(count == 0);
}
TEST_CASE("bounded jobs preserve dependency order and release borrowed context") {
    for (unsigned workers : {0U, 2U}) {
        Jobs jobs(workers);
        int count = 0;
        for (std::size_t i = 0; i < Jobs::capacity; ++i) {
            REQUIRE(std::get<std::size_t>(
                        jobs.submit(increment, &count, i ? i - 1 : Jobs::no_dependency)) == i);
        }
        REQUIRE(std::get<Error>(jobs.submit(increment, &count)) == Error::capacity);
        REQUIRE(std::holds_alternative<std::monostate>(jobs.start()));
        REQUIRE(std::get<Error>(jobs.submit(increment, &count)) == Error::busy);
        jobs.drain();
        REQUIRE(count == 64);
        REQUIRE(std::get<std::size_t>(jobs.submit(increment, &count)) == 0);
        jobs.shutdown(); // Pending work cancelled; context can now die.
        REQUIRE(count == 64);
        REQUIRE(std::get<Error>(jobs.submit(increment, &count)) == Error::closed);
    }
}
