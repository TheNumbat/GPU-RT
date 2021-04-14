
#pragma once

#include <util/files.h>
#include <lib/lib.h>
#include <stb/stb_image.h>

template<typename A = Mdefault> struct Image {

    explicit Image() = default;
    ~Image() = default;

    Image(const Image& src) = delete;
    Image& operator=(const Image& src) = delete;

    Image(Image&& src) = default;
    Image& operator=(Image&& src) = default;

    u32 w() const {
        return _w;
    }
    u32 h() const {
        return _h;
    }
    u32 bytes() const {
        return _w * _h * 4;
    }
    Pair<u32, u32> dim() const {
        return {_w, _h};
    }

    const u8* data() const {
        return _data.data();
    }

    bool reload(literal path) {

        auto file_data = File::read(path);
        if(!file_data.ok()) return false;

        i32 x, y;
        u8* pixels = stbi_load_from_memory(file_data.value().data(), file_data.value().size(), &x,
                                           &y, null, STBI_rgb_alpha);
        if(!pixels) return false;

        _data.clear();
        _data.extend(x * y * 4);
        _w = x;
        _h = y;

        std::memcpy(_data.data(), pixels, _data.size());
        stbi_image_free(pixels);

        return true;
    }

    static Maybe<Image> load(literal path) {
        Image ret;
        if(ret.reload(path)) {
            return Maybe(std::move(ret));
        }
        return Maybe<Image>();
    }

private:
    Vec<u8, A> _data;
    u32 _w = 0, _h = 0;
};
