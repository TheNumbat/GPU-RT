
#pragma once

#ifdef _WIN32
#include <intrin.h>
#elif defined(__linux__)
#include <pmmintrin.h>
#include <xmmintrin.h>
#endif

template<typename T> struct Vec2_base;
template<typename T> struct Vec3_base;
template<typename T> struct Vec4_base;
template<typename T, usize N> struct VecN_base;

template<typename T, usize N>
using vec_base = typename Conditional<
    N == 2, Vec2_base<T>,
    typename Conditional<N == 3, Vec3_base<T>,
                         typename Conditional<N == 4, Vec4_base<T>, VecN_base<T, N>>::type>::type>::
    type;

template<typename T, usize N> struct vect : vec_base<T, N> {

    using base = vec_base<T, N>;
    static constexpr bool is_simd = N == 4 && Is_Same<T, f32>::value;
    static_assert(N >= 2, "Why use a 1-sized vect?");

    vect() : base{} {};
    vect(__m128 p) : base{} {
        static_assert(is_simd, "Not a SIMD vec!");
        this->pack = p;
    }
    vect(T x) : base{x} {
    }
    vect(T x, T y) : base{x, y} {
    }
    vect(T x, T y, T z) : base{x, y, z} {
    }
    vect(T x, T y, T z, T w) : base{x, y, z, w} {
    }
    template<typename... Ts> vect(Ts... list) : base{list...} {
        static_assert(sizeof...(Ts) == N, "Too few/many arguments!");
        static_assert(All_Type<T, Ts...>::value, "Arguments not of type T!");
    }

    T& operator[](i32 idx) {
        return this->data[idx];
    }
    T operator[](i32 idx) const {
        return this->data[idx];
    }

    vect operator+=(vect v) {
        if constexpr(is_simd)
            this->pack = _mm_add_ps(this->pack, v.pack);
        else
            for(usize i = 0; i < N; i++) this->data[i] += v.data[i];
        return *this;
    }
    vect operator-=(vect v) {
        if constexpr(is_simd)
            this->pack = _mm_sub_ps(this->pack, v.pack);
        else
            for(usize i = 0; i < N; i++) this->data[i] -= v.data[i];
        return *this;
    }
    vect operator*=(vect v) {
        if constexpr(is_simd)
            this->pack = _mm_mul_ps(this->pack, v.pack);
        else
            for(usize i = 0; i < N; i++) this->data[i] *= v.data[i];
        return *this;
    }
    vect operator/=(vect v) {
        if constexpr(is_simd)
            this->pack = _mm_div_ps(this->pack, v.pack);
        else
            for(usize i = 0; i < N; i++) this->data[i] /= v.data[i];
        return *this;
    }

    vect operator+=(T s) {
        if constexpr(is_simd)
            this->pack = _mm_add_ps(this->pack, _mm_set1_ps(s));
        else
            for(usize i = 0; i < N; i++) this->data[i] += s;
        return *this;
    }
    vect operator-=(T s) {
        if constexpr(is_simd)
            this->pack = _mm_sub_ps(this->pack, _mm_set1_ps(s));
        else
            for(usize i = 0; i < N; i++) this->data[i] -= s;
        return *this;
    }
    vect operator*=(T s) {
        if constexpr(is_simd)
            this->pack = _mm_mul_ps(this->pack, _mm_set1_ps(s));
        else
            for(usize i = 0; i < N; i++) this->data[i] *= s;
        return *this;
    }
    vect operator/=(T s) {
        if constexpr(is_simd)
            this->pack = _mm_div_ps(this->pack, _mm_set1_ps(s));
        else
            for(usize i = 0; i < N; i++) this->data[i] /= s;
        return *this;
    }

    vect operator+(vect o) const {
        if constexpr(is_simd)
            return {_mm_add_ps(this->pack, o.pack)};
        else {
            vect r;
            for(usize i = 0; i < N; i++) r.data[i] = this->data[i] + o.data[i];
            return r;
        }
    }
    vect operator-(vect o) const {
        if constexpr(is_simd)
            return {_mm_sub_ps(this->pack, o.pack)};
        else {
            vect r;
            for(usize i = 0; i < N; i++) r.data[i] = this->data[i] - o.data[i];
            return r;
        }
    }
    vect operator*(vect o) const {
        if constexpr(is_simd)
            return {_mm_mul_ps(this->pack, o.pack)};
        else {
            vect r;
            for(usize i = 0; i < N; i++) r.data[i] = this->data[i] * o.data[i];
            return r;
        }
    }
    vect operator/(vect o) const {
        if constexpr(is_simd)
            return {_mm_div_ps(this->pack, o.pack)};
        else {
            vect r;
            for(usize i = 0; i < N; i++) r.data[i] = this->data[i] / o.data[i];
            return r;
        }
    }

    vect operator+(T s) const {
        if constexpr(is_simd)
            return {_mm_add_ps(this->pack, _mm_set1_ps(s))};
        else {
            vect r;
            for(usize i = 0; i < N; i++) r.data[i] = this->data[i] + s;
            return r;
        }
    }
    vect operator-(T s) const {
        if constexpr(is_simd)
            return {_mm_sub_ps(this->pack, _mm_set1_ps(s))};
        else {
            vect r;
            for(usize i = 0; i < N; i++) r.data[i] = this->data[i] - s;
            return r;
        }
    }
    vect operator*(T s) const {
        if constexpr(is_simd)
            return {_mm_mul_ps(this->pack, _mm_set1_ps(s))};
        else {
            vect r;
            for(usize i = 0; i < N; i++) r.data[i] = this->data[i] * s;
            return r;
        }
    }
    vect operator/(T s) const {
        if constexpr(is_simd)
            return {_mm_div_ps(this->pack, _mm_set1_ps(s))};
        else {
            vect r;
            for(usize i = 0; i < N; i++) r.data[i] = this->data[i] / s;
            return r;
        }
    }

    bool operator==(vect o) const {
        if constexpr(is_simd)
            return _mm_movemask_ps(_mm_cmpeq_ps(this->pack, o.pack)) == 0xf;
        else {
            bool good = true;
            for(usize i = 0; good && i < N; i++) good = good && this->data[i] == o.data[i];
            return good;
        }
    }
    bool operator!=(vect o) const {
        return !(*this == o);
    }

    vect abs() const {
        vect r;
        for(usize i = 0; i < N; i++) r.data[i] = abs(this->data[i]);
        return r;
    }
    vect operator-() {
        if constexpr(is_simd)
            this->pack = _mm_sub_ps(_mm_set1_ps(0.0f), this->pack);
        else
            for(usize i = 0; i < N; i++) this->data[i] = -this->data[i];
        return *this;
    }

    vect normalize() {
        static_assert(Is_Float<T>::value, "Not a float vec!");
        T l = len();
        if constexpr(is_simd)
            this->pack = _mm_div_ps(this->pack, _mm_set1_ps(l));
        else
            for(usize i = 0; i < N; i++) this->data[i] /= l;
        return *this;
    }
    vect normalized() const {
        static_assert(Is_Float<T>::value, "Not a float vec!");
        vect r;
        T l = len();
        if constexpr(is_simd)
            r.pack = _mm_div_ps(this->pack, _mm_set1_ps(l));
        else
            for(usize i = 0; i < N; i++) r.data[i] = this->data[i] / l;
        return r;
    }

    T len2() const {
        return dot(*this, *this);
    }
    T len() const {
        static_assert(Is_Float<T>::value, "Not a float vec!");
        return sqrt(len2());
    }

    const T* begin() const {
        return this->data;
    }
    const T* end() const {
        return this->data + N;
    }
    T* begin() {
        return this->data;
    }
    T* end() {
        return this->data + N;
    }
};

