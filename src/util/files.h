
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace File {
std::optional<std::vector<unsigned char>> read(std::string path);
}
