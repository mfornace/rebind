/**
 * @brief Speicalized C++ wrappers for Python objects
 * @file Binding.h
 */

#include <cpytest/Test.h>

namespace cpy {

/******************************************************************************/

struct PyTestCase : Object {
    using Object::Object;

    Value operator()(Context ctx, ArgPack const &pack) {
        AcquireGIL lk(static_cast<ReleaseGIL *>(ctx.metadata));
        Object args = cpy::to_python(pack);
        if (!args) throw python_error();
        Object o = {PyObject_CallObject(Object::ptr, +args), false};
        if (!o) throw python_error();
        Value out;
        if (!cpy::from_python(out, std::move(o))) throw python_error();
        return out;
    }
};

/******************************************************************************/

struct PyHandler {
    Object object;
    ReleaseGIL *unlock = nullptr;

    bool operator()(Event event, Scopes const &scopes, Logs &&logs) {
        if (!+object) return false;
        AcquireGIL lk(unlock); // reacquire the GIL (if it was released)

        Object pyevent = to_python(static_cast<Integer>(event));
        if (!pyevent) return false;

        Object pyscopes = to_python(scopes);
        if (!pyscopes) return false;

        Object pylogs = to_python(logs);
        if (!pylogs) return false;

        Object out = {PyObject_CallFunctionObjArgs(+object, +pyevent, +pyscopes, +pylogs, nullptr), false};
        if (PyErr_Occurred()) throw python_error();
        return bool(out);
    }
};

/******************************************************************************/

}