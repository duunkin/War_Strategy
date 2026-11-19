# Changelog

Mọi thay đổi đáng chú ý của dự án War Strategy sẽ được ghi nhận tại đây.

## [M2] - 2026-11-19
### Đã thêm (Added)
- **Sinh bản đồ tất định (Map Generation)**:
  - Bộ sinh số ngẫu nhiên giả 64-bit `FastRng` (SplitMix64) thuần số nguyên, không dùng `std::rand`.
  - Thuật toán `SimplexNoise` 2D và đa tầng (octaves) tính toán hoàn toàn bằng số nguyên và fixed-point (Q16.16), đảm bảo cùng seed ra cùng kết quả 100% trên Windows, Linux và macOS.
  - Thuật toán hạ thấp độ cao viền mép bản đồ (border falloff) đảm bảo 4 viền ngoài cùng luôn là biển.
  - Cơ chế phân loại độ cao: sắp xếp độ cao toàn bộ 12.288 ô và chọn 1/6 số ô thấp nhất (2.048 ô) làm biển, đảm bảo chuẩn xác tỉ lệ 5 đất : 1 biển.
  - Đủ 7 loại địa hình lưu trong `sim::WorldMap`: Biển (Sea), Đất bằng (Plain), Đồi (Hill), Núi (Mountain), Rừng (Forest), Sông (River), Hồ (Lake).
  - Rừng sinh từ tầng Simplex noise thứ hai trên các ô đất bằng.
  - Sông bắt nguồn từ vùng núi/đồi cao, chảy uốn lượn xuống thấp ra biển hoặc hồ.
  - Hồ nước ngọt nội địa sinh tại các vùng thung lũng trũng sâu hoặc điểm kết thúc của dòng chảy.
  - `MapGenConfig`: Struct tập trung toàn bộ tham số và ngưỡng sinh bản đồ để dễ dàng tinh chỉnh.
- **Hiển thị & Giao diện (Render)**:
  - `MapRenderer`: Tô màu từng ô theo loại địa hình với bảng màu chiến trường trực quan, có culling tối ưu hiệu năng.
  - Hệ thống `Minimap` ở góc dưới phải màn hình (kích thước 256x192) hiển thị toàn bộ bản đồ thế giới và khung nhìn camera hiện tại.
  - Hỗ trợ click hoặc kéo chuột trên Minimap để camera nhảy tới vị trí tương ứng.
  - Phím `R` và `F5` (`SDL_SCANCODE_R`, `SDL_SCANCODE_F5`): Sinh bản đồ mới với seed ngẫu nhiên.
  - Nút bấm chuột "Tao ban do moi" ở góc trên bên phải màn hình.
  - Thông báo "Seed moi: <seed>" hiển thị nổi bật trong 2 giây khi đổi bản đồ.
  - Hiện thời gian build ở tiêu đề cửa sổ (`__DATE__ __TIME__`).
- **Kiểm thử (Tests)**:
  - Thêm bộ test tự động `tests/test_mapgen.cpp` tích hợp `ctest` kiểm tra tính tất định theo hash, tỉ lệ 5 đất : 1 biển, viền biển và sự hiện diện của 7 loại địa hình.
### Đã sửa (Fixed)
- Sửa lỗi phím R không sinh lại bản đồ: chuyển sang dùng scancode phần cứng `SDL_SCANCODE_R` và `SDL_SCANCODE_F5`, lọc `repeat == 0`, dọn dẹp các log chẩn đoán vào macro `WS_DEBUG_INPUT`.

## [M1] - 2026-11-19
### Đã thêm (Added)
- Khởi tạo cửa sổ SDL2 1280x720 và vòng lặp game chính (GameApp).
- Hệ thống Camera hỗ trợ di chuyển phím WASD / phím mũi tên, rê chuột mép màn hình, và lăn chuột thu phóng (zoom).
- Vẽ lưới ô vuông thế giới 128x96 ô (mỗi ô 32x32 pixel).
- TextRenderer hiển thị FPS góc màn hình.