template<typename T, usize N> struct Type_Info<vect<T, N>> {
    static constexpr char name[] = "vect";
    static constexpr usize size = sizeof(vect<T, N>);
    static constexpr Type_Type type = Type_Type::record_;
    static constexpr char _data[] = "data";
    using members = Type_List<Record_Field<T[N], 0, _data>>;
};

template<typename T> struct Vec2_base {
    union {
        struct {
            T x, y;
        };
        T data[2];
    };
};
template<typename T> struct Vec3_base {
    union {
        struct {
            T x, y, z;
        };
        struct {
            vect<T, 2> xy;
            T _z;
        };
        struct {
            T _x;
            vect<T, 2> yz;
        };
        T data[3];
    };
};
template<typename T> struct Vec4_base {
    union {
        struct {
            T x, y, z, w;
        };
        struct {
            vect<T, 2> xy;
            vect<T, 2> zw;
        };
        struct {
            T _x;
            vect<T, 2> yz;
            T _w;
        };
        struct {
            vect<T, 3> xyz;
            T __w;
        };
        struct {
            T __x;
            vect<T, 3> yzw;
        };
        T data[4];
    };
};
template<> struct Vec4_base<f32> {
    union {
        struct {
            f32 x, y, z, w;
        };
        struct {
            vect<f32, 2> xy;
            vect<f32, 2> zw;
        };
        struct {
            f32 _x;
            vect<f32, 2> yz;
            f32 _w;
        };
        struct {
            vect<f32, 3> xyz;
            f32 __w;
        };
        struct {
            f32 __x;
            vect<f32, 3> yzw;
        };
        f32 data[4];
        __m128 pack;
    };
};
template<typename T, usize N> struct VecN_base { T data[N]; };

