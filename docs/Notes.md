## Current questions

### Runtime type information?

i.e. it would be nice to request a `Array` which is

- number of dimensions `M`
- exactly shape `N1, N2 ... NM`
- exactly of type `T`

or a `Tuple` which is:

- length `N`
- of types `T1, T2, ... TN`

Otherwise in Python we could also use run time type `Index, void*`.
But I would say honestly this is not really that useful compared to the current approach.

Because we take an optimistic approach, there is not a logical difference to doing `Array` vs `Array<T>` because we always assume our type can be convertible. The main advantage is for eliminating temporaries.

That is `std::vector<int> -> std::vector<double>` can go straight to:
- `Array<double>` -- this actually doesn't have much advantage.
- `Span<double>` aliasing the vector's storage. this would eliminate the temporary allocation.

(This is sort of a dumb example...would be more useful for structured types.)

I would say constraining the length of `Array` is a pretty minor gain. On the other hand constraining the rank may be moderately useful. Constraining the type is also only a minor gain.

Hmm ... seems actually like most of the constraints are not that useful given the optimistic assumptions.

The main thing which would be nice is to be able to do `std::pair<int, double> -> rust::pair<int, double>` without doing a whole allocation cycle through `Tuple`.

Another nicety would be to be able to reuse allocations within `Array` ... mm but this is pretty hard to be useful if structured types are used, because the destructor has to be run.

With these objectives, seems like maybe a good approach is just to enable the following:

```c++
ref = std::pair<std::string, unsigned>();
std::array<Ref, 2> key_value = ref.splat<2>();
std::pair<std::string, int> out;
key_value[0].get_to(out.first);
key_value[1].get_to(out.second);
```

There are problems here. Mainly, the storage of key_value is unclear if it's supposed to give an rvalue.

Well, if `ref` is an rvalue, it has control over the destruction of its value.
Let's say `ref` contains `std::string`.
We know this allocation is on `char[]`-like storage because of the destructor use...well do we?
If we do, then we could just give the reference to `key_value` and `key_value` will delete stuff appropriately.
Problem is...we really don't know this because of `heap`.

An easier option is maybe to allow

```c++
ref.splat<std::string, int>();
```

But this is actually sort of hard too. You'd be destructively moving out of a `std::pair` which is not really allowed.

Hmm. How hard is it to get the destructive move to work?
- Let's say we have `std::pair<std::string, int>` in `Ref` with `stack`.
- Because it's `stack` we know we're actually sitting on `char[]` storage. Otherwise we couldn't have made `stack`.
- So if we change `ref.index` to `null`, no destructor will be run automatically.
- So we could do:

```c++
auto splat(Ref &ref, std::pair<std::string, int> &self) noexcept {
    // if not aggregate-like like pair, have to cast to something that does not run any destructor except members
    if (ref.mode() == stack) {
        std::array<Ref, 2> refs{self.first, self.second}; // pseudocode
        ref.index = nullptr;
        return refs;
    }
    if (ref.mode() == heap) {
        // here this is difficult. I think we just have to change the allocation so it's done with storage. then it's fine.
        std::array<Ref, 2> refs{self.first, self.second};
        ref.index = Index::of<storage_like<self>>(); // so deallocation occurs.
    }
    if (ref.mode() == other) return lvalue refs;
}
```

The key is that the number of `Ref`s must exactly replicate the type, otherwise it's bad news. (Could destruct trailing members, but this seems...not obviously a good idea.)

I think this is actually very feasible. What about for a Python `tuple` type thing?
First, we really wouldn't need it, because map already going through array currently.

Pyhton objects can go to lvalue easily so this actually not too hard.

## Avoiding rvalue references

there are 5 possible tags: 0=const, 1=lvalue, 2=rvalue, 3=heap, 4=stack
easy to create each possibility from C++
to invoke a C++ specific function we'd cast rvalue and value to the same thing

not obvious how to create an rvalue tag from rust
for rust we probably have to move the value in which means, likely, allocating
what happens if we call rust with an rvalue tag...?
so probably we won't be able to handle it except by invoking C++ and getting a new copy

