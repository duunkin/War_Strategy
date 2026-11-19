# Lộ trình phát triển War Strategy

- [x] **M1: Nền tảng hiển thị cơ bản**
  - Cửa sổ SDL2, vòng lặp game (Game Loop).
  - Hệ thống Camera cuộn (WASD, chuột mép màn hình, zoom lăn chuột).
  - Vẽ lưới ô vuông kích thước 128x96 ô (mỗi ô 32x32 pixel).
  - TextRenderer hiển thị FPS.
  - *Kết quả*: Hoàn thành.

- [x] **M2: Sinh bản đồ tất định & Minimap**
  - Thuật toán Simplex Noise và PRNG (SplitMix64) thuần số nguyên / fixed-point trong `src/mapgen/`.
  - Không dùng float, không dùng SDL, không dùng std::rand để đảm bảo tính tất định trên cả Windows, Linux và macOS.
  - Viền bản đồ luôn là biển nhờ thuật toán hạ độ cao giảm dần về mép (border falloff).
  - Chuẩn tỉ lệ 5 đất : 1 biển (lấy 1/6 số ô thấp nhất làm Biển).
  - Đủ 7 loại địa hình: Biển (Sea), Đất bằng (Plain), Đồi (Hill), Núi (Mountain), Rừng (Forest), Sông (River), Hồ (Lake).
  - Toàn bộ ngưỡng cấu hình đặt tập trung trong `MapGenConfig`.
  - MapRenderer tô màu từng ô theo loại địa hình; hệ thống Minimap ở góc dưới phải hỗ trợ click/drag để nhảy camera.
  - Phím `R`: sinh bản đồ ngẫu nhiên mới và hiển thị seed trên màn hình.
  - Bộ kiểm thử tự động `tests/test_mapgen.cpp` kiểm tra tính tất định, tỉ lệ đất/biển và 7 loại địa hình.
  - *Kết quả*: Hoàn thành, 100% test passed.

- [ ] **M3: Đơn vị bộ binh, di chuyển & tìm đường (A*)**
  - Chọn đơn vị, di chuyển theo nhóm, tìm đường A* có tính trọng số địa hình.
  - *Kết quả*: Điều khiển được quân trên bản đồ.

- [ ] **M4: Hệ thống chiến đấu cơ bản & Tư lệnh**
  - Tầm đánh, sát thương, giáp, điều kiện thắng/thua khi Tư lệnh chết.

- [ ] **M5: Khí tài hiện đại & Sương mù chiến tranh (Fog of War)**
  - Tăng thiết giáp, pháo binh, không quân, tàu chiến. Cơ chế sương mù tầm nhìn.

- [ ] **M6: Giao diện người dùng (UI) & Âm thanh**
  - Thanh thông tin đơn vị, ghép nhóm phím tắt (Ctrl+1..9), âm thanh hiệu ứng.

- [ ] **M7: Trí tuệ nhân tạo (AI) chiến thuật**
  - Cây hành vi (Behavior Tree), chiến thuật đánh úp, rút lui, bảo vệ Tư lệnh.

- [ ] **M8: Chế độ Headless, AI vs AI, Replay**
  - Chạy mô phỏng không giao diện tốc độ cao, ghi log lệnh và phát lại trận đấu.

- [ ] **M9: AI tự tối ưu & Huấn luyện**
  - AI tự tinh chỉnh tham số sau các trận đấu.

- [ ] **M10: Hoàn thiện, cân bằng & Đóng gói đa nền tảng**
  - Chơi ván đấu trọn vẹn, đóng gói phát hành Windows, Linux, macOS.
