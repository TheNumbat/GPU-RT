
#pragma once

template<typename T, typename A = Mdefault> struct Stack {

    explicit Stack() {
    }
    explicit Stack(u32 capacity) : _data(capacity) {
    }

    explicit Stack(const Stack& src) = delete;
    Stack& operator=(const Stack& src) = delete;

    Stack(Stack&& src) = default;
    Stack& operator=(Stack&& src) = default;

    Stack copy() const {
        return {_data.copy()};
    }

    template<typename U = T>
    requires std::is_copy_constructible_v<U>
    void push(const T& value) {
        _data.push(value);
    }

    void push(T&& value) {
        _data.push(std::move(value));
    }

    T pop() {
        return _data.pop();
    }

    void clear() {
        _data.clear();
    }

    const T* begin() const {
        return _data.begin();
    }
    const T* end() const {
        return _data.end();
    }
    T* begin() {
        return _data.begin();
    }
    T* end() {
        return _data.end();
    }

    const Vec<T, A>& vec() const {
        return _data;
    }

private:
    Vec<T, A> _data;
    friend struct Type_Info<Stack>;
};

template<typename T, typename A> struct Type_Info<Stack<T, A>> {
    static constexpr char name[] = "Stack";
    static constexpr usize size = sizeof(Stack<T, A>);
    static constexpr Type_Type type = Type_Type::record_;
    static constexpr char _data[] = "data";
    typedef Stack<T, A> __offset;
    using members = Type_List<Record_Field<Vec<T, A>, offsetof(__offset, _data), _data>>;
};
