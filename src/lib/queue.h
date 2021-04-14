
#pragma once

template<typename T, typename A = Mdefault> struct Queue {

    explicit Queue(u32 capacity = 0) {
        _data = A::template make<T>(capacity);
        _size = 0;
        _capacity = capacity;
    }

    explicit Queue(const Queue& src) = delete;
    Queue& operator=(const Queue& src) = delete;

    Queue(Queue&& src) {
        *this = std::move(src);
    }
    Queue& operator=(Queue&& src) {
        this->~Queue();
        _data = src._data;
        _size = src._size;
        _last = src._last;
        _capacity = src._capacity;
        src._data = null;
        src._size = 0;
        src._last = 0;
        src._capacity = 0;
        return *this;
    }

    ~Queue() {
        for(T& v : *this) {
            v.~T();
        }
        A::dealloc(_data);
        _data = null;
        _size = _capacity = _last = 0;
    }

    Queue copy(Queue source) const {
        Queue ret = {A::template make<T>(_capacity), _size, _last, _capacity};
        std::memcpy(ret._data, _data, sizeof(T) * _capacity);
        return ret;
    }

    void grow() {
        u32 new_capacity = _capacity ? 2 * _capacity : 8;

        T* new_data = A::template make<T>(new_capacity);
        T* start = _data + _last - _size;

        if(_size <= _last)
            std::memcpy((void*)new_data, start, sizeof(T) * _size);
        else {
            u32 first = _size - _last;
            std::memcpy((void*)new_data, start + _capacity, sizeof(T) * first);
            std::memcpy((void*)(new_data + first), _data, sizeof(T) * _last);
        }

        A::dealloc(_data);

        _last = _size;
        _capacity = new_capacity;
        _data = new_data;
    }

    template<typename U = T>
    requires std::is_copy_constructible_v<U>
    T& push(const T& value) {
        return push(T{value});
    }

    T& push(T&& value) {
        if(_size == _capacity) grow();

        _data[_last] = std::move(value);
        T& ret = _data[_last];

        _size++;
        _last = _last == _capacity - 1 ? 0 : _last + 1;
        return ret;
    }

    T&& pop() {
        assert(_size > 0);
        u32 idx = _size <= _last ? _last - _size : _last - _size + _capacity;
        _size--;
        return std::move(_data[idx]);
    }
    void clear() {
        for(T& v : *this) {
            v.~T();
        }
        _size = _last = 0;
    }

    T& front() {
        assert(!empty());
        return _data[start_idx()];
    }
    T& back() {
        assert(!empty());
        return _data[end_idx()];
    }
    T& penultimate() {
        assert(_size > 1);
        u32 idx = _last == 0 ? _capacity - 2 : _last == 1 ? _capacity - 1 : _last - 2;
        return _data[idx];
    }
    bool empty() const {
        return _size == 0;
    }
    bool full() const {
        return _size == _capacity;
    }
    u32 size() const {
        return _size;
    }

    template<typename Q, typename E> struct itr {
        itr(Q& _q, u32 idx, u32 cons) : q(_q), place(idx), consumed(cons) {
        }

        itr operator++() {
            place = place == q._capacity - 1 ? 0 : place + 1;
            consumed++;
            return *this;
        }
        itr operator++(int) {
            itr i = *this;
            place = place == q._capacity - 1 ? 0 : place + 1;
            consumed++;
            return i;
        }
        E& operator*() {
            return q._data[place];
        }
        E* operator->() {
            return &q._data[place];
        }
        bool operator==(const itr& rhs) {
            return &q == &rhs.q && place == rhs.place && consumed == rhs.consumed;
        }
        bool operator!=(const itr& rhs) {
            return &q != &rhs.q || place != rhs.place || consumed != rhs.consumed;
        }

        Q& q;
        u32 place, consumed;
    };
    typedef itr<Queue, T> iterator;
    typedef itr<const Queue, const T> const_iterator;

    u32 start_idx() const {
        return _size <= _last ? _last - _size : _last - _size + _capacity;
    }
    u32 end_idx() const {
        return _last == 0 ? _capacity - 1 : _last - 1;
    }
    const_iterator begin() const {
        return const_iterator(*this, start_idx(), 0);
    }
    const_iterator end() const {
        return const_iterator(*this, _last, _size);
    }
    iterator begin() {
        return iterator(*this, start_idx(), 0);
    }
    iterator end() {
        return iterator(*this, _last, _size);
    }

private:
    T* _data = null;
    u32 _size = 0, _last = 0, _capacity = 0;
    friend struct Type_Info<Queue>;
};

template<typename T, typename A> struct Type_Info<Queue<T, A>> {
    static constexpr char name[] = "Queue";
    static constexpr usize size = sizeof(Queue<T, A>);
    static constexpr Type_Type type = Type_Type::record_;
    static constexpr char _data[] = "data";
    static constexpr char _size[] = "size";
    static constexpr char _last[] = "last";
    static constexpr char _capacity[] = "capacity";
    typedef Queue<T, A> __offset;
    using members = Type_List<Record_Field<T*, offsetof(__offset, _data), _data>,
                              Record_Field<u32, offsetof(__offset, _size), _size>,
                              Record_Field<u32, offsetof(__offset, _last), _last>,
                              Record_Field<u32, offsetof(__offset, _capacity), _capacity>>;
};
