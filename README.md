## Package name
`cpy` is short but not great otherwise.

## CMake
User needs to give shared library target for now so that exports all occur

## Variant
- `complex<double>` is probably not that useful, but it's included (in Python so whatever)
- std::string is the biggest object on my architecture (24 bytes).
- std::any is 32 bytes. Will probably add this in so user can take advantage.
- Should rethink if variant<..., any> is better than just any.
- Also would like vector types in the future (`vector<Value>` or `Vector<T>...`?)

## Object size
- Can explicitly instantiate Context copy constructors etc.
- Write own version of std::function (maybe)
- Already hid the std::variant (has some impact on readability but decreased object size a lot)

## Library/module name
One option is leave as is. The library is built in whatever file, always named libcpy.
This is only usable if Python > 3.4 and specify -a file_name.
And, I think there can't be multiple modules then, because all named libcpy right? Yes.
So, I agree -a file_name is fine. Need to change PyInit_NAME.
So when user builds, they have to make a small module file of their desired name.
We can either do this with macros or with CMake. I guess macros more generic.

## FileLine
Finalize incorporation of FileLine (does it need to go in vector, can it be separate?)

It's pretty trivial to construct, for sure. constexpr now.

The only difference in cost is that
- it appends to the vector even when not needed (instead of having reserved slot)
- it constructs 2 values even when not needed
- I think these are trivial, leave as is.
- It is flushed on every event (maybe should last for multiple events? or is that confusing?)

## Debugger
- break_into_debugger()
- Debugger (hook into LLDB possible?)

## Exceptions
HandlerError has its own exception, which is not caught by test runner. All others are.

## Signals
- possible to use `PyErr_SetInterrupt`
- but the only issue is when C++ running a long time without Python
- that could be the case on a test with no calls to handle of a given type.
- actually it appears there's no issue if threads are being used!
- so default is just to use 1 worker thread.


