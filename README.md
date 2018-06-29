
- [Install](#install)
    - [Requirements](#requirements)
    - [Python](#python)
    - [CMake](#cmake)
- [Writing tests in C++](#writing-tests-in-c)
    - [Unit test declaration](#unit-test-declaration)
    - [Context API](#context-api)
        - [Logging](#logging)
        - [Assertions](#assertions)
        - [Leaving a test early](#leaving-a-test-early)
        - [Timings](#timings)
        - [Approximate comparison](#approximate-comparison)
    - [Macros](#macros)
    - [Values](#values)
        - [Configuration hook for conversion to Value](#configuration-hook-for-conversion-to-value)
        - [Configuration hook for conversion from `Value`](#configuration-hook-for-conversion-from-value)
    - [Test adaptors](#test-adaptors)
        - [C++ type-erased function](#c-type-erased-function)
        - [Type-erased value](#type-erased-value)
        - [Python function](#python-function)
    - [Templated functions](#templated-functions)
- [Running tests from the command line](#running-tests-from-the-command-line)
    - [Writing your own script](#writing-your-own-script)
    - [An example](#an-example)
- [To do](#to-do)
    - [Package name](#package-name)
    - [CMake](#cmake)
    - [Breaking out of tests early](#breaking-out-of-tests-early)
    - [Variant](#variant)
    - [Debugger](#debugger)
- [Done](#done)
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

### Context API

`Context` has essentially the same intrinsic thread safety as `std::vector` and other STL containers. You can copy `Context` as needed to run things in parallel. However, the registered handlers must be thread safe when called concurrently for this to work. The included Python handlers are thread safe.

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
// handle args if a handler registered for success/failure
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
ct.handle(Skipped); return;
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
`Value` is a simple wrapper around `std::variant`.

```c++
// Look up a registered value
Value v = cpy::get_value("my-value-name");
// Run a test case from another test case
Value v = cpy::call("my function", args...);
// Cast to different types
v.as_bool(); v.as_double(); v.as_integer(); v.as_view(); v.as_string();
```

#### Configuration hook for conversion to Value
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

#### Configuration hook for conversion from `Value`

The default behavior for casting to a type from a `Value` can be specialized.
```c++
template <class T, class=void> // T, the type to convert to
struct CastVariant {
    template <class U>
    bool check(U const &); // Return true if type T can be cast from type U

    template <class U>
    T operator()(U &u); // Return casted type T from type U
};
```

### Test adaptors

#### C++ type-erased function

The standard test takes any type of functor and converts it into (more or less) `std::function<Value(Context, ArgPack)>`.

If a C++ exception occurs while running this type of test, the runner generally reports and catches it. However, handlers may throw instances of `HandlerError` (subclass of `std::exception`), which are not caught.

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
cpy::Pack<int, double, bool>::for_each([](auto t) {
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
    auto max = ct.start_time + std::chrono::duration<double>(get_value("max_time").as_double());
    for (int i = 0; i != n; ++i) {
        if (Clock::now() > max) return;
        test();
    }
}
```
Then we can write a repetitive test which short-circuits like so:
```c++
UNIT_TEST("my-test") = [](Context ct) {
    repeat_test(ct, 500, [&] {run_some_random_test();});
};
```

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
