# Traits




## Destruct

```c++
stat(T& value, storage s)
```

**Arguments**:

- `value`: value that will be destructed, and if `Destruct::Heap`, also deallocated
- `s`: one of `Destruct::Stack` or `Destruct::Heap`.

Output status:

- `OK`: destruction took place
- `Impossible`: destruction was impossible. This might change to calling `std::terminate()`.







## Copy

```c++
stat(void *out, T const &self, Code length)
```

**Arguments**:

- `out`: output location
- `self`: held object
- `length`: `sizeof` output location

Output status:

- `Stack`: OK.
- `Heap`: OK.
- `Impossible`: OK.
- `Exception`: OK.
- `OutOfMemory`: OK.

Change to allow assignment too?


## `Relocate`

Combine with Copy?





## `Info`

```c++
stat(Idx& out, void const*& type, ara_str& name) noexcept;
```

**Arguments**:

- `out`: output index
- `t`: output const pointer

The output `stat` is always `OK`, and on output:

-  `t` is a const reference to the language specific type information
-  `out` is the type index of `t`.
-  `s` contains some unspecified representation of the type name





## Compare

*Motivation*: ubiquitous concept with relatively easy automatic deduction and language-level support, but good to standardize between languages (e.g. `<=>` etc.).
*Concern*: different types ... I think these should not be allowed for this usage.
*Concern*: only boolean output ... seems like a plus to me to get around operator abuse.

```c++
stat(signed char& inout, T const &source, Index other, Pointer other) noexcept;
```

```c++
enum Input : signed char {Ternary, Eq, Ne, Lt, Gt, Le, Ge};
```

```c++
enum Output {Less, Equal, Greater, False, True, Incomparable};
```

`Ternary` returns one of `Less`, `Equal`, `Greater`.
All others return `False` or `True`.
`Equivalent` -- fix to make more like spaceship...






## Hash

*Motivation*: ubiquitous concept with relatively easy automatic deduction and language-level support. seems like always output `size_t`. Not a big deal but useful, then `Ref` can have a hash combining the type and value as well.


```c++
stat(size_t& out, T const& source) noexcept;
```

Generate hash value. Return `OK` or `Impossible`.




## String

*Motivation*: standardize usage between languages, help programmer. similar to type_info, no guarantee on formatting etc, but at least can provide some sensible defaults...

```c++
stat(ara_string& out, T const& source) noexcept;
```


## Number stuff

*Motivation*: relatively standard between languages. tedious to implement on a method/string based approach.
*Concern*: hard to know what to default type inference to though.
moderate priority but doesn't affect core functionality.

unary

- `+`
- `-`
- `~`
- `abs`: probably but not sure
- `log`, `exp`, etc: I don't think so.

binary

- `*`:
- `%`:
- `/`:
- `**`:
- `//`:
- `<<`:
- `>>`:
- `&`:
- `|`:
- `^`:

mutate versions of the binaries
unlikely:

- `@`
- `&&`
- `||`





## Increment/decrement

*Motivaton*: not standard across languages, but easy to use, maybe helpful for iterators. not a priority.

- `++`: Probably... (definitely not suffix though, and not returning new value)
- `--`: Probably... (definitely not suffix though, and not returning new value)

Can also think about address/dereference? No idea where address is useful...seems directly gettable without knowing the type. Dereference could be useful just to unify some semantics.

- `&`: I don't think so.
- `*`: Maybe ... no equivalent in some languages but common in others...might be useful.





## Boolean?

*Motivation*: common usage and clear-defined concept. mostly for convenience though... not a priority.

```c++
stat(T const& source) noexcept;
```

Generate boolean value. Return `True`, `False`, or `Impossible`.





## Attribute

*Motivation*: completely ubiquitous from what I can tell, at least for the accessor
*Concern*: possible wrinkle of `x.0` in Rust.
*Concern*: not completely clear why this is needed over method-based api ... need to establish if it is just a shortcut, or if it implies specific constraints (e.g. lifetimes or movability). probably C++ shouldn't be allowed to return reference member. Seems that a logical constraint would be if you have a value `x` then `x.y` returns mutable reference which can be freely messed with. In general difficult to figure out rvalue things... can't do equivalent of `std::move(x.y)` because no way to get `x.y` into an rvalue (would have to provide `x.move_y()` providing value `y` from lvalue `x`).

`v.i` or `v.i = `

Normally takes `Str`.
Assignment operator is optional, seems like mostly for convenience.

```c++
stat(Target& out, Pointer self, Str name) noexcept;
```

