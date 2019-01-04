
### Constructors

```c++
constexpr Variable() noexcept = default;

/// Reference type
template <class T, std::enable_if_t<!(std::is_same_v<std::decay_t<T>, T>), int> = 0>
Variable(Type<T>, typename SameType<T>::type t) noexcept;

/// Non-Reference type
template <class T, class ...Ts, std::enable_if_t<(std::is_same_v<std::decay_t<T>, T>), int> = 0>
Variable(Type<T>, Ts &&...ts);

template <class T, std::enable_if_t<!std::is_base_of_v<VariableData, no_qualifier<T>>, int> = 0>
Variable(T &&t);

Variable(Variable &&v) noexcept : VariableData(static_cast<VariableData const &>(v))
Variable(Variable const &v) : VariableData(static_cast<VariableData const &>(v))
Variable & operator=(Variable &&v) noexcept;
Variable & operator=(Variable const &v);
~Variable();
```

### Mutations

```c++
void reset();
void assign(Variable v);
```

### Descriptors

```c++
void const * data() const;
char const * name() const;
std::type_index type() const;
std::type_info const & info() const;
Qualifier qualifier() const;
ActionFunction action() const;
bool is_stack_type() const;

constexpr bool has_value() const;
explicit constexpr operator bool() const;
```

### Reference
```c++
Variable reference() &;
Variable reference() const &;
Variable reference() &&;
```

### Conversions
```c++
// request any type T by non-custom conversions
Variable request_variable(Dispatch &msg, std::type_index const, Qualifier q=Qualifier::V) const;

// request reference T by custom conversions
template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
std::remove_reference_t<T> *request(Dispatch &msg={}, Type<T> t={}) const;

template <class T, std::enable_if_t<!std::is_reference_v<T>, int> = 0>
std::optional<T> request(Dispatch &msg={}, Type<T> t={}) const;

// request non-reference T by custom conversions
template <class T>
T downcast(Dispatch &msg={}, Type<T> t={}) const;
```

```c++
// return pointer to target if it is trivially convertible to requested type
template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
std::remove_reference_t<T> *target(Type<T> t={}) const &;

// return pointer to target if it is trivially convertible to requested type
template <class T, std::enable_if_t<std::is_reference_v<T>, int> = 0>
std::remove_reference_t<T> *target(Type<T> t={}) &;
```


## Conversion

`Request` takes a `Variable`, tries to return the given type `T`, throws `msg.error(...)` if impossible.


```c++
template <class T, class SFINAE=void>
struct Request {
    T operator()(Variable const &r, Dispatch &msg);
};
```

`Response` takes a `Variable` tries to put a requested type `idx` in it, otherwise does nothing.

```c++
template <class T, class SFINAE=void>
struct Response {
    void operator()(Variable &out, T const &t, std::type_index idx);
};
```

`Renderer` modifies a `Document`.

```c++
template <class T, class SFINAE=void>
struct Renderer {
    void operator()(Document &doc);
};
```



`Request<T>` models `Reference, Dispatch & -> T` and attempts to return a `T` from a `Reference` and `Dispatch &`.

It could be changed to return `optional<T>`.

`Response<T>` models `Value &, T t, type_index -> void` and attempts to put a `T` into a `Value` with a given `type_index`.

Also, `Response<T>` models `Qualifier, T, type_index -> void *` and attempts to return a qualified reference to type `T`. It could be changed to return a `Reference`.

Instead of the above, we could always try to return a `Reference` and use the `Dispatch` storage for lifetime extension. Mostly semantics.

Could a `Function` return a `Reference`?

## Function binding

python binding, easy objective.

we want to wrap a function which is summed up in C++ as:

```
char const * function_name;
std::vector<char const *> keywords;
std::function<Value(Value)> function;
```

This means we need some storage space.

Either we keep functions in static storage
- we'd have to lookup by string, maybe not great...

Or we put the function into a `PyCFunctionObject` which is defined as:

```c++
typedef struct {
    PyObject_HEAD
    PyMethodDef *m_ml;     /* Description of the C function to call */
    PyObject    *m_self;   /* Passed as 'self' arg to the C func, can be NULL */
    PyObject    *m_module; /* The __module__ attribute, can be anything */
} PyCFunctionObject;
```

