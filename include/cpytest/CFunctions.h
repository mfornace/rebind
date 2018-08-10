#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wregister"
#include <Python.h>
#pragma GCC diagnostic pop

#ifdef __cplusplus
extern "C" {
#endif

PyObject * cpy_run_test     (PyObject *, PyObject *);
PyObject * cpy_find_test    (PyObject *, PyObject *);
PyObject * cpy_n_tests      (PyObject *, PyObject *);
PyObject * cpy_compile_info (PyObject *, PyObject *);
PyObject * cpy_test_names   (PyObject *, PyObject *);
PyObject * cpy_test_info    (PyObject *, PyObject *);
PyObject * cpy_n_parameters (PyObject *, PyObject *);
PyObject * cpy_add_test     (PyObject *, PyObject *);
PyObject * cpy_add_value    (PyObject *, PyObject *);

#ifdef __cplusplus
}
#endif
