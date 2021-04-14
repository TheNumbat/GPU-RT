
#pragma once

template<typename... Ts> literal scratch_format(literal fmt, Ts&&... args);

template<typename A, typename E, Type_Type T> struct format_type {};

template<typename A> struct format_type<A, void, Type_Type::void_> {
    static u32 write(astring<A> out, u32 idx) {
        return out.write(idx, "void");
    }
    static u32 size() {
        return 4;
    }
};

template<typename A, typename E> struct format_type<A, E, Type_Type::int_> {
    static u32 write(astring<A> out, u32 idx, const E& val) {
        if(Type_Info<E>::sgn) {
            switch(Type_Info<E>::size) {
            case 1: return snprintf(out.str() + idx, out.len() - idx, "%hhd", (char)val);
            case 2: return snprintf(out.str() + idx, out.len() - idx, "%hd", (short)val);
            case 4: return snprintf(out.str() + idx, out.len() - idx, "%d", (int)val);
            case 8: return snprintf(out.str() + idx, out.len() - idx, "%lld", (long long int)val);
            }
        } else {
            switch(Type_Info<E>::size) {
            case 1: return snprintf(out.str() + idx, out.len() - idx, "%hhu", (unsigned char)val);
            case 2: return snprintf(out.str() + idx, out.len() - idx, "%hu", (unsigned short)val);
            case 4: return snprintf(out.str() + idx, out.len() - idx, "%u", (unsigned int)val);
            case 8:
                return snprintf(out.str() + idx, out.len() - idx, "%llu",
                                (long long unsigned int)val);
            }
        }
        return 0;
    }
    static u32 size(const E& val) {
        if(Type_Info<E>::sgn) {
            switch(Type_Info<E>::size) {
            case 1: return snprintf(null, 0, "%hhd", (char)val);
            case 2: return snprintf(null, 0, "%hd", (short)val);
            case 4: return snprintf(null, 0, "%d", (int)val);
            case 8: return snprintf(null, 0, "%lld", (long long int)val);
            }
        } else {
            switch(Type_Info<E>::size) {
            case 1: return snprintf(null, 0, "%hhu", (unsigned char)val);
            case 2: return snprintf(null, 0, "%hu", (unsigned short)val);
            case 4: return snprintf(null, 0, "%u", (unsigned int)val);
            case 8: return snprintf(null, 0, "%llu", (long long unsigned int)val);
            }
        }
        return 0;
    }
};

template<typename A, typename E> struct format_type<A, E, Type_Type::float_> {
    static u32 write(astring<A> out, u32 idx, const E& val) {
        return snprintf(out.str() + idx, out.len() - idx, "%f", val);
    }
    static u32 size(const E& val) {
        return snprintf(null, 0, "%f", val);
    }
};

template<typename A, typename E> struct format_type<A, E, Type_Type::bool_> {
    static u32 write(astring<A> out, u32 idx, const E& val) {
        return val ? out.write(idx, "true") : out.write(idx, "false");
    }
    static u32 size(const E& val) {
        return val ? 4 : 5;
    }
};

template<typename A, typename E> struct format_type<A, E, Type_Type::char_> {
    static u32 write(astring<A> out, u32 idx, const E& val) {
        if(val == '\0')
            return out.write(idx, ' ');
        else
            return out.write(idx, val);
    }
    static u32 size(const E& val) {
        return 1;
    }
};

template<typename A> struct format_type<A, char const*, Type_Type::ptr_> {
    static u32 write(astring<A> out, u32 idx, char const* val) {
        return out.write(idx, literal(val));
    }
    static u32 size(char const* val) {
        return (u32)std::strlen(val);
    }
};

template<typename A> struct format_type<A, void*, Type_Type::ptr_> {
    static u32 write(astring<A> out, u32 idx, void* val) {
        return snprintf(out.str() + idx, out.len() - idx, "%p", val);
    }
    static u32 size(void* val) {
        return snprintf(null, 0, "%p", val);
    }
};

