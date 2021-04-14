
#pragma once

template<typename T, typename... Ts> struct Type_List {
    using head = T;
    using tail = Type_List<Ts...>;
};
template<typename T> struct Type_List<T> {
    using head = T;
    using tail = void;
};
template<> struct Type_List<void> {
    using head = void;
    using tail = void;
};

template<typename T> struct No_Const { using type = T; };
template<typename T> struct No_Const<const T> { using type = T; };

template<typename T> struct No_Ptr { using type = T; };
template<typename T> struct No_Ptr<T*> { using type = T; };

template<typename T> struct No_Ref { using type = T; };
template<typename T> struct No_Ref<T&> { using type = T; };
template<typename T> struct No_Ref<T&&> { using type = T; };

template<bool b, typename T, typename F> struct Conditional {};
template<typename T, typename F> struct Conditional<true, T, F> { using type = T; };
template<typename T, typename F> struct Conditional<false, T, F> { using type = F; };

template<typename L, typename R> struct Is_Same { static constexpr bool value = false; };
template<typename T> struct Is_Same<T, T> { static constexpr bool value = true; };

template<typename L, typename R> concept Same_As = Is_Same<L, R>::value;

template<typename T, typename H, typename... Ts> struct All_Type {
    static constexpr bool value = Is_Same<T, H>::value && All_Type<T, Ts...>::value;
};
template<typename T, typename H> struct All_Type<T, H> {
    static constexpr bool value = Is_Same<T, H>::value;
};

template<typename T> struct Is_Float { static constexpr bool value = false; };
template<> struct Is_Float<f32> { static constexpr bool value = true; };
template<> struct Is_Float<f64> { static constexpr bool value = true; };

enum class Type_Type : u8 {
    void_,
    char_,
    int_,
    float_,
    bool_,
    string_,
    array_,
    ptr_,
    fptr_,
    enum_,
    record_
};

template<bool E, class Result = void>
struct Enable_If {};

template<class Result>
struct Enable_If<true, Result> {
    using type = Result;
};

template<typename E, E V, const char* N> struct Enum_Field {
    static constexpr char const* name = N;
    static constexpr E val = V;
};
template<typename T, usize O, const char* N> struct Record_Field {
    static constexpr char const* name = N;
    static constexpr usize offset = O;
    using type = T;
};

template<typename T> struct Type_Info;

template<typename E, typename H, typename T, typename F> struct enum_iterator {
    static void traverse(F&& f) {
        f(H::val, H::name);
        enum_iterator<E, typename T::head, typename T::tail>::traverse(f);
    }
};
template<typename E, typename H, typename F> struct enum_iterator<E, H, void, F> {
    static void traverse(F&& f) {
        f(H::val, H::name);
    }
};
template<typename E, typename F> struct enum_iterator<E, void, void, F> {
    static void traverse(F&&) {
    }
};
template<typename E, typename F> void enum_iterate(F&& f) {
    enum_iterator<E, typename Type_Info<E>::members::head,
                  typename Type_Info<E>::members::tail>::traverse(f);
}
template<typename E> literal enum_name(E val) {
    const char* name = null;
    enum_iterate<E>([&name, val](E cval, literal cname) {
        if(cval == val) name = cname.str();
    });
    return name ? name : "???";
}

template<typename T> struct Type_Info<const T> : Type_Info<typename No_Const<T>::type> {
    static constexpr bool is_const = true;
};

template<typename T> struct Type_Info<T&> : Type_Info<typename No_Ref<T>::type> {
    static constexpr bool is_ref = true;
};

template<typename T> struct Type_Info<T&&> : Type_Info<typename No_Ref<T>::type> {
    static constexpr bool is_rref = true;
};

template<typename T> struct Type_Info<T*> {
    using to = typename No_Const<T>::type;
    decltype(Type_Info<to>::name) name = Type_Info<to>::name;
    static constexpr usize size = sizeof(typename No_Const<T>::type*);
    static constexpr Type_Type type = Type_Type::ptr_;
};

template<typename T, usize N> struct Type_Info<T[N]> {
    using underlying = T;
    decltype(Type_Info<T>::name) name = Type_Info<T>::name;
    static constexpr usize size = sizeof(T[N]);
    static constexpr usize len = N;
    static constexpr Type_Type type = Type_Type::array_;
};

template<typename T, usize N> struct Type_Info<const T[N]> {
    using underlying = T;
    decltype(Type_Info<T>::name) name = Type_Info<T>::name;
    static constexpr usize size = sizeof(T[N]);
    static constexpr usize len = N;
    static constexpr Type_Type type = Type_Type::array_;
};

