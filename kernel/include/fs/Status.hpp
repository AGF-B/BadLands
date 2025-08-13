#pragma once

template<class ErrT, typename V>
class Response {
public:
    explicit inline Response(ErrT error) : isError{true}, error{error} {}
    explicit inline Response(V value) : isError{false}, value{value} {}

    inline bool CheckError() const {
        return isError;
    }

    inline ErrT GetError() const {
        return error;
    }

    inline V GetValue() const {
        return value;
    }

private:
    bool isError;
    [[maybe_unused]] ErrT error;
    [[maybe_unused]] V value;
};

namespace FS {
    enum class Status {
        SUCCESS,
        INVALID_PARAMETER,
        UNSUPPORTED,
        NOT_READY,
        DEVICE_ERROR,
        READ_PROTECTED,
        WRITE_PROTECTED,
        LOCK_PROTECTED,
        VOLUME_CORRUPTED,
        VOLUME_FULL,
        NOT_FOUND,
        ALREADY_EXISTS,
        UNAVAILABLE,
        ACCESS_DENIED,
        TIMEOUT,
        ABORTED,
        OUT_OF_BOUNDS,
        IN_USE
    };

    template<typename V>
    class Response : public ::Response<Status, V> {
    public:
        explicit inline Response(Status status) : ::Response<Status, V>(status) {}
        explicit inline Response(V value) : ::Response<Status, V>(value) {}
    };
}
