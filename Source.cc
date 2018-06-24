#include <cpy/Suite.h>

namespace cpy {

Suite & suite() {
    static std::vector<TestCase> static_suite;
    return static_suite;
}

void register_test(TestCase c) {
    suite().emplace_back(std::move(c));
}

/******************************************************************************/

// Context::Context() = default;

// Context::Context(Context const &) = default;
// Context::Context(Context &&) noexcept = default;
// Context & Context::operator=(Context const &) = default;
// Context & Context::operator=(Context &&) noexcept = default;

Context::Context(Scopes s, std::vector<Callback> h, std::vector<Counter> *c, void *m)
    : scopes(std::move(s)), callbacks(std::move(h)), counters(c), metadata(m) {logs.reserve(4);} // advance reserve, may help a bit?

/******************************************************************************/

Value call(std::string_view s, Context c, ArgPack pack) {
    Value v;
    auto const &cases = suite();
    auto it = std::find_if(cases.begin(), cases.end(), [=](auto const &c) {return c.name == s;});
    if (it == cases.end())
        throw std::runtime_error("Test case \"" + std::string(s) + "\" not found");
    if (!it->function(v, std::move(c), std::move(pack)))
        throw std::runtime_error("Test case \"" + std::string(s) + "\" failed with an exception");
    return v;
}

Value get_value(std::string_view s) {
    auto const &cases = suite();
    auto it = std::find_if(cases.begin(), cases.end(), [=](auto const &c) {return c.name == s;});
    if (it == cases.end())
        throw std::runtime_error("Test case \"" + std::string(s) + "\" not found");
    ValueAdaptor const *p = it->function.target<ValueAdaptor>();
    if (!p)
        throw std::runtime_error("Test case \"" + std::string(s) + "\" is not a simple value");
    return p->value;
}

Value::Value(Value &&v) noexcept : var(std::move(v.var)) {}
Value::Value(Value const &v) noexcept : var(v.var) {}
Value & Value::operator=(Value const &v) noexcept {var = v.var; return *this;}
Value & Value::operator=(Value &&v) noexcept {var = std::move(v.var); return *this;}

Value::Value(std::monostate v) noexcept : var(v) {}
Value::Value(bool v) noexcept : var(v) {}
Value::Value(std::size_t v) noexcept : var(v) {}
Value::Value(std::ptrdiff_t v) noexcept : var(v) {}
Value::Value(double v) noexcept : var(v) {}
Value::Value(std::complex<double> v) noexcept : var(v) {}
Value::Value(std::string v) noexcept : var(std::move(v)) {}
Value::Value(std::string_view v) noexcept : var(std::move(v)) {}

std::string_view Value::as_view() const {return std::get<std::string_view>(var);}
double Value::as_double() const {return std::get<double>(var);}

// Value::Value(std::vector<bool> v) : var(std::move(v)) {}
// Value::Value(std::vector<std::size_t> v) : var(std::move(v)) {}
// Value::Value(std::vector<std::ptrdiff_t> v) : var(std::move(v)) {}
// Value::Value(std::vector<double> v) : var(std::move(v)) {}
// Value::Value(std::vector<std::complex<double>> v) : var(std::move(v)) {}
// Value::Value(std::vector<std::string> v) : var(std::move(v)) {}
// Value::Value(std::vector<std::string_view> v) : var(std::move(v)) {}

Value::~Value() = default;

/******************************************************************************/

double current_time() noexcept {
    auto t = std::chrono::high_resolution_clock::now().time_since_epoch();
    return std::chrono::duration<double>{t}.count();
}

/******************************************************************************/

}