`PyMethodDef` has the function pointer, docstring, call flags.

So we would put our data into the `PyObject *m_self` part. This could be the `std::function` part.

This means `self` must be a `PyObject *`. In pybind11 looks like it's a capsule object. A capsule object appears to be anything, as long as it has a destructor.

Or we find another way to make a callable function. We could just make the function a class. I think this is probably easier... then we have to set the getset stuff as in nupack to expose the `__text_signature__`.

## Document

In C++, without necessary python inclusion, we define a document which more or less represents a module.

It is a map from names to free functions and classes.

- Document:
    - map of `String` to (`Function` or `Class`)
- Function: a simple immutable callable object
    - `Value(ArgPack)`
    - `Vector<String>` keywords (also types?)
- Class: an object containing named members and methods
    - map of `String` to (`Member` or `Method`)
- Member: provides access to a member given an instance of a C++ class
    - summarizable as `Value(Any &)`
- Value: a C++ type which is a variant of
    - `Any`
    - `Real`
    - `Integer`
    - ... other built-ins
- Method: same as function but passes `this` by reference as first argument
    - `Value(Any &, ArgPack)`
    - `Vector<String> keywords`

### Missing functionality in the above

- Nested classes
- Static functions
- Class static variables
- Namespaces

I am...not sure these are important details. Static functions are different
in C++ and Python. Without these inclusions the document is pretty flat I believe.

Yeah, I think these types are suspect as they can't be handled by many output
representations (such as JSON or whatever). I think it's better to just include
them as "nupack.State.static_var" for example. Nothing is really lost then. It's just scoping.

The one that is a bit different is `Method`. It could be written as a free function
taking this as the first argument. This would lose some information (that it's for a class, that it's a method).

I believe methods should also take the first variable by reference, whereas free functions don't.

OK methods are different.

Also it seems that class is not simply a std any. More interface than that.

It is possible to write the stuff in terms of any though... oh wait not really. Meh. It's probably better to roll our own.

I wonder, if we have a given interface of virtuals, then the Model<T> models it,
then we have derived class of Model<Derived<T>>, can we make the base class stuff work?

Probably. As long as the value is last in the class won't it work...?

You may be able to add these details via annotation, e.g. naming a function
`Vector.append` would automatically have Python put it as a class method.

### References

The above specification of `Function` takes all arguments by value. In the case of
built in values, that may be OK. For instance, the buffer is probably going to be
shared anyway so none of them are expensive to copy.

For built ins, it would however prevent functions like `int f(double &)` which modify
their inputs. From the point of view of python that's probably fine because these built-ins
are immutable anyway so calling the functions like that would be sort of dumb.

From the point of view of C++ in general, that is a little harder to justify. But
I think for built-ins it's an OK sacrifice to make.

However, the real problem is with classes. At the very least methods need to take the
this argument by mutable reference. Otherwise it's pretty unusable. Sure.

But beyond that, seems like functions that modify arguments should be allowed (?).
Hmm. It's true that this doesn't go well with networking/distributed functionality.

If need to modify, then a function can't be summarizable as taking just values.
One option:
    - argument is either `double`, `int`, `class`, or `class &`. This is pretty ugly.
    - argument is either `double`, `int`, `class &`. Prevents class conversions ...
    argument is either `double`, `int`, `shared_ptr<class>`. Pretty much same as reference right? Yes but allows storage of the instance without copy. class would have `shared_ptr` inside probably.

Question is, is it that bad to prevent modification?

- value: can't modify, and sometimes make copies instead of const &
- const &: can't modify, can't move
- &: can modify, can't move, can't put in const
- &&: I think basically the same as value

