#include <cpy/Document.h>

namespace cpy {

bool Debug = false;

/******************************************************************************/

Document & document() noexcept {
    static Document static_document;
    return static_document;
}
/******************************************************************************/

void lvalue_fails(Variable const &v, Dispatch &msg, std::type_index t) {
    char const *s = "could not bind to lvalue reference";
    if (v.type() == t) {
        if (v.qualifier() == Qualifier::R) s = "could not convert rvalue to lvalue reference";
        if (v.qualifier() == Qualifier::C) s = "could not convert const value to lvalue reference";
        if (v.qualifier() == Qualifier::V) s = "could not convert value to lvalue reference";
    }
    msg.error(s, v.type(), t);
}
/******************************************************************************/

void rvalue_fails(Variable const &v, Dispatch &msg, std::type_index t) {
    char const *s = "could not bind to rvalue reference";
    if (v.type() == t) {
        if (v.qualifier() == Qualifier::L) s = "could not convert lvalue to rvalue reference";
        if (v.qualifier() == Qualifier::C) s = "could not convert const value to rvalue reference";
    }
    msg.error(s, v.type(), t);
}
/******************************************************************************/

void Variable::assign(Variable v) {
    DUMP("assigning!", qual, v.qual, name(), v.name());
    if (qual == Qualifier::V) {
        if (v.qual == Qualifier::V) {
            *this = std::move(v);
        } else {
            DUMP("assign1");
            // for now, just make a fresh value copy of the variable
            if (auto p = handle())
                act(ActionType::destroy, pointer(), nullptr);
            static_cast<VariableData &>(*this) = v;
            qual = Qualifier::V;
            DUMP(name(), qualifier(), name(), stack, v.stack);
            DUMP(&v, v.pointer());
            // e.g. value = lvalue which means
            act((v.qual == Qualifier::R) ? ActionType::move : ActionType::copy, v.pointer(), this);
            DUMP(stack, v.stack);
        }
    } else if (qual == Qualifier::C) {
        DUMP("assign bad");
        throw std::invalid_argument("Cannot assign to const Variable");
    } else { // qual == Qualifier::L or Qualifier::R
        DUMP("assigning reference", name(), &buff, pointer(), v.name());
        // qual, type, etc are unchanged
        act(ActionType::assign, pointer(), &v);
        if (v.has_value())
            throw std::invalid_argument("Could not coerce Variable to matching type");
    }
}

/******************************************************************************/

Variable Variable::request_variable(Dispatch &msg, std::type_index const t, Qualifier q) const {
    DUMP((act != nullptr), " asking for ", t.name(), q, "from", name(), qualifier());
    Variable v;
    if (t == type()) { // Exact type match
        auto info = reinterpret_cast<std::type_info const * const &>(t);
        if (q == Qualifier::V) { // Make a copy or move
            v.qual = Qualifier::V;
            v.idx = info;
            v.act = act;
            v.stack = stack;
            act((qual == Qualifier::R) ? ActionType::move : ActionType::copy, pointer(), &v);
        } else if (q == Qualifier::C || q == qual) { // Bind a reference
            DUMP("yope");
            reinterpret_cast<void *&>(v.buff) = pointer();
            v.idx = info;
            v.act = act;
            v.qual = q;
            v.stack = stack;
        } else {
            DUMP("nope");
            msg.error("Source and target qualifiers are not compatible");
        }
    } else {
        auto src = (qual == Qualifier::V) ? Qualifier::C : qual;
        DUMP(src);
        ::new(static_cast<void *>(&v.buff)) RequestData{t, &msg, src, q};
        act(ActionType::response, pointer(), &v);
        DUMP(v.has_value(), v.name(), q, v.qualifier());

        if (!v.has_value()) {
            DUMP("no value");
            msg.error("Did not respond with anything");
        } else if (v.type() != t)  {
            DUMP("wrong type", v.name(), t.name());
            msg.error("Did not respond with correct type");
            v.reset();
        } else if (v.qualifier() != q) {
            DUMP("wrong qualifier", q, v.qualifier());
            msg.error("Did not respond with correct qualifier");
            v.reset();
        }
    }
    DUMP(v.has_value(), v.type() == t, v.qualifier() == q, v.type().name(), q, t.name());
    auto v2 = v;
    return v;
}

/******************************************************************************/

TypeData & Document::type(std::type_index t, std::string s, Variable data) {
    auto it = contents.try_emplace(std::move(s), TypeData()).first;
    if (auto p = it->second.target<TypeData &>()) {
        p->data[t] = std::move(data);
        types[t] = &(*it);
        return *p;
    }
    DUMP(typeid(Type<decltype(it->second)>).name());
    DUMP(t.name(), " ", s, " ", data.name());
    DUMP(it->second.name(), it->second.qualifier());
    s.insert(0, "tried to declare both a non-type and a type for the same key ");
    throw std::runtime_error(std::move(s));
}

Function & Document::find_method(std::type_index t, std::string name) {
    if (auto it = types.find(t); it != types.end()) {
        if (auto p = it->second->second.target<TypeData &>())
            return p->methods.emplace(std::move(name), Function()).first->second;
        name.insert(0, "tried to declare a method ");
        name.insert(name.size(), "for a non-type key ");
        name += it->second->first;
        throw std::runtime_error(std::move(name));
    }
    name.insert(0, "tried to declare a method ");
    name.insert(name.size(), "for the undeclared type ");
    name.insert(name.size(), t.name());
    throw std::runtime_error(std::move(name));
}

Function & Document::find_function(std::string s) {
    auto it = contents.emplace(std::move(s), Type<Function>()).first;
    if (auto f = it->second.target<Function &>()) return *f;
    throw std::runtime_error("tried to declare both a non-function and a function for the same key " + it->first);
}

/******************************************************************************/

}
