#include "GameApp.hpp"

#if __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif

/**
 * @brief Điểm khởi đầu chính của chương trình.
 *
 * Mốc 1: Khởi tạo cửa sổ SDL2 1280x720, vẽ lưới ô 32x32 cho bản đồ 128x96,
 * điều khiển camera (WASD, mũi tên, mép chuột, zoom con lăn chuột) và hiển thị FPS.
 */
int main(int argc, char* argv[]) {
    std::string replayFile;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--replay" || arg == "-r") && i + 1 < argc) {
            replayFile = argv[++i];
        }
    }

    app::GameApp app;
    if (!replayFile.empty()) {
        app.setReplayFile(replayFile);
    }
    return app.run();
}