&:
- &: easily bind to & and const & functions
- can't call with && or const &
- & would be confusing if type conversion was applied (no diagnostic of "cannot bind to lvalue")
- for && functions, could be an extra copy instead of a move (wouldn't know if lvalue was intended to be moved)

const &:
- can call with & const & &&
- no moves possible
- no modifications possible

or maybe shared_ptr:
- easily bind to & and const &
- for &&, could possibly copy if refcount > 1 else move
- would be easy to wrap python things (have to think about GIL but...probably OK)
- can't call with const &. can't call directly with &&.

hmm, annoying but python just doesn't play very well with moves. for that purpose
it is better to just work with mutable objects, probably in the form of shared_ptr.

At this point our `Value` would look like:

- `ptrdiff_t`
- `double`
- `std::string`
- `std::vector<Value>`
- `std::shared_ptr<Data>`
- `std::shared_ptr<Class>`

Hmm, this is just sort of annoying. It just looks weird if the signature can take
built-ins by value only but not class. Though that is how python is.

I guess the choice has to be, how seamless do we want this to be. An immutable
implementation has its appeal, but it's not very similar to either C++ or Python.



### Collapsing the above.

Also member is not obvious. Can keep immutable for now though.
For right now method and member have some overlap too. And not sure why method has
to be scoped within class?

It is possible for the above that Member and Method could be specified at the top-level
instead of nested within Class:

```
auto cls = open_class("Foo", x);

def("Foo.dump", [](Foo &f) {return f.dump();});
def("Foo.length", [](Foo &f) {return f.length;});
```

Sure, this is a valid pattern of specification. But should the result be nested or not?

One advantage of not nesting methods is that a document of just members would be trivially separable
from a document of members and functions. Well actually I was going to split members off.

One advantage of nesting is then that the different parts of a document are more obvious.
One disadvantage of flat is that you'd be putting more reliance on the string formatting (i.e. split on '.').

### An immutable API

Consider the following set of C++ functions:

```c++
void mutate(A &a);
double measure(A const &a);
double take(A a, int);
```

The equivalent immutable API specifies:

```
A mutated(A a) {mutate(a); return a;}
double measure(A const &a); // as before
double take(A); // as before
```

Basically, in the immutable API every argument must be taken either
- by value
- by const reference.

Any conversions may be logically applied since this is the case. No references need to be bound.

This specifies the constraints of the "immutable function API".

### An abstract API corresponding to the immutable API

We need to wrap functions which take arguments by value or by reference.

Option 1: `std::any(Vector<std::any>)`

This is OK for the by value part -- we move in our values. No copies made.
However, it's not OK for the const & functions. If we move in our values,
they are lost to the outside world. If we copy in our values, we are making unnecessary copies.

Option 2: `std::any(Vector<std::any const &>)`

This is clearly OK for the const & functions. But for the value functions, need to make a copy inside
instead of a move.

Option 3: `std::any(Vector<std::any &>)`

In this case, the const & functions can bind, and the value functions can move. But, we can't call
the API with a const &. We would need to const_cast it off. And our & would be moved from.

This one seems to be the answer, but it does mean that if calling the API from C++, we would
need to always cast to lvalue and remember that:
- the function may move our input value. (if call by value).
- the function will not modify the input value in any other way. (no mutations).
- the function may not move or modify our input value. (if const &).

I think this is acceptable. We may want to store the function signature as runtime attributes in the future.

Also, obviously we can't work with `Vector<std::any>`. So, either we use `std::shared_ptr` or we
pass in raw pointer `Vector<std::any *>`. That depends on what we use in the Python implementation.

### Value implementation

From before we were thinking about a variant of builtins plus a class object. The class object can
be `std::any` now I think. We may want to think about just use `std::any` in general.

This is the next question to decide! 1) whether to use variant. 2) whether to use std::any or some
sort of unique_ptr/shared_ptr/roll-your-own interface. Not sure why I'd do the latter to be honest...

`std::any` does force copy constructible constraint but I think I'm OK with that. That is already
supposed to be in the Python API. Could write a move-only version in the future but probably not
that useful I think. Things that are not copyable from the Python POV should probably just be shared_ptr.
Not like unique_ptr is much help in Python anyway.

OK, for 2 we can use any, at least for now. Final question is variant. If no variant,
we just use any. in C++ signature, if we're looking for bool, we have to scan through possibilities
bool, int, uint, double. hmm.

Value (before) was basically
```c++
    std::monostate,
    bool,
    Integer,
    Real,
    std::string,
    std::string_view,
    Binary,
    std::any,
    Vector<Value>
```

