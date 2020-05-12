#pragma once
#include <ara/Core.h>
#include <vector>
#include <string>

namespace ara {


template <class T>
using Vector = std::vector<T>;

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

static_assert(!std::is_copy_constructible_v<ArrayView>);
static_assert(!is_copy_constructible_v<ArrayView>);
static_assert(std::is_nothrow_move_constructible_v<ArrayView>);

/******************************************************************************/

/******************************************************************************/

template <class R, class V>
R from_iters(V &&v) {return R(std::make_move_iterator(std::begin(v)), std::make_move_iterator(std::end(v)));}

template <class R, class V>
R from_iters(V const &v) {return R(std::begin(v), std::end(v));}

/******************************************************************************/

template <class T, class=void>
struct HasData : std::false_type {};

template <class T>
struct HasData<T, std::enable_if_t<(std::is_pointer_v<decltype(std::data(std::declval<T>()))>)>> : std::true_type {};

/******************************************************************************/

template <class T, class Iter1, class Iter2>
bool dump_range(Target &v, Iter1 b, Iter2 e) {
    // if (v.accepts<Sequence>()) {
    //     Sequence s;
    //     s.reserve(std::distance(b, e));
    //     for (; b != e; ++b) {
    //         if constexpr(std::is_same_v<T, Value>) s.emplace_back(*b);
    //         else if constexpr(!std::is_same_v<T, Ref>) s.emplace_back(Type<T>(), *b);
    //     }
    //     return v.set_if(std::move());
    // }
    // if (v.accepts<Vector<T>>()) {
    //     return v.emplace_if<Vector<T>>(b, e);
    // }
    return false;
}

template <class T>
struct DumpVector {
    using E = std::decay_t<typename T::value_type>;

    bool operator()(Target &v, T const &t) const {
        if (dump_range<E>(v, std::begin(t), std::end(t))) {
            return true;
        }
        // if constexpr(HasData<T const &>::value) {
        //     if (v.accepts<ArrayView>()) return v.emplace_if<ArrayView>(std::data(t), std::size(t));
        // }
        return false;
    }

    bool operator()(Target &v, T &t) const {
        if (dump_range<E>(v, std::cbegin(t), std::cend(t))) return true;
        // if constexpr(HasData<T &>::value)
        //     if (v.accepts<ArrayView>()) return v.emplace_if<ArrayView>(std::data(t), std::size(t));
        return false;
    }

    bool operator()(Target &v, T &&t) const {
        return dump_range<E>(v, std::make_move_iterator(std::begin(t)), std::make_move_iterator(std::end(t)));
    }
};

template <class V>
struct LoadVector {
    using T = std::decay_t<typename V::value_type>;

    template <class P>
    static std::optional<V> get(P &pack) {
        V out;
        out.reserve(pack.size());
        // s.indices.emplace_back(0);
        for (auto &x : pack) {
            if (auto p = std::move(x).load(Type<T>()))
                out.emplace_back(std::move(*p));
            else return {}; //s.error();
            // ++s.indices.back();
        }
        // s.indices.pop_back();
        return out;
    }

    std::optional<V> operator()(Ref &v) const {
        // if (auto p = v.load<Vector<T>>()) return get(*p, s);
        // if constexpr (!std::is_same_v<V, Sequence> && !std::is_same_v<T, Ref>)
        //     if (auto p = v.load<Sequence>()) return get(*p);
        return {}; //s.error("expected sequence", Index::of<V>());
    }
};

/******************************************************************************/

template <class T, class A>
struct Dumpable<std::vector<T, A>> : DumpVector<std::vector<T, A>> {};

template <class T, class A>
struct Loadable<std::vector<T, A>> : LoadVector<std::vector<T, A>> {};

/******************************************************************************/

}