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

REBINDC(Index) REBINDC(Value_type)(REBINDC(Value) *v) {
    // REBINDC(Index)[16] REBINDC(variable_type)(REBINDC(value) *v);
    auto t = reinterpret_cast<rebind::Value const *>(v)->index();
    static_assert(sizeof(t) == sizeof(REBINDC(Index)));
    return reinterpret_cast<REBINDC(Index) const &>(t);
}

char const * REBINDC(TypeIndex_name)(REBINDC(Index) v) {
    return reinterpret_cast<rebind::Index const &>(v).info().name();
}