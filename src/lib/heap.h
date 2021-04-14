
#pragma once

// note: the Heap iterator iterates the array linearly, not in Heap order

template<typename T, typename A = Mdefault> struct Heap {

    explicit Heap(u32 capacity = 0) {
        _data = A::template make<T>(capacity);
        _size = 0;
        _capacity = capacity;
    }

    explicit Heap(const Heap& src) = delete;
    Heap& operator=(const Heap& src) = delete;

    Heap(Heap&& src) {
        *this = std::move(src);
    }
    Heap& operator=(Heap&& src) {
        this->~Heap();
        _data = src._data;
        _size = src._size;
        _capacity = src._capacity;
        src._data = null;
        src._size = 0;
        src._capacity = 0;
        return *this;
    }

    ~Heap() {
        for(T& v : *this) {
            v.~T();
        }
        A::dealloc(_data);
        _data = null;
        _size = _capacity = 0;
    }

    Heap copy() const {
        Heap ret = {A::template make<T>(_capacity), _size, _capacity};
        memcpy(ret._data, _data, sizeof(T) * _size);
        return ret;
    }

    void grow() {
        u32 new_capacity = _capacity ? 2 * _capacity : 8;

        T* new_data = A::template make<T>(new_capacity);
        std::memcpy(new_data, _data, sizeof(T) * _size);
        A::dealloc(_data);

        _capacity = new_capacity;
        _data = new_data;
    }
    void clear() {
        for(T& v : *this) {
            v.~T();
        }
        _size = 0;
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

    template<typename U = T>
    requires std::is_copy_constructible_v<U>
    void push(const T& value) {
        push(T{value});
    }

    void push(T&& value) {
        if(full()) grow();
        _data[_size++] = std::move(value);
        reheap_up(_size - 1);
    }

    T pop() {
        assert(_size > 0);
        T ret = std::move(_data[0]);
        if(--_size) {
            _data[0] = std::move(_data[_size]);
            reheap_down(0);
        }
        return ret;
    }

    const T* begin() const {
        return _data;
    }
    const T* end() const {
        return _data + _size;
    }
    T* begin() {
        return _data;
    }
    T* end() {
        return _data + _size;
    }

private:
    void reheap_up(u32 idx) {
        if(!idx) return;
        u32 parent = (idx - 1) / 2;

        T tmp = std::move(_data[idx]);
        T par = std::move(_data[parent]);
        if(tmp > par) {
            _data[idx] = std::move(par);
            _data[parent] = std::move(tmp);
            reheap_up(parent);
        }
    }

    void reheap_down(u32 idx) {
        T val = std::move(_data[idx]);

        u32 left = idx * 2 + 1;
        u32 right = left + 1;

        if(right < _size) {
            T lval = std::move(_data[left]);
            T rval = std::move(_data[right]);
            if(lval > val && lval >= rval) {
                _data[idx] = std::move(lval);
                _data[left] = std::move(val);
                reheap_down(left);
            } else if(rval > val && rval >= lval) {
                _data[idx] = std::move(rval);
                _data[right] = std::move(val);
                reheap_down(right);
            }
        } else if(left < _size) {
            T lval = std::move(_data[left]);
            if(lval > val) {
                _data[idx] = std::move(lval);
                _data[left] = std::move(val);
            }
        }
    }

    T* _data = null;
    u32 _size = 0;
    u32 _capacity = 0;
    friend struct Type_Info<Heap>;
};

template<typename T, typename A> struct Type_Info<Heap<T, A>> {
    static constexpr char name[] = "Heap";
    static constexpr usize size = sizeof(Heap<T, A>);
    static constexpr Type_Type type = Type_Type::record_;
    static constexpr char _data[] = "data";
    static constexpr char _size[] = "size";
    static constexpr char _capacity[] = "capacity";
    typedef Heap<T, A> __offset;
    using members = Type_List<Record_Field<T*, offsetof(__offset, _data), _data>,
                              Record_Field<u32, offsetof(__offset, _size), _size>,
                              Record_Field<u32, offsetof(__offset, _capacity), _capacity>>;
};
