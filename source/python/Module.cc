#include <ara-py/Raw.h>

struct Example;
// #define ARA_CAT_IMPL(s1, s2, s3, s4) s1##s2##s3##s4
// #define ARA_PY_INIT(s1, s2, s3, s4) ARA_CAT_IMPL(s1, s2, s3, s4)

extern "C" {

PyMODINIT_FUNC PyInit_cpp(void) {
    return ara::py::init_module<Example>();
}

}