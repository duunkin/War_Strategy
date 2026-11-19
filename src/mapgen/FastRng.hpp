#pragma once

#include <cstdint>

namespace mapgen {

/**
 * @brief Bộ sinh số ngẫu nhiên giả (PRNG) 64-bit tất định dựa trên SplitMix64.
 *
 * Đặc điểm:
 * - Không sử dụng std::rand, không phụ thuộc vào hệ số libc của các hệ điều hành khác nhau.
 * - Cho cùng chuỗi số ngẫu nhiên trên Windows, Linux và macOS khi cùng seed.
 * - Hoàn toàn không dùng số thực (float/double), hiệu năng cực cao và chuẩn xác 100%.
 */
class FastRng {
public:
    explicit FastRng(uint64_t seed = 1337ULL) {
        setSeed(seed);
    }

    void setSeed(uint64_t seed) {
        // Tránh seed = 0 dẫn đến trạng thái rỗng
        m_state = (seed == 0) ? 0x853c49e6748fea9bULL : seed;
        // Warm up một vài chu kỳ để các bit khuếch tán đều
        for (int i = 0; i < 4; ++i) {
            next64();
        }
    }

    uint64_t next64() {
        uint64_t z = (m_state += 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }

    uint32_t next32() {
        return static_cast<uint32_t>(next64());
    }

    // Trả về số nguyên trong khoảng [minVal, maxVal] (bao gồm cả hai đầu)
    int32_t range(int32_t minVal, int32_t maxVal) {
        if (minVal >= maxVal) {
            return minVal;
        }
        uint32_t span = static_cast<uint32_t>(maxVal - minVal + 1);
        return minVal + static_cast<int32_t>(next32() % span);
    }

private:
    uint64_t m_state{0};
};

} // namespace mapgen
