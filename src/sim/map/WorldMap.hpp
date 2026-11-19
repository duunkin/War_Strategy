#pragma once

/**
 * @brief Lớp WorldMap đại diện cho bản đồ thế giới trong phần mô phỏng (sim).
 *
 * Lưu ý quan trọng:
 * - Module sim là logic thuần túy, tuyệt đối KHÔNG include thư viện SDL.
 * - Tránh dùng số thực (float) cho logic game, toàn bộ tọa độ và kích thước dùng số nguyên.
 */
namespace sim {

class WorldMap {
public:
    // Khởi tạo kích thước mặc định: 128x96 ô, mỗi ô 32x32 pixel
    explicit WorldMap(int width = 128, int height = 96, int tileSize = 32);
    ~WorldMap() = default;

    // Số ô theo trục ngang (cột)
    [[nodiscard]] int getWidth() const;

    // Số ô theo trục dọc (hàng)
    [[nodiscard]] int getHeight() const;

    // Kích thước của mỗi ô vuông (pixel)
    [[nodiscard]] int getTileSize() const;

    // Chiều rộng tổng thể bản đồ tính bằng pixel thế giới (width * tileSize)
    [[nodiscard]] int getWorldWidth() const;

    // Chiều cao tổng thể bản đồ tính bằng pixel thế giới (height * tileSize)
    [[nodiscard]] int getWorldHeight() const;

    // Kiểm tra xem một tọa độ ô (tileX, tileY) có nằm trong bản đồ không
    [[nodiscard]] bool isInBounds(int tileX, int tileY) const;

private:
    int m_width;     // Số ô chiều ngang (128)
    int m_height;    // Số ô chiều dọc (96)
    int m_tileSize;  // Kích thước ô (32 pixel)
};

} // namespace sim
