#include <rebind-python/Common.h>

namespace rebind::py {

Object UnionType, TypeError;

std::unordered_map<Object, Object> type_translations{}, output_conversions{}, input_conversions{};

std::unordered_map<Index, std::pair<Object, Object>> python_types{};


void initialize_global_objects() {
    TypeError = {PyExc_TypeError, true};

    auto t = Object::from(PyImport_ImportModule("typing"));
    UnionType = Object::from(PyObject_GetAttrString(t, "Union"));
}

void clear_global_objects() {
    input_conversions.clear();
    output_conversions.clear();
    type_translations.clear();
    python_types.clear();
    UnionType = nullptr;
    TypeError = nullptr;
}

std::vector<std::pair<std::string_view, Index>> Buffer::formats = {
    {"d", fetch<double>()},
    {"f", fetch<float>()},
    {"c", fetch<char>()},
    {"b", fetch<signed char>()},
    {"B", fetch<unsigned char>()},
    {"?", fetch<bool>()},
    {"h", fetch<short>()},
    {"H", fetch<unsigned short>()},
    {"i", fetch<int>()},
    {"I", fetch<unsigned int>()},
    {"l", fetch<long>()},
    {"L", fetch<unsigned long>()},
    {"q", fetch<long long>()},
    {"Q", fetch<unsigned long long>()},
    {"n", fetch<ssize_t>()},
    {"s", fetch<char[]>()},
    {"p", fetch<char[]>()},
    {"N", fetch<size_t>()},
    {"P", fetch<void >()}
};

#define REBIND_TMP(C, T) {Scalar::C, fetch<T>(), sizeof(T) * CHAR_BIT}

std::vector<std::tuple<Scalar, Index, unsigned>> scalars = {
    REBIND_TMP(Bool,         bool),
    REBIND_TMP(Char,         char),
    REBIND_TMP(SignedChar,   signed char),

    REBIND_TMP(UnsignedChar, unsigned char),
    REBIND_TMP(UnsignedChar, char16_t),
    REBIND_TMP(UnsignedChar, char32_t),

    REBIND_TMP(Unsigned,     std::uint8_t),
    REBIND_TMP(Unsigned,     std::uint16_t),
    REBIND_TMP(Unsigned,     std::uint32_t),
    REBIND_TMP(Unsigned,     std::uint64_t),

    REBIND_TMP(Unsigned,     unsigned short),
    REBIND_TMP(Unsigned,     unsigned int),
    REBIND_TMP(Unsigned,     unsigned long),
    REBIND_TMP(Unsigned,     unsigned long long),

    REBIND_TMP(Signed,       std::int8_t),
    REBIND_TMP(Signed,       std::int16_t),
    REBIND_TMP(Signed,       std::int32_t),
    REBIND_TMP(Signed,       std::int64_t),

    REBIND_TMP(Signed,       short),
    REBIND_TMP(Signed,       int),
    REBIND_TMP(Signed,       long),
    REBIND_TMP(Signed,       long long),

    REBIND_TMP(Float,        float),
    REBIND_TMP(Float,        double),
    REBIND_TMP(Float,        long double),
    REBIND_TMP(Pointer,      void *),
};
#undef REBIND_TMP

}
