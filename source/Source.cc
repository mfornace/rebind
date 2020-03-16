#include <rebind/Document.h>
#include <stdexcept>
#include <string_view>

namespace rebind {

/******************************************************************************/

template <class ...Ts>
std::string cat(Ts const &...ts) {
    std::stringstream ss;
    ((ss << ts), ...);
    return ss.str();
}

/******************************************************************************/

std::function<std::string(char const *)> demangle;

bool Debug = true;

/******************************************************************************/

Document & document() noexcept {
    static Document static_document;
    return static_document;
}

void render_default(Document &, std::type_info const &t) {
    if (Debug) std::cout << "Not rendering type " << t.name() << std::endl;
}

/******************************************************************************/

// void lvalue_fails(Variable const &v, Scope &msg, Index t) {
//     char const *s = "could not convert to lvalue reference";
//     if (v.type() == t) {
//         if (v.qualifier() == Rvalue) s = "could not convert rvalue to lvalue reference";
//         if (v.qualifier() == Const) s = "could not convert const value to lvalue reference";
//         if (v.qualifier() == Value) s = "could not convert value to lvalue reference";
//     }
//     msg.error(s, t);
// }
// /******************************************************************************/

// void rvalue_fails(Variable const &v, Scope &msg, Index t) {
//     char const *s = "could not convert to rvalue reference";
//     if (v.type() == t) {
//         if (v.qualifier() == Lvalue) s = "could not convert lvalue to rvalue reference";
//         if (v.qualifier() == Const) s = "could not convert const value to rvalue reference";
//     }
//     msg.error(s, t);
// }
/******************************************************************************/

// void Variable::assign(Variable v) {
//     // if *this is a value, make a copy or move the right hand side in
//     if (qualifier() == Value) {
//         if (v.qualifier() == Value) {
//             *this = std::move(v);
//         } else {
//             DUMP("assign1");
//             if (auto p = handle())
//                 act(ActionType::destroy, pointer(), nullptr);
//             // Copy data but set the qualifier to Value
//             static_cast<VariableData &>(*this) = v;
//             idx.set_qualifier(Value);
//             // DUMP(name(), qualifier(), name(), stack, v.stack);
//             // DUMP(&v, v.pointer());
//             // e.g. value = lvalue which means
//             // Move variable if it held RValue
//             act((v.qualifier() == Rvalue) ? ActionType::move : ActionType::copy, v.pointer(), this);
//             // DUMP(stack, v.stack);
//         }
//     } else if (qualifier() == Const) {
//         DUMP("assign bad");
//         throw std::invalid_argument("Cannot assign to const Variable");
//     } else { // qual == Lvalue or Rvalue
//         DUMP("assigning reference", type(), &buff, pointer(), v.type());
//         // qual, type, etc are unchanged
//         act(ActionType::assign, pointer(), &v);
//         if (v.has_value())
//             throw std::invalid_argument("Could not coerce Variable to matching type");
//     }
// }

/******************************************************************************/

// Variable Variable::request_var(Scope &msg, Index const &t, Qualifier q) const {
//     DUMP((act != nullptr), " asking for ", t, "from", q, type());
//     Variable v;
//     if (!has_value()) {
//         // Nothing to do; from_pointer always fails
//     } else if (idx.matches(t)) { // Exact type match
//         // auto info = reinterpret_cast<std::type_info const * const &>(t);
//         if (t.qualifier() == Value) { // Make a copy or move
//             v.idx = t;
//             v.act = act;
//             v.stack = stack;
//             act((q == Rvalue) ? ActionType::move : ActionType::copy, pointer(), &v);
//         } else if (t.qualifier() == Const || t.qualifier() == q) { // Bind a reference
//             DUMP("yope", t, type(), q);
//             reinterpret_cast<void *&>(v.buff) = pointer();
//             v.idx = t;
//             v.act = act;
//             v.stack = stack;
//         } else {
//             DUMP("nope");
//             msg.error("Source and target qualifiers are not compatible");
//         }
//     } else {
//         ::new(static_cast<void *>(&v.buff)) RequestData{t, &msg, q};
//         act(ActionType::response, pointer(), &v);
//         // DUMP(v.has_value(), v.name(), q, v.qualifier());

//         if (!v.has_value()) {
//             DUMP("response returned no value");
//             msg.error("Did not respond with anything");
//         } else if (v.type() != t)  {
//             DUMP("response gave wrong type", v.type(), t);
//             msg.error("Did not respond with correct type");
//             v.reset();
//         }
//     }
//     DUMP(v.type(), t);
//     return v;
// }

/******************************************************************************/

void set_source(WrongType &err, std::type_info const &t, Value &&v) {
    if (auto p = v.target<std::string>()) {
        err.source = std::move(*p);
    } else if (auto p = v.target<std::string_view>()) {
        err.source = std::move(*p);
    } else if (auto p = v.target<Index>()) {
        err.source = p->name();
    } else {
        err.source = t.name();
    }
}

/******************************************************************************/

void Document::type(Index t, std::string_view s, Value &&data, Table table) {
    DUMP("type ", t, s);
    auto it = contents.try_emplace(std::string(s), Type<Vector<Table>>()).first;
    if (auto p = it->second.target<Vector<Table>>()) {
        p->emplace_back(table);
        return;
    }
    throw std::runtime_error(cat("tried to declare a type on a non-type key (key=", s, ", type=", t.name(), ")"));
}

template <class T>
T & remove_const(T const &t) {return const_cast<T &>(t);}

Function & Document::find_method(Index t, std::string_view name) {
    DUMP("find_method ", t, name);
    if (auto it = types.find(t); it != types.end()) {
        return remove_const(it->second->methods)[std::string(name)];
    } else {
        throw std::runtime_error(cat("tried to declare a method on an undeclared type (key=", name, ", type=", t.name(), ")"));
    }
}

Function & Document::find_function(std::string_view s) {
    DUMP("function ", s);
    auto it = contents.emplace(s, Type<Function>()).first;
    DUMP("emplaced ", s);
    DUMP(it->second.name(), bool(it->second));
    if (auto f = it->second.target<Function>()) return *f;
    DUMP("bad", s);
    throw std::runtime_error(cat(
        "tried to declare a function on a non-function key (key=",
        it->first, ", type=", it->second.name(), ")"));
}

/******************************************************************************/

}


// #if __has_include(<boost/core/demangle.hpp>)
// #   include <boost/core/demangle.hpp>
//     namespace rebind::runtime {
//         std::string demangle(char const *s) {return boost::demangle(s);}

//         char const * unknown_exception_description() noexcept {
//             return abi::__cxa_current_exception_type()->name();
//         }
//     }
#if __has_include(<cxxabi.h>)
#   include <cxxabi.h>

    namespace rebind::runtime {
        using namespace __cxxabiv1;

        std::string demangle(char const *s) {
            int status = 0;
            char * buff = __cxa_demangle(s, nullptr, nullptr, &status);
            if (!buff) return s;
            std::string out = buff;
            std::free(buff);
            return out;
        }

        char const * unknown_exception_description() noexcept {
            return abi::__cxa_current_exception_type()->name();
        }
    }
#else
    namespace rebind::runtime {
        std::string demangle(char const *s) {return s;}

        char const * unknown_exception_description() noexcept {return "C++: unknown exception";}
    }
#endif
