# Changelog

Mọi thay đổi đáng chú ý của dự án War Strategy sẽ được ghi nhận tại đây.

## [M9] - 2026-11-20
### Đã thêm (Added)
- **Chuẩn hóa không gian tham số AI (`src/ai/AiParams.hpp`)**:
  - Hàm `clampParams(AiParams& p)` kiểm soát chặt chẽ biên vật lý và chiến thuật: `decisionIntervalTicks` [8, 40], `aggression` [0.15, 1.50], `retreatHpRatio` [0.05, 0.70], `commanderSafeRadiusTiles` [4.0, 22.0], `artilleryStandOffMargin` [1.0, 6.0], `artilleryIdealRangeRatio` [0.50, 0.95], `scoutingWeight` [0.10, 3.00], `focusFireWeight` [0.20, 3.00], `targetMemoryTicks` [20, 400], `scoutingGroupCount` [1, 6].
  - Cơ chế vệ sinh số thực (sanitization): tự động triệt tiêu `NaN`, `Inf` về giá trị mặc định hợp lý trước khi clamp.
  - Toán tử so sánh `operator==` và `operator!=` cho `AiParams`.
- **Metadata huấn luyện & JSON Round-Trip (`src/ai/AiTelemetry.hpp`)**:
  - Struct `AiTrainingMetadata` lưu trữ `generation`, `fitness`, `winRateVsBaseline`, `trainedMatches`, `algorithm`, `timestamp`.
  - Các hàm tiện ích `saveTrainedAiProfile` và `loadTrainedAiProfile`: hỗ trợ cả định dạng mới có metadata bọc ngoài lẫn định dạng JSON phẳng cũ, tự động gọi `clampParams` sau khi đọc để đảm bảo tính an toàn dữ liệu.
- **Module Huấn luyện tự học thuần C++20 (`src/ai/AiTrainer.hpp`, `AiTrainer.cpp`)**:
  - Nằm trong thư viện tĩnh `war_sim_core`, 100% không phụ thuộc SDL2, đảm bảo thread-safe tuyệt đối (không chia sẻ trạng thái static/global mutable giữa các luồng).
  - **Mirror Match Evaluator**: Hàm `evaluatePair` tổ chức 2 trận đối xứng trên cùng 1 seed (Trận A: Candidate Blue vs Opponent Red; Trận B: Opponent Blue vs Candidate Red) nhằm triệt tiêu hoàn toàn sai lệch lợi thế sân nhà hoặc thứ tự duyệt phe.
  - **Hàm điểm Fitness liên tục**: Kết hợp điểm phân loại kết quả (Diệt Tư lệnh: +1000, Quét sạch quân: +800, Timeout: +300; Thua trừ điểm đối xứng) + Thưởng thắng sớm (`speedBonus`) + Điểm định hình (`shaping`: chênh lệch quân số sống x0.25, số đơn vị sống x5.0, máu Tư lệnh x2.0).
  - **Thuật toán 1 — Tiến hóa quần thể (`--mode evo`)**: Quần thể gồm $N$ cá thể `AiParams`, đánh giá qua Mirror Match với Baseline M7 và Hall of Fame; áp dụng chọn lọc tinh hoa (Elitism), lai ghép (Crossover) và đột biến hạ nhiệt (Annealing Mutation) tất định qua `FastRng`; lưu profile ngay khi phá kỷ lục.
  - **Thuật toán 2 — Thích nghi sau trận thua (`--mode adapt`)**: Chẩn đoán nguyên nhân thất bại qua `MatchDiagnosis` (`CommanderAssassinated`, `TroopAttrition`, `OutgunnedOrTimeout`), gọi `adaptParamsAfterLoss` tinh chỉnh tham số có định hướng và tiến hành tái đấu kiểm chứng (Validation Rematch).
  - **Showcase Replay**: Tự động sinh replay trận biểu diễn Champion AI vs Baseline M7 (`ReplayRecorder`) sau khi kết thúc huấn luyện để xem trực quan trên đồ họa SDL2.
