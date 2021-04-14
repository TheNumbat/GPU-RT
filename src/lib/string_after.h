
#pragma once

template<typename A> struct Type_Info<astring<A>> {
    static constexpr char name[] = "string";
    static constexpr usize size = sizeof(astring<A>);
    static constexpr Type_Type type = Type_Type::string_;
};

template<typename A>
inline astring<A>::astring(u32 cap) {
    assert(cap > 0);
    _str = A::template alloc<char>(cap);
    _cap = cap;
    _len = 1;
}

template<typename A>
inline char astring<A>::operator[](u32 idx) const {
    assert(idx < _len);
    return _str[idx];
}

template<typename A>
inline char& astring<A>::operator[](u32 idx) {
    assert(idx < _len);
    return _str[idx];
}

template<typename A>
inline void astring<A>::cut(u32 s) {
    assert(s <= _len);
    _len = _cap = s;
    _str[_len - 1] = 0;
}

template<typename A> 
inline literal astring<A>::sub_end(u32 s) const {
    assert(s <= _len);
    literal ret;
    ret._str = _str + s;
    ret._len = _len - s;
    return ret;
}

template<typename A> 
inline literal astring<A>::sub_begin(u32 s) const {
    assert(s <= _len);
    return literal(_str, s);
}

inline void literal::cut(u32 s) {
    assert(s <= _len);
    _len = s;
    _str[_len - 1] = 0;
}

inline literal literal::sub_end(u32 s) const {
    assert(s <= _len);
    return literal(_str + s, _len - s);
}

inline literal literal::sub_begin(u32 s) const {
    assert(s <= _len);
    return literal(_str, s);
}

template<typename A> template<typename SA> 
inline u32 astring<A>::write(u32 idx, const astring<SA>& cpy) {
    assert(_cap && idx + cpy._len - 1 < _len);
    std::memcpy(_str + idx, cpy._str, cpy._len - 1);
    return cpy._len - 1;
}

template<typename A> 
inline u32 astring<A>::write(u32 idx, const astring<void>& cpy) {
    assert(_cap && idx + cpy._len - 1 < _len);
    std::memcpy(_str + idx, cpy._str, cpy._len - 1);
    return cpy._len - 1;
}

template<typename A> 
inline u32 astring<A>::write(u32 idx, char cpy) {
    assert(_cap && idx < _len);
    _str[idx] = cpy;
    return 1;
}

template<typename SA> inline u32 literal::write(u32 idx, const astring<SA>& cpy) {
    assert(idx + cpy.len() - 1 < _len);
    std::memcpy(_str + idx, cpy.str(), cpy.len() - 1);
    return cpy.len() - 1;
}

inline u32 literal::write(u32 idx, const astring<void>& cpy) {
    assert(idx + cpy.len() - 1 < _len);
    std::memcpy(_str + idx, cpy.str(), cpy.len() - 1);
    return cpy.len() - 1;
}

inline u32 literal::write(u32 idx, char cpy) {
    assert(idx < _len);
    _str[idx] = cpy;
    return 1;
}

template<typename A>
inline astring<A> last_file(astring<A> path) {

    if(path.len() == 0) return "";

    u32 loc = path.len() - 1;
    int found = 0;
    for(; loc != 0; loc--) {
        if(path[loc] == '\\' || path[loc] == '/') {
            found = 1;
            break;
        }
    }
    return path.sub_end(loc + found);
}

inline char literal::operator[](u32 idx) const {
    assert(idx < _len);
    return _str[idx];
}

inline bool literal::starts_with(literal prefix) const {
    return strncmp(prefix._str, _str, prefix.len() - 1) == 0;
}

template<typename A>
inline bool astring<A>::starts_with(literal prefix) const {
    return strncmp(prefix._str, _str, prefix.len() - 1) == 0;
}

