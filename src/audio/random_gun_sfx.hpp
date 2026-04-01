#include <string>

template<int N>
std::string random_gun_sfx(const char* prefix) {
    int idx = 1 + (rand() % N);
    return std::string(prefix) + std::to_string(idx);
}
