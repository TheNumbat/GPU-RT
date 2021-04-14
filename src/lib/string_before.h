
#pragma once

#include <cstring>

template<typename A> struct astring;
template<typename T> struct Type_Info;

// Literal string/string_view
template<> struct astring<void> {

    explicit astring() = default;
    astring(const astring& src) = default;
    astring(astring&& src) = default;

    astring& operator=(const astring& src) = default;
    astring& operator=(astring&& src) = default;

    astring(const char* lit) {
        _str = (char*)lit;
        _len = (u32)std::strlen(lit) + 1;
    }
    explicit astring(const char* arr, u32 c) {
        _str = (char*)arr;
        _len = c;
    }

    char* str() {
        return _str;
    }
    const char* str() const {
        return _str;
    }
    u32 len() const {
        return _len;
    }

    char operator[](u32 idx) const;
    astring sub_end(u32 s) const;
    astring sub_begin(u32 s) const;

    const char* begin() {
        return _str;
    }
    const char* end() {
        if(_len) return _str + _len - 1;
        return _str;
    }

    bool starts_with(astring<void> prefix) const;

    // danger, writes to "literal" string
    template<typename SA> u32 write(u32 idx, const astring<SA>& cpy);
    u32 write(u32 idx, const astring<void>& cpy);
    u32 write(u32 idx, char cpy);
    void cut(u32 s);

private:
    char* _str = null;
    u32 _len = 0;
    friend struct Type_Info<astring<void>>;
};

template<typename A> struct astring {

    explicit astring() = default;
    explicit astring(const astring& src) = delete;
    astring& operator=(const astring& src) = delete;
    explicit astring(u32 cap);

    ~astring() {
        A::dealloc(_str);
        _str = null;
        _len = _cap = 0;
    }

    astring(astring&& src) {
        *this = std::move(src);
    }
    astring& operator=(astring&& src) {
        this->~astring();
        _str = src._str;
        _len = src._len;
        _cap = src._cap;
        src._str = null;
        src._len = 0;
        src._cap = 0;
    }

    astring copy() const {
        astring ret(_cap);
        ret._len = _len;
        std::memcpy(ret._str, _str, _len);
        return ret;
    }

    astring<void> view() const {
        return astring(_str, _len);
    }

    char operator[](u32 idx) const;
    char& operator[](u32 idx);

    char* begin() {
        return _str;
    }
    char* end() {
        if(_len) return _str + _len - 1;
        return _str;
    }

    char* str() {
        return _str;
    }
    const char* str() const {
        return _str;
    }
    u32 len() const {
        return _len;
    }
    u32 cap() const {
        return _cap;
    }

    void cut(u32 s);
    void set_len(u32 len) {
        _len = len;
    }

    astring<void> sub_end(u32 s) const;
    astring<void> sub_begin(u32 s) const;

    bool starts_with(astring<void> prefix) const;

    template<typename SA> u32 write(u32 idx, const astring<SA>& cpy);
    u32 write(u32 idx, const astring<void>& cpy);
    u32 write(u32 idx, char cpy);

private:
    char* _str = null;
    u32 _cap = 0;
    u32 _len = 0;
    friend struct Type_Info<astring>;
};

using string = astring<Mdefault>;
using literal = astring<void>;

static thread_local char g_scratch_buf_underlying[4096];
static thread_local literal g_scratch_buf(g_scratch_buf_underlying, 4096);

template<typename L, typename R> bool operator==(astring<L> l, astring<R> r) {
    if(l.len() != r.len()) return false;
    return strncmp(l.str(), r.str(), l.len()) == 0;
}

template<typename L, typename R> bool operator!=(astring<L> l, astring<R> r) {
    if(l.len() != r.len()) return true;
    return strncmp(l.str(), r.str(), l.len()) != 0;
}

template<typename A> astring<A> last_file(astring<A> path);
