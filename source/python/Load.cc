#include <ara-py/Builtins.h>
#include <ara-py/Load.h>
#include <ara-py/Variable.h>

#include <ara/Ref.h>
#include <ara/Core.h>

#include <unordered_map>

namespace ara::py {

/******************************************************************************/

std::string repr(PyObject* o) {
    if (!o) return "null";
    Shared out(PyObject_Repr(+o), false);
    if (!out) throw PythonError();
#   if PY_MAJOR_VERSION > 2
        return PyUnicode_AsUTF8(+out); // PyErr_Clear
#   else
        return PyString_AsString(+out);
#   endif
}

/******************************************************************************/

std::unordered_map<Shared, Shared> output_conversions;

/******************************************************************************/

Shared try_load(Ref &r, Instance<> t, Shared root) {
    DUMP("try_load", r.name(), "to type", repr(+t), "with root", repr(+root));
    return map_output(t, [&](auto T) {
        return decltype(T)::load(r, t, root);
    });
}

/******************************************************************************/

}