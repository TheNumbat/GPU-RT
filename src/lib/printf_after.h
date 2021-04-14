
#pragma once

template<typename A, typename SA> struct format_type<A, astring<SA>, Type_Type::string_> {
    static u32 write(astring<A> out, u32 idx, const astring<SA>& val) {
        return out.write(idx, val);
    }
    static u32 size(const astring<SA>& val) {
        if(val.len() > 0) return val.len() - 1;
        return 0;
    }
};

template<typename A> struct format_type<A, std::thread::id, Type_Type::int_> {
    static u32 write(astring<A> out, u32 idx, std::thread::id val) {
        return format_type<A, u32, Type_Type::int_>::write(out, idx, hash(val));
    }
    static u32 size(std::thread::id val) {
        return format_type<A, u32, Type_Type::int_>::size(hash(val));
    }
};

template<typename A, typename M, typename T> struct vec_printer {
    static u32 write(astring<A> out, u32 idx, const T& val) {
        u32 start = idx, i = 0;
        idx += out.write(idx, '[');
        for(const auto& item : val) {
            idx +=
                format_type<A, typename No_Ref<M>::type, Type_Info<M>::type>::write(out, idx, item);
            if(i++ != val.size() - 1) idx += out.write(idx, ", ");
        }
        idx += out.write(idx, ']');
        return idx - start;
    }
    static u32 size(const T& val) {
        u32 idx = 2, i = 0;
        for(const auto& item : val) {
            idx += format_type<A, typename No_Ref<M>::type, Type_Info<M>::type>::size(item);
            if(i++ != val.size() - 1) idx += 2;
        }
        return idx;
    }
};

template<typename A, typename M, typename VA>
struct format_type<A, Vec<M, VA>, Type_Type::record_> {

    static u32 write(astring<A> out, u32 idx, const Vec<M, VA>& val) {
        return vec_printer<A, M, Vec<M, VA>>::write(out, idx, val);
    }
    static u32 size(const Vec<M, VA>& val) {
        return vec_printer<A, M, Vec<M, VA>>::size(val);
    }
};

template<typename A, typename T> struct format_type<A, Maybe_Ref<T>, Type_Type::record_> {
    static u32 write(astring<A> out, u32 idx, const Maybe_Ref<T>& val) {
        u32 start = idx;
        if(val.ok()) {
            idx += out.write(idx, "Ok(");
            idx += format_type<A, typename No_Ref<T>::type, Type_Info<T>::type>::write(
                out, idx, val.value().get());
            idx += out.write(idx, ')');
        } else {
            idx += out.write(idx, "None");
        }
        return idx - start;
    }
    static u32 size(const Maybe_Ref<T>& val) {
        u32 idx = 0;
        if(val.ok()) {
            idx += 3;
            idx += format_type<A, typename No_Ref<T>::type, Type_Info<T>::type>::size(
                val.value().get());
            idx += 1;
        } else {
            idx += 4;
        }
        return idx;
    }
};

template<typename A, typename T0, typename T1>
struct format_type<A, Pair<T0, T1>, Type_Type::record_> {
    static u32 write(astring<A> out, u32 idx, const Pair<T0, T1>& val) {
        u32 start = idx;
        idx += out.write(idx, '{');
        idx += format_type<A, typename No_Ref<T0>::type, Type_Info<T0>::type>::write(out, idx,
                                                                                     val.first);
        idx += out.write(idx, ',');
        idx += format_type<A, typename No_Ref<T1>::type, Type_Info<T1>::type>::write(out, idx,
                                                                                     val.second);
        idx += out.write(idx, '}');
        return idx - start;
    }
    static u32 size(const Pair<T0, T1>& val) {
        u32 idx = 0;
        idx += format_type<A, typename No_Ref<T0>::type, Type_Info<T0>::type>::size(val.first);
        idx += format_type<A, typename No_Ref<T1>::type, Type_Info<T1>::type>::size(val.second);
        return idx + 3;
    }
};