template<typename A, typename E> struct format_type<A, E, Type_Type::array_> {
    using underlying = typename Type_Info<E>::underlying;
    using format_underlying = format_type<A, underlying, Type_Info<underlying>::type>;

    static u32 write(astring<A> out, u32 idx, const E& val) {
        u32 start = idx;
        idx += out.write(idx, "[");

        for(u32 i = 0; i < Type_Info<E>::len; i++) {
            idx += format_underlying::write(out, idx, val[i]);
            if(i != Type_Info<E>::len - 1) {
                idx += out.write(idx, ", ");
            }
        }

        idx += out.write(idx, "]");
        return idx - start;
    }
    static u32 size(const E& val) {
        u32 idx = 2;
        for(u32 i = 0; i < Type_Info<E>::len; i++) {
            idx += format_underlying::size(val[i]);
            if(i != Type_Info<E>::len - 1) {
                idx += 2;
            }
        }
        return idx;
    }
};

template<typename A, typename E> struct format_type<A, E, Type_Type::ptr_> {
    using to = typename Type_Info<E>::to;
    using format_to = format_type<A, typename No_Ref<to>::type, Type_Info<to>::type>;

    static u32 write(astring<A> out, u32 idx, const E& val) {
        u32 start = idx;
        idx += out.write(idx, '(');
        if(val)
            idx += format_to::write(out, idx, *val);
        else
            idx += out.write(idx, "null");
        idx += out.write(idx, ')');
        return idx - start;
    }
    static u32 size(const E& val) {
        if(val)
            return 2 + format_to::size(*val);
        else
            return 6;
    }
};

template<typename A> struct format_type<A, decltype(nullptr), Type_Type::ptr_> {
    static u32 write(astring<A> out, u32 idx, decltype(nullptr) val) {
        return out.write(idx, "(null)");
    }
    static u32 size(decltype(nullptr) val) {
        return 6;
    }
};

template<typename A, typename E> struct format_type<A, E, Type_Type::fptr_> {
    static u32 write(astring<A> out, u32 idx, const E& val) {
        u32 start = idx;
        idx += out.write(idx, '(');
        if(val)
            idx += out.write(idx, Type_Info<E>::name);
        else
            idx += out.write(idx, "null");
        idx += out.write(idx, ')');
        return idx - start;
    }
    static u32 size(const E& val) {
        if(val)
            return 2 + (u32)std::strlen(Type_Info<E>::name);
        else
            return 6;
    }
};

// NOTE(max): format_member could be replaced with the new enum_iterate, but too lazy at the moment.

template<typename A, typename E, typename H, typename T> struct format_member {
    static u32 write(astring<A> out, u32 idx, const E& val) {
        if(val == H::val)
            return out.write(idx, H::name);
        else
            return format_member<A, typename No_Ref<E>::type, typename T::head,
                                 typename T::tail>::write(out, idx, val);
    }
    static u32 size(const E& val) {
        if(val == H::val)
            return (u32)std::strlen(H::name);
        else
            return format_member<A, typename No_Ref<E>::type, typename T::head,
                                 typename T::tail>::size(val);
    }
};

template<typename A, typename E, typename H> struct format_member<A, E, H, void> {
    static u32 write(astring<A> out, u32 idx, const E& val) {
        if(val == H::val)
            return out.write(idx, H::name);
        else
            return out.write(idx, "???");
    }
    static u32 size(const E& val) {
        if(val == H::val)
            return (u32)std::strlen(H::name);
        else
            return 3;
    }
};

template<typename A, typename E> struct format_member<A, E, void, void> {
    static u32 write(astring<A> out, u32 idx, const E& val) {
        return out.write(idx, "???");
    }
    static u32 size(const E& val) {
        return 3;
    }
};

template<typename A, typename E> struct format_type<A, E, Type_Type::enum_> {
    using underlying = typename Type_Info<E>::underlying;
    using members = typename Type_Info<E>::members;

    static u32 write(astring<A> out, u32 idx, const E& val) {
        u32 start = idx;
        idx += out.write(idx, Type_Info<E>::name);
        idx += out.write(idx, "::");
        idx += format_member<A, typename No_Ref<E>::type, typename members::head,
                             typename members::tail>::write(out, idx, val);
        return idx - start;
    }
    static u32 size(const E& val) {
        u32 idx = 2;
        idx += (u32)std::strlen(Type_Info<E>::name);
        idx += format_member<A, typename No_Ref<E>::type, typename members::head,
                             typename members::tail>::size(val);
        return idx;
    }
};