well actually in rust we could do the following:
```rust
// stack version:
fn(..., T t) {
    let t = ManualDrop::new(t); // t will not be destructed automatically now.
    let v = Value{ ptr: &t, type: (T, Temporary) }
    // ... work with v ... possibly call its destructor early
    // v is destructed, so the destructor for "t" is run, but *not* the deleter
}
fn(..., T t) {
    let t = Box::new(t);
    let v = Value{ ptr: t.into_raw(), type: (T, heap) }
    ... work with v ...
    v is destructed, so the deleter for "t" is run
}
```

Similar in C++:
```c++
template <class T>
union ManualDestructor {T value;}

void f(..., T &&t) {
    ManualDrop<T> t0;
    new(&t0.value) T(std::move(t));
    Value v{p, (T, Temporary)};
    ... work with v ... possibly call its destructor early
}
// heap version
void f(..., T t) {
    auto p = new T(t);
    Value v{p, (T, heap)}
    ... work with v ...
    p is deleted now.
}
```

there's no sense taking a T&& which is not move constructible so that should be fine.
the only overhead is in the move construction at the beginning, which might be
optimized and should be assumed pretty negligible anyway.
in this case, there are no rvalue references needed!

## Load, Dump

`Load` takes a reference and returns a `std::optional<T>`. The reference could be
- `Write`
- `Read`
- `Temporary`
- `None`

Example:
`Write<string>` could convert to `string &`, `string const &`, `string_view`, `mutable_string_view`.
`Read<string>` could convert to `string const &`, `string_view`.
`Temporary<string>` could convert to `std::vector<char>`



`Dump` takes an object of type `T` and emplaces a value:
- `optional<string> &` could convert to `string &` or `string const &` or `string`
- `optional<string> const &` could convert to `string const &` or `string`
- `optional<string> &&` could convert to `string`

Note that allocation is avoided in `Dump` because `Dump` is called by `Load` which can allocate space on the stack ahead of time.

Function call
`Call` takes sequence of references which are `Write`, `Read`, or `Temporary`.

It has to be able to return
- `Write`: a mutable reference
- `Read`: a const reference
- `None`: nothing
- `Exception`: an exception
- `Value`: a value

The first three are easy.
In C++ `Exception` is easy via `exception_ptr` (same size as pointer).
In Rust I think we'd have to return `Result<T>` which is either T or `Box<dyn ...>`, latter is 16 bytes.

It is possible we could allocate space in advance. that would indicate we're doing basically
```c++
stat f(storage &, index, function) {
    return index.request(storage, Ref::temporary(function()));
}
```

One issue in the above is the exception thing, we don't necessarily have space for it...although I guess we can heap allocate in that case. Then storage just needs to be at least ... 16 ... for destructor plus pointer.

Or we can accept we don't know what comes out, which may put on heap:
```c++
stat f(value &, function) {
    value.emplace(function());
}
```

here we'd be fine for reference, exception (I think), none, just value is a little suboptimal if it doesn't fit.

we could put in a runtime type deal:

```c++
stat f(index &, void *storage, size, function) { // I think alignment we just assume void * and otherwise allocate.
    if (O fits in storage) {
        new(storage) O(function());
    } else {
        new (storage) (void *)(new O(function()));
    }
    index = O;
}
```

then we don't deal with Value as a cross-language concept.

```c++
// Call function, cast to out index, storing the result in storage
template <class F>
stat call(Index out, void *storage, void const *self, ArgView args) {
    Out return_value = invoke(static_cast<F const *>(self), args);
    // Response<decltype(return_value)>()(return_value);
    // auto &storage = static_cast<aligned_storage<Out>>()
    // return out.load(return_value, storage)
    // or
    return dump_to(return_value, storage)
}


bool dump(Output &o, std::string &&s) {
    if (o.equals<string>()) return o.emplace(std::move(s));
    if (o.equals<cstring>()) return o.emplace(std::move(s));
    return o.load(std::move(s));
}

bool load(std::optional<string> &s, Ref &r) {
    if (auto o = r.target<string_view>()) s.emplace(*o);
}
```

## Function call types

1. The function returns `void`.

