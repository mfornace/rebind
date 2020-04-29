#include <ara/Schema.h>

#include <ara-rust/Module.h>
#include <deque>

/******************************************************************************/

namespace ara {
    std::deque<Table> rust_tables;
}

/******************************************************************************/

void ara_value_drop(ara_value *x) noexcept {
    using namespace ara;
    reinterpret_cast<Value *>(x)->~Value();
}

// void ara_value_new(ara_value *v) { // noexcept
    // return reinterpret_cast<ara_value *>(new ara::Value(std::string()));
// }

namespace ara {void init(ara::Schema &);}

ara_bool ara_init() noexcept {
    try {
        ara::init(ara::schema());
        DUMP("worked!");
        return 1;
    } catch (...) {
        DUMP("failed!");
        return 0;
    }
}

ara_bool ara_value_copy(ara_value *v, ara_value const *o) noexcept {
    // try {
    //     return reinterpret_cast<ara_value *>(new ara::Value(
    //         *reinterpret_cast<ara::Value const *>(v)));
    // } catch (...) {
    //     return nullptr;
    // }
    return true;
}

// ara_index ara_value_index(ara_value *v) {
    // ara_index[16] ARAC(variable_type)(ara_value *v);
//     auto t = reinterpret_cast<ara::Value const *>(v)->index();
//     static_assert(sizeof(t) == sizeof(ara_index));
//     return reinterpret_cast<ara_index const &>(t);
// }

char const * ara_index_name(ara_index v) noexcept {
    return ara::raw::name(v).data();
}

inline std::string_view as_view(ara_str const &s) {
    return {reinterpret_cast<char const *>(s.data), s.size};
}

inline ara::ArgView as_view(ara_args const &s) {
    return {reinterpret_cast<ara::Ref *>(s.data), s.size};
}

ara_value const * ara_lookup(ara_str s) noexcept {
    auto const &c = ara::schema().contents;
    DUMP("looking up", as_view(s), as_view(s).size());
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

ara_index ara_table_emplace(
    ara_str name,
    void const *info,
    ara::CTable::drop_t drop,
    ara::CTable::copy_t copy,
    ara::CTable::to_value_t to_value,
    ara::CTable::to_ref_t to_ref,
    ara::CTable::assign_if_t assign_if,
    ara::CTable::from_ref_t from_ref) {

    auto &t = ara::rust_tables.emplace_back();
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

ara_bool ara_value_call_value(ara_value *out, ara_value const *v, ara_args a) noexcept {
    auto const &f = *reinterpret_cast<ara::Value const *>(v);
    try {
        return f.call_to(reinterpret_cast<ara::Value &>(out), ara::Caller(), as_view(a));
    } catch(std::exception const &e) {
        DUMP(e.what());
        return 0;
    }
}

/******************************************************************************/