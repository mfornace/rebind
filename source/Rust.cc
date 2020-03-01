#include <rebind/Document.h>

#include <rebind-rust/Module.h>

int REBINDC(add)() {return 123;}

void REBINDC(destruct)(REBINDC(Value) *x) { // noexcept
    delete reinterpret_cast<rebind::Value *>(x);
}

REBINDC(Value) * REBINDC(Value_new)() { // noexcept
    return reinterpret_cast<REBINDC(Value) *>(new rebind::Value(std::string()));
}

REBINDC(Value) * REBINDC(Value_copy)(REBINDC(Value) *v) {
    try {
        return reinterpret_cast<REBINDC(Value) *>(new rebind::Value(
            *reinterpret_cast<rebind::Value const *>(v)));
    } catch (...) {
        return nullptr;
    }
}

REBINDC(TypeIndex) REBINDC(Value_type)(REBINDC(Value) *v) {
    // REBINDC(TypeIndex)[16] REBINDC(variable_type)(REBINDC(value) *v);
    auto t = reinterpret_cast<rebind::Value const *>(v)->index();
    static_assert(sizeof(t) == sizeof(REBINDC(TypeIndex)));
    return reinterpret_cast<REBINDC(TypeIndex) const &>(t);
}

char const * REBINDC(TypeIndex_name)(REBINDC(TypeIndex) v) {
    return reinterpret_cast<rebind::TypeIndex const &>(v).info().name();
}