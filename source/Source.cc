#include <sfb/Call.h>
#include <sfb/Error.h>
#include <stdexcept>
#include <fstream>
#include <iostream>
#include <sfb/Core.h>

/******************************************************************************/

extern "C" {

SFB_DEFINE(void,      void);
SFB_DEFINE(cpp_bool,  bool);
SFB_DEFINE(char,      char);
SFB_DEFINE(uchar,     unsigned char);
SFB_DEFINE(int,       int);
SFB_DEFINE(long,      long);
SFB_DEFINE(longlong,  long long);
SFB_DEFINE(ulonglong, unsigned long long);
SFB_DEFINE(unsigned,  unsigned);
SFB_DEFINE(float,     float);
SFB_DEFINE(double,    double);

// SFB_DEFINE_str() {return }
// sfb::Switch<TYPE>::call

SFB_DEFINE(bool,   sfb::Bool);
SFB_DEFINE(str,    sfb::Str);
SFB_DEFINE(bin,    sfb::Bin);
SFB_DEFINE(string, sfb::String);
SFB_DEFINE(binary, sfb::Binary);
SFB_DEFINE(span,   sfb::Span);
SFB_DEFINE(array,  sfb::Array);
SFB_DEFINE(tuple,  sfb::Tuple);
SFB_DEFINE(view,   sfb::View);
SFB_DEFINE(index,  sfb::Index);

}

// static_assert(sizeof(std::exception_ptr) == 8);
// #if __has_include(<boost/core/demangle.hpp>)
// #   include <boost/core/demangle.hpp>
//     namespace sfb::runtime {
//         std::string demangle(char const *s) {return boost::demangle(s);}

//         char const * unknown_exception_description() noexcept {
//             return abi::__cxa_current_exception_type()->name();
//         }
//     }

#if __has_include(<cxxabi.h>)
#   include <cxxabi.h>
    namespace sfb {
        struct demangle_raii {
            char *buff;
            ~demangle_raii() {std::free(buff);}
        };

        std::string demangle(char const* name) {
            // if (t == typeid(std::string)) return "std::string";
            using namespace __cxxabiv1;
            int status = 0;
            char* buff = __cxa_demangle(name, nullptr, nullptr, &status);
            if (!buff) return name;
            demangle_raii guard{buff};
            std::string out = buff;
            while (true) {
                auto pos = out.rfind("::__1::");
                if (pos == std::string::npos) break;
                out.erase(pos, 5);
            }
            return out;
        }

        char const* unknown_exception_description() noexcept {
            return abi::__cxa_current_exception_type()->name();
        }
    }
#else
    namespace sfb {
        std::string demangle(char const *s) {return s;}

        char const *unknown_exception_description() noexcept {return "C++: unknown exception";}
    }
#endif

/******************************************************************************/