No constraint on output I think.
Could be `Ref`?
If it were `Ref`, would disallow things like exceptions, lifetime annotation, value output.
The current signature would be fine though excluding rust indices and only using `Str`.

```c++
M& attribute(T& t, Str name); // access lvalue
M const& attribute(T const& t, Str name); // access const
M attribute(T t, Str name); // move single member
M attribute(T& t, Str name); // move single member out --> not guaranteed though. C++ is fine but rust can't allow this.
void attribute(T& t, Str name, Ref other); // setattr
```

Just not sure about enforcing `Str name` ... integer could also be used even in C++ ... I guess it depends if Index conflicts.




## Index

Normally takes `Integer`.
`v[i]` or `v[i] =`

Problems:

- slicing: contiguous, open-ended, strided, reversed ...
- advanced, ND indexing? Definitely don't want this.
- extension to map-like use? I don't think so...?

I guess it is possible for C++/Rust tuples to be sliced ... so potentially possible.

```c++
stat(Target& out, Pointer self, IntegerOrSlice i) noexcept;
```

I'm not sure how useful the slicing is really.

```c++
M& index(T& t, Integer i);
M const& index(T const& t, Integer i);
M index(T t, Integer i);
M index(T& t, Integer i);
M index(T& t, Integer i);
```

Hmm I guess the defining feature of attribute vs index is really just `Str` vs `Integer`, not the syntax `[]` vs `.`. Makes me more confident in having both as well-defined things.

If index and attribute are used, seems like dereference may be same deal -- just an object which contains only one unambiguous member.

We would be missing a generic getattr...seems ok because this is really not a language feature, maps work in very different ways (e.g. C++ `map[i]` is totally different than Python `map[i]`...).

The slicing is a question...but just doesn't seem like a core operation... can always do `v.load<Span>().slice()` or `v.load<View>().slice()` via native implementations. This would be more of a splat operation.

Yeah I think slices are a language specific thing, should not implement as part of index. Prefer to use method api.

## Dump

`Dump` allows you to dump a value of type `T` into a less constrained value.

```c++
stat(Target &out, Pointer source, Mode mode) noexcept;
```

**Arguments**:

- `source`: pointer to the source object
- `mode`: qualifier of the source object
- `out`: output target

**On input**:

- `index`: Requested type index
- `output`: Output storage address (see Target)
- `length`: Output storage capacity in bytes (sizeof)
- `lifetime`: Bit mask for dependent reference argument indices. (I think this can hold the inplace bool).
- `mode`: Requested qualifier, see Target

If output already exists ...

- Need to add bool somewhere for output being already present. There should be no logical difference (I believe) for output being present or not present. This usage is mostly for optimization of storage and allocations.
- Need to figure out what to do with exceptions, because they don't necessarily have an output location now...

**Outputs**:

- `None`: No output due to dump being impossible
- `Write`: Holds a mutable reference as output
- `Read`: Holds an immutable reference as output
- `Stack`: Output was emplaced in the given stack storage
- `Heap`: Output was allocated on the heap
- `Exception`: Exception object was emplaced or allocated
- `OutOfMemory`: No output due to memory failure

**On output**:

- `index`: if `Exception`, index of the exception; if rvalue, new index of the source object (?)
- `output`: if heap, reference, or heap exception output, pointer to output.
- `lifetime`: see below
- `length`: unspecified (in future can perhaps return sizeof the output object, if useful?)
- `mode`: if `Exception`, mode of the held exception; if rvalue, new mode of the source object

Semantics:

*When returning a new output:*

- The value can be a reference or non-reference
- If the source is `&` or `const&`, the value can be an unsafe reference to the source object.
- If the source is `&&`, the value can be an unsafe reference to the source storage.

A referencing output will return 1 for `lifetime`.

*When returning an object in place:*

We can either make the target input a `T*` or a `char*` containing a `T*`. Second has an extra stack allocation but has possible output location for exceptions. First has no stack allocation and exceptions must go on heap. First probably cleaner.

- The target output must be a writable reference to an existing value of that type
- The lifetime may be 0 (no reference), 1 (reference on old), 2 (reference on rhs), 3 (reference on both)
- In the case of exceptions, exception is heap allocated.

!!!note
    How can `tp_traverse` be implemented? One option is to do a callback style.
    Another is to use `Dump` to give `Span` or `View` (a list of references)
    We can probably rely on `View` being not that large.
    `Span` could be large. We'd have to either crawl through everything (bad) or detect if a type can contain python (also bad).
    Alternative is no GC...easiest and GC sucks ... but not totally Python compliant... common in some other languages too (JS I think at least).
    Seems like a difficulty for reference types in general.
    OK I think it will be fine, we just need to prevent trivial insertion of an rvalue `Shared` into a C++ function, we can track referencing with the same mechanism as constness.

