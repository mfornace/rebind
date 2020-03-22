#include <rebind/Schema.h>

#include <rebind-rust/Module.h>

/******************************************************************************/

void rebind_value_destruct(rebind_value *x) { // noexcept
    delete reinterpret_cast<rebind::Value *>(x);
}

void rebind_value_new(rebind_value *v) { // noexcept
    // return reinterpret_cast<rebind_value *>(new rebind::Value(std::string()));
}

bool rebind_value_copy(rebind_value *v, rebind_value const *o) {
    // try {
    //     return reinterpret_cast<rebind_value *>(new rebind::Value(
    //         *reinterpret_cast<rebind::Value const *>(v)));
    // } catch (...) {
    //     return nullptr;
    // }
    return true;
}


void rebind_value_move(rebind_value *v, rebind_value *o) {
    // try {
    //     return reinterpret_cast<rebind_value *>(new rebind::Value(
    //         *reinterpret_cast<rebind::Value const *>(v)));
    // } catch (...) {
    //     return nullptr;
    // }
}

// rebind_index rebind_value_index(rebind_value *v) {
    // rebind_index[16] REBINDC(variable_type)(rebind_value *v);
//     auto t = reinterpret_cast<rebind::Value const *>(v)->index();
//     static_assert(sizeof(t) == sizeof(rebind_index));
//     return reinterpret_cast<rebind_index const &>(t);
// }

char const * rebind_index_name(rebind_index v) {
    return rebind::raw::name(v).data();
}

/******************************************************************************/