template<typename A, typename T> struct format_type<A, Maybe<T>, Type_Type::record_> {
    static u32 write(astring<A> out, u32 idx, const Maybe<T>& val) {
        u32 start = idx;
        if(val.ok()) {
            idx += out.write(idx, "Ok(");
            idx += format_type<A, T, Type_Info<T>::type>::write(out, idx, val.value());
            idx += out.write(idx, ')');
        }
        return idx - start;
    }
    static u32 size(const Maybe<T>& val) {
        u32 idx = 0;
        if(val.ok()) {
            idx += 3;
            idx += format_type<A, T, Type_Info<T>::type>::size(val.value());
            idx += 1;
        }
        return idx;
    }
};

template<typename A, typename M> struct format_type<A, Slice<M>, Type_Type::record_> {
    static u32 write(astring<A> out, u32 idx, const Slice<M>& val) {
        return vec_printer<A, M, Slice<M>>::write(out, idx, val);
    }
    static u32 size(const Slice<M>& val) {
        return vec_printer<A, M, Slice<M>>::size(val);
    }
};

template<typename A, typename M, typename HA>
struct format_type<A, Heap<M, HA>, Type_Type::record_> {
    static u32 write(astring<A> out, u32 idx, const Heap<M, HA>& val) {
        u32 start = idx, i = 0;
        idx += out.write(idx, "heap[");
        for(auto& item : val) {
            idx += format_type<A, M, Type_Info<M>::type>::write(out, idx, item);
            if(i++ != val.size() - 1) idx += out.write(idx, ", ");
        }
        idx += out.write(idx, ']');
        return idx - start;
    }
    static u32 size(const Heap<M, HA>& val) {
        u32 idx = 6, i = 0;
        for(auto& item : val) {
            idx += format_type<A, M, Type_Info<M>::type>::size(item);
            if(i++ != val.size() - 1) idx += 2;
        }
        return idx;
    }
};

template<typename A, typename K, typename V, typename MA, Hash<K> H>
struct format_type<A, Map<K, V, MA, H>, Type_Type::record_> {
    static u32 write(astring<A> out, u32 idx, const Map<K, V, MA, H>& val) {
        u32 start = idx, i = 0;
        idx += out.write(idx, '[');
        for(auto& item : val) {
            idx += out.write(idx, '{');
            idx += format_type<A, const K&, Type_Info<K>::type>::write(out, idx, item.key);
            idx += out.write(idx, " : ");
            idx += format_type<A, const V&, Type_Info<V>::type>::write(out, idx, item.value);
            idx += out.write(idx, '}');
            if(i++ != val.size() - 1) idx += out.write(idx, ", ");
        }
        idx += out.write(idx, ']');
        return idx - start;
    }
    static u32 size(const Map<K, V, MA, H>& val) {
        u32 idx = 2, i = 0;
        for(auto& item : val) {
            idx += 5;
            idx += format_type<A, const K&, Type_Info<K>::type>::size(item.key);
            idx += format_type<A, const V&, Type_Info<V>::type>::size(item.value);
            if(i++ != val.size() - 1) idx += 2;
        }
        return idx;
    }
};

template<typename A, typename M, typename SA>
struct format_type<A, Stack<M, SA>, Type_Type::record_> {
    static u32 write(astring<A> out, u32 idx, const Stack<M, SA>& val) {
        u32 start = idx;
        idx += out.write(idx, "Stack");
        idx += format_type<A, Vec<M, SA>, Type_Info<Vec<M, SA>>::type>::write(out, idx, val.vec());
        return idx - start;
    }
    static u32 size(const Stack<M, SA>& val) {
        u32 idx = 5;
        idx += format_type<A, Vec<M, SA>, Type_Info<Vec<M, SA>>::type>::size(val.vec());
        return idx;
    }
};

template<typename A, typename M, typename QA>
struct format_type<A, Queue<M, QA>, Type_Type::record_> {
    static u32 write(astring<A> out, u32 idx, const Queue<M, QA>& val) {
        u32 start = idx, i = 0;
        idx += out.write(idx, "queue[");
        for(auto& item : val) {
            idx += format_type<A, M, Type_Info<M>::type>::write(out, idx, item);
            if(i++ != val.size() - 1) idx += out.write(idx, ", ");
        }
        idx += out.write(idx, ']');
        return idx - start;
    }
    static u32 size(const Queue<M, QA>& val) {
        u32 idx = 7, i = 0;
        for(auto& item : val) {
            idx += format_type<A, M, Type_Info<M>::type>::size(item);
            if(i++ != val.size() - 1) idx += 2;
        }
        return idx;
    }
};

