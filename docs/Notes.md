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
    let t = ManualDrop::new(t); // t will not be destructed manually now.
    let v = Value{ ptr: &t, type: (T, stack) }
    ... work with v ...
    v is destructed, so the destructor for "t" is run, but *not* the deleter
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
    Value v{p, (T, stack)};
    ... work with v ...
    v is destructed, but not deleted!
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