template<> struct Type_Info<void> {
    static constexpr char name[] = "void";
    static constexpr usize size = 0u;
    static constexpr Type_Type type = Type_Type::void_;
};
template<> struct Type_Info<decltype(nullptr)> {
    static constexpr char name[] = "nullptr";
    static constexpr usize size = sizeof(nullptr);
    static constexpr Type_Type type = Type_Type::ptr_;
    using to = void;
};
template<> struct Type_Info<std::thread::id> {
    static constexpr char name[] = "thread_id";
    static constexpr usize size = sizeof(std::thread::id);
    static constexpr Type_Type type = Type_Type::int_;
};
template<> struct Type_Info<char> {
    static constexpr char name[] = "char";
    static constexpr usize size = sizeof(char);
    static constexpr bool sgn = true;
    static constexpr Type_Type type = Type_Type::char_;
};
template<> struct Type_Info<i8> {
    static constexpr char name[] = "i8";
    static constexpr usize size = sizeof(i8);
    static constexpr bool sgn = true;
    static constexpr Type_Type type = Type_Type::int_;
};
template<> struct Type_Info<u8> {
    static constexpr char name[] = "u8";
    static constexpr usize size = sizeof(u8);
    static constexpr bool sgn = false;
    static constexpr Type_Type type = Type_Type::int_;
};
template<> struct Type_Info<i16> {
    static constexpr char name[] = "i16";
    static constexpr usize size = sizeof(i16);
    static constexpr bool sgn = true;
    static constexpr Type_Type type = Type_Type::int_;
};
template<> struct Type_Info<u16> {
    static constexpr char name[] = "u16";
    static constexpr usize size = sizeof(u16);
    static constexpr bool sgn = false;
    static constexpr Type_Type type = Type_Type::int_;
};
template<> struct Type_Info<i32> {
    static constexpr char name[] = "i32";
    static constexpr usize size = sizeof(i32);
    static constexpr bool sgn = true;
    static constexpr Type_Type type = Type_Type::int_;
};
template<> struct Type_Info<u32> {
    static constexpr char name[] = "u32";
    static constexpr usize size = sizeof(u32);
    static constexpr bool sgn = false;
    static constexpr Type_Type type = Type_Type::int_;
};
template<> struct Type_Info<i64> {
    static constexpr char name[] = "i64";
    static constexpr usize size = sizeof(i64);
    static constexpr bool sgn = true;
    static constexpr Type_Type type = Type_Type::int_;
};
template<> struct Type_Info<u64> {
    static constexpr char name[] = "u64";
    static constexpr usize size = sizeof(u64);
    static constexpr bool sgn = false;
    static constexpr Type_Type type = Type_Type::int_;
};
template<> struct Type_Info<f32> {
    static constexpr char name[] = "f32";
    static constexpr usize size = sizeof(f32);
    static constexpr Type_Type type = Type_Type::float_;
};
template<> struct Type_Info<f64> {
    static constexpr char name[] = "f64";
    static constexpr usize size = sizeof(f64);
    static constexpr Type_Type type = Type_Type::float_;
};
template<> struct Type_Info<bool> {
    static constexpr char name[] = "bool";
    static constexpr usize size = sizeof(bool);
    static constexpr Type_Type type = Type_Type::bool_;
};

template<typename T> struct Maybe {

    explicit Maybe() {
    }
    explicit Maybe(T&& value) {
        new(_value) T(std::move(std::forward<T>(value)));
        _ok = true;
    }

    ~Maybe() {
        if(_ok) {
            ((T*)_value)->~T();
        }
        _ok = false;
    }

    Maybe(const Maybe& src) = delete;
    Maybe& operator=(const Maybe& src) = delete;

    Maybe(const Maybe&& src) {
        _ok = src._ok;
        std::memcpy(_value, src._value, sizeof(T));
        src._ok = false;
    }
    Maybe& operator=(const Maybe&& src) {
        _ok = src._ok;
        std::memcpy(_value, src._value, sizeof(T));
        src._ok = false;
    }

    bool ok() const {
        return _ok;
    }

    T& value() {
        assert(_ok);
        return (T&)*(T*)_value;
    }
    const T& value() const {
        assert(_ok);
        return (const T&)*(T*)_value;
    }

    T&& move() {
        assert(_ok);
        return (T &&) * (T*)_value;
    }

    operator bool() const {
        return _ok;
    }

protected:
    bool _ok = false;
    char _value[sizeof(T)];
    friend struct Type_Info<Maybe>;
};

template<typename T> struct Maybe_Ref : Maybe<std::reference_wrapper<T>> {
    explicit Maybe_Ref() {
    }
    explicit Maybe_Ref(T& value) : Maybe<std::reference_wrapper<T>>(value) {
    }
    friend struct Type_Info<Maybe_Ref>;
};

template<typename T> struct Type_Info<Maybe<T>> {
    static constexpr char name[] = "Maybe";
    static constexpr usize size = sizeof(Maybe<T>);
    static constexpr Type_Type type = Type_Type::record_;
    static constexpr char _ok[] = "ok";
    static constexpr char _value[] = "value";
    using members = Type_List<Record_Field<T*, offsetof(Maybe<T>, _ok), _ok>,
                              Record_Field<u32, offsetof(Maybe<T>, _value), _value>>;
};

template<typename T> struct Type_Info<Maybe_Ref<T>> {
    static constexpr char name[] = "Maybe";
    static constexpr usize size = sizeof(Maybe_Ref<T>);
    static constexpr Type_Type type = Type_Type::record_;
    static constexpr char _ok[] = "ok";
    static constexpr char _value[] = "value";
    using members = Type_List<Record_Field<T*, offsetof(Maybe_Ref<T>, _ok), _ok>,
                              Record_Field<u32, offsetof(Maybe_Ref<T>, _value), _value>>;
};
