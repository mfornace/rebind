#include <cpy/PythonAPI.h>

namespace cpy {

std::unordered_map<std::type_index, std::string> type_names = {
    {typeid(void),             "void"},
    {typeid(void *),           "pointer"},
    {typeid(bool),             "bool"},
    // {typeid(Integer),          "int"},
    {typeid(Real),             "float64"},
    {typeid(std::string_view), "str"},
    {typeid(std::string),      "str"},
    {typeid(std::type_index),  "TypeIndex"},
    {typeid(Binary),           "Binary"},
    {typeid(BinaryView),       "BinaryView"},
    {typeid(BinaryData),       "BinaryData"},
    {typeid(Function),         "Function"},
    {typeid(Value),            "Value"},
    {typeid(ValuePack),        "ValuePack"},
    {typeid(ArgPack),          "ArgPack"},
    {typeid(char),             "char"},
    {typeid(unsigned char),    "unsigned_char"},
    {typeid(signed char),      "signed_char"},
    {typeid(char16_t),         "char16_t"},
    {typeid(char32_t),         "char32_t"},
    {typeid(int),              "int32"},
    {typeid(float),            "float32"},
    {typeid(long double),      "long_double"},
    {typeid(std::uint8_t),     "uint8"},
    {typeid(std::uint16_t),    "uint16"},
    {typeid(std::uint32_t),    "uint32"},
    {typeid(std::uint64_t),    "uint64"},
    {typeid(std::int8_t),      "int8"},
    {typeid(std::int16_t),     "int16"},
    {typeid(std::int32_t),     "int32"},
    {typeid(std::int64_t),     "int64"}
};


Zip<std::string_view, std::type_index> Buffer::formats = {
    {"d", typeid(double)},
    {"f", typeid(float)},
    {"c", typeid(char)},
    {"b", typeid(signed char)},
    {"B", typeid(unsigned char)},
    {"?", typeid(bool)},
    {"h", typeid(short)},
    {"H", typeid(unsigned short)},
    {"i", typeid(int)},
    {"I", typeid(unsigned int)},
    {"l", typeid(long)},
    {"L", typeid(unsigned long)},
    {"q", typeid(long long)},
    {"Q", typeid(unsigned long long)},
    {"n", typeid(ssize_t)},
    {"s", typeid(char[])},
    {"p", typeid(char[])},
    {"N", typeid(size_t)},
    {"P", typeid(void *)}
};

#define CPY_TMP(C, T) {Scalar::C, typeid(T), sizeof(T)}

Zip<Scalar, std::type_index, unsigned> scalars = {
    CPY_TMP(Bool,         bool),
    CPY_TMP(Char,         char),
    CPY_TMP(SignedChar,   signed char),
    CPY_TMP(UnsignedChar, unsigned char),
    CPY_TMP(UnsignedChar, char16_t),
    CPY_TMP(UnsignedChar, char32_t),
    CPY_TMP(Unsigned,     std::uint8_t),
    CPY_TMP(Unsigned,     std::uint16_t),
    CPY_TMP(Unsigned,     std::uint32_t),
    CPY_TMP(Unsigned,     std::uint64_t),
    CPY_TMP(Signed,       std::int8_t),
    CPY_TMP(Signed,       std::int16_t),
    CPY_TMP(Signed,       std::int32_t),
    CPY_TMP(Signed,       std::int64_t),
    CPY_TMP(Float,        float),
    CPY_TMP(Float,        double),
    CPY_TMP(Float,        long double),
    CPY_TMP(Pointer,      void *),
};
#undef CPY_TMP

}