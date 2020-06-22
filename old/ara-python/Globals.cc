#include <ara-python/Common.h>

namespace ara::py {

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

#define ARA_TMP(C, T) {Scalar::C, fetch<T>(), sizeof(T) * CHAR_BIT}

std::vector<std::tuple<Scalar, Index, unsigned>> scalars = {
    ARA_TMP(Bool,         bool),
    ARA_TMP(Char,         char),
    ARA_TMP(SignedChar,   signed char),

    ARA_TMP(UnsignedChar, unsigned char),
    ARA_TMP(UnsignedChar, char16_t),
    ARA_TMP(UnsignedChar, char32_t),

    ARA_TMP(Unsigned,     std::uint8_t),
    ARA_TMP(Unsigned,     std::uint16_t),
    ARA_TMP(Unsigned,     std::uint32_t),
    ARA_TMP(Unsigned,     std::uint64_t),

    ARA_TMP(Unsigned,     unsigned short),
    ARA_TMP(Unsigned,     unsigned int),
    ARA_TMP(Unsigned,     unsigned long),
    ARA_TMP(Unsigned,     unsigned long long),

    ARA_TMP(Signed,       std::int8_t),
    ARA_TMP(Signed,       std::int16_t),
    ARA_TMP(Signed,       std::int32_t),
    ARA_TMP(Signed,       std::int64_t),

    ARA_TMP(Signed,       short),
    ARA_TMP(Signed,       int),
    ARA_TMP(Signed,       long),
    ARA_TMP(Signed,       long long),

    ARA_TMP(Float,        float),
    ARA_TMP(Float,        double),
    ARA_TMP(Float,        long double),
    ARA_TMP(Pointer,      void *),
};
#undef ARA_TMP

}
