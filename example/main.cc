
#include <rebind-cpp/Schema.h>
#include <rebind-cpp/Core.h>
#include <mutex>


namespace rebind { void init(Schema &s); }


int main() {
    using namespace rebind;
    Schema schema{global_schema()};
    init(schema);

    auto v = schema["global_value"];
    auto p = v->target<int>();
    DUMP(v->name(), *p);

    DUMP(v->load<int>().value());
{
    auto blah = schema["fun"]->call<double>(Caller(), 1, 2.0);
    DUMP(blah.value());
}
{
    DUMP("call void");
    schema["fun"]->call<void>(Caller(), 1, 2.0);
}
{
    DUMP("call Value");
    auto val = schema["fun"]->call<Value>(Caller(), 1, 2.0);
}
{
    DUMP("call void 2");
    double x;
    schema["lref"]->call<void>(Caller(), x);
}
{
    DUMP("call ref");
    double x;
    double *y = schema["lref2"]->call<double &>(Caller(), x);
    DUMP(bool(y));
    DUMP(x, " ", *y, " ", &x == y);
}

{
    DUMP("call ref2");
    double x=5;
    double const *y = schema["lref3"]->call<double const &>(Caller(), x);
    DUMP(bool(y));
    DUMP(x, " ", *y, " ", &x == y);
}
    std::mutex mut;
    auto x = schema["mutex"]->call(Caller(), mut);

{
    auto v = Value(1);
    DUMP(bool(v.load<int>()));
    DUMP(bool(v.load<double>()));
}

    DUMP("done");
    return 0;
}