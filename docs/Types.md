# `Table const *`
A lot like a vtable, set up at initialization

```c++
std::type_info const *info;
void (*destroy)(void *) noexcept;
bool (*relocate)(void *, unsigned short, unsigned short) noexcept;
void *(*copy)(void *);
void *(*request_reference)(void *, Qualifier, std::type_index);
void *(*request_value)(void *, Qualifier, std::type_index);
std::string name, const_name, lvalue_name, rvalue_name;
```

# `Pointer`

Non-owning struct containing:
- `Table const * const *` pointer to type vtable
- `void *` pointer to the referenced object
- `Qualifier` qualifier on the held reference

As the name suggests, pointer semantics are adopted, so copies and moves are trivial. A null pointer is supported.

Methods are provided to wrap vtable functionality including
- `index()`: return the type's `std::type_index`
- `name()`: return the type's name

# `Value`

Opaque type with value semantics:
```c++
Value(Value const &);
Value(Value &&) noexcept;
```

An empty value is supported.

# `Function`
```c++
Function();

template <class F>
Function(F &&f);

template <std::size_t N=0, class F>
static Function from(F &&f);

Function(Function const &);
Function(Function &&) noexcept;

Value operator()(ArgView const &) const;
```

# `RefFunction`

```c++
Pointer operator()(ArgView const &) const;
```