
#include <ara-cpp/Schema.h>
#include <ara/Core.h>
#include <mutex>


ara::Ref const * make_schema();

static ara::Ref module = make_schema()->copy();

int main() {
    using namespace ara;

    // auto v = schema["global_value"];
    // auto p = v.target<int const &>();
    // DUMP(v.name(), *p);
    // DUMP(v.has_value());
    // DUMP(v.get<int>(s).value());
{
    DUMP("testing first function fun with args 1, 2.0");
    auto blah = module.call<double>(Caller(), "fun", 1, 2.0);
    DUMP("got result", blah);
}
{
    DUMP("call void");
    module.call<void>(Caller(), "fun", 1, 2.0);
}
{
    DUMP("call Value");
    auto val = module.call<Value>(Caller(), "fun", 1, 2.0);
}
{
    DUMP("call void 2");
    double x;
    module.call<void>(Caller(), "lref", x);
}
{
    DUMP("call ref");
    double x;
    double &y = module.call<double &>(Caller(), "lref2", x);
    DUMP(x, y, &x == &y);
}

{
    DUMP("call ref2");
    double x=5;
    double const &y = module.call<double const &>(Caller(), "lref3", x);
    DUMP(bool(y));
    DUMP(x, y, &x == &y);
}
    std::mutex mut;
    try {
        auto x = module.call<Value>(Caller(), "mutex", mut);
    } catch (...) {

    }

    auto x = module.call<Value, false>(Caller(), "mutex", mut);

{
    auto v = Value(1);
    DUMP(bool(v.get<int>()));
    DUMP(bool(v.get<double>()));
}

{
    DUMP("Goo stuff");
    Value v = module.call<Value>(Caller(), "Goo.new", 1.5);
    DUMP("got the Goo?", v.name(), int(v.location()));//, int(Value::loc_of<Goo>));

    double y = v.method<double, 1>(Caller(), ".x");
    DUMP("got .x", y);
    double y2 = v.method<double, 1>(Caller(), ".x");
    DUMP("got .x", y2);
    double y3 = v.method<double>(Caller());
}

    DUMP("done");
    return 0;
}