## Load

`Load` is the complement of `Dump`. It allows you to load the type `T` from a less constrained value. One difference is that `Load` generally can not return a reference type `T&`, because that could have been performed more efficiently via `Dump`. (Well I guess it can return a reference type in theory, but this is probably a bad usage.)

`Load` is basically all the same definition as `Dump` except that the places of the source and target type indices are switched.

Typically the holistic way to convert an opaque value to a given type `T` is to perform a sequence like this *pseudocode*:

```c++
template <class T>
optional<T> load(Ref& source) {
    if (T const* t = source.target(Index::of<T>())) {
        return *t;
    }
    if (optional<T> t = source.dump(Index::of<T>())) {
        return t;
    }
    if (optional<T> t = source.load(Index::of<T>())) {
        return t;
    }
    return {};
}
```

!!!note "Note"
    Note that in this case we don't need the indirection around `Load`: we could have directly used `Loadable<T>(ref)`. But the indirection is useful for reducing compile time and handling situations where you only know the output type at runtime.

```c++
stat(Target &out, Pointer source, Mode mode) noexcept;
```

**Arguments**:

- `source`: pointer to the source object
- `mode`: qualifier of the source object
- `out`: output target

**On input**:

- `index`: Type index of *source*
- `output`: Output storage address. (Must satisfy at least void* alignment and size requirements).
- `lifetime`: Bit mask for dependent reference argument indices
- `length`: Output storage capacity in bytes (sizeof)
- `mode`: Requested qualifier, see Target

**Outputs**:

- `None`: No output due to load being impossible
- `Write`: Holds a mutable reference as output
- `Read`: Holds an immutable reference as output
- `Stack`: Output was emplaced in the given stack storage
- `Heap`: Output was allocated on the heap
- `Exception`: Exception object was emplaced or allocated
- `OutOfMemory`: No output due to memory failure

**On output**:

- `index`: if output has been made, index of the output
- `output`: if heap or reference output, pointer to output.
- `lifetime`: bits are 1 for arguments that are depended on by output
- `length`: unspecified
- `mode`: if output has been made, mode of the output










## Call

- `Impossible`
- `WrongNumber`
- `WrongType`
- `WrongReturn`
- `None`
- `Stack`
- `Heap`
- `Write`
- `Read`
- `Exception`
- `OutOfMemory`






## Not implemented

Static methods...probably but not yet

Type traits...mostly a readout of type_traits... maybe useful but not needed yet


!!!note "Others?"
    - Iterable/range
    - Await
    - Length
    - Integer
    - Dereference


### Splat

Traverse top level object hierarchy, return vector of references.
I suppose whereas `Dump` is for conversion, `Splat` would be for direct unpacking for another usage?
Could also return array view where more efficient, I suppose.
Variant to dump as object (same but with names?): probably not, at least not now -- names could be in the type itself usually.
Might be nice to be able to dump to json or something?

```c++
stat(vec<Ref>& out, Ref& self) noexcept; // similar to this
```

We'd have to think about how vital this is: e.g. `.load<View>()` vs `.splat()`. Is there a problem going through the first one?

One issue is in C++, cannot return rvalue `View` from rvalue `T` ... have to intercept at the `Ref` level to do this. Otherwise an allocation is needed to do `Array`, which isn't a huge deal but does have some cost. Depends how important this optimization really is. Wouldn't be able to `Splat` anyway from a user defined destructor class. (Well I suppose you could, if the non-element destructor could be run separately.)

Let's see, so our native types are `None, Bool, String, Str, Binary, Bin, Integer, Float, View, Tuple, Span, Array`. If you're talking about going around `View` than maybe get rid of others? No I don't think so, this is an important level of customization...and it would be tedious to replace each thing with a new slot.

I suppose one real thing in `View` that makes it unique is the capability to transmute the `Ref` storage to avoid extra allocation. The other thing is that it's very low level and should be relatively optimized. (OTOH things like `Integer` are not that optimized so... -- that would argue for a less flexible but faster conversion method maybe).

It is also true I suppose that `Span` could gain the capability to mess with its `Ref` too ... it'd be tricky but could avoid the need for making an allocation of `std::vector<T>` (could transmute the `Ref` into `std::vector<aligned_storage<T>>` or something like that). I wonder if this should be possible for every rvalue method though (be able to transmute the self Ref argument)?

Very unclear right now what the advantage really is over `dump` ... I think almost nothing. We just need to be clear on the rvalue stuff and maybe allow Dump to steal storage.

