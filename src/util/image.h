
#pragma once

#include <optional>
#include <sf_libs/stb_image.h>
#include <string>
#include <util/files.h>
#include <vector>

namespace Util {

struct Image {

    explicit Image() = default;
    ~Image() = default;

    Image(const Image& src) = delete;
    Image& operator=(const Image& src) = delete;

    Image(Image&& src) = default;
    Image& operator=(Image&& src) = default;

    unsigned int w() const {
        return _w;
    }
    unsigned int h() const {
        return _h;
    }
    unsigned int bytes() const {
        return _w * _h * 4;
    }

    std::pair<unsigned int, unsigned int> dim() const {
        return {_w, _h};
    }

    const unsigned char* data() const {
        return _data.data();
    }

    bool reload(std::string path) {

        auto file_data = File::read(path);
        if(!file_data.has_value()) return false;

        int x, y;
        unsigned char* pixels =
            stbi_load_from_memory(file_data.value().data(), (int)file_data.value().size(), &x, &y,
                                  nullptr, STBI_rgb_alpha);
        if(!pixels) return false;

        _data.clear();
        _data.resize(x * y * 4);
        _w = x;
        _h = y;

        std::memcpy(_data.data(), pixels, _data.size());
        stbi_image_free(pixels);

        return true;
    }

    static std::optional<Image> load(std::string path) {
        Image ret;
        if(ret.reload(path)) {
            return {std::move(ret)};
        }
        return std::nullopt;
    }

private:
    std::vector<unsigned char> _data;
    unsigned int _w = 0, _h = 0;
};

} // namespace Util