If we only have `std::any`, hmm. The plus is that it's more generic. Choices like string and whatnot
are a little bit artificial. Some of these choices are driven by Python API. It also has a bit
smaller object size. It also has no ambiguity `any<double>` vs `double`.
The minus is that there is less constrained API. I would assume the copies and moves
 also have vcalls for the POD types. Actually no, just a common function pointer.

Protobuf version

```
double
float
int32
int64
uint32
uint64
sint32
sint64
int64s
fixed32
fixed64
sfixed32
sfixed64
bool
string
bytes
required: a well-formed message must have exactly one of this field.
optional: a well-formed message can have zero or one of this field (but not more than one).
repeated
```

Compared to my spec above, it lacks string_view, Binary, monostate, but adds optional and some integer types and float32.
It also adds unicode string. Of these optional seems most important to incorporate.

Gosh. I think I just like variant I don't know. If we moved it to `std::any` there's just almost no
API constraints. True that to the outside world, the distinctions are artificial. But they're
pretty fundamental in C++. Makes sense they should be handled separately I think.

Yes I think this is true...


### Python implementation

The above API wants all arguments as lvalue references (pointers in practice). We will have these.

In order to make this API work, we want a few methods:
```python
a.move_from(a2) # invalidates a2 and moves it into the any of a
a.copy_from(a2) # copies a2 into a
a.copy() # returns copy of the class
a.valid() # returns if anything is in the any
```

`__getattr__` always returns a value, not a reference.
`__setattr__` is just a mutating function so it's fine.
`a.b.c` involves copies -- possibly a bunch of them. we could think about getattr(a, 'b.c[0].d') or whatever.

There is really no possibility of mutation beyond the declared API, except that `__setattr__` may be
automated. `a.b.c = 0` is just impossible... unless you tried `setattr(a, 'b.c', 0)`. Certainly
`a.b.c.mutate()` would be impossible. You'd have to do at best `setattr(a, 'b.c', a.b.c.mutated())`.
Also would be interesting if you can look at REFCNT, or if you just always move by default.

I would lean towards moving by default but whatever. And I would think about conversions in general.
How permissible are they? Should they be registered in C++? That makes things quite complex so probably not.
On the other hand would be nice for base classes for example.

Also assuming our current implementation idea, I think we can just get the return std::any, make
the object, cast the type to one from the map of type_index.

### Members

```python
variable.member # property that returns a reference to the member with parent as a ward
variable.member = other_variable # setter that calls copy assign from other to member
variable.member.move_from(other_variable) # setter that calls move assign from other to member
variable.move_from(other_variable) # if variable is V, move_from,
```

### Hooks: how to build a document?

I think the easiest way is via the ADL functionality:

```c++
auto define(adl<Foo> x) {
    auto cls = open_class("Foo", x);
    cls("dump", {"self"}, &Foo::dump);
    cls("dump", {"self"}, &Foo::dump);
    return cls;
}
```

And from the top, `define` calls other `define`. Members for us can be done automatically.

Also possible to expose while writing C++ API. Above approach probably better.

### Move semantics

Rust has move by default. `let a = b` moves b into a. b is invalid afterwards.
Some types have Copy trait which lets it copy though.

Interesting idea but I'm still not sure what `x = y.a` should do with `y.a`.

Should it copy? Nurse? Move? I would rather not nurse. Copy is probably a more
sensible default. But then how to move? Unclear in Python semantics. Maybe just `y.move('a')`?
That would be reasonable. However, what about `return x.large_member.small_member`?
It's hard to avoid either a copy or a full shared lvalue reference...

One way would be... `getattr(x, 'large_member.small_member')` or `x.get('large_member', 'small_member')`
or something of that sort. Aside from a bit of typing that's not too bad...

## List of good pybind11 features

- possibly pypy
- static fields, methods (maybe)
- dynamic attributes

## Class export

Hmm. There are some good features in pybind11.

We want to make it easy to extend Python definitions. Example for `Message<T>`

```c++

template <class T>
void define(Module &mod, adl<Message<T>> x) {
    auto cls = mod.open_class(x, "Message");
    cls("size", {"self"}, [](Message<T> const &m) {
        return m.size();
    }, "returns the size of the message");
}

```

or maybe:


