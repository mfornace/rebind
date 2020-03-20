#include <rebind/Document.h>

#include <rebind-rust/Module.h>

/******************************************************************************/

int REBINDM(Value, add)() {return 123;}

void REBINDM(Value, destruct)(REBINDC(Value) *x) { // noexcept
    delete reinterpret_cast<rebind::Value *>(x);
}

void REBINDM(Value, new)(REBINDC(Value) *v) { // noexcept
    // return reinterpret_cast<REBINDC(Value) *>(new rebind::Value(std::string()));
}

bool REBINDM(Value, copy)(REBINDC(Value) *v, REBINDC(Value) const *o) {
    // try {
    //     return reinterpret_cast<REBINDC(Value) *>(new rebind::Value(
    //         *reinterpret_cast<rebind::Value const *>(v)));
    // } catch (...) {
    //     return nullptr;
    // }
    return true;
}


void REBINDM(Value, move)(REBINDC(Value) *v, REBINDC(Value) *o) {
    // try {
    //     return reinterpret_cast<REBINDC(Value) *>(new rebind::Value(
    //         *reinterpret_cast<rebind::Value const *>(v)));
    // } catch (...) {
    //     return nullptr;
    // }
}

REBINDC(Index) REBINDM(Value, index)(REBINDC(Value) *v) {
    // REBINDC(Index)[16] REBINDC(variable_type)(REBINDC(value) *v);
    auto t = reinterpret_cast<rebind::Value const *>(v)->index();
    static_assert(sizeof(t) == sizeof(REBINDC(Index)));
    return reinterpret_cast<REBINDC(Index) const &>(t);
}

char const * REBINDM(Index, name)(REBINDC(Index) v) {
    return reinterpret_cast<rebind::Index const &>(v).info().name();
}

/******************************************************************************/