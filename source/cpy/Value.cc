#include <cpy/Document.h>

namespace cpy {

Document & document() noexcept {
    static Document static_document;
    return static_document;
}

/******************************************************************************/

Function overload(Function f, Function o) {
    if (auto ff = f.target<OverloadedFunction>()) {
        if (auto oo = o.target<OverloadedFunction>())
            ff->overloads.insert(ff->overloads.end(), oo->overloads.begin(), oo->overloads.end());
        else ff->overloads.emplace_back(std::move(o));
        return std::move(f);
    } else if (auto oo = o.target<OverloadedFunction>()) {
        oo->overloads.insert(oo->overloads.begin(), std::move(f));
        return std::move(o);
    } else return OverloadedFunction{{std::move(f), std::move(o)}};
}


}