template<typename A, typename E, typename H, typename T> struct format_field {
    using member = typename H::type;
    static u32 write(astring<A> out, u32 idx, const E& val) {
        u32 start = idx;
        idx += out.write(idx, H::name);
        idx += out.write(idx, " : ");
        idx += format_type<A, typename No_Ref<member>::type, Type_Info<member>::type>::write(
            out, idx, *(member*)((char*)&val + H::offset));
        idx += out.write(idx, ", ");
        idx += format_field<A, E, typename T::head, typename T::tail>::write(out, idx, val);
        return idx - start;
    }
    static u32 size(const E& val) {
        u32 idx = 5;
        idx += (u32)std::strlen(H::name);
        idx += format_type<A, typename No_Ref<member>::type, Type_Info<member>::type>::size(
            *(member*)((char*)&val + H::offset));
        idx += format_field<A, E, typename T::head, typename T::tail>::size(val);
        return idx;
    }
};

template<typename A, typename E, typename H> struct format_field<A, E, H, void> {
    using member = typename H::type;
    static u32 write(astring<A> out, u32 idx, const E& val) {
        u32 start = idx;
        idx += out.write(idx, H::name);
        idx += out.write(idx, " : ");
        idx += format_type<A, typename No_Ref<member>::type, Type_Info<member>::type>::write(
            out, idx, *(member*)((char*)&val + H::offset));
        return idx - start;
    }
    static u32 size(const E& val) {
        u32 idx = 3;
        idx += (u32)std::strlen(H::name);
        idx += format_type<A, typename No_Ref<member>::type, Type_Info<member>::type>::size(
            *(member*)((char*)&val + H::offset));
        return idx;
    }
};

template<typename A, typename E> struct format_field<A, E, void, void> {
    static u32 write(astring<A> out, u32 idx) {
        return 0;
    }
    static u32 size() {
        return 0;
    }
};

template<typename A, typename E> struct format_type<A, E, Type_Type::record_> {
    using members = typename Type_Info<E>::members;

    static u32 write(astring<A> out, u32 idx, const E& val) {
        u32 start = idx;
        idx += out.write(idx, Type_Info<E>::name);
        idx += out.write(idx, '{');
        idx += format_field<A, E, typename members::head, typename members::tail>::write(out, idx,
                                                                                         val);
        idx += out.write(idx, '}');
        return idx - start;
    }
    static u32 size(const E& val) {
        u32 idx = 2;
        idx += (u32)std::strlen(Type_Info<E>::name);
        idx += format_field<A, E, typename members::head, typename members::tail>::size(val);
        return idx;
    }
};

template<typename A, typename... Ts> u32 sprint(astring<A> out, literal fmt, u32 idx) {

    u32 start = idx;
    u32 used = 0;

    while(used < fmt.len() - 1) {
        if(fmt[used] == '%') {
            if(used + 1 < fmt.len()) {
                if(fmt[used + 1] == '%') {
                    used += 1;
                } else {
                    die("Too many format specifiers in format string!");
                    break;
                }
            }
        }

        idx += out.write(idx, fmt[used]);
        used += 1;
    }

    return idx - start;
}
template<typename A, typename T, typename... Ts>
u32 sprint(astring<A> out, literal fmt, u32 idx, T&& first, Ts... args);

template<typename... Ts> u32 sprint_size(literal fmt, u32 idx) {

    u32 start = idx;
    u32 used = 0;

    while(used < fmt.len() - 1) {
        if(fmt[used] == '%') {
            if(used + 1 < fmt.len()) {
                if(fmt[used + 1] == '%') {
                    used += 1;
                } else {
                    die("Too many format specifiers in format string!");
                    break;
                }
            }
        }

        idx += 1;
        used += 1;
    }

    return idx - start;
}
template<typename T, typename... Ts> u32 sprint_size(literal fmt, u32 idx, T&& first, Ts... args);

template<typename A, typename... Ts> astring<A> format(literal fmt, Ts... args);

template<typename... Ts> string format(literal fmt, Ts... args);