- **Chương trình Huấn luyện đa luồng độc lập (`src/train/main_train.cpp` -> `War_Strategy_Train`)**:
  - Hỗ trợ đầy đủ cờ CLI: `--mode <evo|adapt>`, `--generations`, `--population`, `--seeds-per-gen`, `--max-ticks`, `--threads`, `--seed`, `--resume`, `--out-best`, `--log`, `--save-showcase-replay`, `--live`, `--scroll`, `--quiet`, `--print-every`, `--help`.
  - Hiển thị trực quan trạng thái tiến độ thời gian thực kết hợp **đúng 2 dòng telemetry suy nghĩ của AI1[BLUE] và AI2[RED ]** lấy thread-safe từ Worker 0.
  - Bắt tín hiệu `SIGINT` (Ctrl+C): dừng mượt sau trận đang chạy và lưu ngay `bestParams` tốt nhất ra `--out-best`.
- **Tích hợp bộ não AI vào Game chính SDL2 (`src/app/main.cpp`, `GameApp`)**:
  - Thêm cờ `--ai-params <file>` cho `War_Strategy`.
  - Tự động nạp `data/ai_best_params.json` cho `m_enemyAI` nếu có (nếu không truyền `--ai-params` và không ở chế độ `--replay`); fallback êm ái về mặc định.
  - Hiển thị nguồn AI trên HUD (`AI: Trained (Gen X)` / `AI: Custom (<file>)` / `AI: Default`). Chế độ Replay giữ nguyên không bị ảnh hưởng.
- **Bộ kiểm thử tự động toàn diện (`tests/test_train.cpp`)**:
  - `testParamsBoundsAndJsonRoundTrip`: Đột biến 100 lần (kể cả âm, cực lớn, NaN, Inf), xác nhận 100% nằm trong bounds và JSON round-trip khớp 100%.
  - `testMirrorMatchDeterminism`: Xác nhận 2 lần chạy Mirror Match độc lập cùng seed cho kết quả giống nhau 100%.
  - `testPostLossAdaptation`: Xác nhận các kịch bản thua do Tư lệnh chết hoặc mất quân đều điều chỉnh đúng hướng mong đợi và luôn hợp lệ.
  - `testMiniEvolutionEndToEnd`: Chạy mini evolution 2 thế hệ x 4 cá thể x 2 luồng, không data race, xuất đủ file JSON + CSV và nạp lại vào `TacticalAI` ra lệnh mượt mà.
  - Toàn bộ 8/8 test suite của dự án đạt tỉ lệ PASS 100%.

## [M8] - 2026-11-20
### Đã thêm (Added)
- **Tách kiến trúc thư viện lõi `war_sim_core` (Pure C++20, No SDL2)**:
  - Gom toàn bộ logic mô phỏng chiến trường (`src/mapgen/`, `src/sim/`, `src/ai/`, `src/replay/`) thành thư viện tĩnh `war_sim_core`, hoàn toàn không phụ thuộc SDL2 hay SDL2_ttf.
  - Executable đồ họa `War_Strategy` liên kết `war_sim_core` + SDL2 + SDL2_ttf.
  - Executable mới `War_Strategy_Headless` CHỈ liên kết `war_sim_core`, chạy tốc độ cao độc lập trên terminal không cần môi trường GUI.
  - Toàn bộ 7 bộ unit test tái sử dụng `war_sim_core` giúp tối ưu hóa thời gian biên dịch.
