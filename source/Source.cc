#include <ara/Call.h>
#include <ara/Error.h>
#include <stdexcept>
#include <ara/Core.h>

/******************************************************************************/

extern "C" {

ARA_DEFINE(void, void);
ARA_DEFINE(bool, bool);
ARA_DEFINE(char, char);
ARA_DEFINE(uchar, unsigned char);
ARA_DEFINE(int, int);
ARA_DEFINE(long, long);
ARA_DEFINE(longlong, long long);
ARA_DEFINE(ulonglong, unsigned long long);
ARA_DEFINE(unsigned, unsigned);
ARA_DEFINE(float, float);
ARA_DEFINE(double, double);

// ara_define_str() {return }
// ara::Switch<TYPE>::call

ARA_DEFINE(str,    ara::Str);
ARA_DEFINE(bin,    ara::Bin);
ARA_DEFINE(string, ara::String);
ARA_DEFINE(binary, ara::Binary);
ARA_DEFINE(span,   ara::Span);
ARA_DEFINE(array,  ara::Array);
ARA_DEFINE(tuple,  ara::Tuple);
ARA_DEFINE(view,   ara::View);

}

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
        c.index = Index::of<std::exception_ptr>();
    } catch (...) {
        c.output = nullptr;
    }
}

[[noreturn]] void Target::rethrow_exception() {
    if (index() == Index::of<std::exception_ptr>()) {
        std::exception_ptr &ptr = *reinterpret_cast<std::exception_ptr *>(output());
        auto exc = std::move(ptr);
        ptr.~exception_ptr();
        std::rethrow_exception(std::move(exc));
    } else {
        throw Failure{index(), output()};
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
        case Call::Stack:   {throw InvalidStatus("Call: InvalidStatus: Stack", stat);}
        case Call::Heap:    {throw InvalidStatus("Call: InvalidStatus: Heap", stat);}
        case Call::None:    {throw InvalidStatus("Call: InvalidStatus: None", stat);}
        case Call::Const:   {throw InvalidStatus("Call: InvalidStatus: Const", stat);}
        case Call::Mutable: {throw InvalidStatus("Call: InvalidStatus: Mutable", stat);}

        case Call::Impossible:  {throw NotImplemented("Call: Impossible", stat);}
        case Call::WrongType:   {throw WrongType("WrongType");}
        case Call::WrongNumber: {throw WrongNumber(1, 2);}
        case Call::WrongReturn: {throw WrongReturn("WrongReturn");}
                // Postcondition failure
        case Call::OutOfMemory: {throw std::bad_alloc();}
        case Call::Exception: {target.rethrow_exception();}
    }
    throw InvalidStatus("Call: InvalidStatus: Unknown", stat);
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

