#include "script/lua_rule.hpp"
#include <cstdlib>
#include <limits>
#include <memory>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

static_assert(LUA_VERSION_RELEASE_NUM == 50501);

namespace ngen::script {
namespace {
struct Heap {
    std::size_t limit, used{};
};

struct alignas(std::max_align_t) Block {
    std::size_t charge;
};

struct Invocation {
    std::string_view source;
    Facts facts;
    int remaining;
    Heap heap;
    int load_status{};
    bool exhausted{};
};

// This argument order is fixed by Lua's allocator ABI.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void *allocate(void *data, void *address, std::size_t, std::size_t size) {
    auto &heap = static_cast<Invocation *>(data)->heap;
    auto *old = address ? static_cast<Block *>(address) - 1 : nullptr;
    const auto prior = old ? old->charge : 0;
    if (!size) {
        heap.used -= prior;
        std::free(old);
        return nullptr;
    }
    // Charge the retained allocation (including its header), not Lua's
    // logical size. A shrink can reuse its block without ever failing.
    if (size > heap.limit || sizeof(Block) > heap.limit - size)
        return nullptr;
    const auto charge = size + sizeof(Block);
    if (charge <= prior)
        return address;
    if (charge - prior > heap.limit - heap.used)
        return nullptr;
    auto *block = static_cast<Block *>(std::realloc(old, charge));
    if (!block)
        return nullptr;
    block->charge = charge;
    heap.used += charge - prior;
    return block + 1;
}

void count_instruction(lua_State *vm, lua_Debug *) {
    void *data = nullptr;
    lua_getallocf(vm, &data);
    auto *call = static_cast<Invocation *>(data);
    if (call->remaining == 0) {
        call->exhausted = true;
        lua_pushlightuserdata(vm, nullptr);
        lua_error(vm);
        return;
    }
    --call->remaining;
}

// Lua may jump to the outer lua_pcall. This C callback must have only
// trivially destructible locals and must never call C++ gameplay code.
int invoke(lua_State *vm) {
    auto *call = static_cast<Invocation *>(lua_touserdata(vm, 1));
    call->load_status =
        luaL_loadbufferx(vm, call->source.data(), call->source.size(), "quest", "t");
    if (call->load_status != LUA_OK)
        return lua_error(vm);
    lua_pushinteger(vm, call->facts.stage);
    lua_pushinteger(vm, call->facts.event);
    lua_call(vm, 2, LUA_MULTRET);
    return lua_gettop(vm) - 1; // The first slot is the invocation pointer.
}
} // namespace

Decision evaluate(std::string_view source, Facts facts, Limits limits) {
    if (source.empty() || source.size() > 4096 || limits.instructions < 1 ||
        limits.instructions > 100000 || !limits.heap_bytes ||
        limits.heap_bytes > std::size_t{256} * 1024)
        return Failure::input;
    Invocation call{source, facts, limits.instructions, {limits.heap_bytes}};
    const std::unique_ptr<lua_State, decltype(&lua_close)> vm(lua_newstate(allocate, &call, 0),
                                                              &lua_close);
    if (!vm)
        return Failure::heap;
    lua_sethook(vm.get(), count_instruction, LUA_MASKCOUNT, 1);
    lua_pushcfunction(vm.get(), invoke);
    lua_pushlightuserdata(vm.get(), &call);
    const auto status = lua_pcall(vm.get(), 1, LUA_MULTRET, 0);
    if (call.exhausted)
        return Failure::instructions;
    if (status == LUA_ERRMEM || call.load_status == LUA_ERRMEM)
        return Failure::heap;
    if (call.load_status != LUA_OK)
        return Failure::syntax;
    if (status != LUA_OK)
        return Failure::runtime;
    if (lua_gettop(vm.get()) != 1 || !lua_isinteger(vm.get(), 1))
        return Failure::result;
    const auto decision = lua_tointeger(vm.get(), 1);
    if (decision < 0 || decision > std::numeric_limits<std::uint8_t>::max())
        return Failure::result;
    return static_cast<std::uint8_t>(decision);
}
} // namespace ngen::script
