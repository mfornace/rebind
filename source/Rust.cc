#include <rebind/Schema.h>

#include <rebind-rust/Module.h>
#include <deque>

/******************************************************************************/

namespace rebind {
    std::deque<Table> rust_tables;
}

/******************************************************************************/

void rebind_value_drop(rebind_value *x) noexcept {
    using namespace rebind;
    reinterpret_cast<Value *>(x)->~Value();
}

// void rebind_value_new(rebind_value *v) { // noexcept
    // return reinterpret_cast<rebind_value *>(new rebind::Value(std::string()));
// }

namespace rebind {void init(rebind::Schema &);}

rebind_bool rebind_init() noexcept {
    try {
        rebind::init(rebind::schema());
        DUMP("worked!");
        return 1;
    } catch (...) {
        DUMP("failed!");
        return 0;
    }
}

rebind_bool rebind_value_copy(rebind_value *v, rebind_value const *o) noexcept {
    // try {
    //     return reinterpret_cast<rebind_value *>(new rebind::Value(
    //         *reinterpret_cast<rebind::Value const *>(v)));
    // } catch (...) {
    //     return nullptr;
    // }
    return true;
}

// rebind_index rebind_value_index(rebind_value *v) {
    // rebind_index[16] REBINDC(variable_type)(rebind_value *v);
//     auto t = reinterpret_cast<rebind::Value const *>(v)->index();
//     static_assert(sizeof(t) == sizeof(rebind_index));
//     return reinterpret_cast<rebind_index const &>(t);
// }

char const * rebind_index_name(rebind_index v) noexcept {
    return rebind::raw::name(v).data();
}

inline std::string_view as_view(rebind_str const &s) {
    return {reinterpret_cast<char const *>(s.data), s.size};
}

inline rebind::Arguments as_view(rebind_args const &s) {
    return {reinterpret_cast<rebind::Ref *>(s.data), s.size};
}

rebind_value const * rebind_lookup(rebind_str s) noexcept {
    auto const &c = rebind::schema().contents;
    DUMP("looking up ", as_view(s), " ", as_view(s).size());
    if (auto it = c.find(as_view(s)); it != c.end()) {
        auto v = it->second.as_raw();
        DUMP("hmmm");
        auto o = &(it->second.as_raw());
        DUMP("hmmm2");
        return o;
    } else {
        return nullptr;
    }
}

rebind_index rebind_table_emplace(
    rebind_str name,
    void const *info,
    rebind::CTable::drop_t drop,
    rebind::CTable::copy_t copy,
    rebind::CTable::to_value_t to_value,
    rebind::CTable::to_ref_t to_ref,
    rebind::CTable::assign_if_t assign_if,
    rebind::CTable::from_ref_t from_ref) {

    auto &t = rebind::rust_tables.emplace_back();
    t.c.drop = drop;
    t.c.copy = copy;
    t.c.to_value = to_value;
    t.c.to_ref = to_ref;
    t.c.assign_if = assign_if;
    t.c.from_ref = from_ref;
    t.c.info = info;
    t.m_name = as_view(name);
    return &t;
}

rebind_bool rebind_value_call_value(rebind_value *out, rebind_value const *v, rebind_args a) noexcept {
    auto const &f = *reinterpret_cast<rebind::Value const *>(v);
    try {
        return f.call_to(reinterpret_cast<rebind::Value &>(out), rebind::Caller(), as_view(a));
    } catch(std::exception const &e) {
        DUMP(e.what());
        return 0;
    }
}

/******************************************************************************/