#pragma once

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
class ArrayData {
    void *ptr;
    std::type_info const *t;
    bool mut;

public:
    void const *pointer() const {return ptr;}
    bool mutate() const {return mut;}
    std::type_info const &type() const {return t ? *t : typeid(void);}

    ArrayData(void *p, std::type_info const *t, bool mut) : t(t), ptr(p), mut(mut) {}

    template <class T>
    ArrayData(T *t) : ArrayData(const_cast<std::remove_cv_t<T> *>(static_cast<T const *>(t)),
                                &typeid(std::remove_cv_t<T>), std::is_const_v<T>) {}

    template <class T>
    T * target() const {
        if (!mut && !std::is_const<T>::value) return nullptr;
        if (type() != typeid(std::remove_cv_t<T>)) return nullptr;
        return static_cast<T *>(ptr);
    }

    friend std::ostream & operator<<(std::ostream &os, ArrayData const &d) {
        if (!d.t) return os << "ArrayData(<empty>)";
        return os << "ArrayData(" << TypeIndex(*d.t, d.mut ? Const : Lvalue) << ")";
    }
};

/******************************************************************************/

struct ArrayView {
    ArrayData data;
    ArrayLayout layout;
    // to do ... multidimensional support e.g. for std::array<std::array<float, 4>, 4>?
    // I guess that can be done via cast afterwards
};

/******************************************************************************/

}