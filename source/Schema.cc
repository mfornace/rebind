#include <rebind-cpp/Schema.h>

namespace rebind {

RawSchema & global_schema() noexcept {
    static RawSchema schema;
    return schema;
}

}