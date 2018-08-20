#include <cpy/Document.h>

namespace cpy {

Document & document() noexcept {
    static Document static_document;
    return static_document;
}

/******************************************************************************/

WrongTypes wrong_types(ArgPack const &v) {
    WrongTypes out;
    out.indices.reserve(v.size());
    for (auto const &x : v) out.indices.emplace_back(x.var.index());
    return out;
}

/******************************************************************************/

}
