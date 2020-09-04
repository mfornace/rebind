#include <sfb/Schema.h>

#include <sfb-rust/Module.h>
#include <deque>

/******************************************************************************/

namespace sfb {
    std::deque<Table> rust_tables;
}

/******************************************************************************/

void sfb_value_drop(sfb_value *x) noexcept {
    using namespace sfb;
    reinterpret_cast<Value *>(x)->~Value();
}

// void sfb_value_new(sfb_value *v) { // noexcept
    // return reinterpret_cast<sfb_value *>(new sfb::Value(std::string()));
// }

namespace sfb {void init(sfb::Schema &);}

sfb_bool sfb_init() noexcept {
    try {
        sfb::init(sfb::schema());
        DUMP("worked!");
        return 1;
    } catch (...) {
        DUMP("failed!");
        return 0;
    }
}

sfb_bool sfb_value_copy(sfb_value *v, sfb_value const *o) noexcept {
    // try {
    //     return reinterpret_cast<sfb_value *>(new sfb::Value(
    //         *reinterpret_cast<sfb::Value const *>(v)));
    // } catch (...) {
    //     return nullptr;
    // }
    return true;
}

// sfb_index sfb_value_index(sfb_value *v) {
    // sfb_index[16] SFBC(variable_type)(sfb_value *v);
//     auto t = reinterpret_cast<sfb::Value const *>(v)->index();
//     static_assert(sizeof(t) == sizeof(sfb_index));
//     return reinterpret_cast<sfb_index const &>(t);
// }

char const * sfb_index_name(sfb_index v) noexcept {
    return sfb::raw::name(v).data();
}

inline std::string_view as_view(sfb_str const &s) {
    return {reinterpret_cast<char const *>(s.data), s.size};
}

inline sfb::ArgView as_view(sfb_args const &s) {
    return {reinterpret_cast<sfb::Ref *>(s.data), s.size};
}

sfb_value const * sfb_lookup(sfb_str s) noexcept {
    auto const &c = sfb::schema().contents;
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

sfb_index sfb_table_emplace(
    sfb_str name,
    void const *info,
    sfb::CTable::drop_t drop,
    sfb::CTable::copy_t copy,
    sfb::CTable::to_value_t to_value,
    sfb::CTable::to_ref_t to_ref,
    sfb::CTable::assign_if_t assign_if,
    sfb::CTable::from_ref_t from_ref) {

    auto &t = sfb::rust_tables.emplace_back();
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

sfb_bool sfb_value_call_value(sfb_value *out, sfb_value const *v, sfb_args a) noexcept {
    auto const &f = *reinterpret_cast<sfb::Value const *>(v);
    try {
        return f.call_to(reinterpret_cast<sfb::Value &>(out), sfb::Caller(), as_view(a));
    } catch(std::exception const &e) {
        DUMP(e.what());
        return 0;
    }
}

/******************************************************************************/
