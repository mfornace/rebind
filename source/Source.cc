#include <ara/Call.h>
#include <stdexcept>
#include <exception>
#include <string_view>

/******************************************************************************/

// static_assert(sizeof(std::exception_ptr) == 8);
// #if __has_include(<boost/core/demangle.hpp>)
// #   include <boost/core/demangle.hpp>
//     namespace ara::runtime {
//         std::string demangle(char const *s) {return boost::demangle(s);}

//         char const * unknown_exception_description() noexcept {
//             return abi::__cxa_current_exception_type()->name();
//         }
//     }

#if __has_include(<cxxabi.h>)
#   include <cxxabi.h>
    namespace ara {
        using namespace __cxxabiv1;

        std::string demangle(char const *s) {
            int status = 0;
            char * buff = __cxa_demangle(s, nullptr, nullptr, &status);
            if (!buff) return s;
            std::string out = buff;
            std::free(buff);
            return out;
        }

        char const *unknown_exception_description() noexcept {
            return abi::__cxa_current_exception_type()->name();
        }
    }
#else
    namespace ara {
        std::string demangle(char const *s) {return s;}

        char const *unknown_exception_description() noexcept {return "C++: unknown exception";}
    }
#endif

/******************************************************************************/

namespace ara {

/******************************************************************************/

template <class ...Ts>
std::string cat(Ts const &...ts) {
    std::stringstream ss;
    ((ss << ts), ...);
    return ss.str();
}

/******************************************************************************/

bool Debug = true;

void set_debug(bool debug) noexcept {Debug = debug;}
bool debug() noexcept {return Debug;}

/******************************************************************************/

void Target::set_current_exception() noexcept {
    try {
        emplace<std::exception_ptr>(std::current_exception());
        idx = Index::of<std::exception_ptr>();
    } catch (...) {
        out = nullptr;
    }
}

#warning "cleanup"
struct Exception : std::exception {
    Index idx;
    void *ptr;
    Exception(Index i, void *p) : idx(i), ptr(p) {}

    Exception(Exception const &) = delete;
    Exception &operator=(Exception const &) = delete;

    Exception(Exception &&e) noexcept : idx(std::exchange(e.idx, Index())), ptr(e.ptr) {}

    Exception &operator=(Exception &&e) noexcept {idx = std::exchange(e.idx, Index()); return *this;}

    ~Exception() {
        if (idx) Destruct::call(idx, ptr, Destruct::Heap);
    }
};

[[noreturn]] void Target::rethrow_exception() {
    if (idx == Index::of<std::exception_ptr>()) {
        std::exception_ptr &ptr = *reinterpret_cast<std::exception_ptr *>(out);
        auto exc = std::move(ptr);
        ptr.~exception_ptr();
        std::rethrow_exception(std::move(exc));
    } else {
        throw Exception{idx, out};
    }
}

Call::stat Call::wrong_number(Target &target, Code got, Code expected) noexcept {
    target.emplace<ara_input>(got, expected);
    return WrongNumber;
}

Call::stat Call::wrong_type(Target &target, Code n, Index i, Qualifier q) noexcept {
    // target.emplace<ara_index>(ara_tag_index(i, static_cast<ara_tag>(q)), n);
    return WrongType;
}

Call::stat Call::wrong_return(Target &target, Index i, Qualifier q) noexcept {
    target.emplace<ara_index>(ara_tag_index(i, static_cast<ara_tag>(q)));
    return WrongReturn;
}


[[noreturn]] void call_throw(Target &&target, Call::stat stat) {
    switch (stat) {
        case Call::Stack:   {throw CallError("Invalid call status: Stack", stat);}
        case Call::Heap:    {throw CallError("Invalid call status: Heap", stat);}
        case Call::None:    {throw CallError("Invalid call status: None", stat);}
        case Call::Const:   {throw CallError("Invalid call status: Const", stat);}
        case Call::Mutable: {throw CallError("Invalid call status: Mutable", stat);}

        case Call::Impossible:  {throw CallError("Impossible", stat);}
        case Call::WrongType:   {throw CallError("WrongType", stat);}
        case Call::WrongNumber: {throw CallError("WrongNumber", stat);}
        case Call::WrongReturn: {throw CallError("WrongReturn", stat);}
                // Postcondition failure
        case Call::OutOfMemory: {throw std::bad_alloc();}
        case Call::Exception: {target.rethrow_exception();}
    }
    throw std::runtime_error("very bad");
}

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
//         // Nothing to do; from_ref always fails
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

// void set_source(WrongType &err, std::type_info const &t, Value &&v) {
//     if (auto p = v.target<std::string>()) {
//         err.source = std::move(*p);
//     } else if (auto p = v.target<std::string_view>()) {
//         err.source = std::move(*p);
//     } else if (auto p = v.target<Index>()) {
//         err.source = raw::name(*p);
//     } else {
//         err.source = t.name();
//     }
// }

/******************************************************************************/

// void Schema::type(Index t, std::string_view s, Value &&data, Index table) {
//     DUMP("type ", t, s);
//     auto it = contents.try_emplace(std::string(s), Type<Vector<Index>>()).first;
//     if (auto p = it->second.target<Vector<Index>>()) {
//         p->emplace_back(table);
//         return;
//     }
//     throw std::runtime_error(cat("tried to declare a type on a non-type key (key=", s, ", type=", t, ")"));
// }

// Value & Schema::find_property(Index t, std::string_view name) {
//     DUMP("find_method ", t, name);
//     return const_cast<Table &>(*t).properties.emplace(name, nullptr).first->second;
// }

/******************************************************************************/

}

