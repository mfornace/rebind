#pragma once

namespace rebind {

/******************************************************************************/

using Binary = std::basic_string<unsigned char>;

using BinaryView = std::basic_string_view<unsigned char>;

class BinaryData {
    unsigned char *m_begin=nullptr;
    unsigned char *m_end=nullptr;
public:
    constexpr BinaryData() = default;
    constexpr BinaryData(unsigned char *b, std::size_t n) : m_begin(b), m_end(b + n) {}
    constexpr auto begin() const {return m_begin;}
    constexpr auto data() const {return m_begin;}
    constexpr auto end() const {return m_end;}
    constexpr std::size_t size() const {return m_end - m_begin;}
    operator BinaryView() const {return {m_begin, size()};}
};

template <>
struct Response<BinaryData> {
    bool operator()(Variable &out, TypeIndex const &t, BinaryData const &v) const {
        if (t.equals<BinaryView>()) return out.emplace(Type<BinaryView>(), v.begin(), v.size()), true;
        return false;
    }
};

template <>
struct Response<BinaryView> {
    bool operator()(Variable &out, TypeIndex const &, BinaryView const &v) const {
        return false;
    }
};

template <>
struct Request<BinaryView> {
    using handled_types = Pack<BinaryView>;

    std::optional<BinaryView> operator()(Variable const &v, Dispatch &msg) const {
        if (auto p = v.request<BinaryData>()) return BinaryView(p->data(), p->size());
        return msg.error("not convertible to binary view", typeid(BinaryView));
    }
};

template <>
struct Request<BinaryData> {
    using handled_types = Pack<>;

    std::optional<BinaryData> operator()(Variable const &v, Dispatch &msg) const {
        return msg.error("not convertible to binary data", typeid(BinaryData));
    }
};

/******************************************************************************/

}