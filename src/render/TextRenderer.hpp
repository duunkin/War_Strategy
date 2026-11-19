#pragma once

#if __has_include(<SDL2/SDL_ttf.h>)
#include <SDL2/SDL_ttf.h>
#elif __has_include(<SDL_ttf.h>)
#include <SDL_ttf.h>
#else
#include "SDL_ttf.h"
#endif

#if __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif

#include <string>
#include <vector>

namespace render {

/**
 * @brief Lớp TextRenderer phụ trách khởi tạo thư viện SDL2_ttf và vẽ văn bản (FPS, thông số) lên màn hình.
 */
class TextRenderer {
public:
    TextRenderer();
    ~TextRenderer();

    // Khởi tạo thư viện TTF và nạp font chữ
    bool init(const std::vector<std::string>& candidateFontPaths, int fontSize = 16);

    // Giải phóng tài nguyên font và đóng TTF
    void cleanup();

    // Vẽ văn bản tại tọa độ (x, y) trên màn hình
    void renderText(SDL_Renderer* renderer, const std::string& text, int x, int y, SDL_Color color, bool drawBackground = true);

    // Kiểm tra xem font đã được nạp thành công chưa
    [[nodiscard]] bool isLoaded() const;

private:
    TTF_Font* m_font{nullptr};
    bool m_initialized{false};
};

} // namespace render