- **AI Telemetry & Hiển thị 2 dòng Console (`src/ai/AiTelemetry.hpp`)**:
  - `AiThoughtSnapshot`: Cấu trúc dữ liệu gọn nhẹ ghi nhận trạng thái suy nghĩ tức thời của AI (Posture: `SCOUTING`, `ADVANCING`, `ENGAGING`, `DEFEND_CMD`, `RETREATING`, `ALL_OUT`; Ý đồ nổi bật `primaryIntent`: `CmdFlee`, `ArtyStrike`, `AirHunt`, `B2Raid`, `NavalSupport`, `FlankPush`, `HoldForest`, `ScoutFront`; số địch thấy/nhớ qua FoW; phân bổ vai trò `Attacking`/`Scouting`/`Retreating`; đếm số lệnh phát ra).
  - Hàm `formatTelemetryLine`: Định dạng đúng chuẩn 2 dòng trên console, căn thẳng cột bằng ký tự ASCII sạch, không bao giờ lỗi font trên CMD Windows.
  - Hỗ trợ 3 chế độ hiển thị: `--live` (cập nhật tại chỗ 2 dòng qua ANSI cursor-up `\033[2A\r` + Win32 `ENABLE_VIRTUAL_TERMINAL_PROCESSING`), `--scroll` (cuộn từng cặp 2 dòng), `--quiet` (chỉ in tóm tắt kết quả).
  - Điều tiết I/O console tối đa ~20fps để đảm bảo tốc độ mô phỏng đạt tối đa (~70x - 80x realtime).
- **Hệ thống Replay tất định 100% (`src/replay/Replay.hpp`, `Replay.cpp`)**:
  - `ReplayRecorder`: Ghi nhận toàn bộ lệnh `Command` đi vào `WorldSim` theo từng tick qua `CommandHook`, lưu file JSON chuẩn (`seed`, `maxTicks`, `ai1Params`, `ai2Params`, metadata kết quả và payload danh sách lệnh).
  - `ReplayPlayer`: Nạp file replay và bơm chính xác các lệnh vào `WorldSim` tại đúng tick tương ứng mà không cần chạy `TacticalAI`.
  - Tích hợp xem Replay trên bản đồ họa SDL2: Chạy trực tiếp `War_Strategy --replay <file.json>`. Khóa quyền điều khiển quân của người chơi, phím `Space` tạm dừng/tiếp tục, phím `[` và `]` hoặc `-` / `+` điều chỉnh tốc độ phát (`0.5x`, `1x`, `2x`, `4x`, `8x`), HUD hiển thị trạng thái `REPLAY MODE` và tiến độ tick.
- **Chương trình Headless Runner (`src/headless/main_headless.cpp`)**:
  - Hỗ trợ đầy đủ cờ CLI: `--matches <N>`, `--seed <uint64>`, `--max-ticks <N>`, `--ai1-params <file>`, `--ai2-params <file>`, `--ai1-agg <val>`, `--ai2-agg <val>`, `--print-every <ticks>`, `--live`, `--scroll`, `--quiet`, `--log <file.csv>`, `--save-replay <file>`, `--replay-dir <dir>`, `--help`.
  - Ghi log CSV tổng hợp từng trận: `match_id,seed,winner,end_reason,ticks,sim_seconds,wall_ms,ai1_units,ai1_troops,ai1_cmd_hp,ai2_units,ai2_troops,ai2_cmd_hp,ai1_cmds,ai2_cmds`.
  - Bảng tổng kết ANSI/ASCII cuối đợt: thống kê tỉ số AI1/AI2/Draw, phân loại lý do kết thúc, đo lường tốc độ mô phỏng trung bình (`ticks/sec` và `x realtime`).
- **Kiểm thử tự động (`tests/test_replay.cpp`)**:
  - `testReplayDeterminism`: Chạy trận đấu AI vs AI 600 ticks ghi replay -> phát lại trên `WorldSim` mới độc lập -> chứng minh 100% (56/56 đơn vị) khớp tuyệt đối về tọa độ X/Y, HP, quân số và kết quả trận đấu giữa Live và Replay.
  - `testAiTelemetry`: Kiểm tra snapshot trạng thái AI và định dạng 2 dòng CMD.
  - `testBatchAndCsvLog`: Kiểm tra chạy batch nhiều trận, ghi file CSV và tính bất biến kết quả theo seed.

