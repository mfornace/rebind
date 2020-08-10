#pragma once
#include "Raw.h"

namespace ara::py {

// #define REBIND_TMP(C, T) {Scalar::C, typeid(T), sizeof(T) * CHAR_BIT}

// Zip<Scalar, TypeIndex, unsigned> scalars = {
//     REBIND_TMP(Bool,         bool),
//     REBIND_TMP(Char,         char),
//     REBIND_TMP(SignedChar,   signed char),

//     REBIND_TMP(UnsignedChar, unsigned char),
//     REBIND_TMP(UnsignedChar, char16_t),
//     REBIND_TMP(UnsignedChar, char32_t),

//     REBIND_TMP(Unsigned,     std::uint8_t),
//     REBIND_TMP(Unsigned,     std::uint16_t),
//     REBIND_TMP(Unsigned,     std::uint32_t),
//     REBIND_TMP(Unsigned,     std::uint64_t),

//     REBIND_TMP(Unsigned,     unsigned short),
//     REBIND_TMP(Unsigned,     unsigned int),
//     REBIND_TMP(Unsigned,     unsigned long),
//     REBIND_TMP(Unsigned,     unsigned long long),

//     REBIND_TMP(Signed,       std::int8_t),
//     REBIND_TMP(Signed,       std::int16_t),
//     REBIND_TMP(Signed,       std::int32_t),
//     REBIND_TMP(Signed,       std::int64_t),

//     REBIND_TMP(Signed,       short),
//     REBIND_TMP(Signed,       int),
//     REBIND_TMP(Signed,       long),
//     REBIND_TMP(Signed,       long long),

//     REBIND_TMP(Float,        float),
//     REBIND_TMP(Float,        double),
//     REBIND_TMP(Float,        long double),
//     REBIND_TMP(Pointer,      void *),
// };
// #undef REBIND_TMP

/******************************************************************************/

/// type_index from PyBuffer format string (excludes constness)
Index buffer_format(std::string_view s) {
    if (s.size() == 1) switch (s[0]) {
        case 'd': return Index::of<double>();
        case 'f': return Index::of<float>();
        case 'c': return Index::of<char>();
        case 'b': return Index::of<signed char>();
        case 'B': return Index::of<unsigned char>();
        case '?': return Index::of<bool>();
        case 'h': return Index::of<short>();
        case 'H': return Index::of<unsigned short>();
        case 'i': return Index::of<int>();
        case 'I': return Index::of<unsigned int>();
        case 'l': return Index::of<long>();
        case 'L': return Index::of<unsigned long>();
        case 'q': return Index::of<long long>();
        case 'Q': return Index::of<unsigned long long>();
        case 'n': return Index::of<ssize_t>();
        case 's': return Index::of<char[]>();
        case 'p': return Index::of<char[]>();
        case 'N': return Index::of<size_t>();
        case 'P': return Index::of<void *>();
    }
    return Index::of<void>();
    // throw PythonError::type("unrecognized buffer format code: %s", s.data());
    // auto it = std::find_if(Buffer::formats.begin(), Buffer::formats.end(),
    //     [&](auto const &p) {return p.first == s;});
    // return it == Buffer::formats.end() ? typeid(void) : *it->second;
}

char const* buffer_string(Index i) {
    if (i == Index::of<double>()) return "d";
    if (i == Index::of<float>()) return "f";
    if (i == Index::of<char>()) return "c";
    if (i == Index::of<signed char>()) return "b";
    if (i == Index::of<unsigned char>()) return "B";
    if (i == Index::of<bool>()) return "?";
    if (i == Index::of<short>()) return "h";
    if (i == Index::of<unsigned short>()) return "H";
    if (i == Index::of<int>()) return "i";
    if (i == Index::of<unsigned int>()) return "I";
    if (i == Index::of<long>()) return "l";
    if (i == Index::of<unsigned long>()) return "L";
    if (i == Index::of<long long>()) return "q";
    if (i == Index::of<unsigned long long>()) return "Q";
    if (i == Index::of<ssize_t>()) return "n";
    if (i == Index::of<char[]>()) return "s";
    if (i == Index::of<char[]>()) return "p";
    if (i == Index::of<size_t>()) return "N";
    if (i == Index::of<void *>()) return "P";
    throw PythonError::type("unknown array data type");
}

// std::string_view Buffer::format(std::type_info const &t) {
//     auto it = std::find_if(Buffer::formats.begin(), Buffer::formats.end(),
//         [&](auto const &p) {return p.second == &t;});
//     return it == Buffer::formats.end() ? std::string_view() : it->first;
// }

// std::size_t Buffer::itemsize(std::type_info const &t) {
//     auto it = std::find_if(scalars.begin(), scalars.end(),
//         [&](auto const &p) {return std::get<1>(p) == t;});
//     return it == scalars.end() ? 0u : std::get<2>(*it) / CHAR_BIT;
// }

/******************************************************************************/

struct DeleteBuffer {
    void operator()(Py_buffer* p) const noexcept {
        PyBuffer_Release(p);
        delete p;
    }
};

std::unique_ptr<Py_buffer, DeleteBuffer> extract_buffer(Always<> o, int flags) {
    auto buff = std::make_unique<Py_buffer>();
    if (PyObject_GetBuffer(~o, buff.get(), flags) == 0) {
        return std::unique_ptr<Py_buffer, DeleteBuffer>(buff.release());
    } else {
        throw PythonError::type("C++: could not get buffer");
    }
}

std::unique_ptr<Py_buffer, DeleteBuffer> try_buffer(Always<> o, int flags) {
    if (PyObject_CheckBuffer(+o)) return extract_buffer(o, flags);
    return {};
}

// class Buffer {
//     // static Vector<std::pair<std::string_view, std::type_info const *>> formats;
//     bool valid;

// public:
//     Py_buffer view;

//     Buffer(Buffer const &) = delete;
//     Buffer(Buffer &&b) noexcept : view(b.view), valid(std::exchange(b.valid, false)) {}

//     Buffer & operator=(Buffer const &) = delete;
//     Buffer & operator=(Buffer &&b) noexcept {view = b.view; valid = std::exchange(b.valid, false); return *this;}

//     explicit operator bool() const {return valid;}

//     Buffer(PyObject *o, int flags) {
//         DUMP("before buffer", reference_count(o));
//         valid = PyObject_GetBuffer(o, &view, flags) == 0;
//         if (valid) DUMP("after buffer", reference_count(o), view.obj == o);
//     }

//     // static std::type_info const & format(std::string_view s);
//     // static std::string_view format(std::type_info const &t);
//     // static std::size_t itemsize(std::type_info const &t);
//     // static Binary binary(Py_buffer *view, std::size_t len);
//     // static Variable binary_view(Py_buffer *view, std::size_t len);

//     ~Buffer() {
//         if (valid) {
//             PyObject *o = view.obj;
//             // DUMP("before release", reference_count(view.obj));
//             PyBuffer_Release(&view);
//             // DUMP("after release", reference_count(o));
//         }
//     }
// };

/******************************************************************************/

}