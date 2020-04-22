# `ara`

`ara` is an experimental library for creating C++ bindings to Python and (eventually) other languages.

See docs folder for documentation.


Possible Rc like behavior:
1. for type `T`, holds type `T` inside. deletes `T` itself when it is destructed.
2. for type `T &`, holds exclusive ownership over the pointer inside. may or may not delete the pointer upon destruction, depends on its construction.
3. for type `T const &`, holds shared ownership over the pointer, may or may not delete upon destruction.

Could implement with something like `shared_ptr`. optional deleter.
