

# `Pointer`

Non-owning struct containing:
- `Table const *` pointer to type vtable
- `void *` pointer to the referenced object
- `Qualifier` qualifier on the held reference

As the name suggests, pointer semantics are adopted, so copies and moves are trivial. A null pointer is supported.

Methods are provided to wrap vtable functionality including
- `index()`: return the type's `std::type_index`
- `name()`: return the type's name

# `Value`

Wrapper around `Pointer` allowing for value semantics:
```c++
Value(Value const &);
Value(Value &&) noexcept;
```

An empty value is supported.

# `Function`
```
Value operator()(PointerVec const &) const;
Pointer reference(PointerVec const &) const;
```