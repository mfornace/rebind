#include <ara-cpp/Schema.h>

namespace ara {

Value CallReturn<Value>::call(Index i, Mode qualifier, Pointer self, ArgView &args) {
    DUMP("calling to get Value");
    Value out;
    Target target(Index(), &out.storage, sizeof(out.storage),
        Target::Trivial | Target::Relocatable | Target::MoveNoThrow | Target::Heap);
    auto const stat = Call::call(i, target, self, qualifier, args);
    DUMP("called the output!", stat);

    switch (stat) {
        case Call::None: {break;}
        case Call::Stack: {
            out.idx = Tagged(target.index(), Value::Stack);
            break;
        }
        case Call::Heap: {
            out.storage.pointer = target.output();
            out.idx = Tagged(target.index(), Value::Heap);
            break;
        }
        default: call_throw(std::move(target), stat);
    }
    return out;
}

Value CallReturn<Value>::get(Index i, Mode qualifier, Pointer self, ArgView &args) {
    DUMP("calling to get Value");
    Value out;
    Target target(Index(), &out.storage, sizeof(out.storage),
        Target::Trivial | Target::Relocatable | Target::MoveNoThrow | Target::Heap);
    auto const stat = Call::call(i, target, self, qualifier, args);
    DUMP("called the output!", stat);

    switch (stat) {
        case Call::None: {break;}
        case Call::Stack: {
            out.idx = Tagged(target.index(), Value::Stack);
            break;
        }
        case Call::Heap: {
            out.storage.pointer = target.output();
            out.idx = Tagged(target.index(), Value::Heap);
            break;
        }
        case Call::Impossible: {break;}
        case Call::WrongNumber: {break;}
        case Call::WrongType: {break;}
        case Call::WrongReturn: {break;}
        default: call_throw(std::move(target), stat);
    }
    return out;
}

}