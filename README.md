
- [Install](#install)
    - [Requirements](#requirements)
    - [Python](#python)
    - [CMake](#cmake)
- [Writing tests in C++](#writing-tests-in-c)
    - [Unit test declaration](#unit-test-declaration)
    - [`Context` API](#context-api)
        - [Logging](#logging)
        - [Assertions](#assertions)
        - [Leaving a test early](#leaving-a-test-early)
        - [Timings](#timings)
        - [Approximate comparison](#approximate-comparison)
    - [Macros](#macros)
    - [Values](#values)
        - [`Valuable` and conversion of arbitrary types to `Value`](#valuable-and-conversion-of-arbitrary-types-to-value)
        - [`CastVariant` and conversion to `Value`](#castvariant-and-conversion-to-value)
    - [Test adaptors](#test-adaptors)
        - [C++ type-erased function](#c-type-erased-function)
        - [Type-erased value](#type-erased-value)
        - [Python function](#python-function)
    - [Templated functions](#templated-functions)
- [Running tests from the command line](#running-tests-from-the-command-line)
    - [Python threads](#python-threads)
    - [Writing your own script](#writing-your-own-script)
    - [An example](#an-example)
- [Callback C++ API](#callback-c-api)
- [`libcpy` Python API](#libcpy-python-api)
    - [Exposed Python functions via C API](#exposed-python-functions-via-c-api)
    - [Exposed Python C++ API](#exposed-python-c-api)
- [To do](#to-do)
    - [Package name](#package-name)
    - [CMake](#cmake)
    - [Variant](#variant)
    - [Debugger](#debugger)
- [Done](#done)
    - [Breaking out of tests early](#breaking-out-of-tests-early)
    - [Variant](#variant)
    - [CMake](#cmake)
    - [Object size](#object-size)
    - [Library/module name](#librarymodule-name)
    - [FileLine](#fileline)
    - [Exceptions](#exceptions)
    - [Signals](#signals)
    - [To value](#to-value)

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

## Writing tests in C++

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

### `Context` API

Most methods on `Context` are non-const. However, `Context` is fine to be copied or moved around, so it has approximately the same thread safety as `std::vector` and other STL containers. To run things in parallel within C++, just make multiple copies of your `Context` as needed. However, the registered callbacks must be thread safe when called concurrently for this to work. (The included Python callbacks are thread safe.)

#### Logging

```c++
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
// open a child scope (functor takes parameters Context, args...)
ct.section("section name", functor, args...);
```

#### Assertions

```c++
// handle args if a callback registered for success/failure
bool ok = ct.require(true, args...);
// equality check (also not_equal, less, greater, less_eq, greater_eq)
bool ok = ct.equal(left, right, args...);
// check that a function throws a given Exception
bool ok = ct.throw_as<ExceptionType>(function, function_args...);
// check that a function does not throw
bool ok = ct.no_throw(function, function_args...);
```

See also [Approximate comparison](#approximate-comparison) below.

#### Leaving a test early
`Context` functions have no magic for exiting a test early. Write `throw` or `return` yourself if you want that as follows:

```c++
// skip out of the test with a throw
throw cpy::Skip("optional message");
// skip out of the test without throwing.
ct.handle(Skipped, "optional message"); return;
```

#### Timings

```c++
// time a long running computation
typename Clock::duration elapsed = ct.timed(function_returning_void, args...);
auto function_result = ct.timed(function_returning_nonvoid, args...);
// get the start time of the current unit test or section
typename Clock::time_point &start = ct.start_time;
```

#### Approximate comparison

If the user specifies a tolerance manually, `Context::within` checks that either `l == r` or (`|l - r| < tolerance`).
```c++
bool ok = ct.within(l, r, tolerance, args...);
```

Otherwise, `Context::near()` checks that two arguments are approximately equal by using a specialization of `cpy::Approx` for the types given.

```c++
bool ok = ct.near(left, r, args...);
```

For floating point types, `Approx` defaults to checking `|l - r| < eps * (scale + max(|l|, |r|))` where scale is 1 and eps is the square root of the floating point epsilon. When given two different types for `l` and `r`, the type of less precision is used for the epsilon. `Approx` may be specialized for user types.

### Macros

The next macros are defined with `CPY_` prefix if `Macros.h` is included. If not already defined, prefix-less macros are also defined there.
```c++
#define GLUE(x) KeyPair(#x, x) // string of the expression and the expression value
#define LOCATION file_line(__FILE__, __LINE__) // make a datum with the current file and line
#define COMMENT(x) comment(x, LOCATION) // a comment with file and line information
#define UNIT_TEST(name, [comment]) ... // complicated; see above for usage
```

### Values
`Value` is a simple wrapper around a `std::variant`.

```c++
struct Value {
    Variant var;
    bool as_bool() const;
    Real as_real() const;
    Integer as_integer() const;
    std::string_view as_view() const;
    std::string as_string() const;
};
```

The `Value` wrapper provides some convenience functions and also instantiates some `std::variant` templates in the `libcpy` library since `std::variant` can yield a lot of code size. The definition of `Value`  relies on these typedefs:

```c++
using Integer = std::ptrdiff_t;

using Real = double;

using Complex = std::complex<double>;

template <class T>
using Vector = std::vector<T>;

using Variant = std::variant<
    std::monostate,
    bool,
    Integer,
    Real,
    Complex,
    std::string,
    std::string_view
>;
```

Next, some related structs:
```c++
using ArgPack = Vector<Value>; // A runtime length list of arguments
struct KeyPair {std::string_view key; Value value;}; // A key value pair used in logging
using Logs = std::vector<KeyPair>; // Keeps tracked of logged information
```

#### `Valuable` and conversion of arbitrary types to `Value`
To make a Value from an arbitrary object, the following function is provided:
```c++
template <class T>
Value make_value(T &&t);
```

This functions does a compile-time lookup of `Valuable<std::decay_t<T>`. If no such struct is defined, `cpy` converts the object to a string via something like the following. The compiler will then error if `operator<<` has no matches.
```c++
std::ostringstream os;
os << static_cast<T &&>(t);
Value v = os.str()
```

Otherwise, this implementation is assumed to be usable:
```
Value v = Valuable<std::decay_t<T>>()(static_cast<T &&>(t));
```

`Valuable` be specialized as needed. Here are some examples:

```c++
// The declaration present in cpy
template <class T, class=void>
struct Valuable;
// User example: define the default Value making operation for all objects
template <class T, class>
struct Valuable {
    Value operator()(T const &t) const {...}
};
// User example: specialize a Value making operation for a specific type
template <>
struct Valuable<my_type> {
    Value operator()(my_type const &t) const {return "my_type"}
};
// User example: specialize a Value making operation for a trait
template <class T>
struct Valuable<T, std::enable_if_t<(my_trait<T>::value)> {
    Value operator()(T const &t) const {...}
};
```
Look up partial specialization, `std::enable_if`, and/or `std::void_t` for background on how this type of thing can be used.

#### `CastVariant` and conversion to `Value`

The default behavior for casting to a C++ type from a `Value` can also be specialized. The relevant struct to specialize is declared like the following:
```c++
template <class T, class=void> // T, the type to convert to
struct CastVariant {
    template <class U>
    bool check(U const &); // Return true if type T can be cast from type U

    template <class U>
    T operator()(U &u); // Return casted type T from type U if check() returns true
};
```

### Test adaptors

#### C++ type-erased function

The standard test takes any type of functor and converts it into (more or less) `std::function<Value(Context, ArgPack)>`.

If a C++ exception occurs while running this type of test, the runner generally reports and catches it. However, callbacks may throw instances of `CallbackError` (subclass of `std::exception`), which are not caught.

This type of test may be called from anywhere in type-erased fashion:
```c++
Value output = call("my-test-name", (Context) ct, args...); // void() translated to std::monostate
```

The output from the function must be convertible to `Value` in this case.

Or, if the test declaration is visible via non-macro version, you can call it without type erasure:
```c++
auto test1 = unit_test("test 1", [](Context ct, int i) {return MyType{i};);
...
MyType t = test1(ct, 6);
```

#### Type-erased value

Sometimes it's nice to add tests which just return a fixed `Value` without computation. For instance, you can add a value from Python:

```python
lib.add_value('number-of-threads', 4)
```

and retrieve it while running tests in C++:
```c++
auto n = get_value("number-of-threads").as_integer(); // preferred, n = 4
auto n = call("number-of-threads", (Context) ct).as_integer(); // equivalent
```

#### Python function

Or sometimes, you might want to make a type-erased callback to Python.

```python
lib.add_test('number-of-threads', lambda i: i * 2)
```

and retrieve it while running tests in C++:
```c++
auto n = call("number-of-threads", 5).as_integer(); // n = 10
```

### Templated functions

You can test different types via the following within a test case
```c++
cpy::Pack<int, Real, bool>::for_each([](auto t) {
    using type = decltype(*t);
    // do something with type
});
```
For more advanced functionality try something like `boost::hana`.

## Running tests from the command line

```bash
python -m cpy.cli -a mylib # run all tests from mylib.so/mylib.dll/mylib.dylib
```

By default, events are only counted and not logged. To see more output use:

```bash
python -m cpy.cli -a mylib -fe # log information on failures, exceptions
python -m cpy.cli -a mylib -fset # log information on failures, successes, exceptions, timings
```

There are a few other reporters written in the Python package, including writing to JUnit XML, a simple JSON format, and streaming TeamCity directives.

See the command line help `python -m cpy.cli --help` for other options.

### Python threads

`cpy.cli` exposes a command line option for you to specify the number of threads used. The threads are used simply via something like:

```python
from multiprocessing.pool import ThreadPool
ThreadPool(n_threads).imap(tests, run_test)
```

If the number of threads (`--jobs`) is set to 0, no threads are spawned, and everything is run in the main thread. This is a little more lightweight, but signals such as `CTRL-C` (`SIGINT`) will not be caught immediately during execution of a test. This parameter therefore has a default of 1.

Also, `cpy` turns off the GIL by default when running tests, but if you are mucking around in CPython bindings too, you may want to keep it on (with `--gil`).

### Writing your own script

It is easy to write your own executable Python script to wrap the one provided. For example, let's write a test script that lets C++ tests reference a value `max_time`.

```python
#!/usr/bin/env python3
from cpy import cli

parser = cli.parser()
parser.add_argument('--time', type=float, default=float('inf'), help='maximum test time')

args = vars(parser.parse_args())

lib = cli.import_library(args['lib'])
lib.add_value('max_time', args.pop('time'))
# pop any added arguments before passing to cli.main
cli.main(**args)
```

### An example

Let's use the `Value` registered above to write a helper to repeat a test until the allowed test time is used up.
```c++
template <class F>
void repeat_test(Context const &ct, int n, F const &test) {
    auto max = ct.start_time + std::chrono::duration<Real>(get_value("max_time").as_real());
    for (int i = 0; i != n; ++i) {
        if (Clock::now() > max) return;
        test();
    }
}
```
Then we can write a repetitive test which short-circuits like so:
```c++
UNIT_TEST("my-test") = [](Context ct) {
    repeat_test(ct, 500, [&] {run_some_random_test(ct);});
};
```

## Callback C++ API
Events are kept track of via a simple index. It is pretty easy to extend to more event types.

A callback is registered to be called if a single fixed `Event` is signaled. It is implemented as a `std::function`. If no callback is registered for a given event, nothing is called.

```c++
using Event = std::uint32_fast_t;
Event Failure = 0, Success = 1, Exception = 2, Timing = 3, Skipped = 4;
using Callback = std::function<bool(Event, Scopes const &, Logs &&)>;
```

## `libcpy` Python API

The `cpy` Python callbacks all use the official CPython API. Doing so is really not too hard beyond managing `PyObject *` lifetimes.

### Exposed Python functions via C API

In general each of the following functions is callable only with positional arguments:

```python
# Return number of tests
n_tests()
# Add a test from its name and a callable function
# callable should accept arguments (event: int, scopes: tuple(str), logs: tuple(tuple))
add_test(str, callable, [args])
# Find the index of a test from its name
find_test(str)
# Run test given index, callbacks for each event, parameter pack, keep GIL on, capture cerr, capture cout
run_test(int, tuple, tuple, bool, bool, bool)
# Return a tuple of the names of all registered tests
test_names()
# Add a value to the test suite with the given name
add_value(str, object)
# return the number of parameter packs for test of a given index
n_parameters(int)
# Return tuple of (compiler version, compile data, compile time)
compile_info()
# Return (name, file, line, comment) for test of a given index
test_info(int)
```

### Exposed Python C++ API

There isn't much reason to mess with this API unless you write your own callbacks, but here are some basics:

```c++
// PyObject * wrapper implementing the reference counting in RAII style
struct Object;
// For C++ type T, return an Object of it converted into Python, else raise PyError and return null object
Object to_python(T); // defined for T each type in Value, for instance
// Convert a Python object into a Value, return if conversion successful
bool from_python(Value &v, Object o);
// A C++ exception which reflects an existing PyErr status
struct PythonError : CallbackError;
// RAII managers for the Python global interpreter lock (GIL)
struct ReleaseGIL; struct AcquireGIL
// Callback adaptor for a Python object
struct PyCallback;
// RAII managers for std::ostream capturing
struct RedirectStream; struct StreamSync;
// Test adaptor for a Python object
struct PyTestCase : Object;
```

Look in the code for more detail.

## To do

### Package name
`cpy` is short but not great otherwise. Maybe `cpt` or `cptest`.

### CMake
Fix up caching of python include directory.

### Variant
- Should rethink if `variant<..., any>` is better than just `any`.
- Also would like vector types in the future (`vector<Value>` or `vector<T>...`?)
- time or timedelta? function?

### Debugger
- `break_into_debugger()`
- Goes in same callback? Not sure. Could be a large frame stack.
- Debugger (hook into LLDB possible?)

## Done

### Breaking out of tests early
At the very least put `start_time` into Context.
Problem with giving the short circuit API is partially that it can be ignored in a test.
It is fully possible to truncate the test once `start_time` is inside, or any other possible version.
Possibly `start_time` should be given to callback, or `start_time` and `current_time`.
Standardize what callback return value means, add skip event if needed.

### Variant
- `complex<Real>` is probably not that useful, but it's included (in Python so whatever)
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
`CallbackError` and its subclasses are not caught by test runner. All others are.

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
