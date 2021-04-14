
#include "files.h"
#include <fstream>

namespace File {

Maybe<Vec<u8>> read(literal path) {

    std::ifstream file(path.str(), std::ios::ate | std::ios::binary);
    if(!file.good()) {
        return Maybe<Vec<u8>>();
    }

    u32 size = (u32)file.tellg();
    Vec<u8> data(size);

    file.seekg(0);
    file.read((char*)data.data(), size);
    data.extend(size);

    return Maybe(std::move(data));
}

} // namespace File