```c++

template <class T>
void define(adl<Message<T>> x) {
    auto cls = open_class("Message");
    cls("size", {"self"}, [](Message<T> const &m) {
        return m.size();
    }, "returns the size of the message");
    return cls;
}

```

Basically `define` would define the converter implementation.

This would then be retrievable at compile time so that the overloading could be done.

This would allow more of a document model. Define the behavior first, then
pass to the python binder.

The obvious drawback of the document model is that it's not very amenable to messing
with PyObjects and such directly, which might be required at some point (?).

And at some point the api is pretty bound to python, i.e. defining things like `__call__`. We could wrap some of these concepts (`::call()`). But there are also other things
like pickling, thread locks, ...



At least for nupack this defines a python class and possible C++ classes Message.

I think any class can derive from an `any`. Then maybe a `any with members`.

We probably have to store the functor, though not entirely sure where.

We have to decide how much of an intermediate layer there is.

for `std::list` it's more or less


```c++
template <class T>
void define(adl<std::list<T>> x) {
    to_tuple(x);
    from_tuple(x);
}

```

we can also give a struct hook but the above is probably easier for most uses.

we can think about move, const, qualifiers on the type (`x.move()`).

with the member functions we mostly want to erase type ASAP.

recursive definition? mostly just for classes. I...think it's probably good but not
completely sure. may be easier to do this stuff with struct, not sure.

so a lot of this looks like the current code. we can type erase more, and probably simplify too.

a lot of the functions look like in `cpy` with the argpack.

one thing I worry about is losing too much type information...

also how to build. have to think about relationship with `cpy`.

## Summary

`cpy` is a new unit testing library for C++ built on Python bindings. It combines
- a context-based API for running C++ tests, making assertions, and measuring execution times
- a test running and event handler package written in pure Python

These two sides are kept pretty modular; in particular, your C++ testing code doesn't need to know anything about (or include) any Python headers. In terms of API, `cpy` draws on ideas from `Catch` and `doctest` but tries to be less macro-based.

There are a lot of nice features in `cpy` including the abilities to:
- write test event handlers and CLIs in Python (easier than in C++)
- run tests in a threadsafe and parallel manner
- parametrize your tests either in C++, Python, or from the command line
- call tests from other tests within C++ or Python
- replace the given Python handlers with your own without any recompilation of your C++ code

The primary hurdles to using `cpy` are that it requires C++17 (most importantly for `std::variant`) and Python 2.7+/3.3+ for the Python reporters. The build can also be bit more tricky, though it's not as complicated as you might expect.

`cpy` is quite usable but also at a pretty early stage of development. Please try it out and give some feedback!


### `Value`
`Value` is a simple wrapper around a `std::variant` which gives a convenient API:

```c++
struct Value {
    Variant var; // a typedef of std::variant; see below
    bool                     as_bool()    const &;
    Integer                  as_integer() const &;
    Real                     as_real()    const &;
    Complex                  as_complex() const &;
    std::string_view         as_view()    const &;

    std::string              as_string()  const &; // also with signature && (rvalue)
    // Binary                   as_binary()  const &; // also with signature && (rvalue)
    std::any                 as_any()     const &; // also with signature && (rvalue)
    Vector<Value>            as_values()  const &; // also with signature && (rvalue)
};
```

This API is particularly suited for the common use case where you know the proper held type of `Value`. Mutating `as_*` functions are not given. Use the underlying member `var` if you need more advanced behavior.

Beyond providing this API, the `Value` wrapper explicitly instantiates some `std::variant` templates (constructors, destructor) in the `libcpy` library since `std::variant` can yield a lot of code size. The definition of `Value`  relies on these typedefs:

