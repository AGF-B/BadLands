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

template<typename T>
class Optional {
public:
    explicit inline Optional() : hasValue{false} {}
    explicit inline Optional(T value) : hasValue{true}, value{value} {}

    inline bool HasValue() const {
        return hasValue;
    }

    inline T GetValue() const {
        return value;
    }

private:
    bool hasValue;
    [[maybe_unused]] T value;
};

class Success {
public:
    explicit inline Success(bool isSuccess = true) : isSuccess{isSuccess} {}
    static inline Success MakeSuccess() { return Success(true); }
    static inline Success MakeFailure() { return Success(false); }
    
    inline bool IsSuccess() const {
        return isSuccess;
    }

private:
    bool isSuccess;
};
