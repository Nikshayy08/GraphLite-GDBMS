
#pragma once

#include <utility>
#include <variant>

namespace graphlite {

template <typename T, typename E>
class Result {
private:
    std::variant<T, E> data_;

    explicit Result(T value)
        : data_(std::move(value)) {}

    explicit Result(E error)
        : data_(error) {}

public:
    static Result success(T value) {
        return Result(std::move(value));
    }

    static Result failure(E error) {
        return Result(error);
    }

    bool hasValue() const {
        return std::holds_alternative<T>(data_);
    }

    explicit operator bool() const {
        return hasValue();
    }

    T& value() {
        return std::get<T>(data_);
    }

    const T& value() const {
        return std::get<T>(data_);
    }

    E error() const {
        return hasValue() ? E{} : std::get<E>(data_);
    }
};

// Specialization for operations that return no value
template <typename E>
class Result<void, E> {
private:
    bool success_;
    E error_;

    Result(bool success, E error)
        : success_(success), error_(error) {}

public:
    static Result success() {
        return Result(true, E{});
    }

    static Result failure(E error) {
        return Result(false, error);
    }

    bool hasValue() const {
        return success_;
    }

    explicit operator bool() const {
        return success_;
    }

    E error() const {
        return success_ ? E{} : error_;
    }
};

} // namespace graphlite
