
#pragma once

template<typename T> struct Slice;

template<typename T, typename A = Mdefault> struct Vec {

    explicit Vec(u32 capacity = 0) {
        _data = A::template make<T>(capacity);
        _size = 0;
        _capacity = capacity;
    }
    explicit Vec(u32 size, T value) {
        _data = A::template make<T>(size);
        _size = _capacity = size;
        for(u32 i = 0; i < size; i++) _data[i] = value;
    }

    Vec(std::initializer_list<T> init) {
        reserve((u32)init.size());
        for(auto&& v : init) {
            push(v);
        }
    }

    explicit Vec(const Vec& src) = delete;
    Vec& operator=(const Vec& src) = delete;

    Vec(Vec&& src) {
        *this = std::move(src);
    }
    Vec& operator=(Vec&& src) {
        this->~Vec();
        _data = src._data;
        _size = src._size;
        _capacity = src._capacity;
        src._data = null;
        src._size = 0;
        src._capacity = 0;
        return *this;
    }

    ~Vec() {
        for(T& v : *this) {
            v.~T();
        }
        A::dealloc(_data);
        _data = null;
        _size = _capacity = 0;
    }

    Vec copy() const {
        Vec ret = {A::template make<T>(_capacity), _size, _capacity};
        std::memcpy(ret._data, _data, sizeof(T) * _size);
        return ret;
    }

    void grow() {
        u32 new_capacity = _capacity ? 2 * _capacity : 8;
        reserve(new_capacity);
    }
    void clear() {
        for(T& v : *this) {
            v.~T();
        }
        _size = 0;
    }
    void reserve(u32 sz) {
        if(sz > _capacity) {
            T* new_data = A::template make<T>(sz);

            if(_data) {
                std::memcpy((void*)new_data, _data, sizeof(T) * _size);
                A::dealloc(_data);
            }

            _capacity = sz;
            _data = new_data;
        }
    }
    void extend(u32 sz) {
        reserve(_size + sz);
        _size += sz;
    }

    bool empty() const {
        return _size == 0;
    }
    bool full() const {
        return _size == _capacity;
    }

    template<typename U = T>
    requires std::is_copy_constructible_v<U>
    T& push(const T& value) {
        return push(T{value});
    }

    T& push(T&& value) {
        if(full()) grow();
        assert(_size < _capacity);
        _data[_size] = std::move(value);
        return _data[_size++];
    }

    T pop() {
        assert(_size > 0);
        return std::move(_data[_size--]);
    }

    T& front() {
        assert(_size > 0);
        return _data[0];
    }
    T& back() {
        assert(_size > 0);
        return _data[_size - 1];
    }

    T& operator[](u32 idx) {
        assert(idx < _size);
        return _data[idx];
    }
    const T& operator[](u32 idx) const {
        assert(idx < _size);
        return _data[idx];
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

    u32 size() const {
        return _size;
    }
    u32 capacity() const {
        return _capacity;
    }

    void fill() {
        _size = _capacity;
    }

    T* data() {
        return _data;
    }
    T const* data() const {
        return _data;
    }

private:
    T* _data = null;
    u32 _size = 0;
    u32 _capacity = 0;

    friend struct Type_Info<Vec>;
    friend struct Slice<T>;
};

// NOTE(max): Basically a Vec that does not own its memory, i.e. can edit the contained
// data but cannot change the actual allocated memory.
template<typename T> struct Slice {

    explicit Slice() = default;

    template<typename A> explicit Slice(const Vec<T, A>& v) {
        _data = v._data;
        _size = v._size;
        _capacity = v._capacity;
    }

    template<usize N> static Slice slice(Marena<N>& arena, u32 cap) {
        Slice ret;
        ret._data = arena.make<T>(cap);
        ret._size = 0;
        ret._capacity = cap;
        return ret;
    }

    explicit Slice(T* data, u32 size) {
        _data = data;
        _size = size;
        _capacity = size;
    }

    ~Slice() {
        _data = null;
        _size = _capacity = 0;
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

    template<typename U = T>
    requires std::is_copy_constructible_v<U>
    T& push(const T& value) {
        return push(T{value});
    }
    
    T& push(T&& value) {
        assert(_size < _capacity);
        _data[_size] = std::move(value);
        return _data[_size++];
    }
    
    T pop() {
        assert(_size > 0);
        return _data[_size--];
    }

    T& operator[](u32 idx) {
        assert(idx >= 0 && idx < _size);
        return _data[idx];
    }
    const T& operator[](u32 idx) const {
        assert(idx >= 0 && idx < _size);
        return _data[idx];
    }

    T* data() {
        return _data;
    }
    const T* data() const {
        return _data;
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

    u32 size() const {
        return _size;
    }
    u32 capacity() const {
        return _capacity;
    }

private:
    T* _data = null;
    u32 _size = 0;
    u32 _capacity = 0;
    friend struct Type_Info<Slice<T>>;
};

template<typename T, typename A> struct Type_Info<Vec<T, A>> {
    static constexpr char name[] = "Vec";
    static constexpr usize size = sizeof(Vec<T, A>);
    static constexpr Type_Type type = Type_Type::record_;
    static constexpr char _data[] = "data";
    static constexpr char _size[] = "size";
    static constexpr char _capacity[] = "capacity";
    typedef Vec<T, A> __offset;
    using members = Type_List<Record_Field<T*, offsetof(__offset, _data), _data>,
                              Record_Field<u32, offsetof(__offset, _size), _size>,
                              Record_Field<u32, offsetof(__offset, _capacity), _capacity>>;
};

template<typename T> struct Type_Info<Slice<T>> {
    static constexpr char name[] = "Slice";
    static constexpr usize size = sizeof(Slice<T>);
    static constexpr Type_Type type = Type_Type::record_;
    static constexpr char _data[] = "data";
    static constexpr char _size[] = "size";
    static constexpr char _capacity[] = "capacity";
    using members = Type_List<Record_Field<T*, offsetof(Slice<T>, _data), _data>,
                              Record_Field<u32, offsetof(Slice<T>, _size), _size>,
                              Record_Field<u32, offsetof(Slice<T>, _capacity), _capacity>>;
};