```c++
// Signed type, often 64 bits, used for signed and unsigned integers
using Integer = std::ptrdiff_t;
// Floating point type
using Real = double;
// Complex floating point type
using Complex = std::complex<double>;
// Container type
template <class T>
using Vector = std::vector<T>;
// The hard-coded std::variant
using Variant = std::variant<
    std::monostate, // proxy for void() or Python's None
    bool,
    Integer,
    Real,
    // Complex,
    std::string, // this usually has the highest sizeof() due to SSO
    std::string_view,
    Binary,
    std::any,
    Vector<Value> // C++17 ensures this is OK with forward declared Value
>;
```
Since `Variant` is hard-coded in `cpy`, it deserves some rationale:
- `std::monostate` is a useful default for null/optional concepts.
- `bool` is included since it is so commonly handled as a special case.
- `char` is not included since there is not much perceived gain over `Integer`.
- `std::size_t` and other unsigned types are not included for the same reason.
- `std::string_view` is convenient for allocation-less static duration strings, which are common in `cpy`.
- `std::vector` is adopted since it's common. It's hard to support custom allocators.

Next,
- `Complex` is excluded because although easy to support, it's very uncommonly used.
- `std::aligned_union`? I don't believe there is much point in this if `std::any` is included. Probably not worth it anyway
- `Binary` gives const view of contiguous data. Could be useful for passing images, arrays, etc. in as parameters.
- `std::any` is included, mostly only for calling C++ tests from C++ and user extensions.
- It is planned to allow user defined macros to change some of the defaults used.

Consider `std::monostate, bool, Integer, Real, std::string, std::string_view` as concept `Scalar`.

Purposes of `Value`:
- Efficient logging in `Context` without unnecessary conversions (applies to `Scalar` only I think). I can't think of much outside `Scalar` that this is true for. In other cases, C++ conversion to `string` is the fastest option (e.g. compared to conversion to `Vector<Value>`, conversion to `tuple`, conversion to `str`). This is true, it's not a huge performance issue but I like its simplicity.
- Parametrization of C++ tests from Python. `Scalar` is good here. At this point need at least `Vector<Value>` to hold common inputs. Possibly `Binary` or the like is useful too.
- Return values of C++ tests to Python. `Any` might be OK here. I'm not sure conversion to Python is useful for unit testing...
- Parametrization of C++ tests from C++. In this case it seems that `std::any` would be the best option for holding these values. Can probably check types when constructing the `std::any`. Otherwise there's a lot of extra machinery for converting a complicated class to and from `Value`. `Value` is not really type safe once it's being used for non-trivial conversions.
- Return values of C++ tests to C++. `std::any` makes sense here too I think. If it's hard to figure out return type, test has to be visible.

It seems that there are two use cases, one with `std::any` within C++, one with `Scalar` and maybe a couple other limited types for Python. This would indicate that `Value` should only be used for the test interaction and `std::any` should be used for most C++ to C++ operations. If that's true, the next question would be if `std::any` is useful within `Value`. I don't know if it's very common to pass test results to other tests in Python. Probably not. Would just do it directly in C++. Yeah... seems like `std::any` is not very useful to put in the `Value`.

It does seem OK to put vector and binary types in the `Value`. Even though they're not useful for logging, they're useful for parametrization and return values of tests. However, wouldn't we want `ToOutput` to translate a vector to a string for logging but to a vector for output? That might indicate that the logger can accept only `Scalar`...? Or we could keep it as one type and 1) enforce always going to a vector, which is slow but not wrong, or 2) put a bool in the signature for `ToOutput` to indicate whether it's just for logging. If we split the type, can't log the return value necessarily (unless it's one of `Scalar`).

##### Thoughts

I think `Complex` is dumb. No one uses this and the intent is just to give a POD type anyway besides this. Casting `float128` to this is still janky and not type safe.

`std::aligned_union` actually seems useful for conversion of POD types, both from Python and to skip the `std::any` indirection. I would call it `POD` or something. On the other hand it's not type safe either. And `std::any` has some stack space anyway. But it would eliminate some virtual calls probably. I think this is not so useful compared to `std::any`.

I would like `Binary` as `const` data with type-erased destructor I think. Then no unnecessary copies. Should we store a `std::type_index`? Should it be type-specific...I don't think so. Should it in the `std::any`? No, unless everything is.

Should everything be in a `std::any`?

Next, some related structs:
```c++
using ArgPack = Vector<Value>; // A runtime length list of arguments
struct KeyPair {std::string_view key; Value value;}; // A key value pair used in logging
using Logs = Vector<KeyPair>; // Keeps tracked of logged information in a Context
```