## [M7] - 2026-11-20
### Đã thêm (Added)
- **Module Trí tuệ nhân tạo (Tactical AI - `src/ai/`)**:
  - `TacticalAI`: Thiết kế class AI nhận tham số `Faction` điều khiển độc lập, thuần C++20, không phụ thuộc SDL2/Render, sẵn sàng cho chế độ Headless AI vs AI (M8) và huấn luyện tự học (M9).
  - `AiParams`: Cấu trúc tham số chiến thuật tập trung (`decisionIntervalTicks`, `aggression`, `retreatHpRatio`, `commanderSafeRadiusTiles`, `artilleryStandOffMargin`, `scoutingWeight`, `focusFireWeight`, `targetMemoryTicks`, v.v.).
  - Giao tiếp thuần túy qua struct `Command` của `WorldSim`, không can thiệp trực tiếp dữ liệu `Unit`.
  - Đảm bảo tính tất định 100% bằng bộ sinh số ngẫu nhiên `FastRng` theo seed, không dùng `std::rand` hay đồng hồ hệ thống.
  - Tuân thủ nghiêm ngặt Sương mù chiến tranh (Fog of War): chỉ quan sát địch qua `VisionSystem::isUnitVisibleToFaction`, tôn trọng tuyệt đối cơ chế tàng hình của Tư lệnh ngoài 50m, quản lý vị trí cuối cùng thấy địch (`EnemyMemoryRecord`) với thời gian hết hạn (`targetMemoryTicks`).
- **Chiến thuật thông minh cho 5 quân chủng**:
  - **Tư lệnh (Commander)**: Ưu tiên bảo toàn tính mạng, tự động tháo chạy theo hướng ngược lại hiểm họa khi địch lọt vào bán kính nguy hiểm hoặc khi chịu sát thương; ưu tiên chọn ô an toàn tại Rừng/Đồi hoặc phía sau cụm quân chủ lực.
  - **Pháo binh (Caesar)**: Giữ cự ly tầm xa stand-off lý tưởng (80% range), tự động lùi né khi địch áp sát dưới 5 ô, ưu tiên nã pháo vào cụm bộ binh và pháo/tăng địch lộ diện.
  - **Phòng không (Gepard)**: Ưu tiên phát hiện và đánh chặn không quân địch (`Su-57`, `B-2 Spirit`); khi không có máy bay địch sẽ hộ tống bám sát Caesar và Tư lệnh để che chắn phòng không.
  - **Không quân (Su-57 & B-2 Spirit)**: Su-57 tuần tra trinh sát sâu mở sương mù và săn máy bay địch; B-2 Spirit ưu tiên oanh tạc các mục tiêu giá trị cao (Caesar, tăng hạng nặng, Tư lệnh) và chủ động né tránh các vị trí đã phát hiện pháo Gepard của đối phương.
  - **Thủy quân (Mogami & Asagiri)**: 100% chỉ di chuyển trên các ô biển hợp lệ, tuần duyên mở rộng tầm nhìn bờ biển, ưu tiên tiêu diệt tàu chiến địch hoặc chi viện hỏa lực từ biển vào các mục tiêu lục quân địch ven bờ.
  - **Bộ binh & Tăng thiết giáp**: Chia mũi trinh sát tiền tiêu (Puma) mở sương mù đa hướng, tăng chủ lực (Leopard 2A, T-14) tiến công chiếm giữ các vị trí xung yếu, bộ binh tận dụng địa hình Rừng/Đồi để phòng thủ và phục kích; tự động rút lui khi máu giảm dưới ngưỡng `retreatHpRatio`.
- **Tích hợp ứng dụng & Debug**:
  - Tích hợp `TacticalAI` điều khiển phe Enemy đồng bộ theo chu kỳ tick của `WorldSim` trong `GameApp`.
  - Phím `F3`: Bật/tắt AI phe Enemy trong thời gian thực, có hiển thị trạng thái trên HUD và thông báo trực quan.