template<typename A, typename T, usize N> struct format_type<A, vect<T, N>, Type_Type::record_> {
    static u32 write(astring<A> out, u32 idx, const vect<T, N>& val) {
        u32 start = idx, i = 0;
        idx += out.write(idx, "[");
        for(auto& item : val) {
            idx += format_type<A, T, Type_Info<T>::type>::write(out, idx, item);
            if(i++ != N - 1) idx += out.write(idx, ", ");
        }
        idx += out.write(idx, ']');
        return idx - start;
    }
    static u32 size(const vect<T, N>& val) {
        u32 idx = 2, i = 0;
        for(auto& item : val) {
            idx += format_type<A, T, Type_Info<T>::type>::size(item);
            if(i++ != N - 1) idx += 2;
        }
        return idx;
    }
};

template<typename A> struct format_type<A, Mat4, Type_Type::record_> {
    static u32 write(astring<A> out, u32 idx, const Mat4& val) {
        u32 start = idx, i = 0;
        idx += out.write(idx, "[");
        for(auto& item : val) {
            idx += format_type<A, Vec4, Type_Info<Vec4>::type>::write(out, idx, item);
            if(i++ != 3) idx += out.write(idx, ", ");
        }
        idx += out.write(idx, ']');
        return idx - start;
    }
    static u32 size(const Mat4& val) {
        u32 idx = 2, i = 0;
        for(auto& item : val) {
            idx += format_type<A, Vec4, Type_Info<Vec4>::type>::size(item);
            if(i++ != 3) idx += 2;
        }
        return idx;
    }
};

template<typename A, typename T, typename... Ts>
u32 sprint(astring<A> out, literal fmt, u32 idx, T&& first, Ts... args) {

    u32 start = idx;
    u32 used = 0;

    while(true) {
        if(fmt[used] == '%') {
            if(used + 1 < fmt.len() && fmt[used + 1] == '%') {
                used += 1;
            } else {
                break;
            }
        }

        idx += out.write(idx, fmt[used]);
        used += 1;

        if(used == fmt.len() - 1) {
            die("Too few format specifiers in format string!");
        }
    }

    idx += format_type<A, typename No_Ref<T>::type, Type_Info<T>::type>::write(out, idx, first);

    return idx - start + sprint(out, fmt.sub_end(used + 1), idx, args...);
}

template<typename T, typename... Ts> u32 sprint_size(literal fmt, u32 idx, T&& first, Ts... args) {

    u32 start = idx;
    u32 used = 0;

    while(true) {
        if(fmt[used] == '%') {
            if(used + 1 < fmt.len() && fmt[used + 1] == '%') {
                used += 1;
            } else {
                break;
            }
        }

        idx += 1;
        used += 1;

        if(used == fmt.len() - 1) {
            die("Too few format specifiers in format string!");
        }
    }

    idx += format_type<void, typename No_Ref<T>::type, Type_Info<T>::type>::size(first);

    return idx - start + sprint_size(fmt.sub_end(used + 1), idx, std::forward<Ts>(args)...);
}

template<typename A, typename... Ts> astring<A> format(literal fmt, Ts... args) {

    u32 len = sprint_size<Ts...>(fmt, 0, args...);

    astring<A> ret = astring<A>::make(len + 1);
    ret.set_len(len + 1);

    u32 written = sprint<Ts...>(ret, fmt, 0, args...);
    assert(len == written);
    return ret;
}

template<typename... Ts> string format(literal fmt, Ts&&... args) {

    u32 len = sprint_size<Ts...>(fmt, 0, std::forward<Ts>(args)...);

    string ret(len + 1);
    ret.set_len(len + 1);

    u32 written = sprint<Mdefault, Ts...>(ret, fmt, 0, std::forward<Ts>(args)...);
    assert(len == written);

    return ret;
}

template<typename... Ts> literal scratch_format(literal fmt, Ts&&... args) {

    u32 len = sprint_size<Ts...>(fmt, 0, std::forward<Ts>(args)...);

    assert(len < g_scratch_buf.len());

    u32 written = sprint<void, Ts...>(g_scratch_buf, fmt, 0, std::forward<Ts>(args)...);
    assert(len == written);

    g_scratch_buf.write(len, '\0');
    return g_scratch_buf.sub_begin(len + 1);
}