template<typename T, usize N> vect<T, N> min(vect<T, N> x, vect<T, N> y) {
    if constexpr(vect<T, N>::is_simd) return {_mm_min_ps(x.pack, y.pack)};
    vect<T, N> r;
    for(usize i = 0; i < N; i++) r.data[i] = _MIN(x.data[i], y.data[i]);
    return r;
}

template<typename T, usize N> vect<T, N> max(vect<T, N> x, vect<T, N> y) {
    if constexpr(vect<T, N>::is_simd) return {_mm_max_ps(x.pack, y.pack)};
    vect<T, N> r;
    for(usize i = 0; i < N; i++) r.data[i] = _MAX(x.data[i], y.data[i]);
    return r;
}

template<typename T, usize N> T dot(vect<T, N> x, vect<T, N> y) {
    if constexpr(vect<T, N>::is_simd) return vect<f32, 4>{_mm_dp_ps(x.pack, y.pack, 0xf1)}.x;
    T r = {};
    for(usize i = 0; i < N; i++) r += x.data[i] * y.data[i];
    return r;
}

template<typename T, usize N> vect<T, N> lerp(vect<T, N> min, vect<T, N> max, T dist) {
    return min + (max - min) * dist;
}

template<typename T, usize N> vect<T, N> clamp(vect<T, N> x, vect<T, N> min, vect<T, N> max) {
    return max(min(x, max), min);
}

template<typename T, usize N> vect<T, N> normalize(vect<T, N> x) {
    return x.normalized();
}

using Vec2 = vect<f32, 2>;
using Vec3 = vect<f32, 3>;
using Vec4 = vect<f32, 4>;

using Vec2i = vect<i32, 2>;
using Vec3i = vect<i32, 3>;
using Vec4i = vect<i32, 4>;

inline Vec3 cross(Vec3 l, Vec3 r) {
    return {l.y * r.z - l.z * r.y, l.z * r.x - l.x * r.z, l.x * r.y - l.y * r.x};
}

struct Mat4;
inline Mat4 inverse(Mat4 m);
inline Mat4 transpose(Mat4 m);

struct Mat4 {

    union {
        f32 data[16];
        __m128 pack[4];
        Vec4 col[4];
    };

    Mat4()
        : col{{1.0f, 0.0f, 0.0f, 0.0f},
              {0.0f, 1.0f, 0.0f, 0.0f},
              {0.0f, 0.0f, 1.0f, 0.0f},
              {0.0f, 0.0f, 0.0f, 1.0f}} {
    }
    Mat4(Vec4 x, Vec4 y, Vec4 z, Vec4 w) : col{x, y, z, w} {
    }

    static Mat4 look_at(Vec3 pos, Vec3 at, Vec3 up);
    static Mat4 scale(Vec3 s);
    static Mat4 rotate(f32 a, Vec3 axis);
    static Mat4 translate(Vec3 v);
    static Mat4 ortho(f32 l, f32 r, f32 b, f32 t, f32 n, f32 f);
    static Mat4 proj(f32 fov, f32 ar, f32 n);
    static Mat4 transpose(Mat4 m);

    Mat4 operator+=(Mat4 v) {
        for(i32 i = 0; i < 4; i++) pack[i] = _mm_add_ps(pack[i], v.pack[i]);
        return *this;
    }
    Mat4 operator-=(Mat4 v) {
        for(i32 i = 0; i < 4; i++) pack[i] = _mm_sub_ps(pack[i], v.pack[i]);
        return *this;
    }

