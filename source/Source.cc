#include <cpy/Suite.h>

namespace cpy {

Suite & suite() {
    static std::deque<TestCase> static_suite;
    return static_suite;
}

void add_test(TestCase t) {
    suite().emplace_back(std::move(t));
}

/******************************************************************************/

// Context::Context() = default;

// Context::Context(Context const &) = default;
// Context::Context(Context &&) noexcept = default;
// Context & Context::operator=(Context const &) = default;
// Context & Context::operator=(Context &&) noexcept = default;

Context::Context(Scopes s, Vector<Handler> h, Vector<Counter> *c, void *m)
    : scopes(std::move(s)), handlers(std::move(h)), counters(c), metadata(m), start_time(Clock::now()) {}

/******************************************************************************/

Value call(std::string_view s, Context c, ArgPack pack) {
    auto const &cases = suite();
    auto it = std::find_if(cases.begin(), cases.end(), [=](auto const &c) {return c.name == s;});
    if (it == cases.end())
        throw std::runtime_error("Test case \"" + std::string(s) + "\" not found");
    return it->function(c, std::move(pack));
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

/******************************************************************************/

Value::Value(Value &&v) noexcept : var(std::move(v.var)) {}
Value::Value(Value const &v) : var(v.var) {}
Value & Value::operator=(Value const &v) {var = v.var; return *this;}
Value & Value::operator=(Value &&v) noexcept {var = std::move(v.var); return *this;}

Value::Value(std::monostate v)   noexcept : var(v) {}
Value::Value(bool v)             noexcept : var(v) {}
Value::Value(Integer v)          noexcept : var(v) {}
Value::Value(Real v)             noexcept : var(v) {}
Value::Value(Complex v)          noexcept : var(v) {}
Value::Value(std::string v)      noexcept : var(std::move(v)) {}
Value::Value(std::string_view v) noexcept : var(std::move(v)) {}

Value::Value(std::in_place_t, std::any v) noexcept : var(std::move(v)) {}
Value::Value(Vector<Value> v)             noexcept : var(std::move(v)) {}

/******************************************************************************/

bool Value::as_bool() const & {return std::get<bool>(var);}

Integer Value::as_integer() const & {return std::get<Integer>(var);}

Real Value::as_real() const & {return std::get<Real>(var);}

std::string_view Value::as_view() const & {return std::get<std::string_view>(var);}

/******************************************************************************/

std::any Value::as_any() const & {return std::get<std::any>(var);}
std::any Value::as_any() && {return std::get<std::any>(std::move(var));}

std::string Value::as_string() const & {
    if (auto s = std::get_if<std::string_view>(&var))
        return std::string(*s);
    return std::get<std::string>(var);
}

Vector<Value> Value::as_vector() const & {return std::get<Vector<Value>>(var);}
Vector<Value> Value::as_vector() && {return std::get<Vector<Value>>(std::move(var));}

Binary Value::as_binary() const & {return std::get<Binary>(var);}
Binary Value::as_binary() && {return std::get<Binary>(std::move(var));}

Value::~Value() = default;

/******************************************************************************/

}
