#include <lilwil/Stream.h>
#include <cpy/Document.h>
#include <lilwil/Suite.h>
#include <sstream>

namespace lilwil {

// NOTE: this involves double erasure...
struct ValueHandler {
    cpy::Caller context;
    cpy::Function fun;
    bool operator()(Event e, Scopes const &scopes, LogVec<cpy::Variable> &&logs) {
        auto out = fun(context, Integer(e), scopes, std::move(logs));
        if (auto b = out.request<bool>()) return *b;
        if (cpy::Debug) std::cout << "not bool " << cpy::Variable(out).type().name() << std::endl;
        return true;
    }
};

struct ValueTest {
    cpy::Function fun;
    cpy::Variable operator()(Context<cpy::Variable> &ct, cpy::ArgPack args) const {
        // return fun(static_cast<cpy::Caller &>(ct), std::move(args));
        return {};
    }
};

/******************************************************************************/

Vector<cpy::Variable> run_test(cpy::Caller &ct0, std::size_t i, Vector<cpy::Function> calls,
                        cpy::Variable args, bool cout, bool cerr) {
    auto const test = suite<cpy::Variable>().at(i);
    if (!test.function) throw std::runtime_error("Test case has invalid cpy::Function");
    cpy::ArgPack pack;
    if (auto p = args.target<Integer const &>())
        pack = test.parameters.at(*p);
    std::stringstream out, err;
    cpy::Variable return_value;
    double test_time = 0;
    Vector<Counter> counts(calls.size());
    for (auto &c : counts) c.store(0u);
    {
        RedirectStream o(cout_sync, cout ? out.rdbuf() : nullptr);
        RedirectStream e(cerr_sync, cerr ? err.rdbuf() : nullptr);

        Vector<Handler<cpy::Variable>> handlers;
        for (auto &f : calls) handlers.emplace_back(ValueHandler{ct0, std::move(f)});

        Context<cpy::Variable> ct({test.name}, std::move(handlers), &counts);

        auto const start = Clock::now();

        try {return_value = test.function(ct, std::move(pack));}
        catch (ClientError const &) {throw;}
        catch (std::bad_alloc const &) {throw;}
        catch (cpy::WrongType const &e) {
            std::cout << "hmm " << e.what() << e.source.name() << e.dest.name() << std::endl;}
        catch (cpy::WrongNumber const &e) {
            std::cout << "hmm " << e.what() << e.expected << e.received << std::endl;}
        catch (std::exception const &e) {
            std::cout << "error2: " << e.what() << std::endl;
        }
        catch (...) {} // Silence any other exceptions from inside the test

        test_time = std::chrono::duration<double>(Clock::now() - start).count();
    }

    return {std::move(return_value), test_time, cpy::mapped<cpy::Variable>(counts, [](auto const &i) {
        return i.load(std::memory_order_relaxed);
    }), out.str(), err.str()};
}

/******************************************************************************/

bool make_document() {
    auto &doc = cpy::document();
    doc.function("n_tests", [] {
        return suite<cpy::Variable>().size();
    });

    doc.function("compile_info", []() -> Vector<std::string_view> {
        return {__VERSION__ "", __DATE__ "", __TIME__ ""};
    });

    doc.function("test_names", [] {
        return cpy::mapped<cpy::Variable>(suite<cpy::Variable>(), [](auto &&x) {return x.name;});
    });

    doc.function("test_info", [](std::size_t i) -> Vector<cpy::Variable> {
        auto const &c = suite<cpy::Variable>().at(i);
        return {c.name, c.comment.location.file, Integer(c.comment.location.line), c.comment.comment};
    });

    doc.function("n_parameters", [](std::size_t i) {
        return suite<cpy::Variable>().at(i).parameters.size();
    });

    doc.function("add_value", [](std::string_view s, cpy::Variable v) {
        add_test<cpy::Variable>(TestCase<cpy::Variable>{std::string(s), {}, ValueAdaptor<cpy::Variable>{std::move(v)}, {}});
    });

    doc.function("run_test", [](cpy::Caller ct, std::size_t i, Vector<cpy::Function> calls, cpy::Variable args, bool cout, bool cerr) {
        return run_test(ct, i, std::move(calls), std::move(args), cout, cerr);
    });

    doc.function("add_test", [](std::string s, cpy::Function f, Vector<cpy::ArgPack> params) {
        add_test(TestCase<cpy::Variable>{std::move(s), {}, ValueTest{f}, std::move(params)});
    });

    return true;
}

bool blah = make_document();

}