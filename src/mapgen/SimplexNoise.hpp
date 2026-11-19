#pragma once

#include <cstdint>

namespace mapgen {

/**
 * @brief Bộ sinh 2D Simplex Noise sử dụng số nguyên và số thực dấu phẩy tĩnh (fixed-point Q16.16).
 *
 * Đặc điểm:
 * - Không sử dụng bất kỳ phép toán float/double nào để đảm bảo tính tất định trên mọi nền tảng (x86, ARM, Win, Linux, Mac).
 * - Hỗ trợ fractal Brownian motion (nhiều octave) với tần số và biên độ điều chỉnh được.
 * - Khởi tạo bảng hoán vị 512 phần tử từ seed bằng FastRng.
 */
class SimplexNoise {
public:
    explicit SimplexNoise(uint64_t seed = 0);
    ~SimplexNoise() = default;

    // Đặt lại seed và tái tạo bảng hoán vị
    void reseed(uint64_t seed);

    // Lấy mẫu Simplex noise 2D cơ bản tại tọa độ fixed-point (xFP, yFP) với định dạng Q16.16
    [[nodiscard]] int32_t sample2D(int32_t xFP, int32_t yFP) const;

    // Lấy mẫu nhiều tầng (octaves) tại tọa độ ô (tileX, tileY)
    // freqFP: tần số cơ bản ở định dạng fixed-point Q16.16 (ví dụ 800)
    // persistenceFP: hệ số suy giảm biên độ qua từng tầng (ví dụ 32768 = 0.5 trong Q16.16)
    [[nodiscard]] int32_t sampleOctaves(int32_t tileX, int32_t tileY, int octaves, int32_t freqFP, int32_t persistenceFP) const;

private:
    uint8_t m_perm[512]{0};
};

} // namespace mapgen
