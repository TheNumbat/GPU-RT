
#pragma once

template<typename A, typename B> struct Pair {

    explicit Pair() {
    }

    Pair(A&& first, B&& second) : first(std::move(first)), second(std::move(second)) {
    }
    Pair(const A& first, const B& second) : first(first), second(second) {
    }

    explicit Pair(const Pair& src) = default;
    Pair& operator=(const Pair& src) = default;

    Pair(Pair&& src) = default;
    Pair& operator=(Pair&& src) = default;

    A first;
    B second;
    friend struct Type_Info<Pair>;
};

namespace std {
    template<typename A, typename B>
    struct tuple_size<Pair<A,B>> {
        static constexpr size_t value = 2;
    };
    template<typename A, typename B>
    struct tuple_element<0, Pair<A,B>> {
        using type = A;
    };
    template<typename A, typename B>
    struct tuple_element<1, Pair<A,B>> {
        using type = B;
    };
}

template<std::size_t Index, typename A, typename B>
std::tuple_element_t<Index, Pair<A,B>>& get(Pair<A,B>& p) {
    if constexpr (Index == 0) return p.first;
    if constexpr (Index == 1) return p.second;
}
template<std::size_t Index, typename A, typename B>
const std::tuple_element_t<Index, Pair<A,B>>& get(const Pair<A,B>& p) {
    if constexpr (Index == 0) return p.first;
    if constexpr (Index == 1) return p.second;
}
template<std::size_t Index, typename A, typename B>
std::tuple_element_t<Index, Pair<A,B>>&& get(Pair<A,B>&& p) {
    if constexpr (Index == 0) return std::move(p.first);
    if constexpr (Index == 1) return std::move(p.second);
}
template<std::size_t Index, typename A, typename B>
const std::tuple_element_t<Index, Pair<A,B>>&& get(const Pair<A,B>&& p) {
    if constexpr (Index == 0) return std::move(p.first);
    if constexpr (Index == 1) return std::move(p.second);
}

template<typename A, typename B> struct Type_Info<Pair<A, B>> {
    static constexpr char name[] = "Pair";
    static constexpr usize size = sizeof(Pair<A, B>);
    static constexpr Type_Type type = Type_Type::record_;
    static constexpr char _first[] = "first";
    static constexpr char _second[] = "second";
    typedef Pair<A, B> __offset;
    using members = Type_List<Record_Field<Pair<A, B>, offsetof(__offset, _first), _first>,
                              Record_Field<Pair<A, B>, offsetof(__offset, _second), _second>>;
};
