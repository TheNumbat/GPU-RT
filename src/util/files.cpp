
#include "files.h"
#include <fstream>

namespace File {

std::optional<std::vector<unsigned char>> read(std::string path) {

    std::ifstream file(path.c_str(), std::ios::ate | std::ios::binary);
    if(!file.good()) {
        return std::nullopt;
    }

    unsigned int size = (unsigned int)file.tellg();
    std::vector<unsigned char> data(size);

    file.seekg(0);
    file.read((char*)data.data(), size);
    data.resize(size);

    return {std::move(data)};
}

} // namespace File
