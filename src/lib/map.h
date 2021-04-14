
#pragma once

template<typename K> using Hash = u32(K);

template<typename K, typename V, typename A = Mdefault, Hash<K> H = hash> struct Map {
private:
    struct Slot;

public:
    static const inline f32 max_load_factor = 0.9f;

    explicit Map(u32 capacity = 0) {
        if(capacity == 0) capacity = 8;
        _data = Vec<Slot, A>(ceil_pow2(capacity));
        _data.fill();
        _size = 0;
        _probe = 0;
        _usable = 0;
    }

    explicit Map(const Map& src) = delete;
    Map& operator=(const Map& src) = delete;

    Map(Map&& src) {
        *this = std::move(src);
    }
    Map& operator=(Map&& src) {
        this->~Map();
        _data = std::move(src._data);
        _size = src._size;
        _probe = src._probe;
        _usable = src._usable;
        src._size = 0;
        src._probe = 0;
        src._usable = 0;
        return *this;
    }

    ~Map() {
        _size = 0;
        _probe = 0;
        _usable = 0;
    }

    Map copy() const {
        Map ret;
        ret._data = _data.copy();
        ret._size = _size;
        ret._probe = _probe;
        ret._usable = _usable;
        return ret;
    }

    void grow() {
        Vec<Slot, A> next(_data.size() ? _data.size() * 2 : 8);
        next.fill();
        Vec<Slot, A> prev(std::move(_data));
        _data = std::move(next);

        _size = _probe = 0;
        _usable = (u32)std::floor(_data.capacity() * max_load_factor);
        for(auto& e : prev) {
            if(e.valid()) insert(e.key, std::move(e.value));
        }
    }

    void clear() {
        _data.clear();
        _size = _probe = _usable = 0;
    }

    bool empty() const {
        return _size == 0;
    }
    bool full() const {
        return _size == _usable;
    }
    u32 size() const {
        return _size;
    }

    template<typename U = V>
    requires std::is_copy_constructible_v<U>
    V& insert(K key, const V& value) {
        return insert(key, V{value});
    }

    V& insert(K key, V&& value) {
        if(full()) grow();

        u32 idx = H(key) & (_data.capacity() - 1), distance = 0;

        Slot *placement = null, new_slot;
        new_slot.key = key;
        new_slot.value = std::move(value);
        new_slot.set_bucket(idx);
        new_slot.set_valid(true);

        for(;;) {
            Slot& here = _data[idx];
            if(here.valid()) {
                if(here.key == key) {
                    here = std::move(new_slot);
                    return here.value;
                }
                i32 here_dist = idx > here.bucket() ? idx - here.bucket()
                                                    : idx + _data.capacity() - here.bucket();
                if((u32)here_dist < distance) {
                    if(!placement) placement = &here;
                    std::swap(here, new_slot);
                    distance = here_dist;
                }
                distance++;
                idx = idx == _data.capacity() - 1 ? 0 : idx + 1;
                _probe = _MAX(_probe, distance);
            } else {
                here = std::move(new_slot);
                _size++;
                if(placement) return placement->value;
                return _data[idx].value;
            }
        }
    }

    Maybe_Ref<V> try_get(K key) {
        if(empty()) return Maybe_Ref<V>();

        u32 bucket = H(key) & (_data.capacity() - 1);
        u32 idx = bucket, distance = 0;
        for(;;) {
            Slot& s = _data[idx];
            if(s.valid() && s.key == key) return Maybe_Ref<V>(s.value);
            distance++;
            if(distance > _probe) return Maybe_Ref<V>();
            idx = idx == _data.capacity() - 1 ? 0 : idx + 1;
        }
    }

    Maybe_Ref<const V> try_get(K key) const {
        if(empty()) return Maybe_Ref<const V>();

        u32 bucket = H(key) & (_data.capacity() - 1);
        u32 idx = bucket, distance = 0;
        for(;;) {
            Slot& s = _data[idx];
            if(s.valid() && s.key == key) return Maybe_Ref<const V>(s.value);
            distance++;
            if(distance > _probe) return Maybe_Ref<const V>();
            idx = idx == _data.capacity() - 1 ? 0 : idx + 1;
        }
    }

    bool try_erase(K key) {
        u32 bucket = H(key) & (_data.capacity() - 1);
        u32 idx = bucket, distance = 0;
        for(;;) {
            Slot& s = _data[idx];
            if(s.valid() && s.key == key) {
                s.~Slot();
                _size--;
                return true;
            }
            distance++;
            if(distance > _probe) return false;
            idx = idx == _data.capacity() - 1 ? 0 : idx + 1;
        }
    }

    V& get_or_insert(K key, V&& value = V()) {
        Maybe_Ref<V> entry = try_get(key);
        if(entry.ok()) {
            return entry.value().get();
        }
        return insert(key, std::move(value));
    }

    template<typename U = V>
    requires std::is_copy_constructible_v<U>
    V& get_or_insert(K key, const V& value = V()) {
        return get_or_insert(key, V{value});
    }

    V& get(K key) {
        Maybe_Ref<V> value = try_get(key);
        if(!value.ok()) die("Failed to find key %!", key);
        return value.value().get();
    }

    const V& get(K key) const {
        Maybe_Ref<V> value = try_get(key);
        if(!value.ok()) die("Failed to find key %!", key);
        return value.value().get();
    }

    void erase(K key) {
        if(!try_erase(key)) die("Failed to erase key %!", key);
    }

    template<typename M, typename S> struct itr {
        itr(M& map, u32 idx) : _map(map) {
            _place = idx;
            while(_place < _map._data.capacity() && !_map._data[_place].valid()) _place++;
        }
        itr operator++(int) {
            itr i = *this;
            do {
                _place++;
            } while(_place < _map._data.capacity() && !_map._data[_place].valid());
            return i;
        }
        itr operator++() {
            do {
                _place++;
            } while(_place < _map._data.capacity() && !_map._data[_place].valid());
            return *this;
        }
        S& operator*() {
            return _map._data[_place];
        }
        S* operator->() {
            return &_map._data[_place];
        }
        bool operator==(const itr& rhs) {
            return &_map == &rhs._map && _place == rhs._place;
        }
        bool operator!=(const itr& rhs) {
            return &_map != &rhs._map || _place != rhs._place;
        }

        M& _map;
        u32 _place = 0;
    };
    typedef itr<Map, Slot> iterator;
    typedef itr<const Map, const Slot> const_iterator;

    const_iterator begin() const {
        return const_iterator(*this, 0);
    }
    const_iterator end() const {
        return const_iterator(*this, _data.capacity());
    }
    iterator begin() {
        return iterator(*this, 0);
    }
    iterator end() {
        return iterator(*this, _data.capacity());
    }

private:
    struct Slot {
        K key;
        V value;

        Slot() = default;
        Slot(const Slot& src) = default;
        Slot& operator=(const Slot& src) = default;
        Slot(Slot&& src) = default;
        Slot& operator=(Slot&& src) = default;
        ~Slot() {
            _bucket = 0;
        }

        u32 bucket() const {
            return _bucket >> 1;
        }
        bool valid() const {
            return _bucket & 1;
        }
        void set_valid(bool v) {
            _bucket = (_bucket & ~1) | !!v;
        }
        void set_bucket(u32 b) {
            _bucket = (b << 1) | (_bucket & 1);
        }

    private:
        u32 _bucket = 0; // low bit set if valid
    };

    // capacity should always be a power of 2
    Vec<Slot, A> _data;
    u32 _size = 0, _probe = 0, _usable = 0;
    friend struct Type_Info<Map>;
};

template<typename K, typename V, typename A, Hash<K> H> struct Type_Info<Map<K, V, A, H>> {
    static constexpr char name[] = "Map";
    static constexpr usize size = sizeof(Map<K, V, A, H>);
    static constexpr Type_Type type = Type_Type::record_;
    static constexpr char _data[] = "data";
    static constexpr char _size[] = "size";
    static constexpr char _probe[] = "probe";
    static constexpr char _usable[] = "usable";
    typedef Map<K, V, A, H> __offset;
    using members = Type_List<
        Record_Field<Vec<typename Map<K, V, A, H>::Slot, A>, offsetof(__offset, _data), _data>,
        Record_Field<u32, offsetof(__offset, _size), _size>,
        Record_Field<u32, offsetof(__offset, _probe), _probe>,
        Record_Field<u32, offsetof(__offset, _usable), _usable>>;
};