    Mat4 operator+=(f32 s) {
        __m128 add = _mm_set1_ps(s);
        for(i32 i = 0; i < 4; i++) pack[i] = _mm_add_ps(pack[i], add);
        return *this;
    }
    Mat4 operator-=(f32 s) {
        __m128 sub = _mm_set1_ps(s);
        for(i32 i = 0; i < 4; i++) pack[i] = _mm_sub_ps(pack[i], sub);
        return *this;
    }
    Mat4 operator*=(f32 s) {
        __m128 mul = _mm_set1_ps(s);
        for(i32 i = 0; i < 4; i++) pack[i] = _mm_mul_ps(pack[i], mul);
        return *this;
    }
    Mat4 operator/=(f32 s) {
        __m128 div = _mm_set1_ps(s);
        for(i32 i = 0; i < 4; i++) pack[i] = _mm_div_ps(pack[i], div);
        return *this;
    }

    Vec4& operator[](i32 idx) {
        return col[idx];
    }
    Vec4 operator[](i32 idx) const {
        return col[idx];
    }

    Mat4 operator+(Mat4 m) const {
        Mat4 ret;
        for(i32 i = 0; i < 4; i++) ret.pack[i] = _mm_add_ps(pack[i], m.pack[i]);
        return ret;
    }
    Mat4 operator-(Mat4 m) const {
        Mat4 ret;
        for(i32 i = 0; i < 4; i++) ret.pack[i] = _mm_sub_ps(pack[i], m.pack[i]);
        return ret;
    }

    Mat4 operator+(f32 s) const {
        Mat4 ret;
        __m128 add = _mm_set1_ps(s);
        for(i32 i = 0; i < 4; i++) ret.pack[i] = _mm_add_ps(pack[i], add);
        return ret;
    }
    Mat4 operator-(f32 s) const {
        Mat4 ret;
        __m128 sub = _mm_set1_ps(s);
        for(i32 i = 0; i < 4; i++) ret.pack[i] = _mm_sub_ps(pack[i], sub);
        return ret;
    }
    Mat4 operator*(f32 s) const {
        Mat4 ret;
        __m128 mul = _mm_set1_ps(s);
        for(i32 i = 0; i < 4; i++) ret.pack[i] = _mm_mul_ps(pack[i], mul);
        return ret;
    }
    Mat4 operator/(f32 s) const {
        Mat4 ret;
        __m128 div = _mm_set1_ps(s);
        for(i32 i = 0; i < 4; i++) ret.pack[i] = _mm_div_ps(pack[i], div);
        return ret;
    }

    Mat4 operator*=(Mat4 v) {
        *this = *this * v;
        return *this;
    }
    Mat4 operator*(Mat4 m) {
        Mat4 ret;
        for(i32 i = 0; i < 4; i++) {
            ret.pack[i] = _mm_add_ps(_mm_add_ps(_mm_mul_ps(_mm_set1_ps(m[i][0]), pack[0]),
                                                _mm_mul_ps(_mm_set1_ps(m[i][1]), pack[1])),
                                     _mm_add_ps(_mm_mul_ps(_mm_set1_ps(m[i][2]), pack[2]),
                                                _mm_mul_ps(_mm_set1_ps(m[i][3]), pack[3])));
        }
        return ret;
    }

    Mat4 T() const {
        return transpose(*this);
    }
    Mat4 Inv() const {
        return inverse(*this);
    }

    const Vec4* begin() const {
        return col;
    }
    const Vec4* end() const {
        return col + 4;
    }
    Vec4* begin() {
        return col;
    }
    Vec4* end() {
        return col + 4;
    }

    static Mat4 zero, I;
};

template<> struct Type_Info<Mat4> {
    static constexpr char name[] = "Mat4";
    static constexpr usize size = sizeof(Mat4);
    static constexpr Type_Type type = Type_Type::record_;
    static constexpr char _col[] = "col";
    using members = Type_List<Record_Field<Vec4[4], 0, _col>>;
};

// SSE matrix inverse from
// https://lxjk.github.io/2017/09/03/Fast-4x4-Matrix-Inverse-with-SSE-SIMD-Explained.html

