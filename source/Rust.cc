#include <rebind/Document.h>

#include <rebind-rust/Module.h>

// typedef rebind_variable rebind::Variable;
// using rebind_variable = rebind::Variable;

int rebind_add() {return 123;}

void rebind_destruct(rebind_variable *x) { // noexcept
    delete reinterpret_cast<rebind::Variable *>(x);
}

rebind_variable * rebind_variable_new() { // noexcept
    return reinterpret_cast<rebind_variable *>(new rebind::Variable);
}

rebind_variable * rebind_variable_copy(rebind_variable *v) {
    try {
        return reinterpret_cast<rebind_variable *>(new rebind::Variable(
            *reinterpret_cast<rebind::Variable const *>(v)));
    } catch (...) {
        return nullptr;
    }
}