namespace sfb {

int test_address = (std::cout << "Index address " << reinterpret_cast<void*>(Index::of<Index>().base) << " " << reinterpret_cast<void*>(&sfb_define_index) << std::endl, 0);

/******************************************************************************/

template <class ...Ts>
std::string cat(Ts const &...ts) {
    std::stringstream ss;
    ((ss << ts), ...);
    return ss.str();
}

/******************************************************************************/

// std::shared_ptr<std::ostream> debug_stream = std::make_shared<std::ofstream>("debug.log");
std::shared_ptr<std::ostream> debug_stream = std::shared_ptr<std::ostream>(&std::cout, Ignore());

void set_debug_stream(std::string_view s) {
    if (s.empty()) {
        debug_stream.reset();
    } else if (s == "stderr") {
        debug_stream = std::shared_ptr<std::ostream>(&std::cerr, Ignore());
    } else if (s == "stdout") {
        debug_stream = std::shared_ptr<std::ostream>(&std::cout, Ignore());
    } else {
        debug_stream = std::make_shared<std::ofstream>(std::string(s));
    }
}
// bool debug() noexcept {return Debug;}

/******************************************************************************/

void Target::set_current_exception() noexcept {
    try {
        construct<std::exception_ptr>(std::current_exception());
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
    DUMP("Call::wrong_number", "got=", got, "expected=", expected);
    target.construct<sfb_input>(got, expected);
    return WrongNumber;
}

Call::stat Call::wrong_type(Target &target, Code n, Index i, Qualifier q) noexcept {
    DUMP("Call::wrong_type", "arg=", n, "expected=", i.name(), "qualifier=", q);
    // target.construct<sfb_index>(sfb_mode_index(i, static_cast<sfb_mode>(q)), n);
    return WrongType;
}

Call::stat Call::wrong_return(Target &target, Index i, Qualifier q) noexcept {
    target.construct<sfb_index>(sfb_mode_index(i, static_cast<sfb_mode>(q)));
    return WrongReturn;
}


[[noreturn]] void call_throw(Target &&target, Call::stat stat) {
    switch (stat) {
        case Call::Stack:   {throw InvalidStatus("Call: InvalidStatus: Stack", stat);}
        case Call::Heap:    {throw InvalidStatus("Call: InvalidStatus: Heap", stat);}
        case Call::None:    {throw InvalidStatus("Call: InvalidStatus: None", stat);}
        case Call::Read:    {throw InvalidStatus("Call: InvalidStatus: Read", stat);}
        case Call::Write:   {throw InvalidStatus("Call: InvalidStatus: Write", stat);}
        case Call::Index2:  {throw InvalidStatus("Call: InvalidStatus: Index", stat);}

        case Call::Impossible:  {throw NotImplemented("Call: Impossible", stat);}
        case Call::WrongType:   {throw WrongType("WrongType");}
        case Call::WrongNumber: {
            auto const &i = *static_cast<sfb_input const *>(target.output());
            throw WrongNumber(i.code, i.tag);
        }
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
//         if (v.qualifier() == Read) s = "could not convert const value to lvalue reference";
//         if (v.qualifier() == Value) s = "could not convert value to lvalue reference";
//     }
//     msg.error(s, t);
// }

/******************************************************************************/

std::ostream& operator<<(std::ostream& os, Bool const &b) {
    return os << "Bool(" << bool(b) << ")";
}

std::ostream& operator<<(std::ostream& os, Mode m) {
    switch (m) {
        case Mode::Read: return os << "read";
        case Mode::Write: return os << "write";
        case Mode::Stack: return os << "stack";
        case Mode::Heap: return os << "heap";
        default: return os << "<invalid Mode>";
    }
}

std::ostream& operator<<(std::ostream& os, Qualifier q) {
    switch (q) {
        case Qualifier::C: return os << "const";
        case Qualifier::L: return os << "lvalue";
        case Qualifier::R: return os << "rvalue";
        default: return os << "<invalid Qualifier>";
    }
}

std::ostream& operator<<(std::ostream& os, Str const &t) {
    return os << std::string_view(t);
}

std::ostream& operator<<(std::ostream& os, Bin const &) {
    return os << "Bin";
}

std::ostream& operator<<(std::ostream& os, String const &t) {
    return os << std::string_view(t);
}

std::ostream& operator<<(std::ostream& os, Binary const &) {
    return os << "Binary";
}

std::ostream& operator<<(std::ostream& os, Span const &t) {
    return os << "Span(" << t.shape() << ")";
}

std::ostream& operator<<(std::ostream& os, Array const &t) {
    return os << "Array(" << t.span().shape() << ")";
}

std::ostream& operator<<(std::ostream& os, Tuple const &t) {
    return os << "Tuple(" << t.view().shape() << ")";
}

std::ostream& operator<<(std::ostream& os, View const &t) {
    return os << "View(" << t.shape() << ")";
}

std::ostream& operator<<(std::ostream& os, Shape const &t) {
    os << "Shape(";
    for (auto i : t) os << i << ",";
    os << ")";
    return os;
}


// void rvalue_fails(Variable const &v, Scope &msg, Index t) {
//     char const *s = "could not convert to rvalue reference";
//     if (v.type() == t) {
//         if (v.qualifier() == Lvalue) s = "could not convert lvalue to rvalue reference";
//         if (v.qualifier() == Read) s = "could not convert const value to rvalue reference";
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
//     } else if (qualifier() == Read) {
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
//         } else if (t.qualifier() == Read || t.qualifier() == q) { // Bind a reference
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

