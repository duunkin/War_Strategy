# War Strategy - Modern Tactical RTS Game & AI System

A modern real-time strategy (RTS) simulation engine and AI development framework built with C++20 and SDL2, featuring deterministic discrete simulation, procedural terrain generation, fog-of-war vision, combat mechanics, headless execution, and evolutionary self-play AI training.

---

## 1. Architecture Overview

- **Simulation Core (`src/sim/`)**: Deterministic tick-based state updates (20 ticks/sec in-game), spatial queries, vision & fog-of-war calculations, unit state transitions, and damage models.
- **Procedural Map Generator (`src/mapgen/`)**: Simplex-noise heightmaps, hydraulic river routing, biomes, and symmetrical deployment zone generation.
- **Tactical AI (`src/ai/TacticalAI.*`)**: Rule-based hierarchical decision engine with parameterizable behavior (`AiParams`) controlling aggression, scouting depth, focus fire, artillery stand-off margins, and retreat policies.
- **AI Self-Play & Evolution (`src/ai/AiTrainer.*`)**: Population-based evolutionary optimization and post-loss adaptation across fixed validation seeds.
- **Headless Runner (`src/headless/`)**: High-performance headless simulator running over 7,000+ ticks/sec (350x+ realtime) for benchmarking and training.

---

## 2. AI Parameters & Profiles Note

> [!IMPORTANT]
> **Lưu ý về các hồ sơ AI cũ (`data/old/`)**:
> - Các file `ai_best_*.json` ban đầu (`ai_best_params.json`, `ai_best_p1.json`, `ai_best_p2.json`, `ai_best_p3.json`, `ai_best_p4.json`, `ai_best_adapt2.json`) được huấn luyện trước mốc M9.5 khi một số tham số AI (`aggression`, `artilleryStandOffMargin`, `scoutingWeight`, `focusFireWeight`, v.v.) chưa được liên kết trực tiếp vào hành vi tác chiến thực tế.
> - Sau khi hoàn thành cải tiến M9.5, các tham số này đã có tác dụng điều khiển hành vi thực sự (được kiểm chứng qua unit test và công cụ đo độ nhạy).
> - Toàn bộ các file profile cũ đã được chuyển vào thư mục `data/old/` để lưu trữ đối sánh. Các tiến trình huấn luyện mới sẽ sinh hồ sơ tối ưu chuẩn hóa theo bộ tham số mới.

> [!WARNING]
> **Tương thích Replay (Kể từ M9.10)**:
> - Kể từ mốc M9.10, hệ thống chuyển sang giải quyết sát thương đồng thời (`Simultaneous Damage Resolution`) để triệt tiêu hoàn toàn thiên lệch phe do thứ tự thực thi ID đơn vị trong combat.
> - Do sự thay đổi cốt lõi này trong việc thu thập và áp dụng sát thương tại cùng một tick, các tệp replay cũ được ghi lại trước M9.10 sẽ không còn đảm bảo phát lại giống hệt 100% khi chạy trên phiên bản hiện tại.

---

## 3. Build & Test Instructions

### Prerequisites
- CMake 3.20+
- C++20 compatible compiler (MSVC 2022/2026, Clang 15+, GCC 12+)
- Python 3 (for sensitivity and benchmarking tools)

### Building
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Running Unit Tests (8/8 Suites via CTest)
```bash
ctest --test-dir build -C Release --output-on-failure
```

Test suites:
1. `test_mapgen`: Deterministic procedural terrain generation & river flow.
2. `test_units`: Unit stats, movement, and formation behaviors.
3. `test_pathfinding`: A* pathfinding and passable domain validation.
4. `test_combat`: Damage resolution, armor, projectile splash, and line of sight.
5. `test_vision`: Fog of war compliance and line-of-sight caching.
6. `test_ai`: AI determinism, fog compliance, tactical behavior, and parameter sensitivity effectiveness.
7. `test_replay`: Match recording, serialization, and deterministic playback.
8. `test_train`: Parameter clamping, anti-noise determinism, adaptation rules, and validation-based candidate selection.

---

## 4. Parameter Sensitivity Benchmark

To verify the impact of all AI parameters on headless matches:
```bash
python tools/measure_sensitivity.py
```
