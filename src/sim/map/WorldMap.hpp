#pragma once

#include <cstdint>
#include <vector>

/**
 * @brief Lớp WorldMap đại diện cho bản đồ thế giới trong phần mô phỏng (sim).
 *
 * Lưu ý quan trọng:
 * - Module sim là logic thuần túy, tuyệt đối KHÔNG include thư viện SDL.
 * - Tránh dùng số thực (float) cho logic game, toàn bộ tọa độ và kích thước dùng số nguyên.
 */
namespace sim {

/**
 * @brief Các loại địa hình trên bản đồ thế giới (lưu bằng số nguyên uint8_t).
 */
enum class TerrainType : uint8_t {
    Sea = 0,      // Biển (nước mặn bao quanh bản đồ)
    Plain = 1,    // Đất bằng (đồng bằng)
    Hill = 2,     // Đồi thấp
    Mountain = 3, // Núi cao
    Forest = 4,   // Rừng rậm
    River = 5,    // Sông
    Lake = 6      // Hồ nước ngọt nội địa
};

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

    // Tổng số lượng ô trên bản đồ (width * height)
    [[nodiscard]] int getTotalTiles() const;

    // Kiểm tra xem một tọa độ ô (tileX, tileY) có nằm trong bản đồ không
    [[nodiscard]] bool isInBounds(int tileX, int tileY) const;

    // Lấy loại địa hình của ô tại tọa độ (tileX, tileY)
    [[nodiscard]] TerrainType getTile(int tileX, int tileY) const;

    // Thiết lập loại địa hình của ô tại tọa độ (tileX, tileY)
    void setTile(int tileX, int tileY, TerrainType type);

    // Lấy loại địa hình theo chỉ số mảng 1D (index = y * width + x)
    [[nodiscard]] TerrainType getTileByIndex(int index) const;

    // Thiết lập loại địa hình theo chỉ số mảng 1D
    void setTileByIndex(int index, TerrainType type);

    // Truy cập trực tiếp danh sách các ô (đọc dữ liệu)
    [[nodiscard]] const std::vector<TerrainType>& getTiles() const;

    // Thiết lập lại kích thước bản đồ
    void resize(int width, int height, int tileSize = 32);

    // Điền toàn bộ bản đồ bằng một loại địa hình
    void fill(TerrainType type);

    // Đếm số lượng ô của một loại địa hình cụ thể
    [[nodiscard]] int getTileCount(TerrainType type) const;

    // Số lượng ô biển (Sea)
    [[nodiscard]] int getSeaCount() const;

    // Số lượng ô đất liền (tất cả các loại địa hình không phải Biển)
    [[nodiscard]] int getLandCount() const;

    // Tính mã băm FNV-1a 64-bit của toàn bộ bản đồ để kiểm thử tính tất định
    [[nodiscard]] uint64_t computeHash() const;

private:
    int m_width;                         // Số ô chiều ngang (128)
    int m_height;                        // Số ô chiều dọc (96)
    int m_tileSize;                      // Kích thước ô (32 pixel)
    std::vector<TerrainType> m_tiles;   // Mảng 1D chứa loại địa hình từng ô
};

} // namespace sim
