#pragma once
#include "../Convert.h"

namespace rebind {

/******************************************************************************/

template <class B, class E>
bool array_major(B begin, E end) {
    for (std::ptrdiff_t expected = 1; begin != end; ++begin) {
        if (begin->first < 2) continue;
        if (begin->second != expected) return false;
        expected = begin->first * begin->second;
    }
    return true;
}

struct ArrayLayout {
    /// Stores a list of shape, stride pairs
    using value_type = std::pair<std::size_t, std::ptrdiff_t>;
    Vector<value_type> contents;

    ArrayLayout() = default;

    template <class T, class U>
    ArrayLayout(T const &shape, U const &stride) {
        if (std::size(shape) != std::size(stride)) throw std::invalid_argument("ArrayLayout() strides do not match shape");
        contents.reserve(std::size(shape));
        auto st = std::begin(stride);
        for (auto const &sh : shape) contents.emplace_back(sh, *st++);
    }

    // 1 dimensional contiguous array
    ArrayLayout(std::size_t n) : contents{value_type(n, 1)} {}

    friend std::ostream & operator<<(std::ostream &os, ArrayLayout const &l) {
        os << "ArrayLayout(" << l.depth() << "):" << std::endl;
        for (auto const &p : l.contents) os << p.first << ": " << p.second << " "; os << std::endl;
        return os;
    }

    /// Return stride of given dimension
    std::ptrdiff_t stride(std::size_t i) const {return contents[i].second;}

    /// Return shape[i] for a given dimension i
    std::size_t shape(std::size_t i) const {return contents[i].first;}

    /// Synonym of shape(): Return shape[i] for a given dimension i
    std::size_t operator[](std::size_t i) const {return contents[i].first;}

    bool column_major() const {return array_major(contents.begin(), contents.end());}
    bool row_major() const {return array_major(contents.rbegin(), contents.rend());}
    bool contiguous() const;

    /// Number of dimensions
    std::size_t depth() const {return contents.size();}

    /// Total number of elements
    std::size_t n_elem() const {
        std::size_t out = contents.empty() ? 0u : 1u;
        for (auto const &p : contents) out *= p.first;
        return out;
    }
};

/******************************************************************************/

/*
Binary data convenience wrapper for an array of POD data
 */
// class ArrayData {
//     void *ptr;
//     Index t;
//     bool mut;

// public:
//     void const *pointer() const {return ptr;}
//     bool mutate() const {return mut;}
//     std::type_info const &type() const {return t ? *t : typeid(void);}

//     ArrayData(void *p, std::type_info const *t, bool mut) : t(t), ptr(p), mut(mut) {}

//     template <class T>
//     ArrayData(T *t) : ArrayData(const_cast<std::remove_cv_t<T> *>(static_cast<T const *>(t)),
//                                 &typeid(std::remove_cv_t<T>), std::is_const_v<T>) {}

//     template <class T>
//     T * target() const {
//         if (!mut && !std::is_const<T>::value) return nullptr;
//         if (type() != typeid(std::remove_cv_t<T>)) return nullptr;
//         return static_cast<T *>(ptr);
//     }

//     friend std::ostream & operator<<(std::ostream &os, ArrayData const &d) {
//         if (!d.t) return os << "ArrayData()";
//         return os << "ArrayData(" << *d.t << QualifierSuffixes[(d.mut ? Const : Lvalue)] << ")";
//     }
// };

/******************************************************************************/

struct ArrayView {
    Ref data;
    ArrayLayout layout;
};

/******************************************************************************/


using Binary = std::basic_string<unsigned char>;

using BinaryView = std::basic_string_view<unsigned char>;

class BinaryData {
    unsigned char *m_begin=nullptr;
    unsigned char *m_end=nullptr;
public:
    constexpr BinaryData() = default;
    constexpr BinaryData(unsigned char *b, std::size_t n) : m_begin(b), m_end(b + n) {}
    constexpr auto begin() const {return m_begin;}
    constexpr auto data() const {return m_begin;}
    constexpr auto end() const {return m_end;}
    constexpr std::size_t size() const {return m_end - m_begin;}
    operator BinaryView() const {return {m_begin, size()};}
};

/******************************************************************************/

template <>
struct ToValue<BinaryData> {
    bool operator()(Value &v, BinaryData const &t) const {
        if (v.matches<BinaryView>()) return v.place_if<BinaryView>(t.begin(), t.size());
        return false;
    }
};

template <>
struct ToValue<BinaryView> {
    bool operator()(Value &, BinaryView const &v) const {return false;}
};

template <>
struct FromRef<BinaryView> {
    std::optional<BinaryView> operator()(Ref const &v, Scope &s) const {
        if (auto p = v.request<BinaryData>()) return BinaryView(p->data(), p->size());
        return s.error("not convertible to binary view", fetch<BinaryView>());
    }
};

template <>
struct FromRef<BinaryData> {
    std::optional<BinaryData> operator()(Ref const &v, Scope &s) const {
        return s.error("not convertible to binary data", fetch<BinaryData>());
    }
};

/******************************************************************************/

}