```c++
template <class F>
stat call(Index &out, void *r, F const &self, ArgView args) {
    try {
        invoke(self, *args);
        return stat::none;
    } catch (std::bad_alloc) {
        return stat::out_of_memory;
    } catch (...) {
        // exception_ptr is size 8 and nothrow move constructible
        // rust equivalent is size 16 and nothrow move constructible
        // I suppose in python ... it could be size 24 if using PyErr_Fetch ... would be nothrow move constructible though
        new (r) std::exception_ptr(std::current_exception());
        out = std::exception_ptr;
        return stat::exception;
    }
```

So basically the `void *` needs to be at least 24 bytes to cover Python...well, we could keep a variable static perhaps? Meh...

2. The function returns a reference and may throw.

```c++
template <class F>
stat call(Index &out, void *r, F const &self, ArgView args) {
    try {
        new(r) Out(invoke(self, *args));
        out = Index::of<Out>();
        return stat::Read;
    } catch (...) { /* same as for void */ }
```

3. The function returns a non-trivial value and may throw.

```c++
template <class F>
stat call(Index &out, void *r, unsigned size, F const &self, ArgView args) {
    try {
        out = Index::of<Out>();
        if (size <= sizeof(Out)) {
            new(r) Out(invoke(self, *args));
            return stat::Stack;
        } else {
            *static_cast<void **r> = new Out(invoke(self, *args));
            return stat::Heap;
        }
    } catch (...) { /* same as for void */ }
```

The last signature is a superset of the others, and the `size` is guaranteed to be at least 24; The next wrinkle is that we'd like to be able to perform requests in the body. That way we can assert we know the output type, so we can reserve the `size` in advance. If the function returns `void` it seems improbable that we could request anything useful. It is possible we'd want to do the requests on references and values though. This means we augment the above:

```c++
template <class F>
stat call(Index &out, void *r, unsigned size, F const &self, ArgView args) {
    try {
        if (out && out != Index::of<Out>()) {
            return dump(Output(out, r, size), invoke(self, *args)); // not sure size is necessary, seems like the caller would normally know it is enough for out.
        } else {
            new(r) Out(invoke(self, *args));
            out = Index::of<Out>();
            return stat::Read;
        }
    } catch (...) { /* same as for void */ }
```

The main overhead here is in:
- the IO parameter `out`, which originally was just out. We could split it, then the return type would be 16 wide.
- the size, which previously was a compile time constant.
-

So the raw type would imply, for request call:

```c++
stat (*)(void* output, Index requested_index, Len size_of_output, F const &self, ArgView args)
```

For any call...
```c++
stat (*)(IndexOutput &, Len size_of_output, F const &self, ArgView args)
```

where `IndexOutput` is basically like (not sure about order)

```c++
struct IndexOutput {
    Index output_index;
    char output_buffer[size_of_output];
};
```

We could combine these pretty easily (make requested_index optional).

status codes...
- `Exception`: an exception
- `Value`: a value

would also like to return index


## dump and load prototypes

Here are the "blind" approaches.
- The output may be constructed in place if it fits in the buffer
- The output may be allocated if it doesn't fit
- Also, the exception may be constructed in place if it fits.

```c++
// given T&&, try to insert type of index at location void * with allotted buffer size
Dump::stat dump(void *, size, Index, T &&t);

// try to insert type T at void * and given buffer size, given reference
Load::stat load(void *, size, Index, void *);
```

We could also use the "semi-blind" approach. Same as above except the buffer is guaranteed to be big enough. That way the output is never allocated. (This can be selected above at runtime though, not much cost).

We could also use the totally silent, "non-blind" approach. In this case we guarantee that the buffer can fit a `T`. Because we don't have any other guarantee, it seems unlikely that we could return exceptions, so they'd be silent.

```c++
Dump::stat dump(void *, Index, T &&t);
```

Finally we could use a compromise approach. We could guarantee that the buffer can fit both a `T` and a pointer. In the case of an exception, we accept that we might need to allocate the exception. bad_alloc could be handled specifically.

Seems like easiest to always use size.

For call it's different

## Call argument payload
- `Caller` - caller is not easy.
- `ara_str` for method name
- extra `Ref` for tag
- `Ref` for each argument

## Heap vs non-heap python type

Heap allows:

- multiple base classes
- dynamic attribute assignment on the class

Non-heap allows:

- definition as usual like with static types in C++
- probably a bit more efficient

In our rendering we are going to want to modify the following attributes:

- tp_getset
- tp_hash
- tp_getitem

etc.

