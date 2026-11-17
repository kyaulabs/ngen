enum class Code { success };
struct Value { int number{}; };
consteval int sample() { return Value{7}.number; }
static_assert(sample() == 7);
