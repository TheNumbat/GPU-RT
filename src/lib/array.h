
#pragma once

#include "lib.h"

template<typename T, u32 N> struct Array {
    static constexpr u32 capacity = N;

    Array() = default;
    ~Array() = default;

    explicit Array(const Array& src) = default;
    Array& operator=(const Array& src) = delete;

    Array(std::initializer_list<T> init) {
        u32 i = 0;
        for(auto&& val : init) {
            _data[i++] = val;
        }
    }

    Array(Array&& src) = delete;
    Array& operator=(Array&& src) = delete;

    T& operator[](u32 idx) {
        assert(idx < N);
        return _data[idx];
    }
    const T& operator[](u32 idx) const {
        assert(idx < N);
        return _data[idx];
    }

    const T* data() const {
        return _data;
    }
    T* data() {
        return _data;
    }

    Slice<T> slice() {
        return Slice<T>(_data, capacity);
    }

    u32 size() const {
        return capacity;
    }

    const T* begin() const {
        return _data;
    }
    const T* end() const {
        return _data + N;
    }
    T* begin() {
        return _data;
    }
    T* end() {
        return _data + N;
    }

private:
    T _data[N] = {};
    friend struct Type_Info<Array>;
};

template<typename T, u32 N> struct Type_Info<Array<T, N>> {
    static constexpr char name[] = "Array";
    static constexpr usize size = sizeof(T[N]);
    static constexpr Type_Type type = Type_Type::array_;
    static constexpr usize len = N;
    using underlying = T;
};
