#include <cpytest/Stream.h>
#include <cpy/Document.h>
#include <cpytest/Suite.h>

namespace cpy {

// NOTE: this involves double erasure...
struct ValueHandler {
    CallingContext context;
    Function fun;
    bool operator()(Event e, Scopes const &scopes, Logs &&logs) {
        Vector<Input> vals = {Input(Integer(e)), Input(Sequence(scopes)),
            Sequence(mapped<Output>(logs, [](auto &x) {return std::move(x.key);})),
            Sequence(mapped<Output>(logs, [](auto &x) {return std::move(x.value);}))
        };
        return fun(context, vals).as_bool();
    }
};

struct ValueTest {
    Function fun;
    Output operator()(Context &ct, ArgPack args) const {
        return fun(static_cast<CallingContext &>(ct), args);
    }
};

/******************************************************************************/

Vector<Output> run_test(CallingContext &ct0, std::size_t i, Vector<Function> calls,
                        Input args, bool cout, bool cerr) {
    auto const test = suite().at(i);
    if (!test.function) throw std::runtime_error("Test case has invalid Function");
    ArgPack pack;
    if (std::holds_alternative<Integer>(args.var))
        pack = test.parameters.at(args.as_integer());
    if (std::holds_alternative<Sequence>(args.var)) {
        auto const &seq = std::get<Sequence>(args.var);
        pack.reserve(seq.size());
        seq.scan([&](Output o) {pack.emplace_back(std::move(o));});
    }
    std::stringstream out, err;
    Output return_value;
    double test_time = 0;
    Vector<Counter> counts(calls.size());
    for (auto &c : counts) c.store(0u);
    {
        RedirectStream o(cout_sync, cout ? out.rdbuf() : nullptr);
        RedirectStream e(cerr_sync, cerr ? err.rdbuf() : nullptr);

        Vector<Handler> handlers;
    for (auto &c : counts) c.store(0u);
        for (auto &f : calls) handlers.emplace_back(ValueHandler{ct0, std::move(f)});

    for (auto &c : counts) c.store(0u);
        Context ct(ct0, {test.name}, std::move(handlers), &counts);

    for (auto &c : counts) c.store(0u);
        auto const start = Clock::now();

    for (auto &c : counts) c.store(0u);
        try {return_value = test.function(ct, pack);}
        catch (ClientError const &e) {throw e;}
        catch (std::bad_alloc const &e) {throw e;}
    for (auto &c : counts) c.store(0u);
        // catch (...) {} // Silence any other exceptions from inside the test

        test_time = std::chrono::duration<double>(Clock::now() - start).count();
    }

    return {std::move(return_value), test_time, mapped<Output>(counts, [](auto const &i) {
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
        return mapped<Output>(suite(), [](auto &&x) {return x.name;});
    });

    doc.define("test_info", [](std::size_t i) -> Vector<Output> {
        auto const &c = suite().at(i);
        return {c.name, c.comment.location.file, Integer(c.comment.location.line), c.comment.comment};
    });

    doc.define("n_parameters", [](std::size_t i) {
        return suite().at(i).parameters.size();
    });

    doc.define("add_value", [](std::string s, Input v) {
        add_test(TestCase{std::move(s), {}, ValueAdaptor{std::move(v)}});
    });

    doc.define("run_test", [](CallingContext &ct, std::size_t i, Vector<Function> calls, Input args, bool cout, bool cerr) {
        return run_test(ct, i, std::move(calls), std::move(args), cout, cerr);
    });

    doc.define("add_test", [](std::string s, Function f, Vector<ArgPack> params) {
        add_test(TestCase{std::move(s), {}, ValueTest{f}, std::move(params)});
    });

    return true;
}

bool blah = make_document();


void ggg() {
    auto g = Sequence(std::vector<int>());
    auto h = g;
    auto j = std::move(g);
}

}