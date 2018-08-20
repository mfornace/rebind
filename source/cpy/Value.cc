#include <cpy/Document.h>

namespace cpy {

Document & document() noexcept {
    static Document static_document;
    return static_document;
}

/******************************************************************************/

Input::Input(Output &&o) noexcept : var(std::visit([](auto &x) {
    return InputVariant(std::move(x));
}, o.var)) {}

Output::Output(Input &&o) noexcept : var(std::visit([](auto &x) {
    return OutputVariant(std::move(x));
}, o.var)) {}

/******************************************************************************/

WrongTypes wrong_types(ArgPack const &v) {
    WrongTypes out;
    out.indices.reserve(v.size());
    for (auto const &x : v) out.indices.emplace_back(x.var.index());
    return out;
}

/******************************************************************************/

}
