#include <cpy/Document.h>

namespace cpy {

Document & document() noexcept {
    static Document static_document;
    return static_document;
}

/******************************************************************************/

WrongTypes wrong_types(ArgPack const &v) {
    WrongTypes out;
    out.indices.reserve(v.size());
    for (auto const &x : v) out.indices.emplace_back(x.var.index());
    return out;
}

/******************************************************************************/

// Value::Value(Value &&v) noexcept : var(std::move(v.var)) {}
// Value::Value(Value const &v) : var(v.var) {}
// Value::Value(Value &v) : var(v.var) {}
// Value::~Value() = default;
// Value & Value::operator=(Value const &v) {var = v.var; return *this;}
// Value & Value::operator=(Value &&v) noexcept {var = std::move(v.var); return *this;}
// Value::Value(std::monostate v) noexcept : var(v) {}
// Value::Value(bool v)              noexcept : var(v) {}
// Value::Value(Integer v)           noexcept : var(v) {}
// Value::Value(Real v)              noexcept : var(v) {}
// Value::Value(Function v)          noexcept : var(std::move(v)) {}
// Value::Value(Binary v)            noexcept : var(std::move(v)) {}
// Value::Value(std::string v)       noexcept : var(std::move(v)) {}
// Value::Value(std::string_view v)  noexcept : var(std::move(v)) {}
// Value::Value(std::type_index v)   noexcept : var(std::move(v)) {}
// Value::Value(Sequence v)          noexcept : var(std::move(v)) {}


// bool Value::as_bool() const {return std::get<bool>(var);}
// Real Value::as_real() const {return std::get<Real>(var);}
// Integer Value::as_integer() const {return std::get<Integer>(var);}
// std::type_index Value::as_type() const {return std::get<std::type_index>(var);}

Sequence::Sequence(std::initializer_list<Value> const &v)
    : self(std::make_shared<SequenceModel<Vector<Value>>>(v)), m_size(v.size()) {}

void Sequence::scan_function(std::function<void(Value)> const &f) const {scan_functor(f);}

}
