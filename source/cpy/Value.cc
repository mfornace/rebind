#include <cpy/Document.h>

namespace cpy {

Document & document() noexcept {
    static Document static_document;
    return static_document;
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
Value::Value(Function v)         noexcept : var(std::move(v)) {}
Value::Value(Binary v)           noexcept : var(std::move(v)) {}
Value::Value(std::string v)      noexcept : var(std::move(v)) {}
Value::Value(std::string_view v) noexcept : var(std::move(v)) {}
Value::Value(std::type_index v)  noexcept : var(std::move(v)) {}

Value::Value(std::in_place_t, Any v) noexcept : var(std::move(v)) {}
Value::Value(Vector<Value> v)             noexcept : var(std::move(v)) {}

/******************************************************************************/

bool Value::as_bool() const & {return std::get<bool>(var);}

Integer Value::as_integer() const & {return std::get<Integer>(var);}

Real Value::as_real() const & {return std::get<Real>(var);}

std::string_view Value::as_view() const & {return std::get<std::string_view>(var);}
std::type_index Value::as_index() const & {return std::get<std::type_index>(var);}

/******************************************************************************/

Any Value::as_any() const & {return std::get<Any>(var);}
Any Value::as_any() && {return std::get<Any>(std::move(var));}

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

WrongTypes wrong_types(ArgPack const &v) {
    WrongTypes out;
    out.indices.reserve(v.size());
    for (auto const &x : v) out.indices.emplace_back(x.var.index());
    return out;
}

/******************************************************************************/

}
