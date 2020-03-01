
## `Request` for defining how to make your object from an opaque `Pointer`

```c++
template <class T, class SFINAE=void> // T is qualified
struct Request {
    /// Convert variable `r` into type `T`, log failures in Scope
    /// OUT is either T *, T const *, or std::optional<T>
    std::optional<T> operator()(Pointer const &, Scope &) const;

    /// Return if conversion MAY BE possible
    bool operator()(Pointer const &) const;
};
```


## `Response` for defining what `Value` your object can be converted to

```c++
template <class T, class SFINAE=void> // T is unqualified
struct Response {

    /// Convert `t` into type `(idx, Q)`, put it in the Value, and return if conversion took place
    Value operator()(TypeIndex const &, T &) const;
    Value operator()(TypeIndex const &, T const &) const;
    Value operator()(TypeIndex const &, T &&) const;

    /// Return if conversion is possible
    bool operator()(TypeIndex const &, Qualifier, T const &t) const;
};
```

<!-- ### Reference variant

```c++
template <class T, class SFINAE=void> // T is unqualified
struct RefResponse {

    /// Convert `t` into type `(idx, Q)`, put it in the Value, and return if conversion took place
    Pointer operator()(TypeIndex const &, Qualifier, T &) const;
    Pointer operator()(TypeIndex const &, Qualifier, T const &) const;
    Pointer operator()(TypeIndex const &, Qualifier, T &&) const;

    // cheap, no need
    // bool operator()(TypeIndex const &idx, T const &t, Qualifier q) const;
};
``` -->