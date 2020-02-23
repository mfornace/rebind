#include <rebind/Document.h>

#include <rebind-rust/Module.h>

// typedef rebind_variable rebind::Variable;
// using rebind_variable = rebind::Variable;

int rebind_add() {return 123;}

void rebind_destruct(rebind_variable *x) { // noexcept
    delete reinterpret_cast<rebind::Variable *>(x);
}

rebind_variable * rebind_variable_new() { // noexcept
    return reinterpret_cast<rebind_variable *>(new rebind::Variable(std::string()));
}

rebind_variable * rebind_variable_copy(rebind_variable *v) {
    try {
        return reinterpret_cast<rebind_variable *>(new rebind::Variable(
            *reinterpret_cast<rebind::Variable const *>(v)));
    } catch (...) {
        return nullptr;
    }
}

rebind_type_index rebind_variable_type(rebind_variable *v) {
    // rebind_type_index[16] rebind_variable_type(rebind_variable *v);
    auto t = reinterpret_cast<rebind::Variable const *>(v)->type();
    static_assert(sizeof(t) == sizeof(rebind_type_index));
    return reinterpret_cast<rebind_type_index const &>(t);
}

char const * rebind_type_index_name(rebind_type_index v) {
    return reinterpret_cast<rebind::TypeIndex const &>(v).info().name();
}
// TypeIndex rebind_variable_type(rebind_variable *v) {
//     return TypeIndex(); 
// }