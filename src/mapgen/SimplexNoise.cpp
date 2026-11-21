#include "SimplexNoise.hpp"
#include "FastRng.hpp"

#include <algorithm>

namespace mapgen {

SimplexNoise::SimplexNoise(uint64_t seed) {
    reseed(seed);
}

void SimplexNoise::reseed(uint64_t seed) {
    FastRng rng(seed);
    uint8_t p[256];
    for (int i = 0; i < 256; ++i) {
        p[i] = static_cast<uint8_t>(i);
    }
    // Xáo trộn Fisher-Yates bằng FastRng tất định
    for (int i = 255; i > 0; --i) {
        int j = rng.range(0, i);
        std::swap(p[i], p[j]);
    }
    // Sao chép 2 lần vào bảng 512 phần tử để tránh tràn chỉ số
    for (int i = 0; i < 512; ++i) {
        m_perm[i] = p[i & 255];
    }
}

int32_t SimplexNoise::sample2D(int32_t xFP, int32_t yFP) const {
    // 8 hướng gradient chuẩn 2D
    static const int8_t GRAD2[8][2] = {
        { 1,  0}, {-1,  0}, { 0,  1}, { 0, -1},
        { 1,  1}, {-1,  1}, { 1, -1}, {-1, -1}
    };

    // 1. Skewing không gian tọa độ: F2 = (sqrt(3) - 1) / 2 ~ 23988 / 65536
    int64_t s = ((int64_t)(xFP + yFP) * 23988LL) >> 16;
    int64_t xs = xFP + s;
    int64_t ys = yFP + s;

    // Gốc ô đơn vị nguyên trong không gian xiên
    int64_t i = xs >> 16;
    int64_t j = ys >> 16;

    // 2. Unskewing ngược lại không gian ban đầu: G2 = (3 - sqrt(3)) / 6 ~ 13849 / 65536
    int64_t t = (i + j) * 13849LL;
    int64_t X0 = (i << 16) - t;
    int64_t Y0 = (j << 16) - t;

    // Khoảng cách từ điểm gốc ô tới điểm mẫu trong fixed-point Q16.16
    int64_t x0 = xFP - X0;
    int64_t y0 = yFP - Y0;

    // Xác định tam giác đơn (simplex) hiện tại: chia làm 2 nửa
    int64_t i1 = (x0 > y0) ? 1 : 0;
    int64_t j1 = (x0 > y0) ? 0 : 1;

    // Tọa độ tương đối của đỉnh thứ hai và đỉnh thứ ba
    int64_t x1 = x0 - (i1 << 16) + 13849LL;
    int64_t y1 = y0 - (j1 << 16) + 13849LL;

    int64_t x2 = x0 - (1LL << 16) + 27698LL;
    int64_t y2 = y0 - (1LL << 16) + 27698LL;

    // Hàm tính đóng góp của từng đỉnh với đường cong bán kính suy giảm bậc 4
    auto corner = [this](int64_t ci, int64_t cj, int64_t cx, int64_t cy) -> int64_t {
        int64_t d2 = cx * cx + cy * cy;
        constexpr int64_t R2 = (1LL << 31); // 0.5 * 65536^2
        int64_t ct = R2 - d2;
        if (ct <= 0) return 0;

        // Chuẩn hóa ct về khoảng [0, 32768]
        int64_t t_norm = ct >> 16;
        int64_t t2 = (t_norm * t_norm) >> 15;
        int64_t t4 = (t2 * t2) >> 15;

        uint32_t ui = static_cast<uint32_t>(ci) & 255;
        uint32_t uj = static_cast<uint32_t>(cj) & 255;
        int gi = m_perm[ui + m_perm[uj]] & 7;

        int64_t dot = GRAD2[gi][0] * cx + GRAD2[gi][1] * cy;
        return (t4 * dot) >> 15;
    };

    int64_t c0 = corner(i, j, x0, y0);
    int64_t c1 = corner(i + i1, j + j1, x1, y1);
    int64_t c2 = corner(i + 1, j + 1, x2, y2);

    return static_cast<int32_t>(c0 + c1 + c2);
}

int32_t SimplexNoise::sampleOctaves(int32_t tileX, int32_t tileY, int octaves, int32_t freqFP, int32_t persistenceFP) const {
    int64_t total = 0;
    int64_t curAmp = 65536; // Biên độ khởi đầu = 1.0 trong Q16.16
    int32_t curFreq = freqFP;

    for (int o = 0; o < octaves; ++o) {
        int32_t val = sample2D(tileX * curFreq, tileY * curFreq);
        total += (val * curAmp) >> 16;

        curAmp = (curAmp * persistenceFP) >> 16;
        curFreq <<= 1; // Nhân đôi tần số qua từng octave
    }

    return static_cast<int32_t>(total);
}

} // namespace mapgen
