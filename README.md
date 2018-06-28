
## Install

### Requirements
- CMake
- C++17 (fold expressions, `constexpr bool *_v` traits, `std::variant`, `std::string_view`)
- Python 2.7+ or 3.3+

### Python
Run `pip install -e .` in the directory where setup.py is. Module `cpy.cli` is included for command line usage.
It can be run directly as a script `python -m cpy.cli ...` or imported from your own script.
The `cpy` python package is pure Python, so you can also import it without installing if it's in your `$PYTHONPATH`.

### CMake
Use CMake function `cpy_module` to make a CMake target for the given library. Define `-DCPY_PYTHON={my python executable}` or `-DCPY_PYTHON_INCLUDE={include folder for python}` to customize.

## Usage

### Unit test declaration
Unit tests are functors which:
- take a first argument of `cpy::Context` or `cpy::Context &&`
- take any other arguments of a type convertible from `cpy::Value`
- return void or an object convertible to `cpy::Value`

You can use `auto` instead of `cpy::Context` if it is the only parameter. You can't use `auto` for the other parameters unless you specialize the `cpy` signature deduction.

```c++
// unit test of the given name
unit_test("my-test-name", [](cpy::Context ct, ...) {...});
// unit test of the given name and comment
unit_test("my-test-name", "my test comment", [](cpy::Context ct, ...) {...});
// unit test of the given name and comment (source location included)
unit_test("my-test-name", COMMENT("my test comment"), [](cpy::Context ct, ...) {...});
// unit test of the given name (source location included)
UNIT_TEST("my-test-name") = [](cpy::Context ct, ...) {...};
// unit test of the given name and comment (source location included)
UNIT_TEST("my-test-name", "my test comment") = [](cpy::Context ct, ...) {...};
```

### Context functions
`Context` functions have no magic for exiting a test early. Write `throw` or `return` yourself if you want that.

`Context` has essentially the same intrinsic thread safety as `std::vector` and other STL containers. You can copy `Context` as needed to run things in parallel. However, the registered handlers must be thread safe when called concurrently for this to work. The included Python handlers are thread safe.

```c++
// handle args if a handler registered for success/failure
bool ok = ct.require(true, args...);
// equality check (also not_equal, less, greater, less_eq, greater_eq)
bool ok = ct.equal(left, right, args...);
// approximate equality check (based on precision of types given)
bool ok = ct.near(left, right, args...);
// approximate equality check (|left - right| < tolerance)
bool ok = ct.within(left, right, tolerance, args...);
// check that a function throws a given Exception
bool ok = ct.throw_as<ExceptionType>(function, function_args...);
// check that a function does not throw
bool ok = ct.no_throw(function, function_args...);
// skip out of the test with a throw
throw cpy::Skip("optional message");
// skip out of the test without throwing.
ct.handle(Skipped); return;
// log some information before an assertion.
ct.info("working...");
// call ct.info(arg) for each arg in args. returns *this for convenience
Context &same_as_ct = ct(args...);
// log source file location
ct(::cpy::file_line(__FILE__, __LINE__));
// equivalent macro
ct(LOCATION);
// chain statements together if convenient
ct(LOCATION).require(...);
// log a single key pair of information before an assertion.
ct.info("value", 1.5);
// time a long running computation
typename Clock::duration elapsed = ct.timed(function_returning_void, args...);
auto function_result = ct.timed(function_returning_nonvoid, args...);
// open a child scope (functor takes parameters Context, args...)
ct.section("section name", functor, args...);
// get the start time of the current unit test or section
typename Clock::time_point &start = ct.start_time;
```

### Values
`Value` is a simple wrapper around `std::variant`.

```c++
// Look up a registered value
Value v = cpy::get_value("my-value-name");
// Run a test case from another test case
Value v = cpy::call("my function", args...);
// Cast to different types
v.as_bool(); v.as_double(); v.as_integer(); v.as_view(); v.as_string();
```

### Configuration hooks
```c++
// Define the default Value making operation
template <class T, class>
struct Valuable {
    Value operator()(T const &t) const {...}
};
// Specialize a Value making operation
template <class T>
struct Valuable<std::enable_if_t<(my_trait<T>::value)> {
    Value operator()(T const &t) const {...}
};
```

If no default is defined, `cpy` converts the object to a string via something like the following. The compiler will then error if `operator<<` has no matches.
```c++
std::ostringstream os;
os << object;
return os.str()
```

### Templated functions

You can test different types via the following within a test case
```c++
cpy::Pack<int, double, bool>::for_each([](auto t) {
    using type = decltype(*t);
    // do something with type
});
```
For more advanced functionality try something like `boost::hana`.

## To do

### Package name
`cpy` is short but not great otherwise. Maybe `cpt` or `cptest`.

### CMake
Fix up caching of python include directory.

### Breaking out of tests early
At the very least put `start_time` into Context.
Problem with giving the short circuit API is partially that it can be ignored in a test.
It is fully possible to truncate the test once `start_time` is inside, or any other possible version.
Possibly `start_time` should be given to handler, or `start_time` and `current_time`.
Standardize what handler return value means, add skip event if needed.

### Variant
- Should rethink if `variant<..., any>` is better than just `any`.
- Also would like vector types in the future (`vector<Value>` or `Vector<T>...`?)
- time or timedelta? function?

### Debugger
- `break_into_debugger()`
- Goes in same handler? Not sure. Could be a large frame stack.
- Debugger (hook into LLDB possible?)

## Done

### Variant
- `complex<double>` is probably not that useful, but it's included (in Python so whatever)
- `std::string` is the biggest object on my architecture (24 bytes). `std::any` is 32.
- Just use `std::ptrdiff_t` instead of `std::size_t`? Probably.


### CMake
User needs to give shared library target for now so that exports all occur

### Object size
- Can explicitly instantiate Context copy constructors etc.
- Write own version of `std::function` (maybe)
- Already hid the `std::variant` (has some impact on readability but decreased object size a lot)

### Library/module name
One option is leave as is. The library is built in whatever file, always named libcpy.
This is only usable if Python > 3.4 and specify -a file_name.
And, I think there can't be multiple modules then, because all named libcpy right? Yes.
So, I agree -a file_name is fine. Need to change `PyInit_NAME`.
So when user builds, they have to make a small module file of their desired name.
We can either do this with macros or with CMake. I guess macros more generic.

### FileLine
Finalize incorporation of FileLine (does it need to go in vector, can it be separate?)

It's pretty trivial to construct, for sure. constexpr now.

The only difference in cost is that
- it appends to the vector even when not needed (instead of having reserved slot)
- it constructs 2 values even when not needed
- I think these are trivial, leave as is.
- It is flushed on every event (maybe should last for multiple events? or is that confusing?)

### Exceptions
`HandlerError` and its subclasses are not caught by test runner. All others are.

### Signals
- possible to use `PyErr_SetInterrupt`
- but the only issue is when C++ running a long time without Python
- that could be the case on a test with no calls to handle of a given type.
- actually it appears there's no issue if threads are being used!
- so default is just to use 1 worker thread.

### To value
- option 1: leave the above undefined -- user can define a default.
- option 5: leave undefined -- make_value uses stream if it is undefined
- option 3: define it to be the stream operator. then the user has to override the default based on a void_t
- problem then is, e.g. for std::any -- user would have to use std::is_copyable -- but then ambiguous with all their other overloads.
- option 2: define but static_assert(false) in it -- that's bad I think
- option 4: define a void_t stream operator. but then very hard to override
