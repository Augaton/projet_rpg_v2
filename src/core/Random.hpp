#pragma once
#include <random>
#include <vector>

namespace rng {

inline std::mt19937& Engine() {
    static std::mt19937 engine{ std::random_device{}() };
    return engine;
}

inline float Float(float lo = 0.0f, float hi = 1.0f) {
    std::uniform_real_distribution<float> d(lo, hi);
    return d(Engine());
}

inline int Int(int lo, int hi) {
    std::uniform_int_distribution<int> d(lo, hi);
    return d(Engine());
}

inline bool Chance(float p) { return Float() < p; }

template <class T>
const T& Pick(const std::vector<T>& v) { return v[Int(0, (int)v.size() - 1)]; }

} // namespace rng