- **Kiểm thử tự động (`tests/test_ai.cpp`)**:
  - Bộ kiểm thử 4/4 kịch bản: Tính tất định 100% giữa các lần chạy, Tuân thủ Fog-of-War và Tư lệnh tàng hình, Hành vi chiến thuật 5 quân chủng (Commander retreat, Ship on water only, Caesar stand-off), và trận đấu mô phỏng AI vs AI headless 200 ticks.

## [M5] - 2026-11-20
### Đã thêm (Added)
- **Hệ thống chiến đấu cơ bản (Basic Combat)**:
  - Công thức sát thương fixed-point `int64_t` tỉ lệ 1000 (không làm tròn từng phát, chỉ áp mức tối thiểu 1 ở tổng sát thương cả đợt).
  - Nội suy tuyến tính liên tục giữa các mốc giáp (bỏ toàn bộ ngưỡng bậc thang cho HE, AP, súng bộ binh).
  - Tự động chọn vũ khí gây tổng sát thương lớn nhất lên mục tiêu đang nhắm.
  - Tự động quét và nhắm mục tiêu địch gần nhất trong tầm bắn theo quy tắc miền hoạt động (`canTargetAir/Ground/Sea`).
  - Quy tắc tàu chiến: chỉ tàu mới bắn được tàu, tàu bắn được tàu khác và bắn vào bờ, không bắn máy bay.
  - Sát thương nổ lan (Splash): tính độc lập theo giáp và phân loại của từng chấm địch bị trúng trong bán kính splash (hệ số 55%).
  - Cơ chế đạn nổ và đạn xuyên giáp chống hạm (AP) cho tàu Mogami và Asagiri.
  - Trạng thái tất định 100% nhờ bộ sinh số `FastRng` trong `WorldSim`, duyệt các chấm theo thứ tự id cố định.
- **Điều kiện thắng thua & Hết giờ**:
  - Thua khi Tư lệnh chết. Thắng khi Tư lệnh địch chết hoặc địch hết quân (chỉ còn tư lệnh). Hòa khi cả hai bên cùng hết quân.
  - Hết thời gian (`matchDurationSec`, mặc định 3600s): bên có tổng quân gộp nhiều hơn thắng; bằng nhau thì hòa.
  - Khi trận đấu kết thúc: dừng mô phỏng, hiện bảng thông báo kết quả giữa màn hình (Thắng / Thua / Hòa và lý do).
- **Hiển thị & Giao diện**:
  - Vẽ đường đạn ngắn (tracers) giữa chấm bắn và mục tiêu, tự động biến mất sau 3 tick.
  - Hiển thị đồng hồ đếm ngược thời gian ván đấu ở giữa phía trên màn hình.
  - Thanh máu và quân số cập nhật thời gian thực khi chịu sát thương.
  - Chế độ Debug phím `F1`: đổi phe đang điều khiển giữa Player (Xanh) và Enemy (Đỏ), có nhãn thông báo trực quan.
- **Công cụ & Kiểm thử**:
  - `tools/calc_ttk.py`: Script tính toán và cân bằng chỉ số TTK khớp 100% với code C++.
  - `tests/test_combat.cpp`: Bộ kiểm thử toàn diện khóa công thức sát thương, quy tắc nhắm bắn, splash, suy giảm hỏa lực do thương vong, điều kiện thắng thua và tính tất định.

## [M4] - 2026-11-20
### Đã thêm (Added)
- Hệ thống chọn quân chuột đơn lẻ, click chọn và kéo vùng chữ nhật (box selection).
- Thuật toán tìm đường A* 8 hướng tránh góc cắt chéo, rải quân thông minh phân tán đích đến.
- Bảng thông tin đơn vị (info panel) hiển thị loại, quân số, máu, tốc độ và giáp.

## [M3] - 2026-11-20
### Đã thêm (Added)
- Nạp cấu hình 11 loại đơn vị và 8 loại địa hình từ `data/game_config.json`.
- Sinh 30 chấm quân mỗi bên đối xứng qua bản đồ với tỉ lệ quân số thực tế.

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
