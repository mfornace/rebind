#include <cpy/Function.h>
#include <cpytest/Suite.h>

namespace cpy {

// this involves double erasure...
struct ValueHandler {
    BaseContext *context;
    Function fun;
    bool operator()(Event e, Scopes const &s, Logs &&logs) const {
        Vector<Value> vals = {Integer(e), make_value(s),
            vectorize(logs, [](auto &x) {return std::move(x.key);}),
            vectorize(logs, [](auto &x) {return std::move(x.value);})};
        return fun(*context, vals).as_bool();
    }
};

struct ValueTest {
    Function fun;
    Value operator()(Context const &ct, ArgPack args) const {
        return fun(*reinterpret_cast<BaseContext *>(ct.metadata), args);
    }
};

/******************************************************************************/

Vector<Value> run_test(BaseContext &ct0, std::size_t i, Vector<Function> calls, Value args, bool cout, bool cerr, bool no_gil) {
    auto const test = suite().at(i);
    if (!test.function) throw std::runtime_error("Test case has invalid Function");

    ArgPack pack;
    if (std::holds_alternative<Integer>(args.var))
        pack = test.parameters.at(args.as_integer());

    if (std::holds_alternative<Vector<Value>>(args.var))
        pack = std::move(args).as_vector();

    std::stringstream out, err;
    Value return_value;
    double test_time = 0;
    Vector<Counter> counts(calls.size());
    for (auto &c : counts) c.store(0u);

    {
        RedirectStream o(cout_sync, cout ? out.rdbuf() : nullptr);
        RedirectStream e(cerr_sync, cerr ? err.rdbuf() : nullptr);

    // no_gil = no_gil && !test.function.target<PyTestCase>();
    // ReleaseGIL lk(no_gil);
    // if (no_gil) for (auto &c : handlers)
        // if (c) c.target<PyHandler>()->unlock = &lk;

        Vector<Handler> handlers;
        for (auto &f : calls) handlers.emplace_back(ValueHandler{&ct0, std::move(f)});

        Context ct({test.name}, std::move(handlers), &counts, &ct0);

        auto const start = Clock::now();

        try {return_value = test.function(ct, pack);}
        catch (ClientError const &e) {throw e;}
        // catch (...) {} // Silence any other exceptions from inside the test

        test_time = std::chrono::duration<double>(Clock::now() - start).count();
    }

    return {std::move(return_value), test_time, vectorize(counts, [](auto const &i) {
        return i.load(std::memory_order_relaxed);
    }), out.str(), err.str()};
}

/******************************************************************************/

bool make_document() {
    auto &doc = document();
    doc.define("n_tests", [] {
        return suite().size();
    });

    doc.define("compile_info", []() -> Vector<std::string_view> {
        return {__VERSION__ "", __DATE__ "", __TIME__ ""};
    });

    doc.define("test_names", [] {
        return vectorize(suite(), [](auto &&x) {return x.name;});
    });

    doc.define("test_info", [](std::size_t i) -> Vector<Value> {
        auto const &c = suite().at(i);
        return {c.name, c.comment.location.file, Integer(c.comment.location.line), c.comment.comment};
    });

    doc.define("n_parameters", [](std::size_t i) {
        return suite().at(i).parameters.size();
    });

    doc.define("add_value", [](std::string s, Value v) {
        add_test(TestCase{std::move(s), {}, ValueAdaptor{std::move(v)}});
    });

    doc.define2("run_test", [](BaseContext &ct, std::size_t i, Vector<Function> calls, Value args, bool gil, bool cout, bool cerr) {
        return run_test(ct, i, std::move(calls), std::move(args), cout, cerr, !gil);
    });

    doc.define("add_test", [](std::string s, Function f, Vector<ArgPack> params) {
        add_test(TestCase{std::move(s), {}, ValueTest{f}, std::move(params)});
    });

    return true;
}

bool blah = make_document();

}