#define MakeShuffleMask(x, y, z, w) (x | (y << 2) | (z << 4) | (w << 6))
#define VecSwizzleMask(vec, mask) _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(vec), mask))
#define VecSwizzle(vec, x, y, z, w) VecSwizzleMask(vec, MakeShuffleMask(x, y, z, w))
#define VecSwizzle1(vec, x) VecSwizzleMask(vec, MakeShuffleMask(x, x, x, x))
#define VecSwizzle_0022(vec) _mm_moveldup_ps(vec)
#define VecSwizzle_1133(vec) _mm_movehdup_ps(vec)
#define VecShuffle(vec1, vec2, x, y, z, w) _mm_shuffle_ps(vec1, vec2, MakeShuffleMask(x, y, z, w))
#define VecShuffle_0101(vec1, vec2) _mm_movelh_ps(vec1, vec2)
#define VecShuffle_2323(vec1, vec2) _mm_movehl_ps(vec2, vec1)
inline __m128 Mat2Mul(__m128 vec1, __m128 vec2) {
    return _mm_add_ps(_mm_mul_ps(vec1, VecSwizzle(vec2, 0, 3, 0, 3)),
                      _mm_mul_ps(VecSwizzle(vec1, 1, 0, 3, 2), VecSwizzle(vec2, 2, 1, 2, 1)));
}
inline __m128 Mat2AdjMul(__m128 vec1, __m128 vec2) {
    return _mm_sub_ps(_mm_mul_ps(VecSwizzle(vec1, 3, 3, 0, 0), vec2),
                      _mm_mul_ps(VecSwizzle(vec1, 1, 1, 2, 2), VecSwizzle(vec2, 2, 3, 0, 1)));
}
inline __m128 Mat2MulAdj(__m128 vec1, __m128 vec2) {
    return _mm_sub_ps(_mm_mul_ps(vec1, VecSwizzle(vec2, 3, 0, 3, 0)),
                      _mm_mul_ps(VecSwizzle(vec1, 1, 0, 3, 2), VecSwizzle(vec2, 2, 1, 2, 1)));
}
inline Mat4 inverse(Mat4 m) {
    __m128 A = VecShuffle_0101(m.pack[0], m.pack[1]);
    __m128 B = VecShuffle_2323(m.pack[0], m.pack[1]);
    __m128 C = VecShuffle_0101(m.pack[2], m.pack[3]);
    __m128 D = VecShuffle_2323(m.pack[2], m.pack[3]);

    __m128 detSub = _mm_sub_ps(_mm_mul_ps(VecShuffle(m.pack[0], m.pack[2], 0, 2, 0, 2),
                                          VecShuffle(m.pack[1], m.pack[3], 1, 3, 1, 3)),
                               _mm_mul_ps(VecShuffle(m.pack[0], m.pack[2], 1, 3, 1, 3),
                                          VecShuffle(m.pack[1], m.pack[3], 0, 2, 0, 2)));
    __m128 detA = VecSwizzle1(detSub, 0);
    __m128 detB = VecSwizzle1(detSub, 1);
    __m128 detC = VecSwizzle1(detSub, 2);
    __m128 detD = VecSwizzle1(detSub, 3);
    __m128 D_C = Mat2AdjMul(D, C);
    __m128 A_B = Mat2AdjMul(A, B);
    __m128 X_ = _mm_sub_ps(_mm_mul_ps(detD, A), Mat2Mul(B, D_C));
    __m128 W_ = _mm_sub_ps(_mm_mul_ps(detA, D), Mat2Mul(C, A_B));

    __m128 detM = _mm_mul_ps(detA, detD);
    __m128 Y_ = _mm_sub_ps(_mm_mul_ps(detB, C), Mat2MulAdj(D, A_B));
    __m128 Z_ = _mm_sub_ps(_mm_mul_ps(detC, B), Mat2MulAdj(A, D_C));
    detM = _mm_add_ps(detM, _mm_mul_ps(detB, detC));

    __m128 tr = _mm_mul_ps(A_B, VecSwizzle(D_C, 0, 2, 1, 3));
    tr = _mm_hadd_ps(tr, tr);
    tr = _mm_hadd_ps(tr, tr);
    detM = _mm_sub_ps(detM, tr);

    const __m128 adjSignMask = _mm_setr_ps(1.f, -1.f, -1.f, 1.f);
    __m128 rDetM = _mm_div_ps(adjSignMask, detM);

    X_ = _mm_mul_ps(X_, rDetM);
    Y_ = _mm_mul_ps(Y_, rDetM);
    Z_ = _mm_mul_ps(Z_, rDetM);
    W_ = _mm_mul_ps(W_, rDetM);

    Mat4 r;
    r.pack[0] = VecShuffle(X_, Y_, 3, 1, 3, 1);
    r.pack[1] = VecShuffle(X_, Y_, 2, 0, 2, 0);
    r.pack[2] = VecShuffle(Z_, W_, 3, 1, 3, 1);
    r.pack[3] = VecShuffle(Z_, W_, 2, 0, 2, 0);
    return r;
}