#### `ToOutput` and conversion of arbitrary types to `Value`
To make a Value from an arbitrary object, the following function is provided:
```c++
template <class T>
Value make_output(T &&t);
```

This functions does a compile-time lookup of `ToOutput<std::decay_t<T>`. If no such struct is defined, `cpy` converts the object to a string via something like the following. The compiler will then error if `operator<<` has no matches.
```c++
std::ostringstream os;
os << static_cast<T &&>(t);
Value v = os.str();
```

Otherwise, this implementation is assumed to be usable:
```
Value v = ToOutput<std::decay_t<T>>()(static_cast<T &&>(t));
```

`ToOutput` may be specialized as needed. Here are some examples:

```c++
// The declaration present in cpy
template <class T, class=void>
struct ToOutput;
// User example: define the default Value making operation for all objects
template <class T, class>
struct ToOutput {
    Value operator()(T const &t) const {...}
};
// User example: specialize a Value making operation for a specific type
template <>
struct ToOutput<my_type> {
    Value operator()(my_type const &t) const {return "my_type";}
};
// User example: specialize a Value making operation for a trait
template <class T>
struct ToOutput<T, std::enable_if_t<(my_trait<T>::value)> {
    Value operator()(T const &t) const {...}
};
```
Look up partial specialization, `std::enable_if`, and/or `std::void_t` for background on how this type of thing can be used.

#### `FromValue` and conversion from `Value`

The default behavior for casting to a C++ type from a `Value` can also be specialized. The relevant struct to specialize is declared like the following:
```c++
template <class T, class=void> // T, the type to convert to
struct FromValue {
    template <class U>
    bool check(U const &); // Return true if type T can be cast from type U

    template <class U>
    T operator()(U &u); // Return casted type T from type U if check() returns true
};
```


### To value
- option 1: leave the above undefined -- user can define a default.
- option 5: leave undefined -- make_output uses stream if it is undefined
- option 3: define it to be the stream operator. then the user has to override the default based on a void_t
- problem then is, e.g. for std::any -- user would have to use std::is_copyable -- but then ambiguous with all their other overloads.
- option 2: define but static_assert(false) in it -- that's bad I think
- option 4: define a void_t stream operator. but then very hard to override

### Canonical codes
- `new`: constructor
- `()`: class call operator

### FromValue specialization

Valid target types are `T`, `T &&`, `T const &`.
Input types are always `V &&` with `V` one of the variant types.

#### Case 0: a value type (like `float128`)
- `std::any(T)` -> `T`      (convertible to T, T const &, T &&)
- `std::any(T *)` -> `T &&` (convertible to T, T const &, T &&)
- `Real` -> `T`             (convertible to T, T const &, T &&)

#### Case 1: a container type (like `std::list<T>`)
- `std::any(T)` -> `T`      (convertible to T, T const &, T &&)
- `std::any(T *)` -> `T &&` (convertible to T, T const &, T &&)
- `Sequence` -> `T`         (convertible to T, T const &, T &&)

#### Case 2: a class that is convertible from another type
- `std::any(T)` -> `T`      (convertible to T, T const &, T &&)
- `std::any(T *)` -> `T &&` (convertible to T, T const &, T &&)
- `std::any(U)` -> `T`         (convertible to T, T const &, T &&)


### Implementing a base class mutating method
```python
a = b.slice(A.type()) # calls std::move() on the A slice of B, to make a new A
try:
    a.move_from(a.mutate()) # calls A mutator
finally:
    b.slice_from(a) # calls std::move of a into b
```
It's not pretty.

### Reference semantics
As the caller we can call our function with
- `T &`: we expect that our object can be mutated
- `T const &`: we do not expect that our object can be mutated
- `T &&`: we give up our object and don't care about it

As a function we may wish to be called with
- `T`: we don't want to modify the caller, but we do want to modify the input
- `T &`: we do want to modify both
- `T const &`: we don't want to modify either
- `T &&`: we take it

We can convert as follows:
- `T`: we can always convert to this
- `T &`: take `T &` or else make a copy and pass that instead
- `T const &`: we don't want to modify either
- `T &&`: take `T &&` or else make a copy and pass that instead

Mutate means

self goes in, self goes out
if an exception occurs it should be returned as well