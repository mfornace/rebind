#pragma once
#include <memory>

namespace ara {

/******************************************************************************/

/// Interface: return a new frame given a shared_ptr of *this
struct Frame {
    // 1. make a new (unentered) child frame (noexcept). this happens before argument conversion
    virtual std::shared_ptr<Frame> new_frame(std::shared_ptr<Frame> self) noexcept {return self;}
    // 2. enter this frame. this happens once all arguments have been converted properly
    virtual void enter() = 0;
    // 3. handle resource destruction when the frame is done
    virtual ~Frame() {};
};

/******************************************************************************/

struct Caller { // seems to be 16 bytes
    std::weak_ptr<Frame> frame;

    Caller() = default;
    Caller(std::shared_ptr<Frame> const &f) noexcept : frame(f) {}

    explicit operator bool() const {return !frame.expired();}

    void enter() {if (auto p = frame.lock()) p->enter();}

    std::shared_ptr<Frame> new_frame() const noexcept {
        if (auto p = frame.lock()) return p.get()->new_frame(std::move(p));
        return {};
    }

    template <class T>
    T * target() noexcept {
        if (auto p = frame.lock()) return dynamic_cast<T *>(p.get());
        return nullptr;
    }
};

/******************************************************************************/

}