inline Mat4 Mat4::transpose(Mat4 m) {
    Mat4 ret;
    for(i32 i = 0; i < 4; i++)
        for(i32 j = 0; j < 4; j++) ret[i][j] = m[j][i];
    return ret;
}

inline Mat4 Mat4::ortho(f32 l, f32 r, f32 b, f32 t, f32 n, f32 f) {
    Mat4 ret;
    ret[0][0] = 2.0f / (r - l);
    ret[1][1] = 2.0f / (t - b);
    ret[2][2] = 2.0f / (n - f);
    ret[3][0] = (-l - r) / (r - l);
    ret[3][1] = (-b - t) / (t - b);
    ret[3][2] = -n / (f - n);
    return ret;
}

inline Mat4 Mat4::proj(f32 fov, f32 ar, f32 n) {
    f32 f = 1.0f / tanf(Radians(fov) / 2.0f);
    Mat4 ret;
    ret[0][0] = f / ar;
    ret[1][1] = -f;
    ret[2][2] = 0.0f;
    ret[3][3] = 0.0f;
    ret[3][2] = n;
    ret[2][3] = -1.0f;
    return ret;
}

inline Mat4 Mat4::translate(Vec3 v) {
    Mat4 ret;
    ret[3].xyz = v;
    return ret;
}

inline Mat4 Mat4::rotate(f32 a, Vec3 axis) {
    Mat4 ret;
    f32 c = cosf(Radians(a));
    f32 s = sinf(Radians(a));
    axis = normalize(axis);
    Vec3 temp = axis * (1.0f - c);
    ret[0][0] = c + temp[0] * axis[0];
    ret[0][1] = temp[0] * axis[1] + s * axis[2];
    ret[0][2] = temp[0] * axis[2] - s * axis[1];
    ret[1][0] = temp[1] * axis[0] - s * axis[2];
    ret[1][1] = c + temp[1] * axis[1];
    ret[1][2] = temp[1] * axis[2] + s * axis[0];
    ret[2][0] = temp[2] * axis[0] + s * axis[1];
    ret[2][1] = temp[2] * axis[1] - s * axis[0];
    ret[2][2] = c + temp[2] * axis[2];
    return ret;
}

inline Mat4 Mat4::scale(Vec3 s) {
    Mat4 ret;
    ret[0][0] = s.x;
    ret[1][1] = s.y;
    ret[2][2] = s.z;
    return ret;
}

inline Mat4 Mat4::look_at(Vec3 pos, Vec3 at, Vec3 up) {
    Mat4 ret = Mat4::zero;
    Vec3 F = normalize(at - pos);
    Vec3 S = normalize(cross(F, up));
    Vec3 U = cross(S, F);
    ret[0][0] = S.x;
    ret[0][1] = U.x;
    ret[0][2] = -F.x;
    ret[1][0] = S.y;
    ret[1][1] = U.y;
    ret[1][2] = -F.y;
    ret[2][0] = S.z;
    ret[2][1] = U.z;
    ret[2][2] = -F.z;
    ret[3][0] = -dot(S, pos);
    ret[3][1] = -dot(U, pos);
    ret[3][2] = dot(F, pos);
    ret[3][3] = 1.0f;
    return ret;
}

// NOTE(max): these could really just be initialized as static inline in the definition
// of Mat4, but that crashes the visual studio compiler. Thanks microsoft.
inline Mat4 Mat4::I = {{1.0f, 0.0f, 0.0f, 0.0f},
                       {0.0f, 1.0f, 0.0f, 0.0f},
                       {0.0f, 0.0f, 1.0f, 0.0f},
                       {0.0f, 0.0f, 0.0f, 1.0f}};
inline Mat4 Mat4::zero = {{0.0f, 0.0f, 0.0f, 0.0f},
                          {0.0f, 0.0f, 0.0f, 0.0f},
                          {0.0f, 0.0f, 0.0f, 0.0f},
                          {0.0f, 0.0f, 0.0f, 0.0f}};
