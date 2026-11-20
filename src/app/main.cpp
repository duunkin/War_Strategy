#include "GameApp.hpp"

#if __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif

#include <string>

/**
 * @brief Điểm khởi đầu chính của chương trình.
 *
 * Mốc 1 -> Mốc 9:
 * - Hỗ trợ phát Replay với cờ --replay <file.json>
 * - Hỗ trợ nạp bộ não AI đã huấn luyện với cờ --ai-params <file.json>
 * - Tự động nạp data/ai_best_params.json nếu tồn tại khi chơi bình thường
 */
int main(int argc, char* argv[]) {
    std::string replayFile;
    std::string aiParamsFile;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--replay" || arg == "-r") && i + 1 < argc) {
            replayFile = argv[++i];
        } else if ((arg == "--ai-params" || arg == "-p") && i + 1 < argc) {
            aiParamsFile = argv[++i];
        }
    }

    app::GameApp app;
    if (!replayFile.empty()) {
        app.setReplayFile(replayFile);
    }
    if (!aiParamsFile.empty()) {
        app.setAiParamsFile(aiParamsFile);
    }

    return app.run();
}
