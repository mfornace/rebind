## Function prototype

- `Caller &&` caller
- `std::string_view` name
- `Ref` tag
- List of `Ref`: arguments

Any use case for `Value` being accepted as argument?
The difference would be between passing along `int &&` and passing along `int`
Probably better not to have `Value` be a possible argument then.
Slightly interesting that we could pass POD objects through refs though

Returns:
- `Ref`: reference to existing object
- Or `Value`: managed object

## `FromRef` for defining how to make your object from an opaque `Ref`

`T` is currently unqualified. There doesn't seem to be a use case for qualified `T`.

It is also unclear how there would be any use to `r` being a `Value`. The lifetimes involved are then trickier to handle.

```c++
template <class T, class SFINAE=void>
struct FromRef {
    /// Convert variable `r` into type `T`, log failures in Scope
    /// OUT is either T *, T const *, or std::optional<T>
    std::optional<T> operator()(Ref const &r, Scope &msg) const;

    /// Return if conversion MAY BE possible
    // bool operator()(Ref const &) const;
};
```

## `ToValue` for defining what `Value` your object can be converted to

Usually all we want is a `Value` output specified by `Index::of<T>`.
We might sometimes want a `Ref` output specified by `Index::of<T>, Qualifier`.

```c++
template <class T, class SFINAE=void> // T is unqualified
struct ToValue {

    bool operator()(Value &v, Index i, T const &) const;
    bool operator()(Value &v, Index i, T &&) const;
    bool operator()(Value &v, Index i, T &) const;

   /// Define these if needed, though it's often not necessary
    // bool operator()(Value &, T &) const;
    // bool operator()(Value &, T const &) const;
    // bool operator()(Value &, T &&) const;

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
    bool operator()(Ref &, Index i, T &) const;
    bool operator()(Ref &, Index i, T const &) const;
    bool operator()(Ref &, Index i, T &&) const;

    // cheap, no need
    // bool operator()(Index const &idx, T const &t, Qualifier q) const;
};
```