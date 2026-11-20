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

- [x] **M7: Trí tuệ nhân tạo (AI) chiến thuật**
  - AI chiến thuật độc lập phe phái (`TacticalAI`, `AiParams`) thuần C++20, không phụ thuộc render.
  - Tuân thủ 100% sương mù chiến tranh (Fog of War) và cơ chế tàng hình của Tư lệnh.
  - Tất định 100% qua bộ sinh số `FastRng`, xuất lệnh qua struct `Command` chuẩn.
  - Chiến thuật 5 quân chủng: Tư lệnh né giao tranh tháo chạy về rừng/đồi; Caesar stand-off cự ly xa; Gepard đánh chặn máy bay và bảo vệ chỉ huy; Su-57/B-2 trinh sát, oanh tạc mục tiêu giá trị cao và né Gepard; Tàu chiến Mogami/Asagiri tuần duyên và yểm trợ hỏa lực ven bờ; Tăng thiết giáp và bộ binh trinh sát đa hướng, chiếm giữ rừng/đồi, rút lui khi trọng thương.
  - Phím `F3` bật/tắt AI phe Enemy trong runtime để kiểm thử.
  - *Kết quả*: Hoàn thành, 100% test suites passed.

- [x] **M8: Chế độ Headless, AI vs AI, Replay**
  - Tách thư viện lõi tĩnh `war_sim_core` thuần C++20 không phụ thuộc SDL2/Render; executable `War_Strategy_Headless` độc lập tốc độ cao (>1400 ticks/sec, ~70x realtime).
  - Telemetry suy nghĩ và trạng thái AI (`AiThoughtSnapshot`, `AiTelemetry`) hiển thị chính xác 2 dòng căn cột sạch trên CMD (hỗ trợ cờ `--live`, `--scroll`, `--quiet`).
  - Hệ thống Replay tất định 100% (`ReplayRecorder`, `ReplayPlayer`): lưu/phát lại từng lệnh `Command` theo tick chuẩn JSON; cùng seed + replay cho ra kết quả khớp 100% từng đơn vị.
  - Tích hợp trình phát Replay vào bản đồ họa `War_Strategy --replay <file>` với phím điều khiển `Space` (Pause/Resume), `[`/`]` (chỉnh tốc độ 0.5x..8x), HUD hiển thị tiến độ và trạng thái.
  - Bộ runner headless CLI hỗ trợ `--matches`, `--seed`, `--max-ticks`, nạp/ghi đè tham số `AiParams`, xuất log CSV tổng hợp và lưu replay hàng loạt.
  - Bộ kiểm thử tự động `tests/test_replay.cpp` kiểm tra tính tất định 100% giữa Live AI vs Replay Playback, Telemetry snapshot và Batch CSV log.
  - *Kết quả*: Hoàn thành, 7/7 test suites passed 100%.

- [ ] **M9: AI tự tối ưu & Huấn luyện**
  - AI tự tinh chỉnh tham số sau các trận đấu.

- [ ] **M10: Hoàn thiện, cân bằng & Đóng gói đa nền tảng**
  - Chơi ván đấu trọn vẹn, đóng gói phát hành Windows, Linux, macOS.
