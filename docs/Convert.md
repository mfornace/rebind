
## `FromRef` for defining how to make your object from an opaque `Ref`

```c++
template <class T, class SFINAE=void> // T is qualified
struct FromRef {
    /// Convert variable `r` into type `T`, log failures in Scope
    /// OUT is either T *, T const *, or std::optional<T>
    std::optional<T> operator()(Ref const &, Scope &) const;

    /// Return if conversion MAY BE possible
    bool operator()(Ref const &) const;
};
```


## `ToValue` for defining what `Value` your object can be converted to

```c++
template <class T, class SFINAE=void> // T is unqualified
struct ToValue {
   /// Define these if needed, though it's often not necessary
    bool operator()(Value &, T &) const;
    bool operator()(Value &, T const &) const;
    bool operator()(Value &, T &&) const;

    /// Return if conversion is possible
    // bool operator()(Index const &, Qualifier, T const &t) const;
};
```

### Reference variant

Names: Ref, Reference, Handle

```c++
template <class T, class SFINAE=void> // T is unqualified
struct ToRef {

    /// Convert `t` into type `(idx, Q)`, put it in the Value, and return if conversion took place
    bool operator()(Ref &, T &) const;
    bool operator()(Ref &, T const &) const;
    bool operator()(Ref &, T &&) const;

    // cheap, no need
    // bool operator()(Index const &idx, T const &t, Qualifier q) const;
};
```